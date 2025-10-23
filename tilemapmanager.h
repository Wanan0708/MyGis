#ifndef TILEMAPMANAGER_H
#define TILEMAPMANAGER_H

#include <QObject>
#include <QGraphicsScene>
#include <QNetworkAccessManager>
#include <QPixmap>
#include <QMutex>
#include <QQueue>
#include <QSet>
#include <QTimer>
#include <QPointF>

class TileWorker;

// 瓦片键值结构
struct TileKey {
    int x, y, z;
    
    bool operator==(const TileKey &other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

// 为TileKey提供hash函数声明
uint qHash(const TileKey &key, uint seed = 0);

// 瓦片信息结构
struct TileInfo {
    int x, y, z;
    QString url;
    QString filePath;
};

class TileMapManager : public QObject
{
    Q_OBJECT

public:
    explicit TileMapManager(QObject *parent = nullptr);
    ~TileMapManager();

    void initScene(QGraphicsScene *scene);
    void setCenter(double lat, double lon);
    void setZoom(int zoom);
    void setZoomAtMousePosition(int zoom, double sceneX, double sceneY, 
                                double mouseViewportX, double mouseViewportY,
                                int viewportWidth, int viewportHeight);  // 在鼠标位置缩放
    int getZoom() const { return m_zoom; }
    void setTileSource(const QString &urlTemplate);
    void setViewSize(int width, int height);  // 设置视图大小
    void updateTilesForView(double sceneX, double sceneY);  // 根据场景坐标更新瓦片（带阈值）
    void updateTilesForViewImmediate(double sceneX, double sceneY); // 立即更新（无阈值，用于拖拽中）
    void panByPixels(double deltaViewportX, double deltaViewportY); // 依据视口像素位移平移中心
    void setDragging(bool dragging) { m_isDragging = dragging; }
    QPointF getCenterScenePos() const; // 获取当前中心的场景像素坐标
    int getTileSize() const { return m_tileSize; }
    // 可开关设置
    void setEnableGenerationDiscard(bool enabled) { m_enableGenerationDiscard = enabled; }
    void setPrefetchRing(int ring) { m_prefetchRing = ring; }
    
    // 下载指定区域的瓦片地图
    void downloadRegion(double minLat, double maxLat, double minLon, double maxLon, int minZoom, int maxZoom);
    
    // 检查并加载本地瓦片
    void checkLocalTiles();
    
    // 获取本地瓦片信息
    void getLocalTilesInfo();
    
    // 获取当前可用的最大缩放级别
    int getMaxAvailableZoom() const;

private:
    QGraphicsScene *m_scene;
    QNetworkAccessManager *m_networkManager;
    double m_centerLat, m_centerLon;
    int m_zoom;
    int m_tileSize;
    QString m_tileUrlTemplate;
    QString m_cacheDir;
    
    // 视图参数
    int m_viewportTilesX;
    int m_viewportTilesY;
    int m_viewWidth;   // 视图宽度（像素）
    int m_viewHeight;  // 视图高度（像素）
    
    // 瓦片管理
    QHash<TileKey, QGraphicsPixmapItem*> m_tileItems;
    QMutex m_mutex;
    
    // 区域下载相关
    int m_regionDownloadTotal;
    int m_regionDownloadCurrent;
    
    // 工作线程
    QThread *m_workerThread;
    TileWorker *m_worker;
    
    // 下载队列和处理相关
    QQueue<TileInfo> m_pendingTiles;
    QTimer *m_processTimer;
    QTimer *m_dragUpdateTimer = nullptr; // 拖拽节流（由MyForm控制，备用）
    bool m_isProcessing;
    bool m_downloadFinishedEmitted;
    
    // 限制同时处理的请求数量
    int m_maxConcurrentRequests;
    int m_currentRequests;
    bool m_isUpdatingLayout = false;
    bool m_isDragging = false; // 拖拽中抑制场景插入
    // 可开关：任务代与预取
    bool m_enableGenerationDiscard = false;
    int m_generationId = 0;
    int m_prefetchRing = 0; // 0=关闭，1=一圈，2=两圈

    // 最近一次布局参数（用于准确的 scene<->tile 变换）
    int m_lastStartX = 0;
    int m_lastStartY = 0;
    int m_lastEndX = 0;
    int m_lastEndY = 0;
    double m_lastOffsetX = 0.0;
    double m_lastOffsetY = 0.0;
    int m_lastZoomForLayout = -1;
    bool m_layoutValid = false;
    
    // 私有方法
    void latLonToTile(double lat, double lon, int zoom, int &tileX, int &tileY);
    void tileToLatLon(int tileX, int tileY, int zoom, double &lat, double &lon);
    void sceneToLatLon(double sceneX, double sceneY, int zoom, double &lat, double &lon);
    QString getTilePath(int x, int y, int z);
    bool tileExists(int x, int y, int z);
    void saveTile(int x, int y, int z, const QByteArray &data);
    QPixmap loadTile(int x, int y, int z);
    QString getTileUrl(int x, int y, int z);
    void downloadTile(int x, int y, int z);
    void loadTiles();
    void calculateVisibleTiles(bool allowDownload = true);
    void cleanupTiles();
    void repositionTiles();
    int loadLocalTiles();
    void startWorkerThread();
    void stopWorkerThread();
    void checkAndEmitDownloadFinished();
    void flushPendingInserts();
    void enqueueInsert(int x, int y, int z, const QPixmap &pixmap);
    bool shouldUpdateForSceneDelta(double sceneX, double sceneY) const; // 跨瓦片阈值判断

    struct PendingInsert {
        int x;
        int y;
        int z;
        QPixmap pixmap;
    };
    QQueue<PendingInsert> m_pendingInsert;
    QTimer *m_insertTimer = nullptr;
    mutable double m_lastUpdateSceneX = -1;
    mutable double m_lastUpdateSceneY = -1;

private slots:
    void processNextBatch();
    void onTileDownloaded(int x, int y, int z, const QByteArray &data, bool success, const QString &errorString);
    void onTileLoaded(int x, int y, int z, const QPixmap &pixmap, bool success, const QString &errorString);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

signals:
    void downloadProgress(int current, int total);
    void regionDownloadProgress(int current, int total, int zoom);
    void downloadFinished();
    void localTilesFound(int zoomLevel, int tileCount);
    void noLocalTilesFound();
    void requestDownloadTile(int x, int y, int z, const QString &url, const QString &filePath);
    void requestLoadTile(int x, int y, int z, const QString &filePath);
    void zoomChanged(int oldZoom, int newZoom, double mouseLat, double mouseLon);  // 缩放完成，传递鼠标地理坐标
};

#endif // TILEMAPMANAGER_H