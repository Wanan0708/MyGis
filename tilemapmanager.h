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
    int getZoom() const { return m_zoom; }
    void setTileSource(const QString &urlTemplate);
    
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
    bool m_isProcessing;
    bool m_downloadFinishedEmitted;
    
    // 限制同时处理的请求数量
    int m_maxConcurrentRequests;
    int m_currentRequests;
    
    // 私有方法
    void latLonToTile(double lat, double lon, int zoom, int &tileX, int &tileY);
    void tileToLatLon(int tileX, int tileY, int zoom, double &lat, double &lon);
    QString getTilePath(int x, int y, int z);
    bool tileExists(int x, int y, int z);
    void saveTile(int x, int y, int z, const QByteArray &data);
    QPixmap loadTile(int x, int y, int z);
    QString getTileUrl(int x, int y, int z);
    void downloadTile(int x, int y, int z);
    void loadTiles();
    void calculateVisibleTiles();
    void cleanupTiles();
    void repositionTiles();
    int loadLocalTiles();
    void startWorkerThread();
    void stopWorkerThread();
    void checkAndEmitDownloadFinished();

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
};

#endif // TILEMAPMANAGER_H