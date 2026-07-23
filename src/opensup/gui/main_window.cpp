#include "opensup/pch.h"
#include "main_window.h"
#include "ui_main_window.h"
#include "encode_worker.h"
#include "qt_log_handler.h"

#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QIcon>
#include <QTextCursor>
#include <QDateTime>
#include <QFileInfo>
#include <QTimer>
#include <QPushButton>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setFixedSize(970, 689);

    auto icon_path = QApplication::applicationDirPath() + "/../../assets/OpenSup.ico";
    setWindowIcon(QIcon(icon_path));

    auto* log_handler = new qt_log_handler_c(this);
    connect(log_handler, &qt_log_handler_c::logLine, this, &MainWindow::append_log);

    connect(ui->btn_select_bdn, &QPushButton::clicked, this, &MainWindow::select_bdn);
    connect(ui->btn_set_output, &QPushButton::clicked, this, &MainWindow::set_output);
    connect(ui->btn_encode, &QPushButton::clicked, this, &MainWindow::start_encode);
    connect(ui->btn_abort, &QPushButton::clicked, this, &MainWindow::abort_encode);
    connect(ui->btn_copy_log, &QPushButton::clicked, this, &MainWindow::copy_log);
    connect(ui->btn_clear_log, &QPushButton::clicked, this, &MainWindow::clear_log);
}

MainWindow::~MainWindow()
{
    if (m_worker_thread) {
        m_worker_thread->quit();
        m_worker_thread->wait();
    }
    delete ui;
}

void MainWindow::select_bdn()
{
    auto path = QFileDialog::getOpenFileName(this, "Select BDN XML", {},
        "BDN XML files (*.xml);;All files (*.*)");
    if (path.isEmpty()) return;
    m_input_path = path;
    ui->lbl_bdn_file->setText(QFileInfo(path).fileName());
    ui->lbl_bdn_file->setToolTip(path);
    ui->lbl_bdn_file->setStyleSheet("color: #000000");
    check_ready();
}

void MainWindow::set_output()
{
    auto path = QFileDialog::getSaveFileName(this, "Set Output", {},
        "SUP files (*.sup);;All files (*.*)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".sup", Qt::CaseInsensitive))
        path += ".sup";
    m_output_path = path;
    ui->lbl_output_file->setText(QFileInfo(path).fileName());
    ui->lbl_output_file->setToolTip(path);
    ui->lbl_output_file->setStyleSheet("color: #000000");
    check_ready();
}

void MainWindow::check_ready()
{
    bool ready = !m_input_path.isEmpty() && !m_output_path.isEmpty();
    ui->btn_encode->setEnabled(ready);
}

void MainWindow::start_encode()
{
    ui->btn_encode->setEnabled(false);
    ui->btn_abort->setEnabled(true);
    ui->progress_bar->setValue(0);
    ui->lbl_eta->setText("Starting...");

    auto cfg = opensup::core::encode_config_t{};
    cfg.input_path = m_input_path.toStdString();
    cfg.output_path = m_output_path.toStdString();
    cfg.compression = ui->spin_compression->value() / 100.0;
    cfg.acquisition_rate = ui->spin_acqrate->value() / 100.0;
    cfg.quantizer_id = ui->combo_quantizer->currentIndex();
    cfg.threads = (ui->combo_threads->currentText() == "auto")
                  ? 4 : ui->combo_threads->currentText().toInt();
    cfg.overwrite = true;
    cfg.ignore_resolution = ui->chk_ignore_res->isChecked();
    cfg.both_formats = ui->chk_both_formats->isChecked();

    m_worker_thread = new QThread(this);
    auto* worker = new encode_worker_c(cfg);

    worker->moveToThread(m_worker_thread);

    connect(m_worker_thread, &QThread::started, worker, &encode_worker_c::process);
    connect(worker, &encode_worker_c::finished, this, &MainWindow::encode_done);
    connect(worker, &encode_worker_c::progressChanged, this, &MainWindow::update_progress);
    connect(worker, &encode_worker_c::logLine, this, &MainWindow::append_log);
    connect(worker, &encode_worker_c::etaUpdated, this, &MainWindow::update_eta);
    connect(worker, &encode_worker_c::finished, m_worker_thread, &QThread::quit);
    connect(worker, &encode_worker_c::finished, worker, &QObject::deleteLater);
    connect(m_worker_thread, &QThread::finished, m_worker_thread, &QObject::deleteLater);

    m_worker_thread->start();
}

void MainWindow::abort_encode()
{
    if (m_worker_thread && m_worker_thread->isRunning()) {
        m_worker_thread->requestInterruption();
        ui->lbl_eta->setText("Encoding aborted by user");
        ui->btn_abort->setEnabled(false);
        ui->btn_encode->setEnabled(true);
        append_log("Encoding aborted by user.", 6);
    }
}

void MainWindow::copy_log()
{
    QApplication::clipboard()->setText(ui->txt_log->toPlainText());
    ui->btn_copy_log->setText("Copied");
    QTimer::singleShot(2000, [this]() { ui->btn_copy_log->setText("Copy log"); });
}

void MainWindow::clear_log()
{
    ui->txt_log->clear();
    ui->lbl_log_lines->setText("[0 lines]");
}

void MainWindow::update_progress(int percent)
{
    ui->progress_bar->setValue(percent);
}

static const char*
level_to_name(int level)
{
    switch (level) {
        case 0: case 1: return "DEBUG";
        case 2: case 3: case 4: return "INFO";
        case 5: return "WARN";
        case 6: return "PASS";
        case 7: return "ERROR";
        case 8: return "FAIL";
        case 9: return "FATAL";
        default: return "????";
    }
}

void MainWindow::append_log(const QString& text, int level)
{
    QColor color;
    switch (level) {
        case 0: case 1: color = QColor("#888888"); break;
        case 5: color = QColor("#B8860B"); break;
        case 6: color = QColor("#166534"); break;
        case 7: case 8: case 9: color = QColor("#CC0000"); break;
        default: color = QColor("#000000"); break;
    }

    ui->txt_log->setTextColor(color);
    auto ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    ui->txt_log->append(ts + " \u2502 " + QString(level_to_name(level)) + " \u2502 " + text);
    ui->txt_log->setTextColor(QColor("#000000"));

    auto doc = ui->txt_log->document();
    ui->lbl_log_lines->setText(QString("[%1 lines]").arg(doc->blockCount()));
}

void MainWindow::update_eta(const QString& eta)
{
    ui->lbl_eta->setText(eta);
}

void MainWindow::encode_done(bool success)
{
    ui->btn_abort->setEnabled(false);
    ui->btn_encode->setEnabled(true);

    if (success) {
        ui->progress_bar->setValue(100);
        ui->lbl_eta->setText("Done");
    } else {
        ui->progress_bar->setValue(0);
        ui->lbl_eta->setText("Encoding FAILED - see log for details");
    }
}
