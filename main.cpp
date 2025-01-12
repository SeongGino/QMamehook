#include <QCoreApplication>
#include <QTimer>
#include <QDir>
#include "qhookermain.h"

int main(int argc, char *argv[])
{
    QCoreApplication *app = new QCoreApplication(argc, argv);

    qhookerMain mainApp;

    // argc is a count of arguments starting from 1 (executable inclusive),
    // argv is an array containing each argument starting from 0 (executable inclusive).
    if(argc > 1) {
        QStringList arguments;
        for(uint8_t i = 1; i < argc; i++) {
            arguments.append(argv[i]);
        }
        if(arguments.contains("-v")) {
            mainApp.verbosity = true;
            qInfo() << "Enabling verbose output!";
            arguments.removeAt(arguments.indexOf("-v"));
        }
        if(arguments.contains("-p")) {
            if(arguments.length() > 1) {
                // QDir::fromNativeSeparators uses forwardslashes on both OSes, thank Parace
                mainApp.customPath = QDir::fromNativeSeparators(arguments[arguments.indexOf("-p")+1]);
                mainApp.customPathSet = true;
                if(QDir::isRelativePath(mainApp.customPath)) {
                    mainApp.customPath.prepend(QDir::currentPath() + '/');
                }
                if(!mainApp.customPath.endsWith('/')) {
                    mainApp.customPath.append('/');
                }
                qInfo() << "Setting search path to" << mainApp.customPath;
                arguments.removeAt(arguments.indexOf("-p")+1);
            } else {
                qWarning() << "Detected custom path flag without any path specified! Disregarding.";
            }
            arguments.removeAt(arguments.indexOf("-p"));
        }
        if(arguments.contains("-c")) {
            mainApp.closeOnDisconnect = true;
            qInfo() << "Close on disconnect enabled!";
            arguments.removeAt(arguments.indexOf("-c"));
        }
    }

    // connect up the signals
    QObject::connect(&mainApp, SIGNAL(finished()),
                     app, SLOT(quit()), Qt::QueuedConnection);
    QObject::connect(app, SIGNAL(aboutToQuit()),
                     &mainApp, SLOT(aboutToQuitApp()), Qt::QueuedConnection);

    // This code will start the messaging engine in QT and in
    // 10ms it will start the execution in the MainClass.run routine;
    QTimer::singleShot(10, &mainApp, SLOT(run()));

    return app->exec();
}
