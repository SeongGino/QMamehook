#include <QCoreApplication>
#include <QTimer>
#include "qhookermain.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qhookerMain mainApp;

    QString cliArg = argv[1];
    if(cliArg == "-v") {
        mainApp.verbosity = true;
        qInfo() << "Enabling verbose output!";
    }

    // connect up the signals
    QObject::connect(&mainApp, SIGNAL(finished()),
                     &app, SLOT(quit()));
    QObject::connect(&app, SIGNAL(aboutToQuit()),
                     &mainApp, SLOT(aboutToQuitApp()));

    // This code will start the messaging engine in QT and in
    // 10ms it will start the execution in the MainClass.run routine;
    QTimer::singleShot(10, &mainApp, SLOT(run()));

    return app.exec();
}
