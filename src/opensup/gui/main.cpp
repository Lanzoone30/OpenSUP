// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include <QApplication>
#include "main_window.h"
#include "theme_manager.h"
#include "opensup/version.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("OpenSUP");
    app.setApplicationVersion(OPENSUP_VERSION_STRING);

    ThemeManager theme;  // Detect and apply system theme before window shows

    MainWindow window;
    window.show();

    return app.exec();
}
