#pragma once

#include "EmulatorRegistry.h"

#include <QMainWindow>
#include <QProcess>
#include <QVector>

class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onEmulatorChanged();
    void onBuild();
    void onSetExecutable();
    void onSetRomFolder();
    void onLaunchEmulator();
    void onLaunchGame();
    void onProcessOutput();
    void onBuildStepFinished(int exitCode, QProcess::ExitStatus status);

private:
    const EmulatorInfo *currentEmulator() const;
    void refreshEmulatorList();
    void refreshGames();
    void refreshButtons();
    void runNextBuildStep();
    void appendLog(const QString &text);
    void launch(const QString &exe, const QStringList &args);

    QVector<EmulatorInfo> m_emulators;
    QListWidget *m_emulatorList = nullptr;
    QListWidget *m_gameList = nullptr;
    QLabel *m_status = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QPushButton *m_buildBtn = nullptr;
    QPushButton *m_exeBtn = nullptr;
    QPushButton *m_romBtn = nullptr;
    QPushButton *m_launchEmuBtn = nullptr;
    QPushButton *m_launchGameBtn = nullptr;

    QProcess *m_buildProcess = nullptr;
    QVector<QStringList> m_pendingSteps;
    QString m_buildingId;
};
