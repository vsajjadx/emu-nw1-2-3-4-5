#include "Library.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

QString Library::filePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath("library.json");
}

Library::Library()
{
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    for (const auto &v : doc.object().value("games").toArray()) {
        const auto o = v.toObject();
        GameEntry g;
        g.path = o.value("path").toString();
        g.launchPath = o.value("launchPath").toString();
        g.title = o.value("title").toString();
        g.system = o.value("system").toString();
        g.emulatorId = o.value("emulator").toString();
        if (!g.path.isEmpty())
            m_games.append(g);
    }
}

bool Library::contains(const QString &path) const
{
    for (const auto &g : m_games)
        if (g.path == path)
            return true;
    return false;
}

void Library::add(const GameEntry &g)
{
    m_games.append(g);
}

void Library::removeAt(int index)
{
    if (index >= 0 && index < m_games.size())
        m_games.removeAt(index);
}

void Library::save() const
{
    QJsonArray arr;
    for (const auto &g : m_games) {
        QJsonObject o;
        o["path"] = g.path;
        o["launchPath"] = g.launchPath;
        o["title"] = g.title;
        o["system"] = g.system;
        o["emulator"] = g.emulatorId;
        arr.append(o);
    }
    QSaveFile f(filePath());
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(QJsonObject{{"games", arr}}).toJson());
    f.commit();
}
