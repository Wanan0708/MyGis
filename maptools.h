#ifndef MAPTOOLS_H
#define MAPTOOLS_H

#include <QObject>
#include <QPointF>
#include <QVector>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsItem>
#include <QPen>
#include <QBrush>
#include <QIcon>
#include <QHash>
#include <QPainterPath>

class QGraphicsView;
class TileMapManager;
class QMouseEvent;
class QKeyEvent;

struct ToolDescriptor {
    QString id;
    QString name;
    QIcon icon;
    QCursor cursor;
    QString hint;
};

struct ToolContext {
    QGraphicsScene *scene = nullptr;
    QGraphicsView *view = nullptr;
    TileMapManager *tileManager = nullptr;
};

class IMapTool : public QObject {
    Q_OBJECT
public:
    explicit IMapTool(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IMapTool() {}

    virtual ToolDescriptor descriptor() const = 0;
    virtual void onActivate(const ToolContext &ctx) = 0;
    virtual void onDeactivate(const ToolContext &ctx) = 0;

    virtual bool onMousePress(const ToolContext &ctx, QMouseEvent *e) = 0;
    virtual bool onMouseMove(const ToolContext &ctx, QMouseEvent *e) = 0;
    virtual bool onMouseRelease(const ToolContext &ctx, QMouseEvent *e) = 0;
    virtual bool onMouseDoubleClick(const ToolContext &ctx, QMouseEvent *e) = 0;
    virtual bool onKeyPress(const ToolContext &ctx, QKeyEvent *e) = 0;
    virtual void onViewChanged(const ToolContext &ctx) = 0; // e.g., zoom/pan changed → rerender
    // Optional: clear any committed overlays held by the tool
    virtual void clearCommitted() {}

signals:
    void requestStatus(const QString &text);
    void requestDeactivate();
};

class ToolManager : public QObject {
    Q_OBJECT
public:
    explicit ToolManager(QObject *parent = nullptr);

    void setContext(const ToolContext &ctx);
    void registerTool(IMapTool *tool); // ownership stays with manager
    bool activateTool(const QString &id);
    void deactivateTool();
    IMapTool* currentTool() const { return m_current; }

    // Event dispatch
    bool handleMousePress(QMouseEvent *e);
    bool handleMouseMove(QMouseEvent *e);
    bool handleMouseRelease(QMouseEvent *e);
    bool handleMouseDoubleClick(QMouseEvent *e);
    bool handleKeyPress(QKeyEvent *e);
    void refreshForViewChange();

    void clearAllCommitted();

signals:
    void currentToolChanged(const QString &id);
    void requestStatus(const QString &text);
    void requestCursor(const QCursor &cursor);

private:
    ToolContext m_ctx;
    QHash<QString, IMapTool*> m_tools;
    IMapTool *m_current = nullptr;
};

// ============ Utilities ============
namespace MapToolUtil {
    // Haversine distance (meters)
    double haversineMeters(double lat1, double lon1, double lat2, double lon2);
    // Scene point (pixels) -> lat/lon using WebMercator formulas and TileMapManager zoom
    void sceneToLatLon(const ToolContext &ctx, const QPointF &scenePt, int zoom, int tileSize,
                       double &lat, double &lon);
}

// ============ Measure Tools ============
class MeasureBase : public IMapTool {
    Q_OBJECT
public:
    explicit MeasureBase(QObject *parent=nullptr);
    void onActivate(const ToolContext &ctx) override;
    void onDeactivate(const ToolContext &ctx) override;
    bool onKeyPress(const ToolContext &ctx, QKeyEvent *e) override;
    void onViewChanged(const ToolContext &ctx) override;
    void clearCommitted() override;

protected:
    void clearGraphics();
    void ensureGraphics(const ToolContext &ctx);
    void updateRubber(const ToolContext &ctx, const QVector<QPointF> &pts, bool closed);
    void updateLabel(const ToolContext &ctx, const QPointF &pos, const QString &text);
    void commitGeometry(const ToolContext &ctx, const QVector<QPointF> &pts, bool closed, const QString &finalLabelText, bool filled);
    void rebuildCommittedGraphics(const ToolContext &ctx);
    QVector<QPointF> projectLatLon(const ToolContext &ctx, const QVector<QPointF> &latLonList) const;

    QGraphicsPathItem *m_path = nullptr;
    QVector<QGraphicsEllipseItem*> m_nodes;
    QGraphicsSimpleTextItem *m_label = nullptr;
    QVector<QGraphicsItem*> m_committed; // committed shapes persisted on scene
    bool m_isEditing = false;
    // Geo store: QPointF(lon, lat)
    QVector<QPointF> m_pointsGeo; // current editing points in lat/lon
    struct CommittedGeo {
        QVector<QPointF> latLon; // lon,lat
        bool closed = false;
        QString label;
        bool filled = false;
    };
    QVector<CommittedGeo> m_committedGeo;
    QPen m_linePen{QColor(255,122,24), 2.0};
    QBrush m_fill{QColor(255,122,24,50)};
    QBrush m_nodeBrush{QColor(255,122,24)};
    int m_tileSize = 256;
};

class MeasureDistanceTool : public MeasureBase {
    Q_OBJECT
public:
    explicit MeasureDistanceTool(QObject *parent=nullptr);
    ToolDescriptor descriptor() const override;
    bool onMousePress(const ToolContext &ctx, QMouseEvent *e) override;
    bool onMouseMove(const ToolContext &ctx, QMouseEvent *e) override;
    bool onMouseRelease(const ToolContext &ctx, QMouseEvent *e) override;
    bool onMouseDoubleClick(const ToolContext &ctx, QMouseEvent *e) override;
    bool onKeyPress(const ToolContext &ctx, QKeyEvent *e) override;
    void onViewChanged(const ToolContext &ctx) override { MeasureBase::onViewChanged(ctx); }

private:
    QVector<QPointF> m_pointsScene;
};

class MeasureAreaTool : public MeasureBase {
    Q_OBJECT
public:
    explicit MeasureAreaTool(QObject *parent=nullptr);
    ToolDescriptor descriptor() const override;
    bool onMousePress(const ToolContext &ctx, QMouseEvent *e) override;
    bool onMouseMove(const ToolContext &ctx, QMouseEvent *e) override;
    bool onMouseRelease(const ToolContext &ctx, QMouseEvent *e) override;
    bool onMouseDoubleClick(const ToolContext &ctx, QMouseEvent *e) override;
    bool onKeyPress(const ToolContext &ctx, QKeyEvent *e) override;
    void onViewChanged(const ToolContext &ctx) override { MeasureBase::onViewChanged(ctx); }

private:
    QVector<QPointF> m_pointsScene;
};

#endif // MAPTOOLS_H


