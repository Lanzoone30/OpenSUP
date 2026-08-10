// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#pragma once

#include <QMainWindow>
#include <QPair>
#include <QString>
#include <QThread>
#include <QTextCharFormat>
#include <QVector>
#include <atomic>
#include <QSettings>

#include "opensup/core/interface.h"
#include "opensup/gui/translations.h"
#include "opensup/gui/theme_manager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

enum class EncodeState { Idle, Running, Done, Aborted, Failed };

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void select_bdn();
    void set_output();
    void start_encode();
    void abort_encode();
    void copy_log();
    void clear_log();

    void update_progress(int percent);
    void append_log(const QString& text, int level);
    void update_eta(const QString& eta);
    void encode_done(bool success);

    void on_prefer_normal_changed(bool checked);
    void on_both_formats_changed(bool checked);
    void on_lang_changed(int index);
    void on_theme_changed(int index);
    void onThemeChanged();
    void applyComboStyles();

private:
    void check_ready();
    void retranslateUi();
    void closeEvent(QCloseEvent* event) override;

    Ui::MainWindow* ui;
    ThemeManager* m_theme = nullptr;
    Lang m_lang = Lang::EN;
    EncodeState m_encode_state = EncodeState::Idle;
    QString m_input_path;
    QString m_output_path;
    QThread* m_worker_thread = nullptr;
    QVector<QPair<QString, int>> m_log_entries;
    std::atomic<bool> m_abort_flag{false};
    bool m_bar_aborted = false; // true while the bar shows the red aborted state
};
