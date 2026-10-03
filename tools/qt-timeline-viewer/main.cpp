// Copyright (c) 2026 Richard Thomson

#include "Viewer.h"
#include <QApplication>
#include <QMessageBox>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    std::filesystem::path startup_path;
    if (arguments.size() > 1)
    {
#ifdef _WIN32
        startup_path = std::filesystem::path(arguments[1].toStdWString());
#else
        startup_path = std::filesystem::u8path(arguments[1].toStdString());
#endif
    }
    timeline_qt_viewer::Viewer viewer(startup_path);
    viewer.resize(1000, 640);
    viewer.show();
    if (!viewer.diagnostics().empty())
    {
        QStringList messages;
        for (const std::string &message : viewer.diagnostics())
        {
            messages.append(QString::fromUtf8(message.c_str()));
        }
        QMessageBox::warning(&viewer, QStringLiteral("Import diagnostics"), messages.join('\n'));
    }
    return app.exec();
}
