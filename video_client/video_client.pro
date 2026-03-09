QT       += core gui network multimedia sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17 resources_big

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
QMAKE_CXXFLAGS += -Wno-deprecated-copy
SOURCES += \
    aes.c \
    changepwddialog.cpp \
    configmanager.cpp \
    faceservice.cpp \
    facedbdialog.cpp \
    facedbmanager.cpp \
    facedetectcnn.cpp \
    facerecognizerthread.cpp \
    faceutils.cpp \
    main.cpp \
    mainwindow.cpp \
    networkmanager.cpp \
    notifydialog.cpp \
    uimanager.cpp

HEADERS += \
    changepwddialog.h \
    configmanager.h \
    faceservice.h \
    facedbdialog.h \
    facedbmanager.h \
    facerecognizerthread.h \
    faceutils.h \
    mainwindow.h \
    networkmanager.h \
    notifydialog.h \
    uimanager.h

INCLUDEPATH += $$PWD/facedetection_sdk/include


    INCLUDEPATH += /usr/local/include
    LIBS += $$PWD/facedetection_sdk/lib/libfacedetection.a
    LIBS += -L/usr/local/lib /usr/local/lib/libopencv_* -ldlib
    LIBS += -L/usr/lib/x86_64-linux-gnu/openblas-pthread -lopenblas

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# 复制 config.ini 到编译目录
unix {
    copyInput.files = config.ini
    copyInput.path = $$OUT_PWD
    QMAKE_EXTRA_TARGETS += copyInput
    PRE_TARGETDEPS += copyInput
}


# 开启O3优化（最高级别） 加了之后人脸识别速度快很多
QMAKE_CXXFLAGS += -O3 -march=native
QMAKE_CFLAGS += -O3 -march=native


