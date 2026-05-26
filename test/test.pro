QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
win32: LIBS += -ldwmapi

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    project/main_window.cpp \
    project/logger.cpp \
    project/serial_manager.cpp \
    project/keyword_highlighter.cpp

HEADERS += \
    project/main_window.h \
    project/logger.h \
    project/serial_manager.h \
    project/keyword_highlighter.h

FORMS +=

TRANSLATIONS += \
    test_zh_CN.ts

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
