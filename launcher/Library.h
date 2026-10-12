#pragma once

#include <QString>
#include <QVector>

struct GameEntry {
    QString path;        // what the user added (file or folder)
    QString launchPath;  // what is passed to the emulator
    QString title;
    QString system;      // system id, empty if unknown
    QString emulatorId;  // empty = not assigned yet
};

// The saved list of games (JSON in the user's app-data folder).
class Library {
public:
    Library();

    const QVector<GameEntry> &games() const { return m_games; }
    QVector<GameEntry> &games() { return m_games; }

    bool contains(const QString &path) const;
    void add(const GameEntry &g);
    void removeAt(int index);
    void save() const;

private:
    static QString filePath();
    QVector<GameEntry> m_games;
};
