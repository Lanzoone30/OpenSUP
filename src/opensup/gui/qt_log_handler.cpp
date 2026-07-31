// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.

#include "opensup/pch.h"
#include "qt_log_handler.h"
#include "opensup/common/logger.h"

#include <QMetaObject>
#include <QString>

qt_log_handler_c::qt_log_handler_c(QObject* parent)
    : QObject(parent)
{
    opensup::common::logger_c::instance().set_callback(
        [this](const std::string& msg, int level) {
            QMetaObject::invokeMethod(this, [this, msg, level]() {
                emit logLine(QString::fromStdString(msg), level);
            }, Qt::QueuedConnection);
        });
}
