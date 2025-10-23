#include "mapmanagersettings.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>

static QJsonObject toJson(const MapManagerSettings &s)
{
    QJsonObject o;
    o["tileUrlTemplate"] = s.tileUrlTemplate;
    QJsonArray arr; for (const auto &sv : s.servers) arr.push_back(sv);
    o["servers"] = arr;
    o["cacheDir"] = s.cacheDir;
    o["minZoom"] = s.minZoom;
    o["maxZoom"] = s.maxZoom;
    o["maxConcurrent"] = s.maxConcurrent;
    o["rateLimitPerSec"] = s.rateLimitPerSec;
    o["retryMax"] = s.retryMax;
    o["backoffInitialMs"] = s.backoffInitialMs;
    o["prefetchRing"] = s.prefetchRing;
    return o;
}

static MapManagerSettings fromJson(const QJsonObject &o)
{
    MapManagerSettings s;
    if (o.contains("tileUrlTemplate")) s.tileUrlTemplate = o.value("tileUrlTemplate").toString();
    if (o.contains("servers")) {
        s.servers.clear();
        for (auto v : o.value("servers").toArray()) s.servers << v.toString();
    }
    if (o.contains("cacheDir")) s.cacheDir = o.value("cacheDir").toString();
    if (o.contains("minZoom")) s.minZoom = o.value("minZoom").toInt(s.minZoom);
    if (o.contains("maxZoom")) s.maxZoom = o.value("maxZoom").toInt(s.maxZoom);
    if (o.contains("maxConcurrent")) s.maxConcurrent = o.value("maxConcurrent").toInt(s.maxConcurrent);
    if (o.contains("rateLimitPerSec")) s.rateLimitPerSec = o.value("rateLimitPerSec").toInt(s.rateLimitPerSec);
    if (o.contains("retryMax")) s.retryMax = o.value("retryMax").toInt(s.retryMax);
    if (o.contains("backoffInitialMs")) s.backoffInitialMs = o.value("backoffInitialMs").toInt(s.backoffInitialMs);
    if (o.contains("prefetchRing")) s.prefetchRing = o.value("prefetchRing").toInt(s.prefetchRing);
    return s;
}

MapManagerSettings MapManagerSettings::load(const QString &path, bool *ok)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (ok) *ok = false;
        MapManagerSettings s;
        if (s.cacheDir.isEmpty()) s.cacheDir = QDir::currentPath() + "/tilemap";
        return s;
    }
    auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) {
        if (ok) *ok = false;
        MapManagerSettings s;
        if (s.cacheDir.isEmpty()) s.cacheDir = QDir::currentPath() + "/tilemap";
        return s;
    }
    if (ok) *ok = true;
    MapManagerSettings s = fromJson(doc.object());
    if (s.cacheDir.isEmpty()) s.cacheDir = QDir::currentPath() + "/tilemap";
    return s;
}

bool MapManagerSettings::save(const QString &path) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QJsonDocument doc(toJson(*this));
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    return true;
}


