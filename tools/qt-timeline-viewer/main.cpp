// Copyright (c) 2026 Richard Thomson

#include "Viewer.h"
#include <QApplication>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    timeline_qt_viewer::Viewer viewer;
    viewer.resize(1000, 640);
    viewer.show();
    return app.exec();
}
