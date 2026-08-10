// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.

#include "opensup/pch.h"
#include "main_window.h"
#include "ui_main_window.h"
#include "encode_worker.h"
#include "qt_log_handler.h"

#include <QCloseEvent>
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

    ui->lbl_bdn_file_2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    ui->lbl_bdn_file_2->setMinimumHeight(30);
    ui->lbl_output_file_2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    ui->lbl_output_file_2->setMinimumHeight(30);

    ui->lbl_subtitle->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    connect(ui->cmb_language, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::on_lang_changed);
    connect(ui->cmb_theme, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::on_theme_changed);

    m_theme = new ThemeManager(this);
    ui->cmb_theme->setCurrentIndex(static_cast<int>(m_theme->currentTheme()));
    connect(m_theme, &ThemeManager::themeChanged, this, &MainWindow::onThemeChanged);

    applyComboStyles();

    QSettings lang_s("OpenSUP", "OpenSUP");
    ui->cmb_language->setCurrentIndex(lang_s.value("language", 0).toInt());

    auto* log_handler = new qt_log_handler_c(this);
    connect(log_handler, &qt_log_handler_c::logLine, this, &MainWindow::append_log);

    connect(ui->btn_select_bdn_2, &QPushButton::clicked, this, &MainWindow::select_bdn);
    connect(ui->btn_set_output_2, &QPushButton::clicked, this, &MainWindow::set_output);
    connect(ui->btn_encode_2, &QPushButton::clicked, this, &MainWindow::start_encode);
    connect(ui->btn_abort_2, &QPushButton::clicked, this, &MainWindow::abort_encode);
    connect(ui->btn_copy_log_2, &QPushButton::clicked, this, &MainWindow::copy_log);
    connect(ui->btn_clear_log_2, &QPushButton::clicked, this, &MainWindow::clear_log);

    connect(ui->chk_prefer_normal_2, &QCheckBox::toggled, this, &MainWindow::on_prefer_normal_changed);
    connect(ui->chk_both_formats_2, &QCheckBox::toggled, this, &MainWindow::on_both_formats_changed);

    retranslateUi();

    ui->configContainer->setFixedWidth(220);
}

// Stop the worker thread cleanly. The abort flag makes the worker exit at the
// next epoch boundary, so an unbounded wait is safe (bounded by one epoch).
// m_worker_thread is nulled by the finished() connection in start_encode(),
// so we never touch a freed pointer here.
static void stop_worker(QThread* thread, std::atomic<bool>& abort_flag)
{
    if (!thread) return;
    if (thread->isRunning()) {
        abort_flag.store(true);
        thread->requestInterruption();
        thread->wait();
    }
}

MainWindow::~MainWindow()
{
    stop_worker(m_worker_thread, m_abort_flag);
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    stop_worker(m_worker_thread, m_abort_flag);
    event->accept();
}

void MainWindow::select_bdn()
{
    QSettings s("OpenSUP", "OpenSUP");
    auto path = QFileDialog::getOpenFileName(this, "Select BDN XML",
        s.value("last_bdn_dir").toString(),
        "BDN XML files (*.xml);;All files (*.*)");
    if (path.isEmpty()) return;
    s.setValue("last_bdn_dir", QFileInfo(path).absolutePath());
    m_input_path = path;
    ui->lbl_bdn_file_2->setText(QFileInfo(path).fileName());
    ui->lbl_bdn_file_2->setToolTip(path);
    check_ready();
}

void MainWindow::set_output()
{
    QSettings s("OpenSUP", "OpenSUP");
    auto path = QFileDialog::getSaveFileName(this, "Set Output",
        s.value("last_output_dir").toString(),
        "SUP files (*.sup);;All files (*.*)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".sup", Qt::CaseInsensitive))
        path += ".sup";
    s.setValue("last_output_dir", QFileInfo(path).absolutePath());
    m_output_path = path;
    ui->lbl_output_file_2->setText(QFileInfo(path).fileName());
    ui->lbl_output_file_2->setToolTip(path);
    check_ready();
}

void MainWindow::check_ready()
{
    bool ready = !m_input_path.isEmpty() && !m_output_path.isEmpty();
    ui->btn_encode_2->setEnabled(ready);
}

// ── GUI interaction slots ──
void MainWindow::on_lang_changed(int index)
{
    m_lang = (index == 0) ? Lang::EN : Lang::ES;
    retranslateUi();
    QSettings s("OpenSUP", "OpenSUP");
    s.setValue("language", index);
}

void MainWindow::retranslateUi()
{
    // ---- Logo subtitle ----
    ui->lbl_subtitle->setText(tr_str("subtitle", m_lang));

    // ---- Project Scope ----
    ui->grp_files_2->setTitle(tr_str("projectScope", m_lang));
    ui->btn_select_bdn_2->setText(tr_str("selectBdn", m_lang));
    if (!ui->lbl_bdn_file_2->toolTip().isEmpty()) // keep filename if already selected
        ; // filename was set dynamically, skip
    else
        ui->lbl_bdn_file_2->setText(tr_str("noFile", m_lang));
    ui->btn_set_output_2->setText(tr_str("setOutput", m_lang));
    if (!ui->lbl_output_file_2->toolTip().isEmpty())
        ; // path was set dynamically, skip
    else
        ui->lbl_output_file_2->setText(tr_str("destNotSet", m_lang));

    // ---- Parameters ----
    ui->grp_parameters_2->setTitle(tr_str("parameters", m_lang));
    ui->label_colorspace->setText(tr_str("colorSpace", m_lang));
    ui->label_colorspace->setToolTip(tr_str("colorSpaceTip", m_lang));
    ui->label_quantizer->setText(tr_str("quantizer", m_lang));
    ui->combo_quantizer_2->setToolTip(tr_str("quantizerTip", m_lang));

    // ---- Engine Options ----
    ui->grp_options_2->setTitle(tr_str("engineOpts", m_lang));
    ui->chk_allow_normal_2->setText(tr_str("allowNormal", m_lang));
    ui->chk_allow_normal_2->setToolTip(tr_str("tipAllowNormal", m_lang));
    ui->chk_prefer_normal_2->setText(tr_str("preferNormal", m_lang));
    ui->chk_prefer_normal_2->setToolTip(tr_str("tipPreferNormal", m_lang));
    ui->chk_full_palette_2->setText(tr_str("fullPalette", m_lang));
    ui->chk_full_palette_2->setToolTip(tr_str("tipFullPalette", m_lang));
    ui->chk_both_formats_2->setText(tr_str("bothFormats", m_lang));
    ui->chk_both_formats_2->setToolTip(tr_str("tipBothFormats", m_lang));
    ui->chk_overlap_2->setText(tr_str("overlapBuf", m_lang));
    ui->chk_overlap_2->setToolTip(tr_str("tipOverlapBuf", m_lang));
    ui->chk_ignore_res_2->setText(tr_str("ignoreRes", m_lang));
    ui->chk_ignore_res_2->setToolTip(tr_str("tipIgnoreRes", m_lang));

    // ---- Activity Log ----
    ui->lbl_log_title_2->setText(tr_str("activityLog", m_lang));
    ui->lbl_log_lines_2->setText(tr_str("logLines", m_lang).arg(m_log_entries.size()));
    ui->btn_copy_log_2->setText(tr_str("copy", m_lang));
    ui->btn_clear_log_2->setText(tr_str("clear", m_lang));

    // ---- Progress ----
    ui->grp_progress->setTitle(tr_str("progress", m_lang));
    // The right side (lbl_eta_2) always shows only the ETA; state messages
    // go on the left label, just below the "Progress" title.
    switch (m_encode_state) {
        case EncodeState::Idle:
            ui->lbl_progress_text_2->setText(tr_str("standingBy", m_lang));
            break;
        case EncodeState::Running:
            ui->lbl_progress_text_2->setText(tr_str("starting", m_lang));
            break;
        case EncodeState::Done:
            ui->lbl_progress_text_2->setText(tr_str("done", m_lang));
            break;
        case EncodeState::Aborted:
            ui->lbl_progress_text_2->setText(tr_str("abortedShort", m_lang));
            break;
        case EncodeState::Failed:
            ui->lbl_progress_text_2->setText(tr_str("failed", m_lang));
            break;
    }
    ui->btn_encode_2->setText(tr_str("initEncode", m_lang));
    ui->btn_abort_2->setText(tr_str("abort", m_lang));

    // ---- Window title ----
    setWindowTitle(tr_str("windowTitle", m_lang));
}

void MainWindow::on_prefer_normal_changed(bool checked)
{
    // prefer_normal forces allow_normal ON and disables it
    ui->chk_allow_normal_2->setChecked(checked);
    ui->chk_allow_normal_2->setEnabled(!checked);
}

void MainWindow::on_both_formats_changed(bool checked)
{
    // PES output requires full palette
    ui->chk_full_palette_2->setChecked(checked);
    ui->chk_full_palette_2->setEnabled(!checked);
}

void MainWindow::start_encode()
{
    m_encode_state = EncodeState::Running;
    ui->btn_encode_2->setEnabled(false);
    ui->btn_abort_2->setEnabled(true);
    // Restore native style in case a previous run ended in the red aborted state.
    ui->progress_bar_2->setStyleSheet("");
    ui->progress_bar_2->setPalette(QPalette());
    ui->progress_bar_2->setValue(0);
    ui->lbl_progress_text_2->setText(tr_str("starting", m_lang));
    // Right side shows only the ETA; reset it until the worker emits the first one.
    ui->lbl_eta_2->setText("\u2014");

    auto cfg = opensup::core::encode_config_t{};
    cfg.input_path = m_input_path.toStdString();
    cfg.output_path = m_output_path.toStdString();
    cfg.quantizer_id = ui->combo_quantizer_2->currentIndex();
    cfg.overwrite = true;
    cfg.abort_flag = &m_abort_flag;
    cfg.ignore_resolution = ui->chk_ignore_res_2->isChecked();
    cfg.both_formats = ui->chk_both_formats_2->isChecked();
    cfg.allow_normal_case = ui->chk_allow_normal_2->isChecked();
    cfg.full_palette = ui->chk_full_palette_2->isChecked();
    cfg.overlap = ui->chk_overlap_2->isChecked();
    static const char* cs_map[] = { "bt709", "bt601", "bt2020" };
    cfg.bt_matrix = cs_map[ui->combo_colorspace_2->currentIndex()];

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
    // Null the member pointer when the thread finishes (it deletes itself via
    // deleteLater), so closeEvent/destructor never dereference freed memory.
    connect(m_worker_thread, &QThread::finished, this, [this]() {
        m_worker_thread = nullptr;
    });
    connect(m_worker_thread, &QThread::finished, m_worker_thread, &QObject::deleteLater);

    m_worker_thread->start();
}

void MainWindow::abort_encode()
{
    m_abort_flag.store(true);
    if (m_worker_thread && m_worker_thread->isRunning()) {
        m_worker_thread->requestInterruption();
        ui->btn_abort_2->setEnabled(false);
        ui->btn_encode_2->setEnabled(true);
        // Turn the bar red to signal the run was interrupted. We tint the
        // native Highlight color instead of using QSS, so Qt keeps drawing
        // its own segments with the exact same width/spacing.
        m_bar_aborted = true;
        ui->lbl_progress_text_2->setText(tr_str("abortedShort", m_lang));
        QPalette pal = ui->progress_bar_2->palette();
        pal.setColor(QPalette::Highlight, QColor("#E5484D"));
        ui->progress_bar_2->setPalette(pal);
        append_log("Encoding aborted by user.", 6);
    }
}

void MainWindow::copy_log()
{
    QApplication::clipboard()->setText(ui->txt_log_2->toPlainText());
    ui->btn_copy_log_2->setText(tr_str("copied", m_lang));
    QTimer::singleShot(2000, [this]() {
        ui->btn_copy_log_2->setText(tr_str("copyLog", m_lang));
    });
}

void MainWindow::clear_log()
{
    ui->txt_log_2->clear();
    ui->lbl_log_lines_2->setText(tr_str("logLines", m_lang).arg(0));
    // Reset the progress bar to 0 and restore the native look (undoes the
    // red aborted state if the last run was interrupted).
    ui->progress_bar_2->setStyleSheet("");
    ui->progress_bar_2->setPalette(QPalette());
    ui->progress_bar_2->setValue(0);
    m_encode_state = EncodeState::Idle;
    ui->lbl_progress_text_2->setText(tr_str("standingBy", m_lang));
    // Clear the ETA too; it will be filled again by the next run.
    ui->lbl_eta_2->setText("\u2014");
}

void MainWindow::update_progress(int percent)
{
    ui->progress_bar_2->setValue(percent);
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
    m_log_entries.append({text, level});

    auto pal = QApplication::palette();
    QColor color;
    switch (level) {
        case 0: case 1: color = pal.color(QPalette::PlaceholderText); break;
        case 5: color = QColor::fromRgb(184, 134, 11); break;   // #B8860B warning
        case 6: color = QColor::fromRgb(22, 101, 52); break;    // #166534 success
        case 7: case 8: case 9: color = QColor::fromRgb(204, 0, 0); break; // #CC0000 error
        default: color = pal.color(QPalette::Text); break;
    }

    ui->txt_log_2->setTextColor(color);
    auto ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    ui->txt_log_2->append(ts + " \u2502 " + QString(level_to_name(level)) + " \u2502 " + text);
    ui->txt_log_2->setTextColor(pal.color(QPalette::Text));

    auto doc = ui->txt_log_2->document();
    QString line_txt = tr_str("logLines", m_lang);
    ui->lbl_log_lines_2->setText(line_txt.arg(doc->blockCount()));
}

void MainWindow::update_eta(const QString& eta)
{
    ui->lbl_eta_2->setText(eta);
}

void MainWindow::encode_done(bool success)
{
    m_abort_flag.store(false);
    ui->btn_abort_2->setEnabled(false);
    ui->btn_encode_2->setEnabled(true);

    // State messages live on the left label; the right side (lbl_eta_2)
    // keeps showing only the ETA, so we never write status text into it.
    const bool aborted = m_bar_aborted;
    m_bar_aborted = false;

    if (success) {
        m_encode_state = EncodeState::Done;
        ui->progress_bar_2->setValue(100);
        ui->lbl_progress_text_2->setText(tr_str("done", m_lang));
    } else if (aborted) {
        // Aborted: keep the bar where it stopped, red. Reset happens on
        // clear_log() or the next start_encode().
        m_encode_state = EncodeState::Aborted;
        ui->lbl_progress_text_2->setText(tr_str("abortedShort", m_lang));
    } else {
        m_encode_state = EncodeState::Failed;
        ui->progress_bar_2->setValue(0);
        ui->lbl_progress_text_2->setText(tr_str("failed", m_lang));
    }

    // Revert the status label to "Standing by" after a while, unless the
    // user already started a new run or cleared the log.
    QTimer::singleShot(12000, this, [this] {
        if (m_encode_state == EncodeState::Failed
            || m_encode_state == EncodeState::Done
            || m_encode_state == EncodeState::Aborted) {
            m_encode_state = EncodeState::Idle;
            ui->lbl_progress_text_2->setText(tr_str("standingBy", m_lang));
        }
    });
}

void MainWindow::on_theme_changed(int index)
{
    Q_UNUSED(index);
    m_theme->setTheme(static_cast<Theme>(ui->cmb_theme->currentIndex()));
}

void MainWindow::onThemeChanged()
{
    // Snapshot: move entries out so append_log() doesn't modify while iterating
    auto entries = std::move(m_log_entries);
    ui->txt_log_2->clear();
    for (const auto& entry : entries)
        append_log(entry.first, entry.second);

    QString sbBg, sbHandle;
    if (m_theme->isDark()) {
        sbBg     = "#303544";
        sbHandle = "#4B5563";
    } else {
        sbBg     = "#E5E7EB";
        sbHandle = "#9CA3AF";
    }
    ui->txt_log_2->setStyleSheet(
        QString("border: 1px solid; border-radius: 8px;"
                "font-family: \"JetBrains Mono\", \"Consolas\", monospace;"
                "font-size: 13px; line-height: 1.6; padding: 16px;"
                "QScrollBar:vertical { background: %1; width: 10px; border-radius: 5px; }"
                "QScrollBar::handle:vertical { background: %2; border-radius: 5px; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
        .arg(sbBg, sbHandle));

    // Progress bar: default native rendering (as in the original UI file).

    applyComboStyles();
}

void MainWindow::applyComboStyles()
{
    bool isDark = m_theme ? m_theme->isDark() : false;
    QString borderColor = isDark ? "#9CA3AF" : "#374151";
    QString arrowColor  = isDark ? "#D1D5DB" : "#4B5563";
    QString hoverBorder = isDark ? "#60A5FA" : "#3B82F6";
    QString hoverBg     = isDark ? "#374151" : "#F3F4F6";
    QString dropdownBg  = isDark ? "#1F2937" : "#FFFFFF";

    QString style = QString(
        "QComboBox {"
        "  background: transparent;"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  padding: 6px 12px;"
        "  font-weight: 500;"
        "  min-width: 60px;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 20px;"
        "}"
        "QComboBox::down-arrow {"
        "  image: none;"
        "  border-left: 4px solid transparent;"
        "  border-right: 4px solid transparent;"
        "  border-top: 4px solid %2;"
        "  margin-right: 4px;"
        "}"
        "QComboBox:hover {"
        "  border-color: %3;"
        "  background: %4;"
        "}"
        "QComboBox:on {"
        "  border-color: %3;"
        "}"
        "QComboBox QAbstractItemView {"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  background: %5;"
        "  padding: 4px;"
        "  selection-background-color: %3;"
        "  selection-color: #FFFFFF;"
        "}"
    ).arg(borderColor, arrowColor, hoverBorder, hoverBg, dropdownBg);

    ui->cmb_language->setStyleSheet(style);
    ui->cmb_theme->setStyleSheet(style);
}
