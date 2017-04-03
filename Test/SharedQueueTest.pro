TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
TARGET = SharedQueueTest
DESTDIR = $$PWD/../../Bin
DEPENDPATH += .
INCLUDEPATH += ./

HEADERS +=	$$PWD/../Shared/Core/SharedQueue.h

SOURCES += 	$$PWD/../Shared/Core/SharedQueue.cpp \
			main.cpp \


