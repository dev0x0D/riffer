QT += core gui quick quickcontrols2 testlib dbus multimedia

CONFIG += c++17 testcase
TARGET = riffer_tests
TEMPLATE = app

INCLUDEPATH += ../src /usr/include/soundtouch
LIBS += -lSoundTouch

HEADERS += \
    ../src/backend.h \
    ../src/clock.h \
    ../src/decoder.h \
    ../src/filepicker.h \
    ../src/metronome.h \
    ../src/peaks.h \
    ../src/player.h \
    ../src/portalfilepicker.h \
    ../src/theme.h \
    ../src/translator.h

SOURCES += \
    riffer_tests.cpp \
    ../src/backend.cpp \
    ../src/clock.cpp \
    ../src/decoder.cpp \
    ../src/metronome.cpp \
    ../src/peaks.cpp \
    ../src/player.cpp \
    ../src/portalfilepicker.cpp \
    ../src/theme.cpp \
    ../src/translator.cpp
