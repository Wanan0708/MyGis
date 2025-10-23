#ifndef MANIFESTSTORE_H
#define MANIFESTSTORE_H

#include <QString>
#include <QVector>
#include <QDateTime>

struct DownloadTask {
    QString id;        // uuid-like
    double minLat = 0.0, maxLat = 0.0, minLon = 0.0, maxLon = 0.0;
    int minZoom = 3, maxZoom = 10;
    int priority = 0;
    QString status;    // pending/downloading/paused/completed/cancelled
    int totalTiles = 0;
    int completedTiles = 0;
    int failedTiles = 0;
    QDateTime createdAt;
    QDateTime updatedAt;
};

class ManifestStore {
public:
    explicit ManifestStore(const QString &path);
    bool load();
    bool save() const;

    QVector<DownloadTask> tasks() const { return m_tasks; }
    void upsertTask(const DownloadTask &t);
    void removeTask(const QString &id);
    DownloadTask getTask(const QString &id) const;
    void updateProgress(const QString &id, int completedDelta, int failedDelta);
    void setStatus(const QString &id, const QString &status);
    void setTotalTiles(const QString &id, int total);

private:
    QString m_path;
    QVector<DownloadTask> m_tasks;
};

#endif // MANIFESTSTORE_H


