#pragma once

#include "Detector.h"

#include <QString>
#include <QStringList>
#include <QVector>

struct Emulator {
    QString id;
    QString name;
    QString dir;            // folder under <app>/emulators/
    QStringList systems;    // system ids this emulator runs (see systemId())
    QStringList exeNames;   // executable file names to look for, in order of preference
    QStringList args;       // command line; "{game}" is replaced by the game path
    QString exePath;        // resolved at load time, empty if the emulator isn't bundled

    bool available() const { return !exePath.isEmpty(); }
};

class EmulatorRegistry {
public:
    EmulatorRegistry();

    const QVector<Emulator> &all() const { return m_emulators; }
    const Emulator *byId(const QString &id) const;

    // Emulators able to run any of the detected systems, best match first, no duplicates.
    QVector<const Emulator *> forDetection(const Detection &d) const;

    // Folder the emulators are bundled in (<app>/emulators).
    static QString emulatorsRoot();

    // Starts `emu` with the given game. Returns an error message, or an empty string on success.
    static QString launch(const Emulator &emu, const QString &gamePath);

private:
    QVector<Emulator> m_emulators;
};
