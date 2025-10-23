#ifndef MAPMANAGERSETTINGS_H
#define MAPMANAGERSETTINGS_H

#include <QString>
#include <QStringList>

struct MapManagerSettings {
    QString tileUrlTemplate = "https://{server}.tile.openstreetmap.org/{z}/{x}/{y}.png";
    QStringList servers = {"a", "b", "c"};
    QString cacheDir; // 例如: E:/Project/.../tilemap

    int minZoom = 3;
    int maxZoom = 10;

    int maxConcurrent = 8;   // 并发下载数
    int rateLimitPerSec = 8; // 每秒请求数

    int retryMax = 3;
    int backoffInitialMs = 3000; // 指数退避起始

    int prefetchRing = 1; // 0/1/2

    bool useAsyncNetwork = false; // 是否使用全异步网络下载
    bool browseDownload = true;    // 边看边下：可视区域缺失瓦片自动下载

    static MapManagerSettings load(const QString &path, bool *ok = nullptr);
    bool save(const QString &path) const;
};

#endif // MAPMANAGERSETTINGS_H


