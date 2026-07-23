#pragma once

#include <QObject>
#include "opensup/core/interface.h"

class encode_worker_c : public QObject {
    Q_OBJECT

public:
    explicit encode_worker_c(const opensup::core::encode_config_t& config);

public slots:
    void process();

signals:
    void progressChanged(int percent);
    void logLine(const QString& text, int level);
    void etaUpdated(const QString& eta);
    void finished(bool success);

private:
    opensup::core::encode_config_t m_config;
};
