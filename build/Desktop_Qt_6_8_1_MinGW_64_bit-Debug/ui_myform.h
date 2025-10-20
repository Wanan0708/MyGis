/********************************************************************************
** Form generated from reading UI file 'myform.ui'
**
** Created by: Qt User Interface Compiler version 6.8.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MYFORM_H
#define UI_MYFORM_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListView>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MyForm
{
public:
    QGridLayout *gridLayout;
    QVBoxLayout *verticalLayout_2;
    QWidget *functionalArea;
    QPushButton *newButton;
    QPushButton *openButton;
    QPushButton *saveButton;
    QPushButton *saveAsButton;
    QPushButton *undoButton;
    QPushButton *redoButton;
    QLabel *statusLabel;
    QProgressBar *progressBar;
    QLabel *headerLabel;
    QPushButton *loadMapButton;
    QPushButton *zoomInButton;
    QPushButton *zoomOutButton;
    QPushButton *panButton;
    QPushButton *loadTileMapButton;
    QPushButton *zoomInTileMapButton;
    QPushButton *zoomOutTileMapButton;
    QSplitter *splitter;
    QListView *listView;
    QGraphicsView *graphicsView;

    void setupUi(QWidget *MyForm)
    {
        if (MyForm->objectName().isEmpty())
            MyForm->setObjectName("MyForm");
        MyForm->resize(800, 500);
        gridLayout = new QGridLayout(MyForm);
        gridLayout->setSpacing(4);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setContentsMargins(4, 4, 4, 4);
        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setSpacing(0);
        verticalLayout_2->setObjectName("verticalLayout_2");
        functionalArea = new QWidget(MyForm);
        functionalArea->setObjectName("functionalArea");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(functionalArea->sizePolicy().hasHeightForWidth());
        functionalArea->setSizePolicy(sizePolicy);
        functionalArea->setMinimumSize(QSize(0, 150));
        functionalArea->setMaximumSize(QSize(16777215, 150));
        newButton = new QPushButton(functionalArea);
        newButton->setObjectName("newButton");
        newButton->setGeometry(QRect(10, 10, 80, 24));
        openButton = new QPushButton(functionalArea);
        openButton->setObjectName("openButton");
        openButton->setGeometry(QRect(100, 10, 80, 24));
        saveButton = new QPushButton(functionalArea);
        saveButton->setObjectName("saveButton");
        saveButton->setGeometry(QRect(190, 10, 80, 24));
        saveAsButton = new QPushButton(functionalArea);
        saveAsButton->setObjectName("saveAsButton");
        saveAsButton->setGeometry(QRect(280, 10, 80, 24));
        undoButton = new QPushButton(functionalArea);
        undoButton->setObjectName("undoButton");
        undoButton->setGeometry(QRect(370, 10, 80, 24));
        redoButton = new QPushButton(functionalArea);
        redoButton->setObjectName("redoButton");
        redoButton->setGeometry(QRect(460, 10, 80, 24));
        statusLabel = new QLabel(functionalArea);
        statusLabel->setObjectName("statusLabel");
        statusLabel->setGeometry(QRect(10, 40, 500, 20));
        progressBar = new QProgressBar(functionalArea);
        progressBar->setObjectName("progressBar");
        progressBar->setGeometry(QRect(10, 65, 500, 20));
        progressBar->setValue(0);
        progressBar->setTextVisible(true);
        headerLabel = new QLabel(functionalArea);
        headerLabel->setObjectName("headerLabel");
        headerLabel->setGeometry(QRect(10, 90, 100, 20));
        QFont font;
        font.setBold(true);
        headerLabel->setFont(font);
        loadMapButton = new QPushButton(functionalArea);
        loadMapButton->setObjectName("loadMapButton");
        loadMapButton->setGeometry(QRect(10, 120, 80, 24));
        zoomInButton = new QPushButton(functionalArea);
        zoomInButton->setObjectName("zoomInButton");
        zoomInButton->setGeometry(QRect(100, 120, 80, 24));
        zoomOutButton = new QPushButton(functionalArea);
        zoomOutButton->setObjectName("zoomOutButton");
        zoomOutButton->setGeometry(QRect(190, 120, 80, 24));
        panButton = new QPushButton(functionalArea);
        panButton->setObjectName("panButton");
        panButton->setGeometry(QRect(280, 120, 80, 24));
        loadTileMapButton = new QPushButton(functionalArea);
        loadTileMapButton->setObjectName("loadTileMapButton");
        loadTileMapButton->setGeometry(QRect(370, 120, 100, 24));
        zoomInTileMapButton = new QPushButton(functionalArea);
        zoomInTileMapButton->setObjectName("zoomInTileMapButton");
        zoomInTileMapButton->setGeometry(QRect(480, 120, 80, 24));
        zoomOutTileMapButton = new QPushButton(functionalArea);
        zoomOutTileMapButton->setObjectName("zoomOutTileMapButton");
        zoomOutTileMapButton->setGeometry(QRect(570, 120, 80, 24));

        verticalLayout_2->addWidget(functionalArea);

        splitter = new QSplitter(MyForm);
        splitter->setObjectName("splitter");
        splitter->setOrientation(Qt::Orientation::Horizontal);
        splitter->setHandleWidth(4);
        listView = new QListView(splitter);
        listView->setObjectName("listView");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy1.setHorizontalStretch(1);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(listView->sizePolicy().hasHeightForWidth());
        listView->setSizePolicy(sizePolicy1);
        splitter->addWidget(listView);
        graphicsView = new QGraphicsView(splitter);
        graphicsView->setObjectName("graphicsView");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy2.setHorizontalStretch(4);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(graphicsView->sizePolicy().hasHeightForWidth());
        graphicsView->setSizePolicy(sizePolicy2);
        splitter->addWidget(graphicsView);

        verticalLayout_2->addWidget(splitter);


        gridLayout->addLayout(verticalLayout_2, 0, 0, 1, 1);


        retranslateUi(MyForm);

        QMetaObject::connectSlotsByName(MyForm);
    } // setupUi

    void retranslateUi(QWidget *MyForm)
    {
        MyForm->setWindowTitle(QCoreApplication::translate("MyForm", "Form", nullptr));
        newButton->setText(QCoreApplication::translate("MyForm", "New", nullptr));
        openButton->setText(QCoreApplication::translate("MyForm", "Open", nullptr));
        saveButton->setText(QCoreApplication::translate("MyForm", "Save", nullptr));
        saveAsButton->setText(QCoreApplication::translate("MyForm", "Save As", nullptr));
        undoButton->setText(QCoreApplication::translate("MyForm", "Undo", nullptr));
        redoButton->setText(QCoreApplication::translate("MyForm", "Redo", nullptr));
        statusLabel->setText(QCoreApplication::translate("MyForm", "Ready", nullptr));
        headerLabel->setText(QCoreApplication::translate("MyForm", "Map Controls:", nullptr));
        loadMapButton->setText(QCoreApplication::translate("MyForm", "Load Map", nullptr));
        zoomInButton->setText(QCoreApplication::translate("MyForm", "Zoom In", nullptr));
        zoomOutButton->setText(QCoreApplication::translate("MyForm", "Zoom Out", nullptr));
        panButton->setText(QCoreApplication::translate("MyForm", "Pan", nullptr));
        loadTileMapButton->setText(QCoreApplication::translate("MyForm", "Load Tile Map", nullptr));
        zoomInTileMapButton->setText(QCoreApplication::translate("MyForm", "Zoom In TM", nullptr));
        zoomOutTileMapButton->setText(QCoreApplication::translate("MyForm", "Zoom Out TM", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MyForm: public Ui_MyForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MYFORM_H
