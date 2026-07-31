// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.

#include <QApplication>
#include "main_window.h"
#include "theme_manager.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("OpenSUP");
    app.setApplicationVersion("1.0.0");

    ThemeManager theme;  // Detect and apply system theme before window shows

    MainWindow window;
    window.show();

    return app.exec();
}
