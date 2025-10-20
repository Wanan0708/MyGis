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

TileWorker::TileWorker(QObject *parent)
    : QObject(parent)
{
    qDebug() << "TileWorker constructor called";
}

void TileWorker::loadTileFromFile(int x, int y, int z, const QString &filePath)
{
    qDebug() << "TileWorker::loadTileFromFile called for tile:" << x << y << z << "filePath:" << filePath;
    
    // 检查文件是否存在
    QFile file(filePath);
    if (!file.exists()) {
        qDebug() << "Tile file does not exist:" << filePath;
        emit tileLoaded(x, y, z, QPixmap(), false, 
                       QString("Tile file does not exist: %1").arg(filePath));
        return;
    }
    
    QPixmap pixmap(filePath);
    if (!pixmap.isNull()) {
        qDebug() << "Successfully loaded tile from file:" << filePath;
        emit tileLoaded(x, y, z, pixmap, true, QString());
    } else {
        qDebug() << "Failed to load tile from file:" << filePath;
        emit tileLoaded(x, y, z, QPixmap(), false, 
                       QString("Failed to load tile from file: %1").arg(filePath));
    }
}

void TileWorker::downloadAndSaveTile(int x, int y, int z, const QString &url, const QString &filePath)
{
    qDebug() << "TileWorker::downloadAndSaveTile called for tile:" << x << y << z << "URL:" << url << "filePath:" << filePath;
    
    // 使用Qt的异步机制，在单独的线程中执行下载任务
    QTimer::singleShot(0, this, [this, x, y, z, url, filePath]() {
        downloadAndSaveTileAsync(x, y, z, url, filePath);
    });
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
    // 创建网络访问管理器
    QNetworkAccessManager manager;
    
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
    QNetworkReply *reply = manager.get(request);
    
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