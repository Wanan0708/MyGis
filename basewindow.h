// basewindow.h
#ifndef BASEWINDOW_H
#define BASEWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include "customtitlebar.h"

class BaseWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit BaseWindow(QWidget *parent = nullptr);
    void setContentWidget(QWidget *content);

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    // 边框缩放
    enum ResizeDirection {
        None = 0, Top = 1, Bottom = 2, Left = 4, Right = 8,
        TopLeft = Top | Left, TopRight = Top | Right,
        BottomLeft = Bottom | Left, BottomRight = Bottom | Right
    };
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    CustomTitleBar *titleBar = nullptr;
    QWidget *contentContainer = nullptr;
    QWidget *centralWidgetContainer = nullptr;

    ResizeDirection getResizeDirection(const QPoint &pos) const;
    ResizeDirection m_resizeDir = None;
    bool m_resizing = false;
    QRect m_startGeometry;
    QPoint m_startMousePos;
    static const int BORDER_WIDTH;
};

#endif // BASEWINDOW_H
