#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>

namespace {
enum Column { ColTitle, ColSystem, ColEmulator, ColPath };
constexpr int IndexRole = Qt::UserRole;

const System kAllSystems[] = {System::PS1, System::PS2, System::PS3, System::PS4, System::PS5,
                              System::GameCube, System::Wii, System::Switch, System::Xbox360};
} // namespace

QString MainWindow::systemLabel(const QString &id)
{
    for (System s : kAllSystems)
        if (id == QLatin1String(systemId(s)))
            return QString::fromLatin1(systemName(s));
    return tr("Unknown");
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Emu Launcher"));
    resize(1000, 640);
    setAcceptDrops(true);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search games..."));
    m_search->setClearButtonEnabled(true);
    m_systemFilter = new QComboBox;
    m_systemFilter->addItem(tr("All systems"), QString());
    for (System s : kAllSystems)
        m_systemFilter->addItem(QString::fromLatin1(systemName(s)), QString::fromLatin1(systemId(s)));

    auto *addFiles = new QAction(tr("Add games..."), this);
    auto *addFolder = new QAction(tr("Add folder..."), this);
    auto *status = new QAction(tr("Emulator status"), this);
    auto *quit = new QAction(tr("Exit"), this);

    auto *file = menuBar()->addMenu(tr("&File"));
    file->addAction(addFiles);
    file->addAction(addFolder);
    file->addSeparator();
    file->addAction(quit);
    menuBar()->addMenu(tr("&Help"))->addAction(status);

    auto *bar = addToolBar(tr("Library"));
    bar->setMovable(false);
    bar->addAction(addFiles);
    bar->addAction(addFolder);
    bar->addSeparator();
    bar->addWidget(m_systemFilter);
    bar->addWidget(m_search);

    m_tree = new QTreeWidget;
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({tr("Title"), tr("System"), tr("Emulator"), tr("Path")});
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSortingEnabled(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->header()->setSectionResizeMode(ColTitle, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(ColPath, QHeaderView::Interactive);
    m_tree->setColumnWidth(ColSystem, 130);
    m_tree->setColumnWidth(ColEmulator, 120);
    m_tree->setColumnWidth(ColPath, 260);
    setCentralWidget(m_tree);

    connect(addFiles, &QAction::triggered, this, &MainWindow::onAddFiles);
    connect(addFolder, &QAction::triggered, this, &MainWindow::onAddFolder);
    connect(status, &QAction::triggered, this, &MainWindow::showEmulatorStatus);
    connect(quit, &QAction::triggered, this, &QWidget::close);
    connect(m_search, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(m_systemFilter, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(m_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        launchIndex(item->data(ColTitle, IndexRole).toInt());
    });
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &MainWindow::onContextMenu);

    refresh();
}

// ---------------------------------------------------------------- adding games

void MainWindow::onAddFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Add games"), QString(),
        tr("Games (*.iso *.bin *.cue *.chd *.cso *.zso *.gz *.img *.rvz *.wbfs *.gcz *.gcm *.wad *.dol "
           "*.nsp *.xci *.nro *.xex *.pbp *.m3u *.self *.exe);;All files (*)"));
    if (!files.isEmpty())
        addPaths(files);
}

void MainWindow::onAddFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Add a game folder or a folder full of games"));
    if (!dir.isEmpty())
        addPaths({dir});
}

void MainWindow::addPaths(const QStringList &paths)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    int added = 0;
    for (const QString &p : paths)
        scanPath(QDir::cleanPath(p), 0, true, added);
    QApplication::restoreOverrideCursor();

    m_library.save();
    refresh();
    statusBar()->showMessage(added > 0 ? tr("Added %n game(s).", nullptr, added) : tr("No new games found."), 5000);
}

// A folder that is itself a game (PS3/PS4/PS5 dump) is added as one entry;
// any other folder is searched recursively.
void MainWindow::scanPath(const QString &path, int depth, bool explicitAdd, int &added)
{
    const QFileInfo fi(path);
    if (fi.isDir()) {
        const Detection d = detectGame(path.toStdString());
        if (!d.candidates.empty()) {
            if (addGame(path, d))
                ++added;
            return;
        }
        if (depth > 8)
            return;
        const auto children = QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &c : children)
            scanPath(c.absoluteFilePath(), depth + 1, false, added);
        return;
    }
    if (!fi.isFile())
        return;
    // While scanning, ignore files that can't be games; a file picked by hand is always added.
    if (!explicitAdd && !isKnownGameExtension(path.toStdString()))
        return;
    const Detection d = detectGame(path.toStdString());
    // While scanning, skip things we couldn't identify (random .bin / .exe files etc.).
    if (!explicitAdd && d.candidates.empty())
        return;
    if (addGame(path, d))
        ++added;
}

bool MainWindow::addGame(const QString &path, const Detection &d)
{
    if (m_library.contains(path))
        return false;

    GameEntry g;
    g.path = path;
    g.launchPath = d.launchPath.empty() ? path : QString::fromStdString(d.launchPath);
    g.title = QString::fromStdString(d.title);
    if (g.title.isEmpty())
        g.title = QFileInfo(path).completeBaseName();
    if (!d.candidates.empty())
        g.system = QString::fromLatin1(systemId(d.candidates.front()));

    // Assign automatically only when there is exactly one possible emulator.
    const auto emus = m_registry.forDetection(d);
    if (emus.size() == 1)
        g.emulatorId = emus.front()->id;

    m_library.add(g);
    return true;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e)
{
    QStringList paths;
    for (const QUrl &u : e->mimeData()->urls())
        if (u.isLocalFile())
            paths << u.toLocalFile();
    if (!paths.isEmpty())
        addPaths(paths);
}

// ---------------------------------------------------------------- library view

void MainWindow::refresh()
{
    const QString needle = m_search->text().trimmed();
    const QString sys = m_systemFilter->currentData().toString();

    m_tree->setSortingEnabled(false);
    m_tree->clear();
    const auto &games = m_library.games();
    for (int i = 0; i < games.size(); ++i) {
        const GameEntry &g = games[i];
        if (!sys.isEmpty() && g.system != sys)
            continue;
        if (!needle.isEmpty() && !g.title.contains(needle, Qt::CaseInsensitive)
            && !g.path.contains(needle, Qt::CaseInsensitive))
            continue;

        const Emulator *emu = m_registry.byId(g.emulatorId);
        QString emuText = tr("(pick on launch)");
        if (emu)
            emuText = emu->available() ? emu->name : tr("%1 (not included)").arg(emu->name);

        auto *item = new QTreeWidgetItem({g.title, g.system.isEmpty() ? tr("Unknown") : systemLabel(g.system),
                                          emuText, QDir::toNativeSeparators(g.path)});
        item->setData(ColTitle, IndexRole, i);
        item->setToolTip(ColPath, g.path);
        m_tree->addTopLevelItem(item);
    }
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(ColTitle, Qt::AscendingOrder);

    int available = 0;
    for (const auto &e : m_registry.all())
        available += e.available() ? 1 : 0;
    statusBar()->showMessage(tr("%1 games in library • %2 of %3 emulators included. Drop games here to add them.")
                                 .arg(games.size()).arg(available).arg(m_registry.all().size()));
}

int MainWindow::selectedIndex() const
{
    const auto *item = m_tree->currentItem();
    return item ? item->data(ColTitle, IndexRole).toInt() : -1;
}

void MainWindow::onContextMenu(const QPoint &pos)
{
    const auto *item = m_tree->itemAt(pos);
    if (!item)
        return;
    const int index = item->data(ColTitle, IndexRole).toInt();
    if (index < 0 || index >= m_library.games().size())
        return;

    QMenu menu(this);
    menu.addAction(tr("Launch"), this, [this, index] { launchIndex(index); });

    auto *with = menu.addMenu(tr("Open with"));
    for (const Emulator &e : m_registry.all()) {
        const QString id = e.id;
        auto *a = with->addAction(e.available() ? e.name : tr("%1 (not included)").arg(e.name));
        a->setEnabled(e.available());
        connect(a, &QAction::triggered, this, [this, index, id] {
            m_library.games()[index].emulatorId = id;
            m_library.save();
            launchIndex(index);
            refresh();
        });
    }

    menu.addAction(tr("Show in folder"), this, [this, index] {
        const QFileInfo fi(m_library.games()[index].path);
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath()));
    });
    menu.addSeparator();
    menu.addAction(tr("Remove from library"), this, [this, index] {
        m_library.removeAt(index);
        m_library.save();
        refresh();
    });
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

// ---------------------------------------------------------------- launching

const Emulator *MainWindow::chooseEmulator(const GameEntry &g)
{
    // Prefer emulators that fit what the file looks like; otherwise offer all of them.
    QVector<const Emulator *> options = m_registry.forDetection(detectGame(g.path.toStdString()));
    if (options.isEmpty())
        for (const Emulator &e : m_registry.all())
            options.append(&e);

    QStringList names;
    for (const Emulator *e : options)
        names << (e->available() ? e->name : tr("%1 (not included)").arg(e->name));

    bool ok = false;
    const QString picked = QInputDialog::getItem(this, tr("Choose emulator"),
                                                 tr("Which emulator should run \"%1\"?").arg(g.title), names, 0, false, &ok);
    return ok ? options.value(names.indexOf(picked)) : nullptr;
}

void MainWindow::launchIndex(int index)
{
    if (index < 0 || index >= m_library.games().size())
        return;
    GameEntry &g = m_library.games()[index];

    if (!QFileInfo::exists(g.path)) {
        QMessageBox::warning(this, tr("Game not found"), tr("This file no longer exists:\n%1").arg(g.path));
        return;
    }

    const Emulator *emu = m_registry.byId(g.emulatorId);
    if (!emu) {
        emu = chooseEmulator(g);
        if (!emu)
            return;
        g.emulatorId = emu->id; // remembered for next time
        m_library.save();
        refresh();
    }

    const QString err = EmulatorRegistry::launch(*emu, g.launchPath.isEmpty() ? g.path : g.launchPath);
    if (!err.isEmpty())
        QMessageBox::warning(this, tr("Cannot launch"), err);
    else
        statusBar()->showMessage(tr("Started %1 with %2").arg(g.title, emu->name), 5000);
}

void MainWindow::showEmulatorStatus()
{
    QString text;
    for (const Emulator &e : m_registry.all()) {
        QStringList systems;
        for (const QString &s : e.systems)
            systems << systemLabel(s);
        text += QStringLiteral("%1  %2  (%3)\n")
                    .arg(e.available() ? QStringLiteral("✔") : QStringLiteral("✘"), e.name, systems.join(", "));
    }
    text += tr("\nSearched in:\n%1").arg(QDir::toNativeSeparators(EmulatorRegistry::emulatorsRoot()));
    QMessageBox::information(this, tr("Emulator status"), text);
}
