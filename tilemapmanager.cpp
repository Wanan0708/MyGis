#include "tilemapmanager.h"
#include "tileworker.h"
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QNetworkRequest>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QDebug>
#include <QThread>
#include <QMetaType>
#include <QDateTime>
#include <QTextStream>
#include <QCoreApplication>  // 添加这个头文件
#include <QEventLoop>        // 添加这个头文件
#include <QTime>             // 添加这个头文件
#include <cmath>
#include <QTimer>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 为TileKey提供hash函数
uint qHash(const TileKey &key, uint /*seed*/)
{
    return qHash(key.x) ^ qHash(key.y) ^ qHash(key.z);
}

// 日志记录函数
void logMessage(const QString &message)
{
    // 记录日志到文件
    QFile logFile("tilemap_debug.log");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " - " << message << "\n";
        logFile.close();
    }
    
    // 同时输出到控制台
    qDebug() << "[TileMapManager]" << message;
}

TileMapManager::TileMapManager(QObject *parent)
    : QObject(parent)
    , m_scene(nullptr)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_centerLat(39.9042)  // 默认北京坐标
    , m_centerLon(116.4074)
    , m_zoom(10)
    , m_tileSize(256)
    // 使用更可靠的瓦片服务器URL - 使用多个备用服务器
    , m_tileUrlTemplate("https://{server}.tile.openstreetmap.org/{z}/{x}/{y}.png")
    , m_viewportTilesX(5)
    , m_viewportTilesY(5)
    , m_regionDownloadTotal(0)
    , m_regionDownloadCurrent(0)
    , m_workerThread(nullptr)
    , m_worker(nullptr)
    , m_processTimer(new QTimer(this))
    , m_isProcessing(false)
    , m_downloadFinishedEmitted(false)
    , m_maxConcurrentRequests(10)  // 增加同时处理的请求数量到5，提高下载效率
    , m_currentRequests(0)
{
    logMessage("TileMapManager constructor started");
    
    // 注册元类型
    qRegisterMetaType<QPixmap>("QPixmap");
    qRegisterMetaType<QString>("QString");
    qRegisterMetaType<TileInfo>("TileInfo");
    
    // 创建缓存目录 - 使用项目根目录下的tilemap文件夹
    // 获取项目根目录（从当前工作目录向上查找，直到找到.pro文件）
    QString projectRoot = QDir::currentPath();
    QDir dir(projectRoot);
    
    // 如果当前在build目录中，向上查找项目根目录
    while (!dir.exists("CustomTitleBarApp.pro") && !dir.isRoot()) {
        dir.cdUp();
        projectRoot = dir.absolutePath();
    }
    
    m_cacheDir = projectRoot + "/tilemap";
    logMessage(QString("Current working directory: %1").arg(QDir::currentPath()));
    logMessage(QString("Project root directory: %1").arg(projectRoot));
    logMessage(QString("Cache directory: %1").arg(m_cacheDir));
    
    // 确保缓存目录存在
    QDir cacheDir(m_cacheDir);
    if (!cacheDir.exists()) {
        logMessage("Creating cache directory");
        if (!cacheDir.mkpath(".")) {
            logMessage("Failed to create cache directory!");
        } else {
            logMessage("Cache directory created successfully");
        }
    } else {
        logMessage("Cache directory already exists");
    }
    
    // 设置处理定时器
    m_processTimer->setSingleShot(true);
    connect(m_processTimer, &QTimer::timeout, this, &TileMapManager::processNextBatch);
    
    // 启动工作线程
    startWorkerThread();
    
    // 检查网络访问功能
    logMessage(QString("Network manager is accessible: %1").arg(m_networkManager != nullptr));
    logMessage("TileMapManager constructor finished");
}

TileMapManager::~TileMapManager()
{
    // 停止处理定时器
    if (m_processTimer) {
        m_processTimer->stop();
    }
    
    // 等待所有下载任务完成后再停止工作线程
    if (m_isProcessing && (m_currentRequests > 0 || !m_pendingTiles.isEmpty())) {
        qDebug() << "Waiting for downloads to complete before stopping worker thread";
        // 等待最多30秒让下载任务完成
        QTime waitTime = QTime::currentTime().addSecs(30);
        while ((m_currentRequests > 0 || !m_pendingTiles.isEmpty()) && QTime::currentTime() < waitTime) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        }
    }
    
    // 停止工作线程
    stopWorkerThread();
    
    // 清理资源
    cleanupTiles();
}

void TileMapManager::startWorkerThread()
{
    qDebug() << "TileMapManager::startWorkerThread called";
    if (!m_workerThread) {
        m_workerThread = new QThread(this);
        m_worker = new TileWorker;
        m_worker->moveToThread(m_workerThread);
        
        // 连接工作线程的信号和槽
        connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
        connect(this, &TileMapManager::requestDownloadTile, m_worker, &TileWorker::downloadAndSaveTile);
        connect(this, &TileMapManager::requestLoadTile, m_worker, &TileWorker::loadTileFromFile);
        connect(m_worker, &TileWorker::tileDownloaded, this, &TileMapManager::onTileDownloaded);
        connect(m_worker, &TileWorker::tileLoaded, this, &TileMapManager::onTileLoaded);
        
        m_workerThread->start();
        qDebug() << "Worker thread started";
    }
}

void TileMapManager::stopWorkerThread()
{
    if (m_workerThread) {
        qDebug() << "Stopping worker thread, current requests:" << m_currentRequests 
                 << "pending tiles:" << m_pendingTiles.size();
        m_workerThread->quit();
        // 等待线程结束，最多等待5秒
        if (!m_workerThread->wait(5000)) {
            qDebug() << "Worker thread did not finish in time, terminating";
            m_workerThread->terminate();
            m_workerThread->wait();
        }
        m_workerThread = nullptr;
        m_worker = nullptr;
        qDebug() << "Worker thread stopped";
    }
}

void TileMapManager::initScene(QGraphicsScene *scene)
{
    m_scene = scene;
}

void TileMapManager::setCenter(double lat, double lon)
{
    m_centerLat = lat;
    m_centerLon = lon;
    loadTiles();
}

void TileMapManager::setZoom(int zoom)
{
    int oldZoom = m_zoom;
    m_zoom = qBound(0, zoom, 19);  // 限制缩放级别在0-19之间
    
    qDebug() << "Changing zoom from" << oldZoom << "to" << m_zoom;
    
    // 在设置新的缩放级别后，先清理不需要的瓦片
    cleanupTiles();
    
    // 如果场景存在，调整视图中心以适应新的缩放级别
    if (m_scene) {
        // 计算当前中心点的瓦片坐标
        int centerTileX, centerTileY;
        latLonToTile(m_centerLat, m_centerLon, m_zoom, centerTileX, centerTileY);
        
        // 设置场景矩形以包含当前视图的瓦片
        int viewportTiles = qMax(m_viewportTilesX, m_viewportTilesY);
        QRectF sceneRect(0, 0, viewportTiles * m_tileSize, viewportTiles * m_tileSize);
        m_scene->setSceneRect(sceneRect);
        
        qDebug() << "Set scene rect:" << sceneRect << "for zoom:" << m_zoom;
    }
    
    // 重新定位所有已加载的瓦片
    repositionTiles();
    
    loadTiles();
    
    // 重置区域下载计数器，因为缩放级别改变后之前的区域下载任务已无效
    m_regionDownloadTotal = 0;
    m_regionDownloadCurrent = 0;
    m_downloadFinishedEmitted = false;
}

void TileMapManager::loadTiles()
{
    if (!m_scene) return;
    
    // 计算当前视图范围内的瓦片
    calculateVisibleTiles();
}

void TileMapManager::downloadRegion(double minLat, double maxLat, double minLon, double maxLon, int minZoom, int maxZoom)
{
    logMessage("TileMapManager::downloadRegion called");
    logMessage(QString("Starting region download:"));
    logMessage(QString("  Lat range: %1 to %2").arg(minLat).arg(maxLat));
    logMessage(QString("  Lon range: %1 to %2").arg(minLon).arg(maxLon));
    logMessage(QString("  Zoom range: %1 to %2").arg(minZoom).arg(maxZoom));
    
    // 清空之前的下载队列
    m_pendingTiles.clear();
    
    // 重置计数器
    m_regionDownloadTotal = 0;
    m_regionDownloadCurrent = 0;
    m_downloadFinishedEmitted = false;
    m_currentRequests = 0;
    
    // 计算所有层级的瓦片并添加到下载队列
    for (int zoom = minZoom; zoom <= maxZoom; zoom++) {
        int minTileX, minTileY, maxTileX, maxTileY;
        latLonToTile(maxLat, minLon, zoom, minTileX, minTileY); // 注意：maxLat对应minTileY
        latLonToTile(minLat, maxLon, zoom, maxTileX, maxTileY); // 注意：minLat对应maxTileY
        
        // 确保坐标顺序正确
        if (minTileX > maxTileX) qSwap(minTileX, maxTileX);
        if (minTileY > maxTileY) qSwap(minTileY, maxTileY);
        
        // 限制瓦片范围
        int maxTile = (1 << zoom) - 1;
        minTileX = qMax(0, minTileX);
        minTileY = qMax(0, minTileY);
        maxTileX = qMin(maxTile, maxTileX);
        maxTileY = qMin(maxTile, maxTileY);
        
        int totalTileCount = (maxTileX - minTileX + 1) * (maxTileY - minTileY + 1);
        int downloadTileCount = 0; // 需要下载的瓦片数量
        int existingTileCount = 0; // 已存在的瓦片数量
        
        // 优化：只将需要下载的瓦片添加到队列，已存在的瓦片直接计入进度
        for (int x = minTileX; x <= maxTileX; x++) {
            for (int y = minTileY; y <= maxTileY; y++) {
                // 检查瓦片是否已存在
                if (!tileExists(x, y, zoom)) {
                    // 瓦片不存在，添加到下载队列
                    TileInfo info;
                    info.x = x;
                    info.y = y;
                    info.z = zoom;
                    info.url = getTileUrl(x, y, zoom);
                    info.filePath = getTilePath(x, y, zoom);
                    m_pendingTiles.enqueue(info);
                    downloadTileCount++;
                } else {
                    // 瓦片已存在，直接计入完成进度
                    existingTileCount++;
                    m_regionDownloadCurrent++;  // 已存在的瓦片直接计为已完成
                }
            }
        }
        
        // 计算所有需要处理的瓦片（包括已存在的和需要下载的）
        m_regionDownloadTotal += totalTileCount;
        logMessage(QString("  Zoom %1: tiles from (%2,%3) to (%4,%5), total %6, existing %7 (skipped), need download %8").arg(zoom).arg(minTileX).arg(minTileY).arg(maxTileX).arg(maxTileY).arg(totalTileCount).arg(existingTileCount).arg(downloadTileCount));
    }
    
    logMessage(QString("Total tiles to process: %1, already completed: %2, need download: %3")
               .arg(m_regionDownloadTotal)
               .arg(m_regionDownloadCurrent)
               .arg(m_pendingTiles.size()));
    
    // 发送初始进度（显示已存在的瓦片进度）
    if (m_regionDownloadCurrent > 0) {
        emit regionDownloadProgress(m_regionDownloadCurrent, m_regionDownloadTotal, minZoom);
    }
    
    if (m_pendingTiles.isEmpty()) {
        logMessage("All tiles already exist locally, emitting downloadFinished");
        m_downloadFinishedEmitted = true;
        emit downloadFinished();
        return;
    }
    
    // 开始处理过程
    logMessage("Starting download process");
    m_isProcessing = true;
    m_processTimer->start(0); // 立即开始处理
}

void TileMapManager::processNextBatch()
{
    qDebug() << "processNextBatch called, isProcessing:" << m_isProcessing 
             << "pendingTiles:" << m_pendingTiles.size() 
             << "currentRequests:" << m_currentRequests;
    
    if (!m_isProcessing) {
        return;
    }
    
    // 检查是否所有瓦片都已处理完毕
    if (m_pendingTiles.isEmpty() && m_currentRequests == 0) {
        qDebug() << "All tiles processed, calling checkAndEmitDownloadFinished";
        checkAndEmitDownloadFinished();
        return;
    }
    
    // 检查是否达到最大并发请求数
    if (m_currentRequests >= m_maxConcurrentRequests) {
        qDebug() << "Max concurrent requests reached, waiting";
        // 确保定时器不会重复启动
        if (!m_processTimer->isActive()) {
            m_processTimer->start(100); // 缩短等待时间到100ms
        }
        return;
    }
    
    // 处理一个瓦片（队列中只包含需要下载的瓦片）
    if (!m_pendingTiles.isEmpty() && m_currentRequests < m_maxConcurrentRequests) {
        TileInfo info = m_pendingTiles.dequeue();
        
        qDebug() << "Processing tile:" << info.x << info.y << info.z << "URL:" << info.url;
        qDebug() << "Remaining tiles in queue:" << m_pendingTiles.size();
        
        // 队列中的瓦片都是需要下载的，直接下载
        qDebug() << "Downloading tile:" << info.x << info.y << info.z;
        m_currentRequests++;
        emit requestDownloadTile(info.x, info.y, info.z, info.url, info.filePath);
        
        // 继续处理下一个批次
        if (!m_pendingTiles.isEmpty() || m_currentRequests > 0) {
            // 确保定时器不会重复启动
            if (!m_processTimer->isActive()) {
                qDebug() << "Starting process timer for next batch";
                m_processTimer->start(100); // 缩短间隔到100ms，提高响应速度
            }
        }
    }
}

void TileMapManager::onTileDownloaded(int x, int y, int z, const QByteArray &data, bool success, const QString &errorString)
{
    qDebug() << "TileMapManager::onTileDownloaded called for tile:" << x << y << z << "success:" << success;
    
    QMutexLocker locker(&m_mutex);
    
    // 减少当前请求数（确保不会小于0）
    m_currentRequests = qMax(0, m_currentRequests - 1);
    
    // 只有在区域下载模式下才更新进度计数器
    bool isRegionDownloadMode = (m_regionDownloadTotal > 0);
    if (isRegionDownloadMode) {
        // 只有实际下载成功时才更新进度计数器（本地加载不计数）
        if (success) {
            m_regionDownloadCurrent++;
            qDebug() << "Updated process count (download):" << m_regionDownloadCurrent << "/" << m_regionDownloadTotal;
        } else {
            qDebug() << "Download failed, not updating progress count";
        }
    }
    
    if (success) {
        qDebug() << "Tile downloaded successfully, saving data size:" << data.size();
        // 保存瓦片到本地
        saveTile(x, y, z, data);
        
        // 创建图片项（仅在场景存在时添加）
        if (m_scene) {
            QPixmap pixmap;
            pixmap.loadFromData(data);
            if (!pixmap.isNull()) {
                QGraphicsPixmapItem *item = m_scene->addPixmap(pixmap);
                // 计算瓦片位置：根据当前缩放级别和瓦片坐标
                // 需要将瓦片坐标转换为当前视图的坐标
                int centerTileX, centerTileY;
                latLonToTile(m_centerLat, m_centerLon, m_zoom, centerTileX, centerTileY);
                
                // 计算瓦片相对于中心瓦片的位置
                double tileX = (x - centerTileX + m_viewportTilesX/2) * m_tileSize;
                double tileY = (y - centerTileY + m_viewportTilesY/2) * m_tileSize;
                item->setPos(tileX, tileY);
                TileKey key = {x, y, z};
                m_tileItems[key] = item;
                qDebug() << "Added tile at position:" << tileX << "," << tileY << "for zoom:" << z << "center:" << centerTileX << "," << centerTileY;
            }
        }
    } else {
        qDebug() << "Tile download failed:" << errorString;
    }
    
    // 只有在区域下载模式下才发送进度信号
    if (isRegionDownloadMode) {
        // 发送区域处理进度信号
        qDebug() << "Emitting regionDownloadProgress signal:" << m_regionDownloadCurrent << "/" << m_regionDownloadTotal << "zoom:" << z;
        emit regionDownloadProgress(m_regionDownloadCurrent, m_regionDownloadTotal, z);
        
        // 检查是否所有任务都已完成
        if (m_regionDownloadTotal > 0 && m_regionDownloadCurrent >= m_regionDownloadTotal && m_currentRequests == 0) {
            if (!m_downloadFinishedEmitted) {
                m_downloadFinishedEmitted = true;
                m_isProcessing = false;
                emit downloadFinished();
            }
        } else {
            // 维持处理流程
            if (m_isProcessing && (m_pendingTiles.size() > 0 || m_currentRequests > 0)) {
                // 确保定时器不会重复启动
                if (!m_processTimer->isActive()) {
                    qDebug() << "Starting process timer after tile download";
                    m_processTimer->start(100);
                }
            } else if (m_pendingTiles.isEmpty() && m_currentRequests == 0) {
                // 所有任务完成，发送完成信号
                if (!m_downloadFinishedEmitted) {
                    m_downloadFinishedEmitted = true;
                    m_isProcessing = false;
                    emit downloadFinished();
                }
            }
        }
    }
}

void TileMapManager::onTileLoaded(int x, int y, int z, const QPixmap &pixmap, bool success, const QString &errorString)
{
    qDebug() << "onTileLoaded called for tile:" << x << y << z << "success:" << success;
    
    QMutexLocker locker(&m_mutex);
    
    // 减少当前请求数（确保不会小于0）
    m_currentRequests = qMax(0, m_currentRequests - 1);
    
    if (success && !pixmap.isNull()) {
        qDebug() << "Tile loaded successfully from local file";
        // 创建图片项（仅在场景存在时添加）
        if (m_scene) {
            // 检查场景是否有效
            QGraphicsPixmapItem *item = m_scene->addPixmap(pixmap);
            // 计算瓦片位置：根据当前缩放级别和瓦片坐标
            // 需要将瓦片坐标转换为当前视图的坐标
            int centerTileX, centerTileY;
            latLonToTile(m_centerLat, m_centerLon, m_zoom, centerTileX, centerTileY);
            
            // 计算瓦片相对于中心瓦片的位置
            double tileX = (x - centerTileX + m_viewportTilesX/2) * m_tileSize;
            double tileY = (y - centerTileY + m_viewportTilesY/2) * m_tileSize;
            item->setPos(tileX, tileY);
            TileKey key = {x, y, z};
            m_tileItems[key] = item;
            qDebug() << "Loaded tile at position:" << tileX << "," << tileY << "for zoom:" << z;
        }
    } else {
        qDebug() << "Tile load failed:" << errorString;
    }
    
    // 注意：优化后，区域下载时已存在的瓦片不会进入队列，所以这里不需要处理进度
    // 此函数主要用于其他场景（如缩放时）的本地瓦片加载
}

void TileMapManager::checkAndEmitDownloadFinished()
{
    // 添加额外的安全检查，防止死循环
    if (m_currentRequests < 0) {
        m_currentRequests = 0;  // 修正计数器
    }
    
    // 检查是否所有任务都已完成
    if (m_regionDownloadTotal > 0 && m_regionDownloadCurrent >= m_regionDownloadTotal && m_currentRequests == 0) {
        if (!m_downloadFinishedEmitted) {
            m_downloadFinishedEmitted = true;
            m_isProcessing = false;
            emit downloadFinished();
        }
    } else if (m_downloadFinishedEmitted && m_currentRequests == 0 && m_pendingTiles.isEmpty()) {
        // 确保所有任务完成
        m_isProcessing = false;
    } else if (m_regionDownloadTotal > 0 && m_regionDownloadCurrent >= m_regionDownloadTotal && m_currentRequests > 0) {
        // 如果所有瓦片都已处理但还有请求在进行中，设置一个超时保护
        static int timeoutCounter = 0;
        timeoutCounter++;
        if (timeoutCounter > 50) {  // 增加超时次数到50次
            m_currentRequests = 0;
            m_isProcessing = false;
            if (!m_downloadFinishedEmitted) {
                m_downloadFinishedEmitted = true;
                emit downloadFinished();
            }
            timeoutCounter = 0;
        } else {
            // 继续等待
            if (!m_processTimer->isActive()) {
                m_processTimer->start(500);
            }
        }
    } else if (m_isProcessing && m_pendingTiles.isEmpty() && m_currentRequests > 0) {
        // 特殊情况：队列为空但还有请求在进行中
        static int emptyQueueCounter = 0;
        emptyQueueCounter++;
        if (emptyQueueCounter > 30) {  // 增加等待次数到30次
            m_currentRequests = 0;
            m_isProcessing = false;
            if (!m_downloadFinishedEmitted) {
                m_downloadFinishedEmitted = true;
                emit downloadFinished();
            }
            emptyQueueCounter = 0;
        } else {
            if (!m_processTimer->isActive()) {
                m_processTimer->start(500);
            }
        }
    } else if (m_isProcessing && m_regionDownloadTotal > 0) {
        // 正常处理过程中，继续检查
        // 确保定时器会继续处理
        if (!m_processTimer->isActive()) {
            m_processTimer->start(300);
        }
    }
    
    // 添加额外的安全检查，确保在任何情况下都能继续处理
    if (m_isProcessing && (m_pendingTiles.size() > 0 || m_currentRequests > 0)) {
        if (!m_processTimer->isActive()) {
            m_processTimer->start(200);
        }
    }
    
    // 特殊情况处理：如果处理已完成但信号未发出
    if (!m_isProcessing && m_regionDownloadTotal > 0 && m_regionDownloadCurrent >= m_regionDownloadTotal && !m_downloadFinishedEmitted) {
        m_downloadFinishedEmitted = true;
        emit downloadFinished();
    }
}

void TileMapManager::setTileSource(const QString &urlTemplate)
{
    m_tileUrlTemplate = urlTemplate;
}

void TileMapManager::latLonToTile(double lat, double lon, int zoom, int &tileX, int &tileY)
{
    // 将经纬度转换为瓦片坐标
    double latRad = lat * M_PI / 180.0;
    int n = 1 << zoom;
    tileX = (int)((lon + 180.0) / 360.0 * n);
    tileY = (int)((1.0 - log(tan(latRad) + (1.0 / cos(latRad))) / M_PI) / 2.0 * n);
    
    // 添加调试信息
    logMessage(QString("latLonToTile: lat=%1, lon=%2, zoom=%3 -> tileX=%4, tileY=%5").arg(lat).arg(lon).arg(zoom).arg(tileX).arg(tileY));
}

void TileMapManager::tileToLatLon(int tileX, int tileY, int zoom, double &lat, double &lon)
{
    // 将瓦片坐标转换为经纬度
    int n = 1 << zoom;
    lon = tileX / (double)n * 360.0 - 180.0;
    double latRad = atan(sinh(M_PI * (1 - 2 * tileY / (double)n)));
    lat = latRad * 180.0 / M_PI;
}

QString TileMapManager::getTilePath(int x, int y, int z)
{
    // 生成瓦片文件的本地路径
    return QString("%1/%2/%3/%4.png").arg(m_cacheDir).arg(z).arg(x).arg(y);
}

bool TileMapManager::tileExists(int x, int y, int z)
{
    // 检查瓦片是否已存在于本地
    QFile file(getTilePath(x, y, z));
    return file.exists();
}

void TileMapManager::saveTile(int x, int y, int z, const QByteArray &data)
{
    // 保存瓦片到本地缓存
    QString tilePath = getTilePath(x, y, z);
    qDebug() << "Saving tile to:" << tilePath;
    
    // 创建目录
    QDir dir(QFileInfo(tilePath).path());
    if (!dir.exists()) {
        qDebug() << "Creating directory:" << QFileInfo(tilePath).path();
        if (!dir.mkpath(".")) {
            qDebug() << "Failed to create directory for tile!";
            return;
        }
    }
    
    // 保存文件
    QFile file(tilePath);
    if (file.open(QIODevice::WriteOnly)) {
        qint64 written = file.write(data);
        file.close();
        qDebug() << "Saved tile, bytes written:" << written;
        
        // 验证文件是否成功写入
        if (written != data.size()) {
            qDebug() << "Warning: Written bytes" << written << "not equal to data size" << data.size();
        }
    } else {
        qDebug() << "Failed to save tile:" << file.errorString() << "Path:" << tilePath;
    }
}

QPixmap TileMapManager::loadTile(int x, int y, int z)
{
    // 从本地加载瓦片
    QString tilePath = getTilePath(x, y, z);
    QPixmap pixmap;
    pixmap.load(tilePath);
    return pixmap;
}

QString TileMapManager::getTileUrl(int x, int y, int z)
{
    // 生成瓦片URL，使用多个服务器以分散负载
    QString url = m_tileUrlTemplate;
    url.replace("{x}", QString::number(x));
    url.replace("{y}", QString::number(y));
    url.replace("{z}", QString::number(z));
    
    // 循环使用不同的服务器 (a, b, c)
    static QStringList servers = {"a", "b", "c"};
    static int serverIndex = 0;
    // 检查URL是否包含{server}占位符
    if (url.contains("{server}")) {
        QString server = servers[serverIndex];
        url.replace("{server}", server);
        serverIndex = (serverIndex + 1) % servers.size();
        qDebug() << "Generated tile URL:" << url << "using server:" << server;
    } else {
        qDebug() << "Generated tile URL:" << url;
    }
    
    return url;
}

void TileMapManager::downloadTile(int x, int y, int z)
{
    qDebug() << "TileMapManager::downloadTile called for tile:" << x << y << z;
    
    // 检查瓦片是否已存在
    if (tileExists(x, y, z)) {
        qDebug() << "Tile already exists, loading from file:" << x << "," << y << "," << z;
        // 请求从文件加载
        QString filePath = getTilePath(x, y, z);
        m_currentRequests++;
        emit requestLoadTile(x, y, z, filePath);
        return;
    }
    
    // 请求下载并保存
    QString url = getTileUrl(x, y, z);
    QString filePath = getTilePath(x, y, z);
    m_currentRequests++;
    qDebug() << "Emitting requestDownloadTile for tile:" << x << y << z << "URL:" << url;
    emit requestDownloadTile(x, y, z, url, filePath);
}

void TileMapManager::calculateVisibleTiles()
{
    if (!m_scene) return;
    
    qDebug() << "Calculating visible tiles for zoom:" << m_zoom << "center:" << m_centerLat << "," << m_centerLon;
    
    // 计算中心点的瓦片坐标
    int centerTileX, centerTileY;
    latLonToTile(m_centerLat, m_centerLon, m_zoom, centerTileX, centerTileY);
    qDebug() << "Center tile coordinates:" << centerTileX << "," << centerTileY;
    
    // 计算视图范围内的瓦片
    int startX = centerTileX - m_viewportTilesX / 2;
    int startY = centerTileY - m_viewportTilesY / 2;
    int endX = centerTileX + m_viewportTilesX / 2;
    int endY = centerTileY + m_viewportTilesY / 2;
    
    // 限制瓦片范围
    int maxTile = (1 << m_zoom) - 1;
    startX = qMax(0, startX);
    startY = qMax(0, startY);
    endX = qMin(maxTile, endX);
    endY = qMin(maxTile, endY);
    
    qDebug() << "Tile range: (" << startX << "," << startY << ") to (" << endX << "," << endY << ")";
    
    // 统计需要下载的瓦片数量
    int tilesToDownload = 0;
    int tilesLoaded = 0;
    
    // 加载或下载瓦片
    for (int x = startX; x <= endX; x++) {
        for (int y = startY; y <= endY; y++) {
            TileKey key = {x, y, m_zoom};
            
            // 如果瓦片已经加载，跳过
            if (m_tileItems.contains(key)) {
                tilesLoaded++;
                continue;
            }
            
            // 检查本地是否存在瓦片
            if (tileExists(x, y, m_zoom)) {
                // 直接从本地加载，不通过工作线程
                QPixmap pixmap = loadTile(x, y, m_zoom);
                if (!pixmap.isNull()) {
                    QGraphicsPixmapItem *item = m_scene->addPixmap(pixmap);
                    
                    // 计算瓦片位置
                    double tileX = (x - centerTileX + m_viewportTilesX/2) * m_tileSize;
                    double tileY = (y - centerTileY + m_viewportTilesY/2) * m_tileSize;
                    item->setPos(tileX, tileY);
                    
                    m_tileItems[key] = item;
                    tilesLoaded++;
                    
                    qDebug() << "Loaded local tile directly:" << x << y << m_zoom;
                }
            } else {
                // 请求下载
                tilesToDownload++;
                QString url = getTileUrl(x, y, m_zoom);
                QString filePath = getTilePath(x, y, m_zoom);
                m_currentRequests++;
                emit requestDownloadTile(x, y, m_zoom, url, filePath);
            }
        }
    }
    
    qDebug() << "Total tiles to download:" << tilesToDownload;
    qDebug() << "Total tiles loaded:" << tilesLoaded;
    
    // 只有在区域下载模式下才发送下载进度信号
    if (m_regionDownloadTotal > 0) {
        emit downloadProgress(tilesLoaded, tilesLoaded + tilesToDownload);
    }
}

void TileMapManager::cleanupTiles()
{
    // 清理视图范围外的瓦片，包括不同缩放级别的瓦片
    QList<TileKey> keysToRemove;
    
    // 计算中心点的瓦片坐标
    int centerTileX, centerTileY;
    latLonToTile(m_centerLat, m_centerLon, m_zoom, centerTileX, centerTileY);
    
    // 计算视图范围（扩大范围以避免频繁清理）
    int startX = centerTileX - m_viewportTilesX / 2 - 2;
    int startY = centerTileY - m_viewportTilesY / 2 - 2;
    int endX = centerTileX + m_viewportTilesX / 2 + 2;
    int endY = centerTileY + m_viewportTilesY / 2 + 2;
    
    // 检查哪些瓦片需要移除
    for (auto it = m_tileItems.begin(); it != m_tileItems.end(); ++it) {
        const TileKey &key = it.key();
        // 只移除不同缩放级别的瓦片，保留当前缩放级别的瓦片
        if (key.z != m_zoom) {
            keysToRemove.append(key);
            qDebug() << "Removing tile from different zoom level:" << key.x << key.y << key.z << "current zoom:" << m_zoom;
        }
        // 对于当前缩放级别，只移除距离中心太远的瓦片
        else if (key.x < startX || key.x > endX || key.y < startY || key.y > endY) {
            keysToRemove.append(key);
            qDebug() << "Removing distant tile:" << key.x << key.y << key.z;
        }
    }
    
    // 移除瓦片
    for (const TileKey &key : keysToRemove) {
        QGraphicsPixmapItem *item = m_tileItems.take(key);
        if (item) {
            // 检查图形项是否属于当前场景
            if (item->scene() == m_scene && m_scene) {
                m_scene->removeItem(item);
            }
            delete item;
        }
    }
    
    qDebug() << "Cleaned up" << keysToRemove.size() << "tiles, remaining:" << m_tileItems.size();
}

void TileMapManager::repositionTiles()
{
    if (!m_scene) return;
    
    qDebug() << "Repositioning tiles for zoom:" << m_zoom;
    
    // 计算当前中心点的瓦片坐标
    int centerTileX, centerTileY;
    latLonToTile(m_centerLat, m_centerLon, m_zoom, centerTileX, centerTileY);
    
    // 重新定位所有当前缩放级别的瓦片
    for (auto it = m_tileItems.begin(); it != m_tileItems.end(); ++it) {
        const TileKey &key = it.key();
        if (key.z == m_zoom) {
            QGraphicsPixmapItem *item = it.value();
            if (item) {
                // 计算瓦片相对于中心瓦片的位置
                double tileX = (key.x - centerTileX + m_viewportTilesX/2) * m_tileSize;
                double tileY = (key.y - centerTileY + m_viewportTilesY/2) * m_tileSize;
                item->setPos(tileX, tileY);
                qDebug() << "Repositioned tile" << key.x << key.y << "to" << tileX << tileY;
            }
        }
    }
}

void TileMapManager::checkLocalTiles()
{
    if (!m_scene) return;
    
    logMessage("Checking for local tiles...");
    
    // 检查缓存目录是否存在
    QDir cacheDir(m_cacheDir);
    if (!cacheDir.exists()) {
        logMessage("Cache directory does not exist, no local tiles found");
        return;
    }
    
    // 查找可用的缩放级别
    QStringList zoomLevels = cacheDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (zoomLevels.isEmpty()) {
        logMessage("No zoom levels found in cache directory");
        return;
    }
    
    // 选择最高的缩放级别作为默认显示级别
    int maxZoom = 0;
    for (const QString &zoomStr : zoomLevels) {
        bool ok;
        int zoom = zoomStr.toInt(&ok);
        if (ok && zoom > maxZoom) {
            maxZoom = zoom;
        }
    }
    
    if (maxZoom > 0) {
        logMessage(QString("Found local tiles, using zoom level: %1").arg(maxZoom));
        
        // 设置缩放级别
        m_zoom = maxZoom;
        
        // 设置场景矩形
        int viewportTiles = qMax(m_viewportTilesX, m_viewportTilesY);
        QRectF sceneRect(0, 0, viewportTiles * m_tileSize, viewportTiles * m_tileSize);
        m_scene->setSceneRect(sceneRect);
        
        // 直接加载本地瓦片，不触发下载
        int tileCount = loadLocalTiles();
        
        logMessage(QString("Local tiles loaded successfully, count: %1").arg(tileCount));
        
        // 发送找到本地瓦片的信号
        emit localTilesFound(maxZoom, tileCount);
    } else {
        logMessage("No valid zoom levels found in cache directory");
        emit noLocalTilesFound();
    }
}

int TileMapManager::loadLocalTiles()
{
    if (!m_scene) return 0;
    
    logMessage(QString("Loading local tiles for zoom: %1").arg(m_zoom));
    
    // 计算当前中心点的瓦片坐标
    int centerTileX, centerTileY;
    latLonToTile(m_centerLat, m_centerLon, m_zoom, centerTileX, centerTileY);
    
    // 计算视图范围内的瓦片
    int startX = centerTileX - m_viewportTilesX / 2;
    int startY = centerTileY - m_viewportTilesY / 2;
    int endX = centerTileX + m_viewportTilesX / 2;
    int endY = centerTileY + m_viewportTilesY / 2;
    
    // 限制瓦片范围
    int maxTile = (1 << m_zoom) - 1;
    startX = qMax(0, startX);
    startY = qMax(0, startY);
    endX = qMin(maxTile, endX);
    endY = qMin(maxTile, endY);
    
    int tilesLoaded = 0;
    
    // 加载本地瓦片
    for (int x = startX; x <= endX; x++) {
        for (int y = startY; y <= endY; y++) {
            TileKey key = {x, y, m_zoom};
            
            // 如果瓦片已经加载，跳过
            if (m_tileItems.contains(key)) {
                tilesLoaded++;
                continue;
            }
            
            // 检查本地是否存在瓦片
            if (tileExists(x, y, m_zoom)) {
                // 直接从文件加载
                QPixmap pixmap = loadTile(x, y, m_zoom);
                if (!pixmap.isNull()) {
                    QGraphicsPixmapItem *item = m_scene->addPixmap(pixmap);
                    
                    // 计算瓦片位置
                    double tileX = (x - centerTileX + m_viewportTilesX/2) * m_tileSize;
                    double tileY = (y - centerTileY + m_viewportTilesY/2) * m_tileSize;
                    item->setPos(tileX, tileY);
                    
                    m_tileItems[key] = item;
                    tilesLoaded++;
                    
                    logMessage(QString("Loaded local tile: %1/%2/%3").arg(x).arg(y).arg(m_zoom));
                }
            }
        }
    }
    
    logMessage(QString("Loaded %1 local tiles").arg(tilesLoaded));
    return tilesLoaded;
}

void TileMapManager::getLocalTilesInfo()
{
    logMessage("=== Local Tiles Information ===");
    
    // 检查缓存目录是否存在
    QDir cacheDir(m_cacheDir);
    if (!cacheDir.exists()) {
        logMessage("Cache directory does not exist");
        return;
    }
    
    // 查找可用的缩放级别
    QStringList zoomLevels = cacheDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (zoomLevels.isEmpty()) {
        logMessage("No zoom levels found in cache directory");
        return;
    }
    
    int totalTiles = 0;
    QMap<int, int> tilesPerZoom;
    
    // 统计每个缩放级别的瓦片数量
    for (const QString &zoomStr : zoomLevels) {
        bool ok;
        int zoom = zoomStr.toInt(&ok);
        if (ok && zoom >= 0 && zoom <= 19) {
            QDir zoomDir(cacheDir.absoluteFilePath(zoomStr));
            int zoomTileCount = 0;
            
            // 统计X坐标目录
            QStringList xDirs = zoomDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &xStr : xDirs) {
                bool xOk;
                int x = xStr.toInt(&xOk);
                if (xOk) {
                    QDir xDir(zoomDir.absoluteFilePath(xStr));
                    // 统计Y坐标文件
                    QStringList yFiles = xDir.entryList(QStringList() << "*.png", QDir::Files);
                    zoomTileCount += yFiles.size();
                }
            }
            
            tilesPerZoom[zoom] = zoomTileCount;
            totalTiles += zoomTileCount;
            
            logMessage(QString("Zoom level %1: %2 tiles").arg(zoom).arg(zoomTileCount));
        }
    }
    
    logMessage(QString("Total tiles: %1").arg(totalTiles));
    logMessage(QString("Available zoom levels: %1").arg(tilesPerZoom.keys().size()));
    
    // 显示详细信息
    QStringList zoomKeys;
    for (int zoom : tilesPerZoom.keys()) {
        zoomKeys << QString::number(zoom);
    }
    zoomKeys.sort();
    
    logMessage(QString("Zoom levels: %1").arg(zoomKeys.join(", ")));
}

int TileMapManager::getMaxAvailableZoom() const
{
    // 检查缓存目录是否存在
    QDir cacheDir(m_cacheDir);
    if (!cacheDir.exists()) {
        return 0; // 没有缓存目录，返回0
    }
    
    // 查找可用的缩放级别
    QStringList zoomLevels = cacheDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (zoomLevels.isEmpty()) {
        return 0; // 没有缩放级别，返回0
    }
    
    int maxZoom = 0;
    for (const QString &zoomStr : zoomLevels) {
        bool ok;
        int zoom = zoomStr.toInt(&ok);
        if (ok && zoom > maxZoom) {
            // 检查这个缩放级别是否有瓦片
            QDir zoomDir(cacheDir.absoluteFilePath(zoomStr));
            QStringList xDirs = zoomDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            if (!xDirs.isEmpty()) {
                // 检查是否有实际的瓦片文件
                bool hasTiles = false;
                for (const QString &xStr : xDirs) {
                    bool xOk;
                    int x = xStr.toInt(&xOk);
                    if (xOk) {
                        QDir xDir(zoomDir.absoluteFilePath(xStr));
                        QStringList yFiles = xDir.entryList(QStringList() << "*.png", QDir::Files);
                        if (!yFiles.isEmpty()) {
                            hasTiles = true;
                            break;
                        }
                    }
                }
                if (hasTiles) {
                    maxZoom = zoom;
                }
            }
        }
    }
    
    qDebug() << "Max available zoom level:" << maxZoom;
    return maxZoom;
}

void TileMapManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    Q_UNUSED(bytesReceived);
    Q_UNUSED(bytesTotal);
    // 这里可以实现下载进度报告
}
