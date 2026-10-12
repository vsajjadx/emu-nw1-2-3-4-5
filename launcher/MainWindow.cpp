#include "MainWindow.h"

#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_emulators(EmulatorRegistry::load())
{
    setWindowTitle(tr("Emu Launcher"));
    resize(1100, 700);

    m_emulatorList = new QListWidget;
    m_emulatorList->setMinimumWidth(240);
    m_gameList = new QListWidget;
    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);

    m_buildBtn = new QPushButton(tr("Build"));
    m_exeBtn = new QPushButton(tr("Set executable..."));
    m_romBtn = new QPushButton(tr("Set game folder..."));
    m_launchEmuBtn = new QPushButton(tr("Launch emulator"));
    m_launchGameBtn = new QPushButton(tr("Launch selected game"));

    auto *buttons = new QHBoxLayout;
    for (auto *b : {m_buildBtn, m_exeBtn, m_romBtn, m_launchEmuBtn, m_launchGameBtn})
        buttons->addWidget(b);

    auto *right = new QWidget;
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->addWidget(m_status);
    rightLayout->addLayout(buttons);
    rightLayout->addWidget(new QLabel(tr("Games")));
    rightLayout->addWidget(m_gameList, 3);
    rightLayout->addWidget(new QLabel(tr("Log")));
    rightLayout->addWidget(m_log, 2);

    auto *split = new QSplitter;
    split->addWidget(m_emulatorList);
    split->addWidget(right);
    split->setStretchFactor(1, 1);
    setCentralWidget(split);

    connect(m_emulatorList, &QListWidget::currentRowChanged, this, &MainWindow::onEmulatorChanged);
    connect(m_buildBtn, &QPushButton::clicked, this, &MainWindow::onBuild);
    connect(m_exeBtn, &QPushButton::clicked, this, &MainWindow::onSetExecutable);
    connect(m_romBtn, &QPushButton::clicked, this, &MainWindow::onSetRomFolder);
    connect(m_launchEmuBtn, &QPushButton::clicked, this, &MainWindow::onLaunchEmulator);
    connect(m_launchGameBtn, &QPushButton::clicked, this, &MainWindow::onLaunchGame);
    connect(m_gameList, &QListWidget::itemDoubleClicked, this, [this] { onLaunchGame(); });

    refreshEmulatorList();
    if (m_emulatorList->count() > 0)
        m_emulatorList->setCurrentRow(0);
}

const EmulatorInfo *MainWindow::currentEmulator() const
{
    const int row = m_emulatorList->currentRow();
    return (row >= 0 && row < m_emulators.size()) ? &m_emulators[row] : nullptr;
}

void MainWindow::refreshEmulatorList()
{
    const int keep = m_emulatorList->currentRow();
    m_emulatorList->blockSignals(true);
    m_emulatorList->clear();
    for (const auto &e : m_emulators) {
        const bool ready = !EmulatorRegistry::findExecutable(e).isEmpty();
        m_emulatorList->addItem(QStringLiteral("%1 %2\n    %3")
                                    .arg(ready ? QStringLiteral("●") : QStringLiteral("○"),
                                         e.name, e.system));
    }
    m_emulatorList->blockSignals(false);
    m_emulatorList->setCurrentRow(keep >= 0 ? keep : 0);
}

void MainWindow::onEmulatorChanged()
{
    refreshButtons();
    refreshGames();
}

void MainWindow::refreshButtons()
{
    const auto *e = currentEmulator();
    const bool building = m_buildProcess != nullptr;
    if (!e) {
        m_status->clear();
        return;
    }
    const QString exe = EmulatorRegistry::findExecutable(*e);
    const QString roms = EmulatorRegistry::romFolder(e->id);
    m_status->setText(tr("<b>%1</b> (%2)<br>Source: %3<br>Executable: %4<br>Game folder: %5")
                          .arg(e->name, e->system,
                               EmulatorRegistry::isSourceCheckedOut(*e) ? tr("checked out") : tr("not downloaded yet"),
                               exe.isEmpty() ? tr("not built") : exe,
                               roms.isEmpty() ? tr("not set") : roms));
    m_buildBtn->setEnabled(!building);
    m_launchEmuBtn->setEnabled(!exe.isEmpty());
    m_launchGameBtn->setEnabled(!exe.isEmpty());
}

void MainWindow::refreshGames()
{
    m_gameList->clear();
    const auto *e = currentEmulator();
    if (!e)
        return;
    const QString folder = EmulatorRegistry::romFolder(e->id);
    if (folder.isEmpty() || !QDir(folder).exists())
        return;

    QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);
    QStringList files;
    while (it.hasNext()) {
        const QString path = it.next();
        if (e->extensions.contains(QFileInfo(path).suffix().toLower()))
            files << path;
    }
    files.sort(Qt::CaseInsensitive);
    for (const QString &path : files) {
        auto *item = new QListWidgetItem(QFileInfo(path).completeBaseName());
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        m_gameList->addItem(item);
    }
}

void MainWindow::onBuild()
{
    const auto *e = currentEmulator();
    if (!e || m_buildProcess)
        return;

    m_buildingId = e->id;
    m_pendingSteps.clear();
    // Fetch the submodule on demand: the launcher repo itself stays small.
    m_pendingSteps.append({"git", "-C", EmulatorRegistry::projectRoot(), "submodule", "update",
                           "--init", "--recursive", "--depth", "1", e->dir});
    for (QStringList step : e->buildSteps) {
        for (QString &arg : step) {
            arg.replace("{src}", EmulatorRegistry::sourceDir(*e));
            arg.replace("{build}", EmulatorRegistry::buildDir(*e));
        }
        m_pendingSteps.append(step);
    }
    QDir().mkpath(EmulatorRegistry::buildDir(*e));

    m_buildProcess = new QProcess(this);
    m_buildProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_buildProcess, &QProcess::readyRead, this, &MainWindow::onProcessOutput);
    connect(m_buildProcess, &QProcess::finished, this, &MainWindow::onBuildStepFinished);
    appendLog(tr("=== Building %1 ===").arg(e->name));
    refreshButtons();
    runNextBuildStep();
}

void MainWindow::runNextBuildStep()
{
    if (m_pendingSteps.isEmpty()) {
        appendLog(tr("=== Build finished ==="));
        m_buildProcess->deleteLater();
        m_buildProcess = nullptr;
        refreshEmulatorList();
        return;
    }
    const QStringList step = m_pendingSteps.takeFirst();
    appendLog("$ " + step.join(' '));
    m_buildProcess->start(step.first(), step.mid(1));
}

void MainWindow::onBuildStepFinished(int exitCode, QProcess::ExitStatus status)
{
    if (status != QProcess::NormalExit || exitCode != 0) {
        appendLog(tr("=== Build failed (exit code %1) ===").arg(exitCode));
        m_pendingSteps.clear();
        m_buildProcess->deleteLater();
        m_buildProcess = nullptr;
        refreshButtons();
        return;
    }
    runNextBuildStep();
}

void MainWindow::onProcessOutput()
{
    if (m_buildProcess)
        appendLog(QString::fromLocal8Bit(m_buildProcess->readAll()).trimmed());
}

void MainWindow::onSetExecutable()
{
    const auto *e = currentEmulator();
    if (!e)
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Select %1 executable").arg(e->name));
    if (path.isEmpty())
        return;
    EmulatorRegistry::setUserExecutable(e->id, path);
    refreshEmulatorList();
}

void MainWindow::onSetRomFolder()
{
    const auto *e = currentEmulator();
    if (!e)
        return;
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Game folder for %1").arg(e->name));
    if (dir.isEmpty())
        return;
    EmulatorRegistry::setRomFolder(e->id, dir);
    refreshButtons();
    refreshGames();
}

void MainWindow::onLaunchEmulator()
{
    if (const auto *e = currentEmulator())
        launch(EmulatorRegistry::findExecutable(*e), {});
}

void MainWindow::onLaunchGame()
{
    const auto *e = currentEmulator();
    auto *item = m_gameList->currentItem();
    if (!e || !item)
        return;
    launch(EmulatorRegistry::findExecutable(*e), {item->data(Qt::UserRole).toString()});
}

void MainWindow::launch(const QString &exe, const QStringList &args)
{
    if (exe.isEmpty())
        return;
    appendLog("$ " + exe + ' ' + args.join(' '));
    if (!QProcess::startDetached(exe, args, QFileInfo(exe).absolutePath()))
        QMessageBox::warning(this, tr("Launch failed"), tr("Could not start %1").arg(exe));
}

void MainWindow::appendLog(const QString &text)
{
    if (!text.isEmpty())
        m_log->appendPlainText(text);
}
