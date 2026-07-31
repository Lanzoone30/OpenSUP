// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.

#pragma once

#include <QObject>
#include <QPalette>

enum class Theme { System = 0, Light = 1, Dark = 2 };

class ThemeManager : public QObject {
    Q_OBJECT
public:
    explicit ThemeManager(QObject* parent = nullptr);

    void setTheme(Theme theme);
    Theme currentTheme() const { return m_theme; }
    bool isDark() const { return m_isDark; }

signals:
    void themeChanged(bool isDark);

private slots:
    void onSystemSchemeChanged();

private:
    void applyPalette();
    static QPalette darkPalette();
    static QPalette lightPalette();

    Theme m_theme = Theme::System;
    bool m_isDark = false;
};