#pragma once

#include "EmulatorRegistry.h"
#include "Library.h"

#include <QMainWindow>

class QComboBox;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Adds files/folders (also used for drag & drop and command-line arguments).
    void addPaths(const QStringList &paths);

protected:
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    void onAddFiles();
    void onAddFolder();
    void onContextMenu(const QPoint &pos);
    void showEmulatorStatus();

    void scanPath(const QString &path, int depth, bool explicitAdd, int &added);
    bool addGame(const QString &path, const Detection &d);
    void refresh();
    int selectedIndex() const;
    void launchIndex(int index);
    const Emulator *chooseEmulator(const GameEntry &g);

    static QString systemLabel(const QString &id);

    EmulatorRegistry m_registry;
    Library m_library;

    QTreeWidget *m_tree = nullptr;
    QLineEdit *m_search = nullptr;
    QComboBox *m_systemFilter = nullptr;
};
