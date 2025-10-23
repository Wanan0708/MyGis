#include "tileworker.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QFileInfo>

TileWorker::TileWorker(QObject *parent)
    : QObject(parent)
{
    qDebug() << "TileWorker constructor called";
}

// 懒创建并复用工作线程内的 QNetworkAccessManager
QNetworkAccessManager* TileWorker::networkManager()
{
    if (!m_manager) {
        m_manager = new QNetworkAccessManager(this);
    }
    return m_manager;
}

void TileWorker::loadTileFromFile(int x, int y, int z, const QString &filePath)
{
    qDebug() << "TileWorker::loadTileFromFile called for tile:" << x << y << z << "filePath:" << filePath;
    // 将磁盘IO放入异步，避免阻塞工作线程
    QTimer::singleShot(0, this, [this, x, y, z, filePath]() {
        QFile file(filePath);
        if (!file.exists()) {
            emit tileLoadedBytes(x, y, z, QByteArray(), false,
                                 QString("Tile file does not exist: %1").arg(filePath));
            return;
        }
        if (!file.open(QIODevice::ReadOnly)) {
            emit tileLoadedBytes(x, y, z, QByteArray(), false,
                                 QString("Failed to open tile file: %1").arg(file.errorString()));
            return;
        }
        QByteArray data = file.readAll();
        file.close();
        if (data.isEmpty()) {
            emit tileLoadedBytes(x, y, z, QByteArray(), false,
                                 QString("Tile file is empty: %1").arg(filePath));
            return;
        }
        // 仅传递字节到主线程解码，避免在工作线程构建 QPixmap 导致崩溃
        emit tileLoadedBytes(x, y, z, data, true, QString());
    });
}

void TileWorker::downloadAndSaveTile(int x, int y, int z, const QString &url, const QString &filePath)
{
    qDebug() << "TileWorker::downloadAndSaveTile called for tile:" << x << y << z << "URL:" << url << "filePath:" << filePath;
    
    // 使用Qt的异步机制，在单独的线程中执行下载任务
    QTimer::singleShot(0, this, [this, x, y, z, url, filePath]() {
        downloadAndSaveTileAsync(x, y, z, url, filePath);
    });
}

void TileWorker::downloadAsync(int x, int y, int z, const QString &url, const QString &filePath)
{
    qDebug() << "TileWorker::downloadAsync called for tile:" << x << y << z << "URL:" << url << "filePath:" << filePath;
    startAsyncRequest(x, y, z, url, filePath, 0);
}

void TileWorker::downloadAndSaveTileAsync(int x, int y, int z, const QString &url, const QString &filePath)
{
    qDebug() << "TileWorker::downloadAndSaveTileAsync started for tile:" << x << y << z;
    
    // 添加重试机制
    int maxRetries = 3;
    int retryCount = 0;
    bool success = false;
    
    while (retryCount < maxRetries && !success) {
        if (retryCount > 0) {
            qDebug() << "Retrying download for tile:" << x << y << z << "attempt:" << (retryCount + 1);
            // 等待一段时间再重试
            QThread::msleep(1000 * retryCount); // 递增等待时间
        }
        
        success = performDownload(x, y, z, url, filePath);
        retryCount++;
    }
    
    if (!success) {
        qDebug() << "Failed to download tile after" << maxRetries << "attempts:" << x << y << z;
        qDebug() << "Emitting tileDownloaded signal (failed) for tile:" << x << y << z;
        emit tileDownloaded(x, y, z, QByteArray(), false, 
                           QString("Failed after %1 attempts").arg(maxRetries));
    }
}

bool TileWorker::performDownload(int x, int y, int z, const QString &url, const QString &filePath)
{
    // 复用网络访问管理器
    QNetworkAccessManager *manager = networkManager();
    
    // 设置网络配置
    QNetworkRequest request{(QUrl(url))};
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    request.setRawHeader("Accept", "image/png,image/svg+xml,image/*;q=0.8,*/*;q=0.5");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9");
    request.setRawHeader("Accept-Encoding", "gzip, deflate");
    request.setRawHeader("Connection", "keep-alive");
    request.setTransferTimeout(30000); // 30秒超时
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    
    qDebug() << "Sending request for URL:" << url;
    
    // 发送请求
    QNetworkReply *reply = manager->get(request);
    
    // 使用事件循环等待响应
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    // 设置超时定时器
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(30000); // 30秒超时
    
    qDebug() << "Starting event loop for tile:" << x << y << z;
    
    // 进入事件循环等待完成或超时
    loop.exec();
    
    qDebug() << "Event loop finished for tile:" << x << y << z;
    qDebug() << "Timeout timer active:" << timeoutTimer.isActive();
    
    // 检查是否超时
    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
        qDebug() << "Request finished normally, checking for errors";
        
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "No error in reply, reading data";
            QByteArray data = reply->readAll();
            qDebug() << "Data size:" << data.size();
            
            // 检查数据是否为空
            if (data.isEmpty()) {
                qDebug() << "Downloaded empty data for tile:" << x << y << z;
                reply->deleteLater();
                return false;
            }
            
            // 检查是否是有效的PNG数据
            if (!data.startsWith(QByteArray::fromHex("89504e47"))) { // PNG文件头
                qDebug() << "Downloaded invalid data (not PNG) for tile:" << x << y << z;
                reply->deleteLater();
                return false;
            }
            
            // 创建目录
            QDir dir(QFileInfo(filePath).path());
            if (!dir.exists()) {
                qDebug() << "Creating directory for tile:" << x << y << z;
                if (!dir.mkpath(".")) {
                    qDebug() << "Failed to create directory for tile:" << x << y << z;
                    reply->deleteLater();
                    return false;
                }
            }
            
            // 保存到文件
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly)) {
                qint64 written = file.write(data);
                file.close();
                qDebug() << "Written" << written << "bytes to file:" << filePath;
                
                if (written != data.size()) {
                    qDebug() << "Incomplete write to file for tile:" << x << y << z;
                    reply->deleteLater();
                    return false;
                } else {
                    qDebug() << "Successfully downloaded tile:" << x << y << z;
                    qDebug() << "Emitting tileDownloaded signal for tile:" << x << y << z;
                    emit tileDownloaded(x, y, z, data, true, QString());
                    reply->deleteLater();
                    return true;
                }
            } else {
                qDebug() << "Failed to save tile to file:" << filePath << "Error:" << file.errorString();
                reply->deleteLater();
                return false;
            }
        } else {
            QString errorString = reply->errorString();
            int errorCode = reply->error();
            int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            
            qDebug() << "Network error for tile:" << x << y << z 
                     << "Error code:" << errorCode 
                     << "HTTP status:" << httpStatusCode 
                     << "Error string:" << errorString;
            
            // 对于某些错误，我们不重试
            if (errorCode == QNetworkReply::ContentNotFoundError) {
                // 404错误，瓦片不存在，不重试
                qDebug() << "Tile not found (404) for tile:" << x << y << z;
                qDebug() << "Emitting tileDownloaded signal (404 error) for tile:" << x << y << z;
                emit tileDownloaded(x, y, z, QByteArray(), false, 
                                   QString("Tile not found (404)"));
                reply->deleteLater();
                return true; // 404错误不需要重试
            } else {
                reply->deleteLater();
                return false; // 其他错误需要重试
            }
        }
    } else {
        // 超时
        qDebug() << "Request timeout for tile:" << x << y << z;
        if (reply->isRunning()) {
            reply->abort();
        }
        reply->deleteLater();
        return false; // 超时需要重试
    }
}

// 添加缺失的槽函数实现（即使它们是空的）
void TileWorker::onDownloadFinished()
{
    // 这个函数在当前实现中不会被调用，因为我们使用事件循环而不是信号槽
    // 但为了满足链接器要求，我们需要提供实现
    qDebug() << "onDownloadFinished called";
}

void TileWorker::onDownloadTimeout()
{
    // 这个函数在当前实现中不会被调用，因为我们使用事件循环而不是信号槽
    // 但为了满足链接器要求，我们需要提供实现
    qDebug() << "onDownloadTimeout called";
}

// 为空实现：当前未使用，仅为满足 moc 生成的元对象调用
void TileWorker::onReplyReadyRead()
{
    // no-op
}

// 为空实现：当前未使用，仅为满足 moc 生成的元对象调用
void TileWorker::onReplyError()
{
    // no-op
}

void TileWorker::configureNetworkRetries(int retryMax, int backoffInitialMs)
{
    m_retryMax = qMax(0, retryMax);
    m_backoffInitialMs = qMax(0, backoffInitialMs);
}

void TileWorker::startAsyncRequest(int x, int y, int z, const QString &url, const QString &filePath, int attempt)
{
    QNetworkAccessManager *manager = networkManager();

    QNetworkRequest request{(QUrl(url))};
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    request.setRawHeader("Accept", "image/png,image/svg+xml,image/*;q=0.8,*/*;q=0.5");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9");
    request.setRawHeader("Accept-Encoding", "gzip, deflate");
    request.setRawHeader("Connection", "keep-alive");
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);

    QNetworkReply *reply = manager->get(request);
    reply->setProperty("tile_x", x);
    reply->setProperty("tile_y", y);
    reply->setProperty("tile_z", z);
    reply->setProperty("tile_filePath", filePath);
    reply->setProperty("tile_url", url);
    reply->setProperty("tile_attempt", attempt);

    // 超时保护（30s）
    QTimer *timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    QObject::connect(timeout, &QTimer::timeout, reply, [reply]() {
        if (reply->isRunning()) reply->abort();
    });
    timeout->start(30000);

    QObject::connect(reply, &QNetworkReply::finished, this, &TileWorker::onReplyFinished);
}

void TileWorker::onReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int x = reply->property("tile_x").toInt();
    int y = reply->property("tile_y").toInt();
    int z = reply->property("tile_z").toInt();
    QString filePath = reply->property("tile_filePath").toString();
    int attempt = reply->property("tile_attempt").toInt();

    auto emitFail = [&](const QString &err){
        emit tileDownloaded(x, y, z, QByteArray(), false, err);
        reply->deleteLater();
    };

    if (reply->error() != QNetworkReply::NoError) {
        // 404 直接视为完成但失败不重试，其它错误交给上层重试策略
        if (reply->error() == QNetworkReply::ContentNotFoundError) {
            emitFail(QStringLiteral("Tile not found (404)"));
        } else {
            if (attempt + 1 < m_retryMax) {
                int backoff = m_backoffInitialMs * qMax(1, (int)qPow(2, attempt));
                QTimer::singleShot(backoff, this, [=]() {
                    startAsyncRequest(x, y, z, reply->property("tile_url").toString(), filePath, attempt + 1);
                });
                reply->deleteLater();
                return;
            } else {
                emitFail(reply->errorString());
            }
        }
        return;
    }

    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        emitFail(QStringLiteral("Empty response"));
        return;
    }
    if (!data.startsWith(QByteArray::fromHex("89504e47"))) {
        emitFail(QStringLiteral("Invalid PNG data"));
        return;
    }

    // 确保目录存在
    QDir dir(QFileInfo(filePath).path());
    if (!dir.exists() && !dir.mkpath(".")) {
        emitFail(QStringLiteral("Failed to create directory"));
        return;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        emitFail(QStringLiteral("Failed to open file for write"));
        return;
    }
    if (f.write(data) != data.size()) {
        f.close();
        emitFail(QStringLiteral("Incomplete write"));
        return;
    }
    f.close();

    emit tileDownloaded(x, y, z, data, true, QString());
    reply->deleteLater();
}