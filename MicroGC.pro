QT += core gui widgets printsupport serialbus

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    communication.cpp \
    controltab.cpp \
    dataprocessing.cpp \
    gassceneitems.cpp \
    interactiveplot.cpp \
    logwidget.cpp \
    main.cpp \
    mainwindow.cpp \
    monitortab.cpp \
    othertab.cpp \
    qcustomplot.cpp

HEADERS += \
    communication.h \
    controltab.h \
    dataprocessing.h \
    gassceneitems.h \
    interactiveplot.h \
    logwidget.h \
    mainwindow.h \
    monitortab.h \
    othertab.h \
    qcustomplot.h

INCLUDEPATH += $$PWD

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    img.qrc

win32: RC_ICONS = img/logo.ico