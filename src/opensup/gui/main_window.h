#pragma once

#include <QMainWindow>
#include <QString>
#include <QThread>
#include <QTextCharFormat>

#include "opensup/core/interface.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

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

private:
    void check_ready();

    Ui::MainWindow* ui;
    QString m_input_path;
    QString m_output_path;
    QThread* m_worker_thread = nullptr;
};
