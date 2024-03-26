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
                mainApp.customPath = QDir::fromNativeSeparators(arguments[arguments.indexOf("-p")+1]);
                // QDir::fromNativeSeparators uses forwardslashes on both OSes, thank Parace
                #ifdef Q_OS_WIN
                // closest way to check for drive letter, since colons aren't allowed in Windows filenames anyways
                if(mainApp.customPath.contains(":/")) {
                #else
                if(mainApp.customPath.contains('/')) {
                #endif // Q_OS_WIN
                    mainApp.customPathSet = true;
                    if(!mainApp.customPath.endsWith('/')) {
                        mainApp.customPath.append('/');
                    }
                    qInfo() << "Setting search path to" << mainApp.customPath;
                    arguments.removeAt(arguments.indexOf("-p")+1);
                } else {
                qWarning() << "Bad custom path specified (must be an absolute path)! Disregarding.";
            }
            } else {
                qWarning() << "Detected custom path flag without any path specified! Disregarding.";
            }
            arguments.removeAt(arguments.indexOf("-p"));
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
