#include "tilemapmanager.h"
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>

void TileMapManager::enqueueInsert(int x, int y, int z, const QPixmap &pixmap)
{
    PendingInsert pi;
    pi.x = x;
    pi.y = y;
    pi.z = z;
    pi.pixmap = pixmap;
    m_pendingInsert.enqueue(pi);
    if (!m_insertTimer->isActive()) {
        m_insertTimer->start();
    }
}

void TileMapManager::flushPendingInserts()
{
    if (!m_scene) {
        m_pendingInsert.clear();
        return;
    }
    int batch = 0;
    const int kMaxPerBatch = 24; // 限制单批量，减小抖动
    while (!m_pendingInsert.isEmpty() && batch < kMaxPerBatch) {
        PendingInsert pi = m_pendingInsert.dequeue();
        // 仅插入当前缩放级别
        if (pi.z != m_zoom) continue;
        QGraphicsPixmapItem *item = m_scene->addPixmap(pi.pixmap);
        double tileXScene = pi.x * m_tileSize;
        double tileYScene = pi.y * m_tileSize;
        item->setPos(tileXScene, tileYScene);
        TileKey key = {pi.x, pi.y, pi.z};
        m_tileItems[key] = item;
        batch++;
    }
    if (!m_pendingInsert.isEmpty()) {
        // 继续下一批
        m_insertTimer->start();
    }
}
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
#include <QTextStream>
#include <QMutex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 为TileKey提供hash函数
uint qHash(const TileKey &key, uint /*seed*/)
{
    return qHash(key.x) ^ qHash(key.y) ^ qHash(key.z);
}

// 日志记录函数（懒打开、线程安全、复用文件句柄）
void logMessage(const QString &message)
{
    static QMutex logMutex;
    QMutexLocker locker(&logMutex);
    static QFile *logFile = nullptr;
    static QTextStream *logStream = nullptr;
    if (!logFile) {
        logFile = new QFile("tilemap_debug.log");
        if (logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            logStream = new QTextStream(logFile);
        } else {
            delete logFile; logFile = nullptr;
        }
    }
    if (logStream) {
        (*logStream) << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
                     << " - " << message << "\n";
        logStream->flush();
    }
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
    , m_viewWidth(800)   // 默认视图宽度
    , m_viewHeight(600)  // 默认视图高度
    , m_regionDownloadTotal(0)
    , m_regionDownloadCurrent(0)
    , m_workerThread(nullptr)
    , m_worker(nullptr)
    , m_processTimer(new QTimer(this))
    , m_insertTimer(new QTimer(this))
    , m_isProcessing(false)
    , m_downloadFinishedEmitted(false)
    , m_maxConcurrentRequests(10)  // 增加同时处理的请求数量到5，提高下载效率
    , m_currentRequests(0)
{
    if (m_verboseLogging) logMessage("TileMapManager constructor started");
    
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
    if (m_verboseLogging) {
        logMessage(QString("Current working directory: %1").arg(QDir::currentPath()));
        logMessage(QString("Project root directory: %1").arg(projectRoot));
        logMessage(QString("Cache directory: %1").arg(m_cacheDir));
    }
    
    // 确保缓存目录存在
    QDir cacheDir(m_cacheDir);
    if (!cacheDir.exists()) {
        if (m_verboseLogging) logMessage("Creating cache directory");
        if (!cacheDir.mkpath(".")) {
            if (m_verboseLogging) logMessage("Failed to create cache directory!");
        } else {
            if (m_verboseLogging) logMessage("Cache directory created successfully");
        }
    } else {
        if (m_verboseLogging) logMessage("Cache directory already exists");
    }
    
    // 设置处理定时器
    m_processTimer->setSingleShot(true);
    connect(m_processTimer, &QTimer::timeout, this, &TileMapManager::processNextBatch);
    // 批量插入定时器（合并下载/加载回调）
    m_insertTimer->setSingleShot(true);
    m_insertTimer->setInterval(16); // ~60fps 合并
    connect(m_insertTimer, &QTimer::timeout, this, &TileMapManager::flushPendingInserts);
    
    // 启动工作线程
    startWorkerThread();
    
    // 检查网络访问功能
    if (m_verboseLogging) logMessage(QString("Network manager is accessible: %1").arg(m_networkManager != nullptr));
    if (m_verboseLogging) logMessage("TileMapManager constructor finished");
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

void TileMapManager::setZoomAtMousePosition(int zoom, double sceneX, double sceneY,
                                            double mouseViewportX, double mouseViewportY,
                                            int viewportWidth, int viewportHeight)
{
    Q_UNUSED(sceneX);
    Q_UNUSED(sceneY);
    
    if (!m_scene) return;
    
    int oldZoom = m_zoom;
    int newZoom = qBound(qMax(3, getDynamicMinZoom()), zoom, 10);
    
    if (m_verboseLogging) logMessage(QString("=== ZOOM %1->%2 === Mouse:(%3,%4) Viewport:%5x%6")
        .arg(oldZoom).arg(newZoom).arg(mouseViewportX).arg(mouseViewportY).arg(viewportWidth).arg(viewportHeight));
    
    // 关键修复：不使用场景坐标，直接用视口相对位置计算
    // 
    // 原理：
    // 在旧缩放级别，地图中心对应 centerTile_old
    // 鼠标相对于视口中心有一个偏移（瓦片单位）
    // 所以鼠标指向的瓦片 = centerTile_old + offset
    //
    // 缩放后：
    // mouseTile_new = mouseTile_old * 2^(newZoom - oldZoom)
    // centerTile_new = mouseTile_new - offset (偏移保持不变，因为视口大小不变)
    
    // 步骤1：获取旧缩放级别的中心瓦片
    int centerTileX_old, centerTileY_old;
    latLonToTile(m_centerLat, m_centerLon, oldZoom, centerTileX_old, centerTileY_old);
    
    // 加上小数部分（高精度）
    int n_old = 1 << oldZoom;
    double centerTileX_old_precise = (m_centerLon + 180.0) / 360.0 * n_old;
    double lat_rad_old = m_centerLat * M_PI / 180.0;
    double centerTileY_old_precise = (1.0 - log(tan(lat_rad_old) + 1.0 / cos(lat_rad_old)) / M_PI) / 2.0 * n_old;
    
    // 步骤2：计算鼠标相对于视口中心的偏移（瓦片单位）
    double mouseOffsetX_pixels = mouseViewportX - viewportWidth / 2.0;
    double mouseOffsetY_pixels = mouseViewportY - viewportHeight / 2.0;
    double mouseOffsetX_tiles = mouseOffsetX_pixels / m_tileSize;
    double mouseOffsetY_tiles = mouseOffsetY_pixels / m_tileSize;
    
    // 步骤3：计算鼠标指向的瓦片（旧缩放级别）
    double mouseTileX_old = centerTileX_old_precise + mouseOffsetX_tiles;
    double mouseTileY_old = centerTileY_old_precise + mouseOffsetY_tiles;
    
    // 转换为地理坐标（验证用）
    double mouseLon_old = mouseTileX_old / n_old * 360.0 - 180.0;
    double lat_rad = atan(sinh(M_PI * (1.0 - 2.0 * mouseTileY_old / n_old)));
    double mouseLat_old = lat_rad * 180.0 / M_PI;
    
    // 步骤4：缩放瓦片坐标
    m_zoom = newZoom;
    int zoomDiff = newZoom - oldZoom;
    double zoomScale = pow(2.0, zoomDiff);
    
    double mouseTileX_new = mouseTileX_old * zoomScale;
    double mouseTileY_new = mouseTileY_old * zoomScale;
    
    // 步骤5：计算新的中心瓦片
    double centerTileX_new = mouseTileX_new - mouseOffsetX_tiles;
    double centerTileY_new = mouseTileY_new - mouseOffsetY_tiles;
    
    // 步骤6：转换为地理坐标
    int n_new = 1 << newZoom;
    m_centerLon = centerTileX_new / n_new * 360.0 - 180.0;
    double lat_rad_new = atan(sinh(M_PI * (1.0 - 2.0 * centerTileY_new / n_new)));
    m_centerLat = lat_rad_new * 180.0 / M_PI;
    
    if (m_verboseLogging) logMessage(QString("  Offset:(%1,%2) MouseGEO:(%3,%4) -> NewCenter:(%5,%6)")
        .arg(mouseOffsetX_tiles, 0, 'f', 3)
        .arg(mouseOffsetY_tiles, 0, 'f', 3)
        .arg(mouseLat_old, 0, 'f', 4)
        .arg(mouseLon_old, 0, 'f', 4)
        .arg(m_centerLat, 0, 'f', 4)
        .arg(m_centerLon, 0, 'f', 4));
    
    // 步骤7：更新场景并重新加载瓦片
    cleanupTiles();
    
    int newMaxTiles = (1 << newZoom);
    int mapWidth = newMaxTiles * m_tileSize;
    int mapHeight = newMaxTiles * m_tileSize;
    m_scene->setSceneRect(0, 0, mapWidth, mapHeight);
    
    loadTiles();
}
void TileMapManager::panByPixels(double deltaViewportX, double deltaViewportY)
{
    if (!m_scene) return;

    // 视口像素 -> 瓦片位移
    double deltaTilesX = deltaViewportX / m_tileSize;
    double deltaTilesY = deltaViewportY / m_tileSize;

    // 当前中心的瓦片小数坐标
    int n = 1 << m_zoom;
    double centerTileX = (m_centerLon + 180.0) / 360.0 * n;
    double lat_rad = m_centerLat * M_PI / 180.0;
    double centerTileY = (1.0 - log(tan(lat_rad) + 1.0 / cos(lat_rad)) / M_PI) / 2.0 * n;

    // 平移后中心
    double newTileX = centerTileX + deltaTilesX;
    double newTileY = centerTileY + deltaTilesY;
    newTileX = qBound(0.0, newTileX, (double)(n - 1));
    newTileY = qBound(0.0, newTileY, (double)(n - 1));

    // 转回经纬度
    m_centerLon = newTileX / n * 360.0 - 180.0;
    double lat_rad_new = atan(sinh(M_PI * (1.0 - 2.0 * newTileY / n)));
    m_centerLat = lat_rad_new * 180.0 / M_PI;

    // 立即重排并按新中心计算可见瓦片
    repositionTiles();
    calculateVisibleTiles();
}

void TileMapManager::sceneToLatLon(double sceneX, double sceneY, int zoom, double &lat, double &lon)
{
    // 安全检查
    if (m_tileSize <= 0) {
        qDebug() << "sceneToLatLon: invalid tile size";
        lat = m_centerLat;
        lon = m_centerLon;
        return;
    }
    
    // 绝对坐标：直接用场景像素换算为瓦片浮点坐标
    int maxTilesAtZoom = (1 << zoom);
    double tileX = sceneX / m_tileSize;
    double tileY = sceneY / m_tileSize;
    
    // 限制在有效范围内
    int maxTile = maxTilesAtZoom - 1;
    tileX = qBound(0.0, tileX, (double)maxTile);
    tileY = qBound(0.0, tileY, (double)maxTile);
    
    // 将瓦片坐标（小数）转换为经纬度
    int n = 1 << zoom;
    lon = tileX / n * 360.0 - 180.0;
    double latRad = atan(sinh(M_PI * (1 - 2 * tileY / n)));
    lat = latRad * 180.0 / M_PI;
    
    if (m_verboseLogging) logMessage(QString("sceneToLatLon(abs): scene(%1,%2) -> tile(%3,%4) -> geo(%5,%6)")
               .arg(sceneX, 0, 'f', 2).arg(sceneY, 0, 'f', 2)
               .arg(tileX, 0, 'f', 4).arg(tileY, 0, 'f', 4)
               .arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6));
}

void TileMapManager::updateTilesForView(double sceneX, double sceneY)
{
    if (!m_scene) return;
    if (!shouldUpdateForSceneDelta(sceneX, sceneY)) return;
    
    // 根据场景坐标计算地理坐标
    double newLat, newLon;
    sceneToLatLon(sceneX, sceneY, m_zoom, newLat, newLon);
    
    // 检查是否需要更新（避免频繁刷新）
    double latDiff = qAbs(newLat - m_centerLat);
    double lonDiff = qAbs(newLon - m_centerLon);
    
    // 只有移动超过一定阈值才更新（减少不必要的刷新）
    // 根据缩放级别动态调整阈值，高缩放级别需要更小的阈值
    double threshold = 1.0 / (1 << m_zoom);  // 优化阈值计算
    if (latDiff < threshold && lonDiff < threshold) {
        qDebug() << "Movement within preloaded area, skipping update. Diff:" << latDiff << "," << lonDiff << "Threshold:" << threshold;
        return;
    }
    
    qDebug() << "Movement exceeded threshold, loading new tiles. Diff:" << latDiff << "," << lonDiff;
    
    // 更新中心点（使用最新视图几何来刷新布局缓存）
    m_centerLat = newLat;
    m_centerLon = newLon;
    if (m_enableGenerationDiscard) {
        m_generationId++;
    }
    
    qDebug() << "Updating tiles for new center:" << m_centerLat << "," << m_centerLon;
    
    // 仅计算并加载可见瓦片（绝对定位无需重排，减少拖拽抖动）
    calculateVisibleTiles();
}

void TileMapManager::updateTilesForViewImmediate(double sceneX, double sceneY)
{
    if (!m_scene) return;
    if (!shouldUpdateForSceneDelta(sceneX, sceneY)) return;
    double newLat, newLon;
    sceneToLatLon(sceneX, sceneY, m_zoom, newLat, newLon);

    // 直接更新中心，无阈值过滤
    m_centerLat = newLat;
    m_centerLon = newLon;
    if (m_enableGenerationDiscard) {
        m_generationId++;
    }

    // 仅计算并加载可见瓦片（绝对定位无需重排，减少拖拽抖动）
    calculateVisibleTiles(true); // 拖拽中也触发下载并即时显示
}

bool TileMapManager::shouldUpdateForSceneDelta(double sceneX, double sceneY) const
{
    // 初次必更新
    if (m_lastUpdateSceneX < 0 || m_lastUpdateSceneY < 0) {
        m_lastUpdateSceneX = sceneX;
        m_lastUpdateSceneY = sceneY;
        return true;
    }
    double dx = qAbs(sceneX - m_lastUpdateSceneX);
    double dy = qAbs(sceneY - m_lastUpdateSceneY);
    // 阈值：跨过 1/3 瓦片或超过最小像素位移
    double pixelThreshold = qMax(4.0, m_tileSize / 3.0);
    if (dx >= pixelThreshold || dy >= pixelThreshold) {
        m_lastUpdateSceneX = sceneX;
        m_lastUpdateSceneY = sceneY;
        return true;
    }
    return false;
}

void TileMapManager::setViewSize(int width, int height)
{
    m_viewWidth = width;
    m_viewHeight = height;
    
    // 根据视图大小计算需要的瓦片数量（预加载更大范围，减少拖动时的刷新频率）
    // 预加载视图大小的3倍范围，提供更好的拖拽体验
    m_viewportTilesX = (width / m_tileSize) * 3 + 6;  // 3倍视图宽度 + 6个额外瓦片
    m_viewportTilesY = (height / m_tileSize) * 3 + 6;  // 3倍视图高度 + 6个额外瓦片
    
    qDebug() << "View size updated:" << width << "x" << height 
             << "Viewport tiles:" << m_viewportTilesX << "x" << m_viewportTilesY;
    
    // 重新加载瓦片以适应新的视图大小
    if (m_scene) {
        loadTiles();
    }
}

int TileMapManager::getDynamicMinZoom() const
{
    // 计算使地图宽高均不小于视口的最小缩放级别
    if (m_tileSize <= 0) return 2;
    double tilesNeededX = (double)m_viewWidth / m_tileSize;
    double tilesNeededY = (double)m_viewHeight / m_tileSize;
    double tilesNeeded = qMax(tilesNeededX, tilesNeededY);
    // 2^zoom >= tilesNeeded => zoom >= ceil(log2(tilesNeeded))
    int minZoom = 0;
    if (tilesNeeded > 1.0) {
        minZoom = (int)std::ceil(std::log2(tilesNeeded));
    }
    // 至少为2，避免z=0/1 过小
    return qMax(2, minZoom);
}

void TileMapManager::setZoom(int zoom)
{
    int oldZoom = m_zoom;
    m_zoom = qBound(qMax(3, getDynamicMinZoom()), zoom, 10);  // 限制为3-10
    
    // 在设置新的缩放级别后，先清理不需要的瓦片
    cleanupTiles();
    
    // 将场景矩形设置为整幅地图的绝对像素尺寸
    if (m_scene) {
        int maxTilesAtZoom = (1 << m_zoom);
        int mapWidth = maxTilesAtZoom * m_tileSize;
        int mapHeight = maxTilesAtZoom * m_tileSize;
        m_scene->setSceneRect(0, 0, mapWidth, mapHeight);
    }
    
    // 重新定位所有已加载的瓦片
    repositionTiles();
    
    // 重新加载瓦片
    loadTiles();
    
    // 重置区域下载计数器，因为缩放级别改变后之前的区域下载任务已无效
    m_regionDownloadTotal = 0;
    m_regionDownloadCurrent = 0;
    m_downloadFinishedEmitted = false;
}

void TileMapManager::loadTiles()
{
    if (!m_scene) return;
    
    // 先根据当前中心重新定位已存在的瓦片，避免拖拽后旧瓦片位置错误
    repositionTiles();

    // 计算当前视图范围内的瓦片并按最新起始/偏移放置新瓦片
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
        emit tileCached(x, y, z, true);
        
        // 只有在非区域下载模式下，且瓦片是当前缩放级别时，才添加到场景
        // 区域下载时不添加到场景，等用户切换到对应层级时再加载
        if (m_scene && !isRegionDownloadMode && z == m_zoom) {
            QPixmap pixmap;
            pixmap.loadFromData(data);
            if (!pixmap.isNull()) {
                enqueueInsert(x, y, z, pixmap);
            }
        }
    } else {
        qDebug() << "Tile download failed:" << errorString;
        emit tileCached(x, y, z, false);
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
            enqueueInsert(x, y, z, pixmap);
        }
        // 本地加载也视为已缓存，通知调度层更新进度
        emit tileCached(x, y, z, true);
    } else {
        qDebug() << "Tile load failed:" << errorString;
        emit tileCached(x, y, z, false);
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

QPointF TileMapManager::getCenterScenePos() const
{
    int n = 1 << m_zoom;
    double tileX = (m_centerLon + 180.0) / 360.0 * n;
    double lat_rad = m_centerLat * M_PI / 180.0;
    double tileY = (1.0 - log(tan(lat_rad) + 1.0 / cos(lat_rad)) / M_PI) / 2.0 * n;
    return QPointF(tileX * m_tileSize, tileY * m_tileSize);
}

void TileMapManager::latLonToTile(double lat, double lon, int zoom, int &tileX, int &tileY)
{
    // 将经纬度转换为瓦片坐标
    double latRad = lat * M_PI / 180.0;
    int n = 1 << zoom;
    tileX = (int)((lon + 180.0) / 360.0 * n);
    tileY = (int)((1.0 - log(tan(latRad) + (1.0 / cos(latRad))) / M_PI) / 2.0 * n);
    
    // 添加调试信息（可选）
    if (m_verboseLogging) logMessage(QString("latLonToTile: lat=%1, lon=%2, zoom=%3 -> tileX=%4, tileY=%5").arg(lat).arg(lon).arg(zoom).arg(tileX).arg(tileY));
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
    if (m_verboseLogging) qDebug() << "Saving tile to:" << tilePath;
    
    // 创建目录
    QDir dir(QFileInfo(tilePath).path());
    if (!dir.exists()) {
        if (m_verboseLogging) qDebug() << "Creating directory:" << QFileInfo(tilePath).path();
        if (!dir.mkpath(".")) {
            if (m_verboseLogging) qDebug() << "Failed to create directory for tile!";
            return;
        }
    }
    
    // 保存文件
    QFile file(tilePath);
    if (file.open(QIODevice::WriteOnly)) {
        qint64 written = file.write(data);
        file.close();
        if (m_verboseLogging) qDebug() << "Saved tile, bytes written:" << written;
        
        // 验证文件是否成功写入
        if (written != data.size()) {
            if (m_verboseLogging) qDebug() << "Warning: Written bytes" << written << "not equal to data size" << data.size();
        }
    } else {
        if (m_verboseLogging) qDebug() << "Failed to save tile:" << file.errorString() << "Path:" << tilePath;
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
        if (m_verboseLogging) qDebug() << "Generated tile URL:" << url << "using server:" << server;
    } else {
        if (m_verboseLogging) qDebug() << "Generated tile URL:" << url;
    }
    
    return url;
}

void TileMapManager::downloadTile(int x, int y, int z)
{
    if (m_verboseLogging) qDebug() << "TileMapManager::downloadTile called for tile:" << x << y << z;
    
    // 检查瓦片是否已存在
    if (tileExists(x, y, z)) {
        if (m_verboseLogging) qDebug() << "Tile already exists locally, count as completed:" << x << "," << y << "," << z;
        // 对于批量下载/调度场景：本地已存在则直接记为完成，不再加载
        emit tileCached(x, y, z, true);
        return;
    }
    
    // 请求下载并保存
    QString url = getTileUrl(x, y, z);
    QString filePath = getTilePath(x, y, z);
    m_currentRequests++;
    if (m_verboseLogging) qDebug() << "Emitting requestDownloadTile for tile:" << x << y << z << "URL:" << url;
    emit requestDownloadTile(x, y, z, url, filePath);
}

void TileMapManager::calculateVisibleTiles(bool allowDownload)
{
    if (!m_scene) {
    if (m_verboseLogging) qDebug() << "calculateVisibleTiles: scene is null";
        return;
    }
    
    if (m_verboseLogging) {
        qDebug() << "=== calculateVisibleTiles ===";
        qDebug() << "Zoom:" << m_zoom << "Center:" << m_centerLat << "," << m_centerLon;
        qDebug() << "Viewport tiles:" << m_viewportTilesX << "x" << m_viewportTilesY;
    }
    
    // 计算中心点的瓦片坐标
    int centerTileX, centerTileY;
    latLonToTile(m_centerLat, m_centerLon, m_zoom, centerTileX, centerTileY);
    if (m_verboseLogging) qDebug() << "Center tile coordinates:" << centerTileX << "," << centerTileY;
    
    // 计算这个缩放级别的最大瓦片数
    int maxTilesAtZoom = (1 << m_zoom);  // 2^zoom
    
    // 计算需要加载的瓦片范围
    // 如果地图瓦片总数小于视图需要的瓦片数，则加载所有可用的瓦片
    int startX, startY, endX, endY;
    
    if (maxTilesAtZoom <= m_viewportTilesX || maxTilesAtZoom <= m_viewportTilesY) {
        // 地图很小，加载所有瓦片
        startX = 0;
        startY = 0;
        endX = maxTilesAtZoom - 1;
        endY = maxTilesAtZoom - 1;
        if (m_verboseLogging) qDebug() << "Small map mode: loading all tiles (0,0) to" << endX << "," << endY;
    } else {
        // 正常模式：以中心点为基准加载周围的瓦片（确保窗口大小不超过viewportTiles）
        startX = centerTileX - m_viewportTilesX / 2;
        startY = centerTileY - m_viewportTilesY / 2;
        endX = startX + m_viewportTilesX - 1;
        endY = startY + m_viewportTilesY - 1;
        
        // 限制瓦片范围并保持窗口大小（尽量）
        int maxTile = maxTilesAtZoom - 1;
        if (startX < 0) { endX = qMin(maxTile, endX - startX); startX = 0; }
        if (startY < 0) { endY = qMin(maxTile, endY - startY); startY = 0; }
        if (endX > maxTile) { int diff = endX - maxTile; startX = qMax(0, startX - diff); endX = maxTile; }
        if (endY > maxTile) { int diff = endY - maxTile; startY = qMax(0, startY - diff); endY = maxTile; }
    }
    
    if (m_verboseLogging) qDebug() << "Tile range: (" << startX << "," << startY << ") to (" << endX << "," << endY << ")";
    
    // 计算需要加载的瓦片总数
    int totalTilesToLoad = (endX - startX + 1) * (endY - startY + 1);
    if (m_verboseLogging) qDebug() << "Total tiles in range:" << totalTilesToLoad;
    
    // 安全检查：避免一次性加载过多瓦片
    // 注意：不要修改 m_viewportTilesX/Y，那是根据视口大小固定的！
    const int MAX_TILES_PER_LOAD = 500;  // 限制每次最多加载500张瓦片
    if (totalTilesToLoad > MAX_TILES_PER_LOAD) {
        if (m_verboseLogging) qDebug() << "WARNING: Too many tiles to load (" << totalTilesToLoad << "), limiting to" << MAX_TILES_PER_LOAD;
        
        // 使用局部变量，不修改全局的viewportTiles
        int reduceBy = (int)sqrt(totalTilesToLoad / (double)MAX_TILES_PER_LOAD);
        int adjustedViewportTilesX = m_viewportTilesX / reduceBy;
        int adjustedViewportTilesY = m_viewportTilesY / reduceBy;
        adjustedViewportTilesX = qMax(5, adjustedViewportTilesX);
        adjustedViewportTilesY = qMax(5, adjustedViewportTilesY);
        
        // 使用调整后的范围重新计算（保持窗口大小）
        startX = centerTileX - adjustedViewportTilesX / 2;
        startY = centerTileY - adjustedViewportTilesY / 2;
        endX = startX + adjustedViewportTilesX - 1;
        endY = startY + adjustedViewportTilesY - 1;
        
        int maxTile = (1 << m_zoom) - 1;
        if (startX < 0) { endX = qMin(maxTile, endX - startX); startX = 0; }
        if (startY < 0) { endY = qMin(maxTile, endY - startY); startY = 0; }
        if (endX > maxTile) { int diff = endX - maxTile; startX = qMax(0, startX - diff); endX = maxTile; }
        if (endY > maxTile) { int diff = endY - maxTile; startY = qMax(0, startY - diff); endY = maxTile; }
        
        if (m_verboseLogging) qDebug() << "Adjusted tile range: (" << startX << "," << startY << ") to (" << endX << "," << endY << ")";
    }
    
    // 统计需要下载的瓦片数量
    int tilesToDownload = 0;
    int tilesLoaded = 0;
    
    // 计算瓦片起始位置（确保小地图居中显示）
    int totalTilesX = endX - startX + 1;
    int totalTilesY = endY - startY + 1;
    double offsetX = (m_viewportTilesX - totalTilesX) * m_tileSize / 2.0;
    double offsetY = (m_viewportTilesY - totalTilesY) * m_tileSize / 2.0;
    
    // 如果是小地图，确保居中
    if (maxTilesAtZoom <= m_viewportTilesX || maxTilesAtZoom <= m_viewportTilesY) {
        offsetX = (m_viewWidth - maxTilesAtZoom * m_tileSize) / 2.0;
        offsetY = (m_viewHeight - maxTilesAtZoom * m_tileSize) / 2.0;
        offsetX = qMax(0.0, offsetX);
        offsetY = qMax(0.0, offsetY);
    }
    
    // 保存最近一次布局参数，供 onTileLoaded/onTileDownloaded 使用
    m_lastStartX = startX;
    m_lastStartY = startY;
    m_lastEndX = endX;
    m_lastEndY = endY;
    m_lastOffsetX = offsetX;
    m_lastOffsetY = offsetY;
    m_lastZoomForLayout = m_zoom;
    m_layoutValid = true;

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
                // 改为异步从文件加载，避免UI线程IO
                QString filePath = getTilePath(x, y, m_zoom);
                m_currentRequests++;
                emit requestLoadTile(x, y, m_zoom, filePath);
                tilesLoaded++;
            } else if (allowDownload) {
                // 允许下载时统一走 downloadTile（内部决定本地/网络）
                tilesToDownload++;
                downloadTile(x, y, m_zoom);
            } else {
                // 拖拽中：跳过下载，避免大量异步回调插队导致抖动/崩溃
            }
        }
    }
    
    if (m_verboseLogging) {
        qDebug() << "Total tiles to download:" << tilesToDownload;
        qDebug() << "Total tiles loaded:" << tilesLoaded;
        qDebug() << "Tile offset:" << offsetX << "," << offsetY;
    }
    
    // 只有在区域下载模式下才发送下载进度信号
    if (m_regionDownloadTotal > 0) {
        emit downloadProgress(tilesLoaded, tilesLoaded + tilesToDownload);
    }
}

void TileMapManager::cleanupTiles()
{
    if (!m_scene) return;
    if (m_isDragging) return; // 拖拽期间不清理，减少抖动
    
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
        }
        // 对于当前缩放级别，只移除距离中心太远的瓦片
        else if (key.x < startX || key.x > endX || key.y < startY || key.y > endY) {
            keysToRemove.append(key);
        }
    }
    
    if (m_verboseLogging) qDebug() << "Cleanup: removing" << keysToRemove.size() << "tiles, keeping" << m_tileItems.size() - keysToRemove.size();
    
    // 移除瓦片（批量操作，减少单个删除的开销）
    for (const TileKey &key : keysToRemove) {
        QGraphicsPixmapItem *item = m_tileItems.take(key);
        if (item) {
            // 检查图形项是否属于当前场景
            if (item->scene() == m_scene) {
                m_scene->removeItem(item);
            }
            delete item;
        }
    }
    
    if (m_verboseLogging) qDebug() << "Cleanup complete. Remaining tiles:" << m_tileItems.size();
}

void TileMapManager::repositionTiles()
{
    if (!m_scene) return;
    
    if (m_verboseLogging) qDebug() << "Repositioning tiles for zoom:" << m_zoom;
    
    // 使用与 calculateVisibleTiles 和 sceneToLatLon 完全相同的逻辑
    int maxTilesAtZoom = (1 << m_zoom);
    int centerTileX, centerTileY;
    latLonToTile(m_centerLat, m_centerLon, m_zoom, centerTileX, centerTileY);
    
    int startX, startY, endX, endY;
    
    if (maxTilesAtZoom <= m_viewportTilesX || maxTilesAtZoom <= m_viewportTilesY) {
        // 小地图模式
        startX = 0;
        startY = 0;
        endX = maxTilesAtZoom - 1;
        endY = maxTilesAtZoom - 1;
    } else {
        // 正常模式
        startX = centerTileX - m_viewportTilesX / 2;
        startY = centerTileY - m_viewportTilesY / 2;
        endX = centerTileX + m_viewportTilesX / 2;
        endY = centerTileY + m_viewportTilesY / 2;
        
        int maxTile = maxTilesAtZoom - 1;
        startX = qMax(0, startX);
        startY = qMax(0, startY);
        endX = qMin(maxTile, endX);
        endY = qMin(maxTile, endY);
    }
    
    // 计算偏移量
    int totalTilesX = endX - startX + 1;
    int totalTilesY = endY - startY + 1;
    double offsetX = (m_viewportTilesX - totalTilesX) * m_tileSize / 2.0;
    double offsetY = (m_viewportTilesY - totalTilesY) * m_tileSize / 2.0;
    
    if (maxTilesAtZoom <= m_viewportTilesX || maxTilesAtZoom <= m_viewportTilesY) {
        offsetX = (m_viewWidth - maxTilesAtZoom * m_tileSize) / 2.0;
        offsetY = (m_viewHeight - maxTilesAtZoom * m_tileSize) / 2.0;
        offsetX = qMax(0.0, offsetX);
        offsetY = qMax(0.0, offsetY);
    }
    
    // 绝对定位：无需随中心变化调整偏移
    for (auto it = m_tileItems.begin(); it != m_tileItems.end(); ++it) {
        const TileKey &key = it.key();
        if (key.z == m_zoom) {
            QGraphicsPixmapItem *item = it.value();
            if (item) {
                double tileX = key.x * m_tileSize;
                double tileY = key.y * m_tileSize;
                item->setPos(tileX, tileY);
                if (m_verboseLogging) qDebug() << "Repositioned tile (" << key.x << "," << key.y << ") to scene(" << tileX << "," << tileY << ")";
            }
        }
    }
    
    if (m_verboseLogging) qDebug() << "Reposition complete (absolute).";
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
    
    // 使用与其他方法完全一致的逻辑
    int maxTilesAtZoom = (1 << m_zoom);
    int centerTileX, centerTileY;
    latLonToTile(m_centerLat, m_centerLon, m_zoom, centerTileX, centerTileY);
    
    int startX, startY, endX, endY;
    
    if (maxTilesAtZoom <= m_viewportTilesX || maxTilesAtZoom <= m_viewportTilesY) {
        // 小地图模式
        startX = 0;
        startY = 0;
        endX = maxTilesAtZoom - 1;
        endY = maxTilesAtZoom - 1;
    } else {
        // 正常模式
        startX = centerTileX - m_viewportTilesX / 2;
        startY = centerTileY - m_viewportTilesY / 2;
        endX = centerTileX + m_viewportTilesX / 2;
        endY = centerTileY + m_viewportTilesY / 2;
        
        int maxTile = maxTilesAtZoom - 1;
        startX = qMax(0, startX);
        startY = qMax(0, startY);
        endX = qMin(maxTile, endX);
        endY = qMin(maxTile, endY);
    }
    
    // 计算偏移量
    int totalTilesX = endX - startX + 1;
    int totalTilesY = endY - startY + 1;
    double offsetX = (m_viewportTilesX - totalTilesX) * m_tileSize / 2.0;
    double offsetY = (m_viewportTilesY - totalTilesY) * m_tileSize / 2.0;
    
    if (maxTilesAtZoom <= m_viewportTilesX || maxTilesAtZoom <= m_viewportTilesY) {
        offsetX = (m_viewWidth - maxTilesAtZoom * m_tileSize) / 2.0;
        offsetY = (m_viewHeight - maxTilesAtZoom * m_tileSize) / 2.0;
        offsetX = qMax(0.0, offsetX);
        offsetY = qMax(0.0, offsetY);
    }
    
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
                    
            // 绝对定位
            double tileX = x * m_tileSize;
            double tileY = y * m_tileSize;
                    item->setPos(tileX, tileY);
                    
                    m_tileItems[key] = item;
                    tilesLoaded++;
                    
                    // logMessage(QString("Loaded local tile (%1,%2) at scene(%3,%4)").arg(x).arg(y).arg(tileX).arg(tileY));
                }
            }
        }
    }
    
    // logMessage(QString("Loaded %1 local tiles. StartTile:(%2,%3) Offset:(%4,%5)")
    //            .arg(tilesLoaded).arg(startX).arg(startY).arg(offsetX).arg(offsetY));
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
