TEMPLATE     = app
QT +=   gui \
  core
CONFIG +=   qt \
  warn_on \
  console \
  debug
FORMS       += viewer.ui
SOURCES     += main.cpp \
  thread.cpp \
  viewer.cpp
HEADERS     += viewer.h thread.h
unix:LIBS += `pkg-config --libs --static cairo`  -L../QCLib -lQC
INCLUDEPATH +=   ../QCLib
QMAKE_LFLAGS += -Bstatic
QMAKE_CXXFLAGS += `pkg-config --cflags cairo`
