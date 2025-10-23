#include "manifeststore.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

static QJsonObject toJson(const DownloadTask &t)
{
    QJsonObject o;
    o["id"] = t.id;
    o["minLat"] = t.minLat; o["maxLat"] = t.maxLat; o["minLon"] = t.minLon; o["maxLon"] = t.maxLon;
    o["minZoom"] = t.minZoom; o["maxZoom"] = t.maxZoom;
    o["priority"] = t.priority;
    o["status"] = t.status;
    o["totalTiles"] = t.totalTiles;
    o["completedTiles"] = t.completedTiles;
    o["failedTiles"] = t.failedTiles;
    o["createdAt"] = t.createdAt.toString(Qt::ISODateWithMs);
    o["updatedAt"] = t.updatedAt.toString(Qt::ISODateWithMs);
    return o;
}

static DownloadTask fromJson(const QJsonObject &o)
{
    DownloadTask t;
    t.id = o.value("id").toString();
    t.minLat = o.value("minLat").toDouble(); t.maxLat = o.value("maxLat").toDouble();
    t.minLon = o.value("minLon").toDouble(); t.maxLon = o.value("maxLon").toDouble();
    t.minZoom = o.value("minZoom").toInt(3); t.maxZoom = o.value("maxZoom").toInt(10);
    t.priority = o.value("priority").toInt(0);
    t.status = o.value("status").toString("pending");
    t.totalTiles = o.value("totalTiles").toInt(0);
    t.completedTiles = o.value("completedTiles").toInt(0);
    t.failedTiles = o.value("failedTiles").toInt(0);
    t.createdAt = QDateTime::fromString(o.value("createdAt").toString(), Qt::ISODateWithMs);
    t.updatedAt = QDateTime::fromString(o.value("updatedAt").toString(), Qt::ISODateWithMs);
    return t;
}

ManifestStore::ManifestStore(const QString &path)
    : m_path(path)
{
}

bool ManifestStore::load()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) { m_tasks.clear(); return false; }
    auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    m_tasks.clear();
    if (!doc.isObject()) return false;
    auto arr = doc.object().value("tasks").toArray();
    for (auto v : arr) m_tasks.push_back(fromJson(v.toObject()));
    return true;
}

bool ManifestStore::save() const
{
    QJsonArray arr; for (const auto &t : m_tasks) arr.push_back(toJson(t));
    QJsonObject root; root["tasks"] = arr;
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

void ManifestStore::upsertTask(const DownloadTask &t)
{
    for (auto &it : m_tasks) {
        if (it.id == t.id) { it = t; return; }
    }
    DownloadTask c = t;
    if (c.id.isEmpty()) c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!c.createdAt.isValid()) c.createdAt = QDateTime::currentDateTime();
    c.updatedAt = c.createdAt;
    m_tasks.push_back(c);
}

void ManifestStore::removeTask(const QString &id)
{
    for (int i = 0; i < m_tasks.size(); ++i) if (m_tasks[i].id == id) { m_tasks.remove(i); return; }
}

DownloadTask ManifestStore::getTask(const QString &id) const
{
    for (const auto &t : m_tasks) if (t.id == id) return t;
    return DownloadTask();
}

void ManifestStore::updateProgress(const QString &id, int completedDelta, int failedDelta)
{
    for (auto &t : m_tasks) {
        if (t.id == id) {
            t.completedTiles += completedDelta;
            t.failedTiles += failedDelta;
            if (t.completedTiles < 0) t.completedTiles = 0;
            if (t.totalTiles > 0 && t.completedTiles > t.totalTiles) t.completedTiles = t.totalTiles;
            t.updatedAt = QDateTime::currentDateTime();
            return;
        }
    }
}

void ManifestStore::setStatus(const QString &id, const QString &status)
{
    for (auto &t : m_tasks) {
        if (t.id == id) {
            t.status = status;
            t.updatedAt = QDateTime::currentDateTime();
            return;
        }
    }
}

void ManifestStore::setTotalTiles(const QString &id, int total)
{
    for (auto &t : m_tasks) {
        if (t.id == id) { t.totalTiles = total; t.updatedAt = QDateTime::currentDateTime(); return; }
    }
}


