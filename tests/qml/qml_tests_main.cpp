#include <QCoreApplication>
#include <QObject>
#include <QtQuickTest/quicktest.h>

class QuickTestSetup final : public QObject
{
    Q_OBJECT

public slots:
    void applicationAvailable()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("Loupe"));
        QCoreApplication::setOrganizationDomain(QStringLiteral("loupe.local"));
        QCoreApplication::setApplicationName(QStringLiteral("LoupeQmlTests"));
    }
};

QUICK_TEST_MAIN_WITH_SETUP(loupe_qml_tests, QuickTestSetup)

#include "qml_tests_main.moc"
