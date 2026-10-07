#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QTableWidget>
#include <QTimer>

#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <utility>

#include "app/main_window.hpp"
#include "app/workbench_session.hpp"

namespace {

bool hasArgument(int argc, char** argv, std::string_view expected)
{
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == expected) {
            return true;
        }
    }
    return false;
}

std::filesystem::path applicationDirectory()
{
#ifdef _WIN32
    return std::filesystem::path(
        QCoreApplication::applicationDirPath().toStdWString());
#else
    const QByteArray text = QCoreApplication::applicationDirPath().toUtf8();
    return std::filesystem::u8path(text.constData(), text.constData() + text.size());
#endif
}

std::filesystem::path sampleOutputPath()
{
    return applicationDirectory().parent_path() / "out" / "mvp_sample.dxf";
}

QString pathToQString(const std::filesystem::path& path)
{
#ifdef _WIN32
    return QString::fromStdWString(path.native());
#else
    const std::u8string text = path.u8string();
    return QString::fromUtf8(reinterpret_cast<const char*>(text.data()),
                             static_cast<qsizetype>(text.size()));
#endif
}

}  // namespace

int main(int argc, char** argv)
{
    if (hasArgument(argc, argv, "--selftest")) {
        QCoreApplication application(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("CDR2DXF Workbench Selftest"));
        return xm::app::runSelfTest(sampleOutputPath(), std::cout, std::cerr);
    }

    const bool uiSmokeTest = hasArgument(argc, argv, "--ui-smoketest");
    if (uiSmokeTest && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    }

    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("CDR2DXF Geometry Workbench"));
    QApplication::setOrganizationName(QStringLiteral("Xuanmei"));
    try {
        xm::app::MainWindow window(
            xm::app::WorkbenchSession::createSampleRoundTrip(sampleOutputPath()));
        window.show();
        if (uiSmokeTest) {
            int smokeExitCode = 1;
            QTimer::singleShot(100, &application, [&] {
                const auto* table = window.findChild<QTableWidget*>(
                    QStringLiteral("entityTable"));
                const auto* warningLabel = window.findChild<QLabel*>(
                    QStringLiteral("warningCount"));
                const QPixmap image = window.grab();
                const std::filesystem::path screenshotPath =
                    applicationDirectory().parent_path() / "out" / "ui_smoke.png";
                const bool structureIsValid = window.isVisible()
                    && table != nullptr && table->rowCount() == 7
                    && warningLabel != nullptr && warningLabel->text() == QStringLiteral("0")
                    && !image.isNull();
                const bool imageSaved = structureIsValid
                    && image.save(pathToQString(screenshotPath), "PNG");
                if (imageSaved) {
                    std::cout << "ui-smoketest rows=7 warnings=0 size="
                              << image.width() << 'x' << image.height()
                              << " screenshot=" << screenshotPath.generic_string()
                              << '\n';
                    smokeExitCode = 0;
                } else {
                    std::cerr << "ui-smoketest failed: invalid widget tree or screenshot\n";
                }
                application.quit();
            });
            const int eventLoopExitCode = application.exec();
            return eventLoopExitCode == 0 ? smokeExitCode : eventLoopExitCode;
        }
        return application.exec();
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, QStringLiteral("启动失败"),
                              QString::fromUtf8(error.what()));
        return 1;
    }
}
