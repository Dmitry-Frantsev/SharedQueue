TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
TARGET = SharedQueueTest
DESTDIR = $$PWD/../Bin
DEPENDPATH += .
INCLUDEPATH += ./

HEADERS +=	$$PWD/../Src/SharedQueue.h

SOURCES += 	$$PWD/../Src/SharedQueue.cpp \
			main.cpp \


