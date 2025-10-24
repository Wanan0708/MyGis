#include "maptools.h"
#include "tilemapmanager.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QGraphicsView>
#include <QtMath>
#include <cmath>

// ================= ToolManager =================
ToolManager::ToolManager(QObject *parent) : QObject(parent) {}

void ToolManager::setContext(const ToolContext &ctx) { m_ctx = ctx; }

void ToolManager::registerTool(IMapTool *tool) {
    if (!tool) return;
    auto id = tool->descriptor().id;
    m_tools.insert(id, tool);
    connect(tool, &IMapTool::requestStatus, this, &ToolManager::requestStatus);
    connect(tool, &IMapTool::requestDeactivate, this, [this]() { this->deactivateTool(); });
}

bool ToolManager::activateTool(const QString &id) {
    if (m_current && m_tools.contains(id) && m_current == m_tools.value(id)) {
        // toggle off if same tool
        deactivateTool();
        return true;
    }
    if (m_current) {
        m_current->onDeactivate(m_ctx);
        m_current = nullptr;
    }
    auto it = m_tools.find(id);
    if (it == m_tools.end()) return false;
    m_current = it.value();
    m_current->onActivate(m_ctx);
    emit currentToolChanged(id);
    emit requestCursor(m_current->descriptor().cursor);
    emit requestStatus(m_current->descriptor().hint);
    return true;
}

void ToolManager::deactivateTool() {
    if (!m_current) { emit requestCursor(QCursor(Qt::ArrowCursor)); return; }
    m_current->onDeactivate(m_ctx);
    m_current = nullptr;
    emit currentToolChanged(QString());
    emit requestCursor(QCursor(Qt::ArrowCursor));
    emit requestStatus(QString());
}

bool ToolManager::handleMousePress(QMouseEvent *e) {
    return m_current ? m_current->onMousePress(m_ctx, e) : false;
}
bool ToolManager::handleMouseMove(QMouseEvent *e) {
    return m_current ? m_current->onMouseMove(m_ctx, e) : false;
}
bool ToolManager::handleMouseRelease(QMouseEvent *e) {
    return m_current ? m_current->onMouseRelease(m_ctx, e) : false;
}
bool ToolManager::handleMouseDoubleClick(QMouseEvent *e) {
    return m_current ? m_current->onMouseDoubleClick(m_ctx, e) : false;
}
bool ToolManager::handleKeyPress(QKeyEvent *e) {
    if (!m_current) return false;
    // 全局 ESC 取消当前工具
    if (e->key() == Qt::Key_Escape) {
        // 让当前工具有机会在 ESC 时提交或放弃
        m_current->onKeyPress(m_ctx, e);
        deactivateTool();
        return true;
    }
    return m_current->onKeyPress(m_ctx, e);
}

void ToolManager::clearAllCommitted() {
    for (auto it = m_tools.begin(); it != m_tools.end(); ++it) {
        if (it.value()) it.value()->clearCommitted();
    }
}

void ToolManager::refreshForViewChange() {
    // 广播给所有已注册工具（包括非当前工具），以便它们重建提交的覆盖
    for (auto it = m_tools.begin(); it != m_tools.end(); ++it) {
        if (it.value()) it.value()->onViewChanged(m_ctx);
    }
}

// ================= Utilities =================
namespace MapToolUtil {
    static constexpr double EARTH_R = 6371000.0;

    double haversineMeters(double lat1, double lon1, double lat2, double lon2) {
        double rlat1 = qDegreesToRadians(lat1);
        double rlon1 = qDegreesToRadians(lon1);
        double rlat2 = qDegreesToRadians(lat2);
        double rlon2 = qDegreesToRadians(lon2);
        double dlat = rlat2 - rlat1;
        double dlon = rlon2 - rlon1;
        double a = qSin(dlat/2)*qSin(dlat/2) + qCos(rlat1)*qCos(rlat2)*qSin(dlon/2)*qSin(dlon/2);
        double c = 2 * qAtan2(qSqrt(a), qSqrt(1-a));
        return EARTH_R * c;
    }

    void sceneToLatLon(const ToolContext &ctx, const QPointF &scenePt, int zoom, int tileSize,
                       double &lat, double &lon) {
        int n = 1 << zoom;
        double tileX = scenePt.x() / tileSize;
        double tileY = scenePt.y() / tileSize;
        lon = tileX / n * 360.0 - 180.0;
        double latRad = qAtan(std::sinh(M_PI * (1 - 2 * tileY / n)));
        lat = qRadiansToDegrees(latRad);
    }
}

// ================= MeasureBase =================
MeasureBase::MeasureBase(QObject *parent) : IMapTool(parent) {}

void MeasureBase::onActivate(const ToolContext &ctx) {
    Q_UNUSED(ctx);
    clearGraphics();
    m_isEditing = true;
    m_pointsGeo.clear();
}

void MeasureBase::onDeactivate(const ToolContext &ctx) {
    Q_UNUSED(ctx);
    // 仅清理编辑态橡皮筋，不清理已提交图形
    clearGraphics();
    m_isEditing = false;
}

bool MeasureBase::onKeyPress(const ToolContext &ctx, QKeyEvent *e) { Q_UNUSED(ctx); Q_UNUSED(e); return false; }

void MeasureBase::clearGraphics() {
    if (m_path) { delete m_path; m_path = nullptr; }
    for (auto *n : m_nodes) { delete n; }
    m_nodes.clear();
    if (m_label) { delete m_label; m_label = nullptr; }
}

void MeasureBase::ensureGraphics(const ToolContext &ctx) {
    if (!m_path) {
        m_path = new QGraphicsPathItem();
        m_path->setPen(m_linePen);
        m_path->setBrush(Qt::NoBrush);
        m_path->setZValue(1000000.0);
        m_path->setAcceptedMouseButtons(Qt::NoButton);
        ctx.scene->addItem(m_path);
    }
    if (!m_label) {
        m_label = new QGraphicsSimpleTextItem();
        m_label->setZValue(1000000.0);
        m_label->setAcceptedMouseButtons(Qt::NoButton);
        ctx.scene->addItem(m_label);
    }
}

void MeasureBase::updateRubber(const ToolContext &ctx, const QVector<QPointF> &pts, bool closed) {
    ensureGraphics(ctx);
    QPainterPath pp;
    if (!pts.isEmpty()) {
        pp.moveTo(pts.first());
        for (int i=1;i<pts.size();++i) pp.lineTo(pts[i]);
        if (closed) pp.closeSubpath();
    }
    m_path->setPath(pp);

    // nodes
    while (m_nodes.size() > pts.size()) { delete m_nodes.takeLast(); }
    for (int i=0;i<pts.size();++i) {
        QGraphicsEllipseItem *node = (i < m_nodes.size()) ? m_nodes[i] : nullptr;
        if (!node) {
            node = new QGraphicsEllipseItem();
            node->setBrush(m_nodeBrush);
            node->setPen(Qt::NoPen);
            node->setRect(-3, -3, 6, 6);
            node->setZValue(1000000.0);
            node->setAcceptedMouseButtons(Qt::NoButton);
            ctx.scene->addItem(node);
            m_nodes.append(node);
        }
        node->setPos(pts[i]);
        node->setVisible(true);
    }
}

void MeasureBase::updateLabel(const ToolContext &ctx, const QPointF &pos, const QString &text) {
    ensureGraphics(ctx);
    m_label->setText(text);
    m_label->setPos(pos + QPointF(8, -8));
}

void MeasureBase::commitGeometry(const ToolContext &ctx, const QVector<QPointF> &pts, bool closed, const QString &finalLabelText, bool filled) {
    if (pts.size() < 2) return;
    // 保存地理坐标以便缩放时重建
    CommittedGeo geo;
    geo.closed = closed; geo.label = finalLabelText; geo.filled = filled;
    for (const auto &p : pts) {
        double lat, lon;
        MapToolUtil::sceneToLatLon(ctx, p, ctx.tileManager->getZoom(), m_tileSize, lat, lon);
        geo.latLon.push_back(QPointF(lon, lat));
    }
    m_committedGeo.push_back(geo);
    QPainterPath pp;
    pp.moveTo(pts.first());
    for (int i=1;i<pts.size();++i) pp.lineTo(pts[i]);
    if (closed) pp.closeSubpath();

    QGraphicsPathItem *pItem = new QGraphicsPathItem(pp);
    pItem->setPen(m_linePen);
    pItem->setBrush(filled ? m_fill : Qt::NoBrush);
    pItem->setZValue(1000000.0);
    pItem->setAcceptedMouseButtons(Qt::NoButton);
    ctx.scene->addItem(pItem);
    m_committed.push_back(pItem);

    for (const auto &pt : pts) {
        auto *node = new QGraphicsEllipseItem(-3, -3, 6, 6);
        node->setBrush(m_nodeBrush);
        node->setPen(Qt::NoPen);
        node->setPos(pt);
        node->setZValue(1000000.0);
        node->setAcceptedMouseButtons(Qt::NoButton);
        ctx.scene->addItem(node);
        m_committed.push_back(node);
    }
    if (!finalLabelText.isEmpty()) {
        auto *txt = new QGraphicsSimpleTextItem(finalLabelText);
        txt->setPos(pts.last() + QPointF(8, -8));
        txt->setZValue(1000000.0);
        txt->setAcceptedMouseButtons(Qt::NoButton);
        ctx.scene->addItem(txt);
        m_committed.push_back(txt);
    }
}

void MeasureBase::clearCommitted() {
    for (auto *it : m_committed) delete it;
    m_committed.clear();
    m_committedGeo.clear();
}

void MeasureBase::onViewChanged(const ToolContext &ctx) {
    // 根据保存的地理坐标重建
    // 先清理现有提交的图形显示，再重建
    for (auto *it : m_committed) delete it;
    m_committed.clear();
    for (const auto &geo : m_committedGeo) {
        // 投影回当前缩放下的 scene 坐标（WebMercator）
        QVector<QPointF> pts;
        pts.reserve(geo.latLon.size());
        int z = ctx.tileManager->getZoom();
        int n = 1 << z;
        for (const auto &ll : geo.latLon) {
            double lon = ll.x();
            double lat = ll.y();
            double x = (lon + 180.0) / 360.0 * n * m_tileSize;
            double latRad = qDegreesToRadians(lat);
            double y = (1.0 - qLn(qTan(latRad/2.0 + M_PI/4.0)) / M_PI) / 2.0 * n * m_tileSize;
            pts.push_back(QPointF(x, y));
        }
        if (pts.size() < 2) continue;
        QPainterPath pp;
        pp.moveTo(pts.first());
        for (int i=1;i<pts.size();++i) pp.lineTo(pts[i]);
        if (geo.closed) pp.closeSubpath();
        auto *pItem = new QGraphicsPathItem(pp);
        pItem->setPen(m_linePen);
        pItem->setBrush(geo.filled ? m_fill : Qt::NoBrush);
        pItem->setZValue(1000000.0);
        pItem->setAcceptedMouseButtons(Qt::NoButton);
        ctx.scene->addItem(pItem);
        m_committed.push_back(pItem);
        for (const auto &pt : pts) {
            auto *node = new QGraphicsEllipseItem(-3, -3, 6, 6);
            node->setBrush(m_nodeBrush);
            node->setPen(Qt::NoPen);
            node->setPos(pt);
            node->setZValue(1000000.0);
            node->setAcceptedMouseButtons(Qt::NoButton);
            ctx.scene->addItem(node);
            m_committed.push_back(node);
        }
        if (!geo.label.isEmpty()) {
            auto *txt = new QGraphicsSimpleTextItem(geo.label);
            txt->setPos(pts.last() + QPointF(8, -8));
            txt->setZValue(1000000.0);
            txt->setAcceptedMouseButtons(Qt::NoButton);
            ctx.scene->addItem(txt);
            m_committed.push_back(txt);
        }
    }
}

// ================= MeasureDistanceTool =================
MeasureDistanceTool::MeasureDistanceTool(QObject *parent) : MeasureBase(parent) {}

ToolDescriptor MeasureDistanceTool::descriptor() const {
    ToolDescriptor d; d.id = "measure_distance"; d.name = QObject::tr("距离测量");
    d.icon = QIcon(); d.cursor = QCursor(Qt::CrossCursor); d.hint = QObject::tr("左键加点, 右键撤销, 双击结束, ESC 取消");
    return d;
}

bool MeasureDistanceTool::onMousePress(const ToolContext &ctx, QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        QPointF scenePt = ctx.view->mapToScene(e->pos());
        m_pointsScene.push_back(scenePt);
        double lat, lon; MapToolUtil::sceneToLatLon(ctx, scenePt, ctx.tileManager->getZoom(), m_tileSize, lat, lon);
        m_pointsGeo.push_back(QPointF(lon, lat));
        updateRubber(ctx, m_pointsScene, false);
        return true;
    }
    if (e->button() == Qt::RightButton && m_isEditing && !m_pointsScene.isEmpty()) {
        m_pointsScene.removeLast();
        if (!m_pointsGeo.isEmpty()) m_pointsGeo.removeLast();
        updateRubber(ctx, m_pointsScene, false);
        return true;
    }
    return false;
}

bool MeasureDistanceTool::onMouseMove(const ToolContext &ctx, QMouseEvent *e) {
    if (m_pointsScene.isEmpty()) return false;
    QVector<QPointF> tmp = m_pointsScene;
    tmp.push_back(ctx.view->mapToScene(e->pos()));
    updateRubber(ctx, tmp, false);
    // compute distance
    double total = 0.0;
    for (int i=1;i<tmp.size();++i) {
        double lat1, lon1, lat2, lon2;
        MapToolUtil::sceneToLatLon(ctx, tmp[i-1], ctx.tileManager->getZoom(), m_tileSize, lat1, lon1);
        MapToolUtil::sceneToLatLon(ctx, tmp[i],   ctx.tileManager->getZoom(), m_tileSize, lat2, lon2);
        total += MapToolUtil::haversineMeters(lat1, lon1, lat2, lon2);
    }
    QString unit = total >= 1000.0 ? QObject::tr(" km") : QObject::tr(" m");
    double val = total >= 1000.0 ? total/1000.0 : total;
    updateLabel(ctx, tmp.last(), QObject::tr("总长: %1%2").arg(val, 0, 'f', val>=100?1:2).arg(unit));
    emit requestStatus(m_label->text());
    return true;
}

bool MeasureDistanceTool::onMouseRelease(const ToolContext &ctx, QMouseEvent *e) {
    Q_UNUSED(ctx); Q_UNUSED(e); return false;
}

bool MeasureDistanceTool::onMouseDoubleClick(const ToolContext &ctx, QMouseEvent *e) {
    Q_UNUSED(e);
    if (m_pointsScene.size() < 2) { clearGraphics(); m_pointsScene.clear(); emit requestDeactivate(); return true; }
    QString finalLabel = m_label ? m_label->text() : QString();
    commitGeometry(ctx, m_pointsScene, false, finalLabel, false);
    clearGraphics();
    m_pointsScene.clear();
    m_pointsGeo.clear();
    emit requestStatus(QObject::tr("测量完成"));
    emit requestDeactivate();
    return true;
}

bool MeasureDistanceTool::onKeyPress(const ToolContext &ctx, QKeyEvent *e) {
    if (e->key() != Qt::Key_Escape) return false;
    if (m_pointsScene.size() < 2) { clearGraphics(); m_pointsScene.clear(); return true; }
    QString finalLabel = m_label ? m_label->text() : QString();
    commitGeometry(ctx, m_pointsScene, false, finalLabel, false);
    clearGraphics();
    m_pointsScene.clear();
    m_pointsGeo.clear();
    return true;
}

// ================= MeasureAreaTool =================
MeasureAreaTool::MeasureAreaTool(QObject *parent) : MeasureBase(parent) {}

ToolDescriptor MeasureAreaTool::descriptor() const {
    ToolDescriptor d; d.id = "measure_area"; d.name = QObject::tr("面积测量");
    d.icon = QIcon(); d.cursor = QCursor(Qt::CrossCursor); d.hint = QObject::tr("左键加点, 右键撤销, 双击结束, ESC 取消");
    return d;
}

bool MeasureAreaTool::onMousePress(const ToolContext &ctx, QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        QPointF scenePt = ctx.view->mapToScene(e->pos());
        m_pointsScene.push_back(scenePt);
        double lat, lon; MapToolUtil::sceneToLatLon(ctx, scenePt, ctx.tileManager->getZoom(), m_tileSize, lat, lon);
        m_pointsGeo.push_back(QPointF(lon, lat));
        updateRubber(ctx, m_pointsScene, m_pointsScene.size()>=3);
        return true;
    }
    if (e->button() == Qt::RightButton && m_isEditing && !m_pointsScene.isEmpty()) {
        m_pointsScene.removeLast();
        if (!m_pointsGeo.isEmpty()) m_pointsGeo.removeLast();
        updateRubber(ctx, m_pointsScene, m_pointsScene.size()>=3);
        return true;
    }
    return false;
}

bool MeasureAreaTool::onMouseMove(const ToolContext &ctx, QMouseEvent *e) {
    if (m_pointsScene.isEmpty()) return false;
    QVector<QPointF> tmp = m_pointsScene;
    tmp.push_back(ctx.view->mapToScene(e->pos()));
    updateRubber(ctx, tmp, tmp.size()>=3);

    // compute area using WebMercator projection + shoelace
    auto merc = [](double lat, double lon){
        double x = MapToolUtil::EARTH_R * qDegreesToRadians(lon);
        double y = MapToolUtil::EARTH_R * qLn(qTan(M_PI/4.0 + qDegreesToRadians(lat)/2.0));
        return QPointF(x,y);
    };
    QVector<QPointF> poly;
    for (const auto &p : tmp) {
        double lat, lon;
        MapToolUtil::sceneToLatLon(ctx, p, ctx.tileManager->getZoom(), m_tileSize, lat, lon);
        poly.push_back(merc(lat, lon));
    }
    double area = 0.0; // m^2
    for (int i=0;i<poly.size();++i) {
        const auto &a = poly[i];
        const auto &b = poly[(i+1)%poly.size()];
        area += a.x()*b.y() - b.x()*a.y();
    }
    area = qAbs(area) * 0.5;
    QString unit = area >= 1e6 ? QObject::tr(" km²") : QObject::tr(" m²");
    double val = area >= 1e6 ? area/1e6 : area;
    updateLabel(ctx, tmp.last(), QObject::tr("面积: %1%2").arg(val, 0, 'f', val>=100?1:2).arg(unit));
    emit requestStatus(m_label->text());
    return true;
}

bool MeasureAreaTool::onMouseRelease(const ToolContext &ctx, QMouseEvent *e) {
    Q_UNUSED(ctx); Q_UNUSED(e); return false;
}

bool MeasureAreaTool::onMouseDoubleClick(const ToolContext &ctx, QMouseEvent *e) {
    Q_UNUSED(e);
    if (m_pointsScene.size() < 3) { clearGraphics(); m_pointsScene.clear(); emit requestDeactivate(); return true; }
    QString finalLabel = m_label ? m_label->text() : QString();
    commitGeometry(ctx, m_pointsScene, true, finalLabel, true);
    clearGraphics();
    m_pointsScene.clear();
    m_pointsGeo.clear();
    emit requestStatus(QObject::tr("测量完成"));
    emit requestDeactivate();
    return true;
}

bool MeasureAreaTool::onKeyPress(const ToolContext &ctx, QKeyEvent *e) {
    if (e->key() != Qt::Key_Escape) return false;
    if (m_pointsScene.size() < 3) { clearGraphics(); m_pointsScene.clear(); return true; }
    QString finalLabel = m_label ? m_label->text() : QString();
    commitGeometry(ctx, m_pointsScene, true, finalLabel, true);
    clearGraphics();
    m_pointsScene.clear();
    m_pointsGeo.clear();
    return true;
}


