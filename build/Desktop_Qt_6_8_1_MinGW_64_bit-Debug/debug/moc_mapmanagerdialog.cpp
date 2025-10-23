/****************************************************************************
** Meta object code from reading C++ file 'mapmanagerdialog.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../mapmanagerdialog.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mapmanagerdialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN16MapManagerDialogE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN16MapManagerDialogE = QtMocHelpers::stringData(
    "MapManagerDialog",
    "requestSaveSettings",
    "",
    "requestStartDownload",
    "requestPause",
    "requestResume",
    "requestPauseTask",
    "taskId",
    "requestResumeTask",
    "requestCancelTask",
    "onTaskProgress",
    "completed",
    "total",
    "getSettings",
    "MapManagerSettings",
    "setSettings",
    "s"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN16MapManagerDialogE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      10,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   74,    2, 0x06,    1 /* Public */,
       3,    0,   75,    2, 0x06,    2 /* Public */,
       4,    0,   76,    2, 0x06,    3 /* Public */,
       5,    0,   77,    2, 0x06,    4 /* Public */,
       6,    1,   78,    2, 0x06,    5 /* Public */,
       8,    1,   81,    2, 0x06,    7 /* Public */,
       9,    1,   84,    2, 0x06,    9 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      10,    3,   87,    2, 0x0a,   11 /* Public */,
      13,    0,   94,    2, 0x10a,   15 /* Public | MethodIsConst  */,
      15,    1,   95,    2, 0x0a,   16 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    7,
    QMetaType::Void, QMetaType::QString,    7,
    QMetaType::Void, QMetaType::QString,    7,

 // slots: parameters
    QMetaType::Void, QMetaType::QString, QMetaType::Int, QMetaType::Int,    7,   11,   12,
    0x80000000 | 14,
    QMetaType::Void, 0x80000000 | 14,   16,

       0        // eod
};

Q_CONSTINIT const QMetaObject MapManagerDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_ZN16MapManagerDialogE.offsetsAndSizes,
    qt_meta_data_ZN16MapManagerDialogE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN16MapManagerDialogE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MapManagerDialog, std::true_type>,
        // method 'requestSaveSettings'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'requestStartDownload'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'requestPause'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'requestResume'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'requestPauseTask'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'requestResumeTask'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'requestCancelTask'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onTaskProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'getSettings'
        QtPrivate::TypeAndForceComplete<MapManagerSettings, std::false_type>,
        // method 'setSettings'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const MapManagerSettings &, std::false_type>
    >,
    nullptr
} };

void MapManagerDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MapManagerDialog *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->requestSaveSettings(); break;
        case 1: _t->requestStartDownload(); break;
        case 2: _t->requestPause(); break;
        case 3: _t->requestResume(); break;
        case 4: _t->requestPauseTask((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->requestResumeTask((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->requestCancelTask((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->onTaskProgress((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 8: { MapManagerSettings _r = _t->getSettings();
            if (_a[0]) *reinterpret_cast< MapManagerSettings*>(_a[0]) = std::move(_r); }  break;
        case 9: _t->setSettings((*reinterpret_cast< std::add_pointer_t<MapManagerSettings>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (MapManagerDialog::*)();
            if (_q_method_type _q_method = &MapManagerDialog::requestSaveSettings; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (MapManagerDialog::*)();
            if (_q_method_type _q_method = &MapManagerDialog::requestStartDownload; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (MapManagerDialog::*)();
            if (_q_method_type _q_method = &MapManagerDialog::requestPause; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (MapManagerDialog::*)();
            if (_q_method_type _q_method = &MapManagerDialog::requestResume; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (MapManagerDialog::*)(const QString & );
            if (_q_method_type _q_method = &MapManagerDialog::requestPauseTask; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (MapManagerDialog::*)(const QString & );
            if (_q_method_type _q_method = &MapManagerDialog::requestResumeTask; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (MapManagerDialog::*)(const QString & );
            if (_q_method_type _q_method = &MapManagerDialog::requestCancelTask; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
    }
}

const QMetaObject *MapManagerDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MapManagerDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN16MapManagerDialogE.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int MapManagerDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void MapManagerDialog::requestSaveSettings()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void MapManagerDialog::requestStartDownload()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void MapManagerDialog::requestPause()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void MapManagerDialog::requestResume()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void MapManagerDialog::requestPauseTask(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void MapManagerDialog::requestResumeTask(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void MapManagerDialog::requestCancelTask(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}
QT_WARNING_POP
