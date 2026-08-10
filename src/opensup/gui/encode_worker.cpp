// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include "opensup/pch.h"
#include "encode_worker.h"
#include "opensup/core/interface.h"
#include "opensup/common/logger.h"

#include <QThread>
#include <QDateTime>
#include <chrono>

encode_worker_c::encode_worker_c(const opensup::core::encode_config_t& config)
    : m_config(config) {}

void encode_worker_c::process()
{
    emit logLine("Encoding started", 2);
    emit logLine(QString("Input:  ") + QString::fromStdString(m_config.input_path), 2);
    emit logLine(QString("Output: ") + QString::fromStdString(m_config.output_path), 2);

    // Progress + ETA per epoch. Runs on the worker thread; progressChanged is
    // a queued connection to the main thread, so emitting here is safe.
    auto encode_start = std::chrono::steady_clock::now();
    m_config.progress_cb = [this, encode_start](int percent, int epoch, int total) {
        emit progressChanged(percent);
        if (total <= 0) return;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - encode_start).count();
        if (epoch >= total) {
            emit etaUpdated(QString("Epoch %1/%2").arg(epoch).arg(total));
            return;
        }
        auto remaining_ms = static_cast<int64_t>(elapsed_ms) * (total - epoch) / epoch;
        auto remaining_s = remaining_ms / 1000;
        auto eta = remaining_s >= 60
            ? QString("%1m %2s").arg(remaining_s / 60).arg(remaining_s % 60)
            : QString("%1s").arg(remaining_s);
        emit etaUpdated(QString("Epoch %1/%2 · ETA %3").arg(epoch).arg(total).arg(eta));
    };

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
