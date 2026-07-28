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

    // Fix left panel width so language toggle doesn't reflow layout
    // configContainer max-width = 400 set in .ui

    connect(ui->cmb_language, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::on_lang_changed);
    connect(ui->cmb_theme, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::on_theme_changed);

    m_theme = new ThemeManager(this);
    ui->cmb_theme->setCurrentIndex(static_cast<int>(m_theme->currentTheme()));
    connect(m_theme, &ThemeManager::themeChanged, this, &MainWindow::onThemeChanged);

    auto* log_handler = new qt_log_handler_c(this);
    connect(log_handler, &qt_log_handler_c::logLine, this, &MainWindow::append_log);

    connect(ui->btn_select_bdn_2, &QPushButton::clicked, this, &MainWindow::select_bdn);
    connect(ui->btn_set_output_2, &QPushButton::clicked, this, &MainWindow::set_output);
    connect(ui->btn_encode_2, &QPushButton::clicked, this, &MainWindow::start_encode);
    connect(ui->btn_abort_2, &QPushButton::clicked, this, &MainWindow::abort_encode);
    connect(ui->btn_copy_log_2, &QPushButton::clicked, this, &MainWindow::copy_log);
    connect(ui->btn_clear_log_2, &QPushButton::clicked, this, &MainWindow::clear_log);

    // GUI interactions: prefer_normal -> allow_normal, both_formats -> full_palette
    connect(ui->chk_prefer_normal_2, &QCheckBox::toggled, this, &MainWindow::on_prefer_normal_changed);
    connect(ui->chk_both_formats_2, &QCheckBox::toggled, this, &MainWindow::on_both_formats_changed);

    // Apply initial language strings
    retranslateUi();

    // Lock left panel width wide enough for both EN and ES text
    ui->configContainer->setFixedWidth(220);
}

MainWindow::~MainWindow()
{
    if (m_worker_thread && m_worker_thread->isRunning()) {
        m_worker_thread->requestInterruption();
        m_worker_thread->quit();
        m_worker_thread->wait(3000);
    }
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_worker_thread && m_worker_thread->isRunning()) {
        m_worker_thread->requestInterruption();
        m_worker_thread->quit();
        m_worker_thread->wait(3000);
    }
    event->accept();
}

void MainWindow::select_bdn()
{
    auto path = QFileDialog::getOpenFileName(this, "Select BDN XML", {},
        "BDN XML files (*.xml);;All files (*.*)");
    if (path.isEmpty()) return;
    m_input_path = path;
    ui->lbl_bdn_file_2->setText(QFileInfo(path).fileName());
    ui->lbl_bdn_file_2->setToolTip(path);
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
}

void MainWindow::retranslateUi()
{
    // ---- Header ----
    ui->lbl_version_2->setText(tr_str("subtitle", m_lang));

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
    switch (m_encode_state) {
        case EncodeState::Idle:
            ui->lbl_progress_text_2->setText(tr_str("standingBy", m_lang));
            break;
        case EncodeState::Running:
            ui->lbl_progress_text_2->setText(tr_str("starting", m_lang));
            break;
        case EncodeState::Done:
            ui->lbl_progress_text_2->setText(tr_str("done", m_lang));
            ui->lbl_eta_2->setText(tr_str("done", m_lang));
            break;
        case EncodeState::Failed:
            ui->lbl_progress_text_2->setText(tr_str("failedShort", m_lang));
            ui->lbl_eta_2->setText(tr_str("failed", m_lang));
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
    ui->progress_bar_2->setValue(0);
    ui->lbl_eta_2->setText(tr_str("starting", m_lang));

    auto cfg = opensup::core::encode_config_t{};
    cfg.input_path = m_input_path.toStdString();
    cfg.output_path = m_output_path.toStdString();
    cfg.quantizer_id = ui->combo_quantizer_2->currentIndex();
    cfg.overwrite = true;
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
    connect(m_worker_thread, &QThread::finished, m_worker_thread, &QObject::deleteLater);

    m_worker_thread->start();
}

void MainWindow::abort_encode()
{
    if (m_worker_thread && m_worker_thread->isRunning()) {
        m_worker_thread->requestInterruption();
        ui->lbl_eta_2->setText(tr_str("aborted", m_lang));
        ui->btn_abort_2->setEnabled(false);
        ui->btn_encode_2->setEnabled(true);
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
    ui->btn_abort_2->setEnabled(false);
    ui->btn_encode_2->setEnabled(true);

    if (success) {
        m_encode_state = EncodeState::Done;
        ui->progress_bar_2->setValue(100);
        ui->lbl_eta_2->setText(tr_str("done", m_lang));
    } else {
        m_encode_state = EncodeState::Failed;
        ui->progress_bar_2->setValue(0);
        ui->lbl_eta_2->setText(tr_str("failed", m_lang));
    }
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

    // Update log scrollbar colors for the current theme
    QString sbBg, sbHandle;
    if (m_theme->isDark()) {
        sbBg     = "#303544";
        sbHandle = "#4B5563";
    } else {
        sbBg     = "#E5E7EB";
        sbHandle = "#9CA3AF";
    }
    // Preserve existing font/border styles + add scrollbar QSS
    ui->txt_log_2->setStyleSheet(
        QString("border: 1px solid; border-radius: 8px;"
                "font-family: \"JetBrains Mono\", \"Consolas\", monospace;"
                "font-size: 13px; line-height: 1.6; padding: 16px;"
                "QScrollBar:vertical { background: %1; width: 10px; border-radius: 5px; }"
                "QScrollBar::handle:vertical { background: %2; border-radius: 5px; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
        .arg(sbBg, sbHandle));
}
