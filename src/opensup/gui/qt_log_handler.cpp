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
