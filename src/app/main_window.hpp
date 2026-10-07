#pragma once

#include <QMainWindow>
#include <QString>

#include <exception>

#include "app/workbench_session.hpp"

class QAction;
class QLabel;
class QPlainTextEdit;
class QTableWidget;

namespace xm::app {

class PreviewView;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(WorkbenchSession session, QWidget* parent = nullptr);

private:
    void createActions();
    void createLayout();
    void refresh();
    void openDxf();
    void exportDxf();
    void reportError(const QString& action, const std::exception& error);

    WorkbenchSession session_;
    PreviewView* preview_ = nullptr;
    QLabel* entityCount_ = nullptr;
    QLabel* totalLength_ = nullptr;
    QLabel* bounds_ = nullptr;
    QLabel* layerCount_ = nullptr;
    QLabel* closedCount_ = nullptr;
    QLabel* warningCount_ = nullptr;
    QTableWidget* entityTable_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
    QAction* openAction_ = nullptr;
    QAction* exportAction_ = nullptr;
    QAction* fitAction_ = nullptr;
    QAction* resetAction_ = nullptr;
};

}  // namespace xm::app
