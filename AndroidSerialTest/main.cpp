#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "serialporthandler.h"

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QtCore/QCoreApplication>
#include <QtCore/private/qandroidextras_p.h>
#endif

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/private/qandroidextras_p.h>
#endif

int main(int argc, char *argv[])
{
    // High-DPI scaling is enabled by default in Qt 6

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    SerialPortHandler serialHandler;
    
#ifdef Q_OS_ANDROID
    // Get Android Context using Qt 6.8.3 API
    QJniObject context = QtAndroidPrivate::context();
    if (context.isValid()) {
        qDebug() << "Got valid Android Context in main.cpp";
        // Pass the context to SerialPortHandler
        serialHandler.initializeAndroidContext(context.object());
    } else {
        qWarning() << "Failed to get Android Context in main.cpp";
    }
#endif
    
    engine.rootContext()->setContextProperty("serialPortHandler", &serialHandler);

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
