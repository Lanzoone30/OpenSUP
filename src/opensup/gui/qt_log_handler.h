// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#pragma once

#include <QObject>

// simple bridge from logger_c → Qt signals.
// Full-featured handler with level filtering if needed later.
class qt_log_handler_c : public QObject {
    Q_OBJECT

public:
    explicit qt_log_handler_c(QObject* parent = nullptr);

signals:
    void logLine(const QString& text, int level);
};
