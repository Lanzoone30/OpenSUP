// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.

#include "theme_manager.h"

#include <QApplication>
#include <QSettings>
#include <QStyle>
#include <QStyleHints>
#include <QWidget>

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
{
    QSettings s("OpenSUP", "OpenSUP");
    m_theme = static_cast<Theme>(s.value("theme", 0).toInt());

    connect(QApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, &ThemeManager::onSystemSchemeChanged);

    applyPalette();
}

void ThemeManager::setTheme(Theme theme)
{
    m_theme = theme;
    QSettings s("OpenSUP", "OpenSUP");
    s.setValue("theme", static_cast<int>(theme));
    applyPalette();
}

void ThemeManager::onSystemSchemeChanged()
{
    if (m_theme == Theme::System)
        applyPalette();
}

void ThemeManager::applyPalette()
{
    if (m_theme == Theme::System) {
        auto scheme = QApplication::styleHints()->colorScheme();
        m_isDark = (scheme == Qt::ColorScheme::Dark);
    } else {
        m_isDark = (m_theme == Theme::Dark);
    }
    QApplication::setPalette(m_isDark ? darkPalette() : lightPalette());

    // Force all existing widgets to inherit the new palette
    // (widgets created before setPalette() don't refresh automatically)
    const auto widgets = QApplication::allWidgets();
    for (QWidget* w : widgets)
        w->setPalette(QPalette());

    emit themeChanged(m_isDark);
}

QPalette ThemeManager::darkPalette()
{
    QPalette p;

    // Base surface - Dark but not pure black (easier on eyes)
    p.setColor(QPalette::Window,          QColor::fromRgb(26, 29, 38));
    p.setColor(QPalette::WindowText,      QColor::fromRgb(235, 237, 245));

    // Input/Text areas - Slightly lighter for contrast
    p.setColor(QPalette::Base,            QColor::fromRgb(38, 42, 54));
    p.setColor(QPalette::AlternateBase,   QColor::fromRgb(48, 53, 68));

    // Primary text - High contrast for readability
    p.setColor(QPalette::Text,            QColor::fromRgb(235, 237, 245));
    p.setColor(QPalette::ButtonText,      QColor::fromRgb(235, 237, 245));
    p.setColor(QPalette::PlaceholderText, QColor::fromRgb(120, 125, 145));

    // Buttons - Distinct from background but not too bright
    p.setColor(QPalette::Button,          QColor::fromRgb(48, 53, 68));

    // Tooltips - Slightly elevated surface
    p.setColor(QPalette::ToolTipBase,     QColor::fromRgb(48, 53, 68));
    p.setColor(QPalette::ToolTipText,     QColor::fromRgb(235, 237, 245));

    // Highlight - Same accent blue for consistency
    p.setColor(QPalette::Highlight,       QColor::fromRgb(59, 130, 246));
    p.setColor(QPalette::HighlightedText, QColor::fromRgb(255, 255, 255));

    // Surface elevation hierarchy (for depth)
    p.setColor(QPalette::Light,           QColor::fromRgb(58, 64, 82));
    p.setColor(QPalette::Midlight,        QColor::fromRgb(48, 53, 68));
    p.setColor(QPalette::Mid,             QColor::fromRgb(38, 42, 54));
    p.setColor(QPalette::Dark,            QColor::fromRgb(26, 29, 38));
    p.setColor(QPalette::Shadow,          QColor::fromRgb(18, 20, 26));

    // Link colors - Professional and accessible
    p.setColor(QPalette::Link,            QColor::fromRgb(96, 165, 250));
    p.setColor(QPalette::LinkVisited,     QColor::fromRgb(167, 139, 250));

    return p;
}

QPalette ThemeManager::lightPalette()
{
    QPalette p;

    // Base surface - Clean white background
    p.setColor(QPalette::Window,          QColor::fromRgb(248, 249, 250));
    p.setColor(QPalette::WindowText,      QColor::fromRgb(26, 32, 44));

    // Input/Text areas - Slightly off-white for readability
    p.setColor(QPalette::Base,            QColor::fromRgb(255, 255, 255));
    p.setColor(QPalette::AlternateBase,   QColor::fromRgb(240, 242, 245));

    // Text colors
    p.setColor(QPalette::Text,            QColor::fromRgb(26, 32, 44));
    p.setColor(QPalette::ButtonText,      QColor::fromRgb(26, 32, 44));
    p.setColor(QPalette::PlaceholderText, QColor::fromRgb(156, 163, 175));

    // Buttons - Slightly tinted for separation
    p.setColor(QPalette::Button,          QColor::fromRgb(235, 237, 240));

    // Tooltips - Clean light style
    p.setColor(QPalette::ToolTipBase,     QColor::fromRgb(255, 255, 255));
    p.setColor(QPalette::ToolTipText,     QColor::fromRgb(26, 32, 44));

    // Highlight colors (for selection, focus, etc.)
    p.setColor(QPalette::Highlight,       QColor::fromRgb(59, 130, 246));
    p.setColor(QPalette::HighlightedText, QColor::fromRgb(255, 255, 255));

    // Surface elevation hierarchy
    p.setColor(QPalette::Light,           QColor::fromRgb(255, 255, 255));
    p.setColor(QPalette::Midlight,        QColor::fromRgb(240, 242, 245));
    p.setColor(QPalette::Mid,             QColor::fromRgb(200, 204, 210));
    p.setColor(QPalette::Dark,            QColor::fromRgb(156, 163, 175));
    p.setColor(QPalette::Shadow,          QColor::fromRgb(107, 114, 128));

    // Link colors
    p.setColor(QPalette::Link,            QColor::fromRgb(59, 130, 246));
    p.setColor(QPalette::LinkVisited,     QColor::fromRgb(139, 92, 246));

    return p;
}