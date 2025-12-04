#include <QGuiApplication>
#include <QStyleHints>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml/qqmlextensionplugin.h>
#include <QLoggingCategory>
#include <QtCore>

Q_IMPORT_QML_PLUGIN(OVParametersQmlPlugin)
Q_IMPORT_QML_PLUGIN(OVStylePlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("OpenViBE Acquisition Server");
    app.setOrganizationName("inria");
    app.setOrganizationDomain("inria.fr");
    QSettings::setDefaultFormat(QSettings::IniFormat);

    QQuickStyle::setStyle("Basic");
    QGuiApplication::styleHints()->setUseHoverEffects(true);
    QLoggingCategory::setFilterRules(QStringLiteral("qt.qml.binding.removal.info=true"));

    //QDirIterator it(":", QDirIterator::Subdirectories);
    //while (it.hasNext()) {
    //    qDebug() << it.next();
    //}

    QQmlApplicationEngine engine;
    //engine.addImportPath("qrc:/");
    //engine.load(QUrl("qrc:/openvibe-acquisition-server-qml/main.qml"));

    engine.addImportPath("qrc:/qt/qml/");
    engine.load(QUrl("qrc:/qt/qml/openvibe-acquisition-server-qml/main.qml"));

    return app.exec();
}
