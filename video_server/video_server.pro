QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11


# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    adccapture_thread.cpp \
    aes.c \
    at24c64.cpp \
    capture_thread.cpp \
    main.cpp \
    mainwindow.cpp \
    mpu6050_thread.cpp \
    passworddialog.cpp \
    rc522.cpp \
    servocontrol.cpp \
    tcpservermanager.cpp \
    hardwaremanager.cpp \
    notificationservice.cpp

HEADERS += \
    adccapture_thread.h \
    at24c64.h \
    capture_thread.h \
    mainwindow.h \
    mpu6050_thread.h \
    passworddialog.h \
    rc522.h \
    servocontrol.h \
    tcpservermanager.h \
    hardwaremanager.h \
    notificationservice.h
    INCLUDEPATH += /home/tom/tool/opencv-3.1.0/install/include
    LIBS += -L/home/tom/tool/opencv-3.1.0/install/lib \
    -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_videoio -lopencv_imgcodecs
#    ../../lib/libopencv_core.so \
#    ../../lib/libopencv_highgui.so \
#    ../../lib/libopencv_imgproc.so \
#    ../../lib/libopencv_videoio.so \
#    ../../lib/libopencv_imgcodecs.so
#    QMAKE_CXXFLAGS += -O3 -mfpu=neon -march=armv7-a
#    QMAKE_CFLAGS   += -O3 -mfpu=neon -march=armv7-a
    QMAKE_CFLAGS   += -mcpu=cortex-a7 -mfpu=neon -mfloat-abi=hard -O3
    QMAKE_CXXFLAGS += -mcpu=cortex-a7 -mfpu=neon -mfloat-abi=hard -O3
# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
