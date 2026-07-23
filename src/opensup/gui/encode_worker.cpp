#include "opensup/pch.h"
#include "encode_worker.h"
#include "opensup/core/interface.h"
#include "opensup/common/logger.h"

#include <QThread>
#include <QDateTime>

encode_worker_c::encode_worker_c(const opensup::core::encode_config_t& config)
    : m_config(config) {}

void encode_worker_c::process()
{
    emit logLine("Encoding started", 2);
    emit logLine(QString("Input:  ") + QString::fromStdString(m_config.input_path), 2);
    emit logLine(QString("Output: ") + QString::fromStdString(m_config.output_path), 2);

    try {
        opensup::core::bdn_render_c renderer(m_config);
        auto result = renderer.execute();

        if (result.success) {
            emit logLine(QString("BDN metadata: %1 events, %2 epochs")
                .arg(result.events).arg(result.epochs), 2);
            emit logLine(QString("Output: %1 segments, %2 ms")
                .arg(result.segments).arg(result.duration_ms), 2);
            emit progressChanged(100);
            emit etaUpdated("Done");
        } else {
            emit logLine(QString("FAILED: ") + QString::fromStdString(result.error), 7);
        }
        emit finished(result.success);

    } catch (const std::exception& e) {
        emit logLine(QString("Error: ") + e.what(), 7);
        emit finished(false);
    }
}
