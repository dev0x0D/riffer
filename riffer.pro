QT += core gui qml quick quickcontrols2 dbus multimedia

CONFIG += c++17 release
TARGET = riffer
TEMPLATE = app

# SoundTouch (LGPL) ships libSoundTouch.so and headers in /usr/include/soundtouch.
INCLUDEPATH += /usr/include/soundtouch
LIBS += -lSoundTouch

HEADERS += \
    src/backend.h \
    src/clock.h \
    src/decoder.h \
    src/filepicker.h \
    src/peaks.h \
    src/player.h \
    src/portalfilepicker.h \
    src/theme.h \
    src/translator.h \
    src/waveformitem.h

SOURCES += \
    src/backend.cpp \
    src/clock.cpp \
    src/decoder.cpp \
    src/main.cpp \
    src/peaks.cpp \
    src/player.cpp \
    src/portalfilepicker.cpp \
    src/theme.cpp \
    src/translator.cpp \
    src/waveformitem.cpp

RESOURCES += src/resources.qrc
