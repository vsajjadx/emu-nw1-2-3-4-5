#include "EmulatorRegistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

static QStringList toList(const QJsonArray &a)
{
    QStringList out;
    for (const auto &v : a)
        out << v.toString();
    return out;
}

// Looks for one of `names` anywhere below `dir` (builds put the exe in different subfolders).
static QString findExe(const QString &dir, const QStringList &names)
{
    if (!QDir(dir).exists())
        return {};
    for (const QString &name : names) {
        QDirIterator it(dir, {name}, QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext())
            return QFileInfo(it.next()).absoluteFilePath();
    }
    return {};
}

QString EmulatorRegistry::emulatorsRoot()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath("emulators");
}

EmulatorRegistry::EmulatorRegistry()
{
    QFile f(QStringLiteral(":/emulators.json"));
    if (!f.open(QIODevice::ReadOnly))
        return;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    for (const auto &item : doc.object().value("emulators").toArray()) {
        const auto o = item.toObject();
        Emulator e;
        e.id = o.value("id").toString();
        e.name = o.value("name").toString();
        e.dir = o.value("dir").toString();
        e.systems = toList(o.value("systems").toArray());
        e.exeNames = toList(o.value("exe").toArray());
        e.args = toList(o.value("args").toArray());
        e.exePath = findExe(QDir(emulatorsRoot()).filePath(e.dir), e.exeNames);
        m_emulators.append(e);
    }
}

const Emulator *EmulatorRegistry::byId(const QString &id) const
{
    for (const auto &e : m_emulators)
        if (e.id == id)
            return &e;
    return nullptr;
}

QVector<const Emulator *> EmulatorRegistry::forDetection(const Detection &d) const
{
    QVector<const Emulator *> out;
    for (System s : d.candidates) {
        for (const auto &e : m_emulators) {
            if (e.systems.contains(QString::fromLatin1(systemId(s))) && !out.contains(&e))
                out.append(&e);
        }
    }
    return out;
}

QString EmulatorRegistry::launch(const Emulator &emu, const QString &gamePath)
{
    if (!emu.available())
        return QObject::tr("%1 is not included in this build of the launcher.").arg(emu.name);

    QStringList args;
    for (QString a : emu.args) {
        a.replace("{game}", QDir::toNativeSeparators(gamePath));
        args << a;
    }
    // Run from the emulator's own folder so it finds its data files.
    if (!QProcess::startDetached(emu.exePath, args, QFileInfo(emu.exePath).absolutePath()))
        return QObject::tr("Could not start %1.").arg(emu.name);
    return {};
}
