#include "EmulatorRegistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

static QStringList toStringList(const QJsonArray &a)
{
    QStringList out;
    for (const auto &v : a)
        out << v.toString();
    return out;
}

QVector<EmulatorInfo> EmulatorRegistry::load()
{
    QVector<EmulatorInfo> result;
    QFile f(QStringLiteral(":/emulators.json"));
    if (!f.open(QIODevice::ReadOnly))
        return result;

    const auto doc = QJsonDocument::fromJson(f.readAll());
    for (const auto &item : doc.object().value("emulators").toArray()) {
        const auto o = item.toObject();
        EmulatorInfo e;
        e.id = o.value("id").toString();
        e.name = o.value("name").toString();
        e.system = o.value("system").toString();
        e.dir = o.value("dir").toString();
        e.extensions = toStringList(o.value("extensions").toArray());
        e.executables = toStringList(o.value("executables").toArray());
        for (const auto &step : o.value("build").toArray())
            e.buildSteps.append(toStringList(step.toArray()));
        result.append(e);
    }
    return result;
}

QString EmulatorRegistry::projectRoot()
{
    // Prefer the checkout the launcher was built from; fall back to the app dir.
    const QString compiled = QStringLiteral(EMU_LAUNCHER_ROOT);
    if (QFileInfo::exists(compiled + "/.gitmodules"))
        return compiled;
    return QCoreApplication::applicationDirPath();
}

QString EmulatorRegistry::sourceDir(const EmulatorInfo &e)
{
    return QDir(projectRoot()).filePath(e.dir);
}

QString EmulatorRegistry::buildDir(const EmulatorInfo &e)
{
    return QDir(projectRoot()).filePath("build-emulators/" + e.id);
}

bool EmulatorRegistry::isSourceCheckedOut(const EmulatorInfo &e)
{
    return QFileInfo::exists(sourceDir(e) + "/.git")
        || !QDir(sourceDir(e)).isEmpty();
}

QString EmulatorRegistry::findExecutable(const EmulatorInfo &e)
{
    const QString user = userExecutable(e.id);
    if (!user.isEmpty() && QFileInfo::exists(user))
        return user;

    for (const QString &rel : e.executables) {
        for (const QString &base : {buildDir(e), sourceDir(e)}) {
            const QString p = QDir(base).filePath(rel);
            const QFileInfo fi(p);
            if (fi.exists() && fi.isFile())
                return fi.absoluteFilePath();
        }
    }
    return {};
}

QString EmulatorRegistry::userExecutable(const QString &id)
{
    return QSettings().value("emulators/" + id + "/exe").toString();
}

void EmulatorRegistry::setUserExecutable(const QString &id, const QString &path)
{
    QSettings().setValue("emulators/" + id + "/exe", path);
}

QString EmulatorRegistry::romFolder(const QString &id)
{
    return QSettings().value("emulators/" + id + "/roms").toString();
}

void EmulatorRegistry::setRomFolder(const QString &id, const QString &path)
{
    QSettings().setValue("emulators/" + id + "/roms", path);
}
