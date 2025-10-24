// customtitlebar.h
#ifndef CUSTOMTITLEBAR_H
#define CUSTOMTITLEBAR_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>

class CustomTitleBar : public QWidget {
    Q_OBJECT

public:
    explicit CustomTitleBar(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;  // 添加这一行

private slots:
    void onClose();
    void onMinimize();
    void onMaximizeRestore();

private:
    QLabel *titleLabel;
    QLabel *centerTitleLabel;
    QPushButton *minButton;
    QPushButton *maxButton;
    QPushButton *closeButton;

    bool m_isMaximized = false;
    QPoint m_dragPos;
    const int kMinVisibleMargin = 80; // 最小可见边距（像素）
};

#endif // CUSTOMTITLEBAR_H