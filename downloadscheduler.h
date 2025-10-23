#ifndef DOWNLOADSCHEDULER_H
#define DOWNLOADSCHEDULER_H

#include <QObject>
#include <QQueue>
#include <QTimer>
#include <QHash>
#include <QtGlobal>
class TileMapManager;
#include "manifeststore.h"
#include "mapmanagersettings.h"

class DownloadScheduler : public QObject {
    Q_OBJECT
public:
    explicit DownloadScheduler(QObject *parent = nullptr);

    void configure(const MapManagerSettings &settings);
    void setManifest(ManifestStore *store);
    void setTileManager(TileMapManager *mgr);

    void start();
    void pause();
    void resume() { start(); }
    void enqueueTask(const DownloadTask &task); // 追加任务到清单并保存
    void pauseTask(const QString &taskId); // TODO
    void resumeTask(const QString &taskId); // TODO
    void cancelTask(const QString &taskId); // TODO

signals:
    void taskProgress(const QString &taskId, int completed, int total);
    void taskStatusChanged(const QString &taskId, const QString &status);
    void allTasksFinished();

private slots:
    void onTick();
    void onTileCached(int x, int y, int z, bool success);

private:
    MapManagerSettings m_settings;
    ManifestStore *m_store = nullptr;
    QTimer m_timer; // 简易令牌：按速率周期发起
    int m_inflight = 0;
    TileMapManager *m_mgr = nullptr;

    struct TileJob { QString taskId; int x; int y; int z; };
    QQueue<TileJob> m_queue;
    bool m_queueBuilt = false;
    void buildQueueFromTasks();

    struct TileKey { int x; int y; int z; };
    struct TileKeyHash { inline size_t operator()(const TileKey &k) const noexcept { return qHash(k.x) ^ (qHash(k.y)<<1) ^ (qHash(k.z)<<2); } };
    struct TileKeyEq { inline bool operator()(const TileKey &a, const TileKey &b) const noexcept { return a.x==b.x && a.y==b.y && a.z==b.z; } };
    QHash<quint64, QString> m_outstanding; // packed key -> taskId
    static inline quint64 packKey(int x,int y,int z){ return (quint64(z)&0x3F)<<58 | (quint64(x)&0x3FFFFFF)<<32 | (quint64(y)&0xFFFFFFFF); }
};

#endif // DOWNLOADSCHEDULER_H


