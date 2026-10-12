#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct EmulatorInfo {
    QString id;
    QString name;
    QString system;
    QString dir;                     // submodule path, relative to project root
    QStringList extensions;          // ROM/ISO extensions this emulator accepts
    QStringList executables;         // candidate executable paths inside the build dir
    QVector<QStringList> buildSteps; // commands; {src} and {build} are substituted
};

class EmulatorRegistry {
public:
    // Loads the bundled emulators.json resource.
    static QVector<EmulatorInfo> load();

    static QString projectRoot();
    static QString sourceDir(const EmulatorInfo &e);
    static QString buildDir(const EmulatorInfo &e);

    // User override (QSettings) first, then well-known locations in the build dir.
    static QString findExecutable(const EmulatorInfo &e);
    static bool isSourceCheckedOut(const EmulatorInfo &e);

    static QString userExecutable(const QString &id);
    static void setUserExecutable(const QString &id, const QString &path);
    static QString romFolder(const QString &id);
    static void setRomFolder(const QString &id, const QString &path);
};
