#include "app/main_window.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QByteArray>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>

#include "app/preview_view.hpp"
#include "core/native_analyzer.hpp"

namespace xm::app {
namespace {

QString pathToQString(const std::filesystem::path& path)
{
#ifdef _WIN32
    return QDir::toNativeSeparators(QString::fromStdWString(path.native()));
#else
    const std::u8string text = path.u8string();
    return QString::fromUtf8(reinterpret_cast<const char*>(text.data()),
                             static_cast<qsizetype>(text.size()));
#endif
}

std::filesystem::path qStringToPath(const QString& value)
{
#ifdef _WIN32
    return std::filesystem::path(value.toStdWString());
#else
    const QByteArray text = value.toUtf8();
    return std::filesystem::u8path(text.constData(), text.constData() + text.size());
#endif
}

QString fixed(double value, int decimals = 6)
{
    return QString::number(value, 'f', decimals);
}

}  // namespace

MainWindow::MainWindow(WorkbenchSession session, QWidget* parent)
    : QMainWindow(parent), session_(std::move(session))
{
    createActions();
    createLayout();
    refresh();
    resize(1200, 800);
}

void MainWindow::createActions()
{
    openAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton),
                              QStringLiteral("打开 DXF"), this);
    exportAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton),
                                QStringLiteral("导出 DXF"), this);
    fitAction_ = new QAction(QStringLiteral("适应窗口"), this);
    resetAction_ = new QAction(QStringLiteral("重置视图"), this);
    QAction* exitAction = new QAction(QStringLiteral("退出"), this);

    openAction_->setShortcut(QKeySequence::Open);
    exportAction_->setShortcut(QKeySequence::SaveAs);
    fitAction_->setShortcut(QKeySequence(Qt::Key_F));
    resetAction_->setShortcut(QKeySequence(Qt::Key_R));

    connect(openAction_, &QAction::triggered, this, [this] { openDxf(); });
    connect(exportAction_, &QAction::triggered, this, [this] { exportDxf(); });
    connect(fitAction_, &QAction::triggered, this, [this] { preview_->fitDocument(); });
    connect(resetAction_, &QAction::triggered, this, [this] { preview_->resetView(); });
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    fileMenu->addAction(openAction_);
    fileMenu->addAction(exportAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);
    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
    viewMenu->addAction(fitAction_);
    viewMenu->addAction(resetAction_);

    QToolBar* toolbar = addToolBar(QStringLiteral("主工具栏"));
    toolbar->setObjectName(QStringLiteral("mainToolbar"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(openAction_);
    toolbar->addAction(exportAction_);
    toolbar->addSeparator();
    toolbar->addAction(fitAction_);
    toolbar->addAction(resetAction_);
}

void MainWindow::createLayout()
{
    preview_ = new PreviewView(this);
    preview_->setObjectName(QStringLiteral("previewView"));

    auto* metricsGroup = new QGroupBox(QStringLiteral("文档指标"), this);
    auto* metricsLayout = new QFormLayout(metricsGroup);
    entityCount_ = new QLabel(metricsGroup);
    totalLength_ = new QLabel(metricsGroup);
    bounds_ = new QLabel(metricsGroup);
    layerCount_ = new QLabel(metricsGroup);
    closedCount_ = new QLabel(metricsGroup);
    warningCount_ = new QLabel(metricsGroup);
    warningCount_->setObjectName(QStringLiteral("warningCount"));
    closedCount_->setToolTip(QStringLiteral(
        "仅表示源文件声明为闭合；有效拓扑闭合需要后续几何阶段验证。"));
    metricsLayout->addRow(QStringLiteral("实体总数"), entityCount_);
    metricsLayout->addRow(QStringLiteral("总长度"), totalLength_);
    metricsLayout->addRow(QStringLiteral("包围盒"), bounds_);
    metricsLayout->addRow(QStringLiteral("图层数"), layerCount_);
    metricsLayout->addRow(QStringLiteral("声明闭合折线"), closedCount_);
    metricsLayout->addRow(QStringLiteral("警告数"), warningCount_);

    entityTable_ = new QTableWidget(this);
    entityTable_->setObjectName(QStringLiteral("entityTable"));
    entityTable_->setColumnCount(5);
    entityTable_->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("类型"), QStringLiteral("图层"),
        QStringLiteral("长度 (mm)"), QStringLiteral("声明闭合")});
    entityTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    entityTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    entityTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    entityTable_->setAlternatingRowColors(false);

    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(metricsGroup);
    rightLayout->addWidget(entityTable_, 1);

    auto* upper = new QSplitter(Qt::Horizontal, this);
    upper->addWidget(preview_);
    upper->addWidget(rightPanel);
    upper->setStretchFactor(0, 4);
    upper->setStretchFactor(1, 2);
    upper->setSizes({800, 400});

    log_ = new QPlainTextEdit(this);
    log_->setObjectName(QStringLiteral("runLog"));
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(5000);
    log_->setPlaceholderText(QStringLiteral("运行日志"));

    auto* mainSplitter = new QSplitter(Qt::Vertical, this);
    mainSplitter->addWidget(upper);
    mainSplitter->addWidget(log_);
    mainSplitter->setStretchFactor(0, 5);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setSizes({650, 150});
    setCentralWidget(mainSplitter);
}

void MainWindow::refresh()
{
    const core::DocumentMetrics& metrics = session_.metrics();
    preview_->setDocument(session_.document());

    entityCount_->setText(QString::number(session_.document().entities().size()));
    totalLength_->setText(fixed(metrics.totalLength, 6) + QStringLiteral(" mm"));
    if (metrics.hasBounds) {
        bounds_->setText(fixed(core::Native2dAnalyzer::width(metrics), 6)
                         + QStringLiteral(" × ")
                         + fixed(core::Native2dAnalyzer::height(metrics), 6)
                         + QStringLiteral(" mm"));
    } else {
        bounds_->setText(QStringLiteral("无"));
    }
    layerCount_->setText(QString::number(session_.document().layers().size()));
    closedCount_->setText(QString::number(metrics.declaredClosedPolylineCount));
    warningCount_->setText(QString::number(session_.warningCount()));

    entityTable_->setRowCount(static_cast<int>(metrics.perEntity.size()));
    for (std::size_t row = 0; row < metrics.perEntity.size(); ++row) {
        const core::EntityMetric& metric = metrics.perEntity[row];
        const int tableRow = static_cast<int>(row);
        entityTable_->setItem(tableRow, 0,
            new QTableWidgetItem(QString::number(metric.id)));
        entityTable_->setItem(tableRow, 1,
            new QTableWidgetItem(QString::fromStdString(metric.kind)));
        entityTable_->setItem(tableRow, 2,
            new QTableWidgetItem(QString::fromStdString(metric.layer)));
        entityTable_->setItem(tableRow, 3,
            new QTableWidgetItem(fixed(metric.length, 6)));
        entityTable_->setItem(tableRow, 4,
            new QTableWidgetItem(metric.declaredClosed
                ? QStringLiteral("是") : QStringLiteral("否")));
    }

    log_->clear();
    log_->appendPlainText(QStringLiteral("已加载：")
                          + pathToQString(session_.sourcePath()));
    for (const std::string& warning : session_.importWarnings()) {
        log_->appendPlainText(QStringLiteral("[DXF] ")
                              + QString::fromUtf8(warning));
    }
    for (const core::AnalysisWarning& warning : metrics.warnings) {
        QString prefix = QStringLiteral("[分析]");
        if (warning.entityId.has_value()) {
            prefix += QStringLiteral("[实体 %1]").arg(*warning.entityId);
        }
        log_->appendPlainText(prefix + QLatin1Char(' ')
                              + QString::fromUtf8(warning.message));
    }
    if (session_.warningCount() == 0U) {
        log_->appendPlainText(QStringLiteral("未发现警告。"));
    }

    const QString fileName = pathToQString(session_.sourcePath().filename());
    setWindowTitle(QStringLiteral("CDR2DXF Geometry Workbench — %1").arg(fileName));
    statusBar()->showMessage(QStringLiteral("实体 %1，图层 %2，警告 %3")
        .arg(session_.document().entities().size())
        .arg(session_.document().layers().size())
        .arg(session_.warningCount()));
}

void MainWindow::openDxf()
{
    const QString selected = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开 ASCII DXF"),
        pathToQString(session_.sourcePath().parent_path()),
        QStringLiteral("DXF 文件 (*.dxf);;所有文件 (*)"));
    if (selected.isEmpty()) {
        return;
    }
    try {
        session_ = WorkbenchSession::loadDxf(qStringToPath(selected));
        refresh();
    } catch (const std::exception& error) {
        reportError(QStringLiteral("打开 DXF"), error);
    }
}

void MainWindow::exportDxf()
{
    std::filesystem::path suggested = session_.sourcePath();
    std::filesystem::path exportName = suggested.stem();
    exportName += std::filesystem::path("-export.dxf");
    suggested.replace_filename(exportName);
    const QString selected = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出 ASCII DXF R12"), pathToQString(suggested),
        QStringLiteral("DXF 文件 (*.dxf)"));
    if (selected.isEmpty()) {
        return;
    }
    try {
        std::filesystem::path destination = qStringToPath(selected);
        if (destination.extension().empty()) {
            destination += std::filesystem::path(".dxf");
        }
        session_.exportDxf(destination);
        log_->appendPlainText(QStringLiteral("已导出：")
                              + pathToQString(destination));
        statusBar()->showMessage(QStringLiteral("DXF 导出完成"), 5000);
    } catch (const std::exception& error) {
        reportError(QStringLiteral("导出 DXF"), error);
    }
}

void MainWindow::reportError(const QString& action, const std::exception& error)
{
    const QString message = QString::fromUtf8(error.what());
    log_->appendPlainText(QStringLiteral("[错误] ") + action
                          + QStringLiteral("：") + message);
    QMessageBox::critical(this, action + QStringLiteral("失败"), message);
}

}  // namespace xm::app
