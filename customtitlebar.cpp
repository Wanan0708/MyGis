// customtitlebar.cpp
#include "customtitlebar.h"
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QPalette>
#include <QStyleOption>
#include <QPainter>

CustomTitleBar::CustomTitleBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(32);

    titleLabel = new QLabel();
    titleLabel->setObjectName("titleLabel");
    // 使用应用图标代替文本标题
    QIcon appIco = qApp->windowIcon();
    QSize icoSize(20, 20);
    QPixmap pm = appIco.pixmap(icoSize);
    titleLabel->setPixmap(pm);
    titleLabel->setFixedSize(icoSize.width() + 8, icoSize.height() + 8);
    titleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    minButton = new QPushButton();
    maxButton = new QPushButton();
    closeButton = new QPushButton();
    
    // 设置按钮的对象名称
    minButton->setObjectName("minButton");
    maxButton->setObjectName("maxButton");
    closeButton->setObjectName("closeButton");

    // 设置图标（从资源加载）
    minButton->setIcon(QIcon(":/new/prefix1/image/minimize.png"));
    maxButton->setIcon(QIcon(":/new/prefix1/image/restore.png"));
    closeButton->setIcon(QIcon(":/new/prefix1/image/close.png"));

    // 设置图标大小（必须！否则可能不显示）
    minButton->setIconSize(QSize(16, 16));
    maxButton->setIconSize(QSize(16, 16));
    closeButton->setIconSize(QSize(16, 16));

    for (auto btn : {minButton, maxButton, closeButton}) {
        btn->setFixedSize(40, 32);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setText("");
    }

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 居中程序名
    centerTitleLabel = new QLabel("MicroGis", this);
    centerTitleLabel->setAlignment(Qt::AlignCenter);
    centerTitleLabel->setStyleSheet("font-weight:600; color: orange;" );

    // 布局：左侧图标，中间标题，右侧窗口控制按钮
    layout->addWidget(titleLabel);
    layout->addStretch();
    layout->addWidget(centerTitleLabel, /*stretch*/0, Qt::AlignCenter);
    layout->addStretch();
    layout->addWidget(minButton);
    layout->addWidget(maxButton);
    layout->addWidget(closeButton);

    connect(closeButton, &QPushButton::clicked, this, &CustomTitleBar::onClose);
    connect(minButton, &QPushButton::clicked, this, &CustomTitleBar::onMinimize);
    connect(maxButton, &QPushButton::clicked, this, &CustomTitleBar::onMaximizeRestore);
}

// 重写paintEvent来确保样式表能正确应用
void CustomTitleBar::paintEvent(QPaintEvent *event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void CustomTitleBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && !m_isMaximized) {
        if (!closeButton->underMouse() && !maxButton->underMouse() && !minButton->underMouse()) {
            m_dragPos = event->globalPos() - window()->frameGeometry().topLeft();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void CustomTitleBar::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton && !m_isMaximized) {
        if (!closeButton->underMouse() && !maxButton->underMouse() && !minButton->underMouse()) {
            auto targetPos = event->globalPos() - m_dragPos;
            auto screen = QGuiApplication::screenAt(event->globalPos());
            if (!screen) screen = QGuiApplication::primaryScreen();
            QRect screenGeo = screen->availableGeometry();

            // 最小可见区域：允许窗口部分出屏，但保留 kMinVisibleMargin 在屏内
            int minVisible = kMinVisibleMargin;
            int minX = screenGeo.left() - width() + minVisible;
            int maxX = screenGeo.right() - minVisible;
            int minY = screenGeo.top(); // 标题栏一般要求在屏内，避免拖不回
            int maxY = screenGeo.bottom() - 1; // 底部可部分出屏

            targetPos.setX(qBound(minX, targetPos.x(), maxX));
            targetPos.setY(qBound(minY, targetPos.y(), maxY));

            window()->move(targetPos);
            event->accept();
            return;
        }
    }
    QWidget::mouseMoveEvent(event);
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        onMaximizeRestore();
    }
    QWidget::mouseDoubleClickEvent(event);
}

void CustomTitleBar::onClose() {
    window()->close();
}

void CustomTitleBar::onMinimize() {
    window()->showMinimized();
}

void CustomTitleBar::onMaximizeRestore() {
    if (m_isMaximized) {
        window()->showNormal();
        maxButton->setIcon(QIcon(":/new/prefix1/image/restore.png")); // 还原为最大化图标
        m_isMaximized = false;
    } else {
        window()->showMaximized();
        maxButton->setIcon(QIcon(":/new/prefix1/image/maximize.png")); // 切换为还原图标
        m_isMaximized = true;
    }
}
