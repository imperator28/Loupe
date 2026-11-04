#include "app/platform/FileAssociation.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QSettings>
#include <QUrl>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#endif

namespace loupe::app::platform {
namespace {

#ifdef Q_OS_WIN
const auto kProgId = QStringLiteral("Loupe.StepFile");
const auto kClasses = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes");
const auto kCapabilities = QStringLiteral("Software\\Loupe\\Capabilities");

QString executable() { return QDir::toNativeSeparators(QCoreApplication::applicationFilePath()); }
#endif

} // namespace

FileAssociation::FileAssociation(QObject* parent)
    : QObject(parent)
{
}

bool FileAssociation::supported() const noexcept
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool FileAssociation::registered() const
{
#ifdef Q_OS_WIN
    const QSettings classes(kClasses, QSettings::NativeFormat);
    const auto command = classes.value(kProgId + QStringLiteral("/shell/open/command/Default")).toString();
    return command.contains(executable(), Qt::CaseInsensitive);
#else
    return false;
#endif
}

bool FileAssociation::registerForStepFiles()
{
#ifdef Q_OS_WIN
    QSettings classes(kClasses, QSettings::NativeFormat);
    classes.setValue(kProgId + QStringLiteral("/Default"), tr("STEP model"));
    classes.setValue(kProgId + QStringLiteral("/DefaultIcon/Default"), QStringLiteral("\"%1\",0").arg(executable()));
    classes.setValue(kProgId + QStringLiteral("/shell/open/command/Default"), QStringLiteral("\"%1\" \"%2\"").arg(executable(), QStringLiteral("%1")));
    for (const auto* extension : {".step", ".stp"}) {
        const auto key = QString::fromLatin1(extension);
        classes.setValue(key + QStringLiteral("/OpenWithProgids/") + kProgId, QString{});
        classes.setValue(QStringLiteral("Applications/Loupe.exe/SupportedTypes/") + key, QString{});
    }
    classes.setValue(QStringLiteral("Applications/Loupe.exe/shell/open/command/Default"),
                     QStringLiteral("\"%1\" \"%2\"").arg(executable(), QStringLiteral("%1")));
    classes.sync();

    // Default Apps lists an app only through RegisteredApplications -> Capabilities.
    QSettings user(QStringLiteral("HKEY_CURRENT_USER"), QSettings::NativeFormat);
    user.setValue(kCapabilities + QStringLiteral("/ApplicationName"), QStringLiteral("Loupe"));
    user.setValue(kCapabilities + QStringLiteral("/ApplicationDescription"), tr("STEP inspector"));
    for (const auto* extension : {".step", ".stp"})
        user.setValue(kCapabilities + QStringLiteral("/FileAssociations/") + QString::fromLatin1(extension), kProgId);
    user.setValue(QStringLiteral("Software/RegisteredApplications/Loupe"), kCapabilities);
    user.sync();

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    emit registeredChanged();
    return classes.status() == QSettings::NoError && user.status() == QSettings::NoError;
#else
    return false;
#endif
}

bool FileAssociation::unregisterForStepFiles()
{
#ifdef Q_OS_WIN
    QSettings classes(kClasses, QSettings::NativeFormat);
    classes.remove(kProgId);
    classes.remove(QStringLiteral("Applications/Loupe.exe"));
    for (const auto* extension : {".step", ".stp"})
        classes.remove(QString::fromLatin1(extension) + QStringLiteral("/OpenWithProgids/") + kProgId);
    QSettings user(QStringLiteral("HKEY_CURRENT_USER"), QSettings::NativeFormat);
    user.remove(QStringLiteral("Software/Loupe/Capabilities"));
    user.remove(QStringLiteral("Software/RegisteredApplications/Loupe"));
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    emit registeredChanged();
    return true;
#else
    return false;
#endif
}

void FileAssociation::openDefaultAppsSettings() const
{
#ifdef Q_OS_WIN
    QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:defaultapps")));
#endif
}

} // namespace loupe::app::platform
