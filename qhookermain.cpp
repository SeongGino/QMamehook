#include "qhookermain.h"
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>

qhookerMain::qhookerMain(QObject *parent)
    : QObject{parent}
{
    // get the instance of the main application
    mainApp = QCoreApplication::instance();
}

void qhookerMain::run()
{
    //qDebug() << "Main app is running!";
    //connect(&tcpSocket, &QAbstractSocket::readyRead, this, &qhookerMain::ReadyRead);
    //connect(&tcpSocket, &QAbstractSocket::errorOccurred, this, &qhookerMain::errorOccurred);

    SerialInit();

    qInfo() << "Waiting for MAME-compatible Network Output @ localhost:8000 ...";

    for(;;) {
        switch(tcpSocket.state()) { // oh, bite me QT designer--these two are the only ones I need. :/
        case QAbstractSocket::UnconnectedState:
            tcpSocket.connectToHost("localhost", 8000);
            if(tcpSocket.waitForConnected(5000)) {
                qInfo() << "Connected to output server instance!";
            } else {
                QThread::sleep(1);
            }
            break;
        case QAbstractSocket::ConnectedState:
            while(tcpSocket.state() == QAbstractSocket::ConnectedState) {
                // in case of emergency for wendies, set to (+)1 instead
                // possible performance implications here?
                #ifdef Q_OS_WIN
                if(tcpSocket.waitForReadyRead(1)) {
                #else
                if(tcpSocket.waitForReadyRead(-1)) {
                #endif // Q_OS_WIN
                    while(!tcpSocket.atEnd()) {
                        ReadyRead();
                    }
                // Apparently wendies maybe possibly might make false positives here,
                // so check if the error is actually the host being closed, to at least stop it from ending early.
                } else if(tcpSocket.error() == QAbstractSocket::RemoteHostClosedError) {
                    qInfo() << "Server closing, disconnecting...";
                    tcpSocket.abort();
                    if(!gameName.isEmpty()) {
                        gameName.clear();
                        if(settings) {
                            delete settings;
                            settingsMap.clear();
                        }
                    }
                    // in case we exit without connecting to a game (*coughFLYCASTcough*)
                    for(uint8_t i = 0; i < serialFoundList.count(); i++) {
                        if(serialPort[i].isOpen()) {
                            serialPort[i].write("E");
                            if(serialPort[i].waitForBytesWritten(2000)) {
                                qInfo() << "Closed port" << i+1;
                                serialPort[i].close();
                            } else {
                                qInfo() << "Sent close signal to port" << i+1 << ", but wasn't sent in time apparently!?";
                            }
                        }
                    }
                }
            }
            break;
        }
    }
}


void qhookerMain::quit()
{
    emit finished();
}

// shortly after quit is called the CoreApplication will signal this routine
// this is a good place to delete any objects that were created in the
// constructor and/or to stop any threads
void qhookerMain::aboutToQuitApp()
{
    // stop threads
    // sleep(1);   // wait for threads to stop.
    // delete any objects
}


void qhookerMain::SerialInit()
{
    serialFoundList = QSerialPortInfo::availablePorts();
    if(serialFoundList.isEmpty()) {
        qWarning() << "No devices found! COM devices need to be found at start time.";
        quit();
    } else {
        // Yeah, sue me, we reading this backwards to make stack management easier.
        for(int i = serialFoundList.length() - 1; i >= 0; --i) {
            // Detect OpenFIRE, GUN4IR, and Blamcon(?) guns (are we the only ones that support this?)
            if(serialFoundList[i].vendorIdentifier() == 9025 ||   // JB
               serialFoundList[i].vendorIdentifier() == 13939 ||  // Props3D
               serialFoundList[i].vendorIdentifier() == 0xF143) { // OpenFIRE
                qInfo() << "Found device @" << serialFoundList[i].systemLocation();
            } else {
                //qDebug() << "Deleting dummy device" << serialFoundList[i].systemLocation();
                serialFoundList.removeAt(i);
            }
        }
        if(serialFoundList.isEmpty()) {
            qWarning() << "No VALID devices found! COM devices need to be found at start time.";
            quit();
        } else {
            serialPort = new QSerialPort[serialFoundList.length()];
            for(uint8_t i = 0; i < serialFoundList.length(); i++) {
                serialPort[i].setPort(serialFoundList[i]);
                qInfo() << "Assigning port no." << i+1;
            }
        }
    }
}


bool qhookerMain::GameSearching(QString input)
{
    if(buffer.isEmpty()) {
        // Split the output in case of connecting mid-way.
        buffer = input.split('\r', Qt::SkipEmptyParts);
    }
    //qDebug() << buffer;
    while(!buffer.isEmpty()) {
        buffer[0] = buffer[0].trimmed();

        // flycast outputs its start signal with code "game" using a game's full title instead of a mame zip name
        if(buffer[0].startsWith("mame_start =") || buffer[0].startsWith("game =")) {
            qInfo() << "Detected game name:";
            // flycast (standalone) ALSO doesn't disconnect at any point,
            // so we terminate and unload any existing settings if a new gameStart is found while a game is already loaded.
            if(!gameName.isEmpty()) {
                gameName.clear();
                if(settings) {
                    delete settings;
                    settingsMap.clear();
                }
            }
            gameName = buffer[0].mid(input.indexOf('=')+2).trimmed();
            qInfo() << gameName;

            if(gameName != "___empty") {
                if(customPathSet) {
                    LoadConfig(customPath + gameName + ".ini");
                } else {
                // TODO: there might be a better path for this? Trying to prevent "../QMamehook/QMamehook/ini" on Windows here.
                #ifdef Q_OS_WIN
                    LoadConfig(QString(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/ini/" + gameName + ".ini"));
                #else
                    LoadConfig(QString(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/QMamehook/ini/" + gameName + ".ini"));
                #endif
                }

                if(settings->contains("MameStart")) {
                    //qInfo() << "Detected start statement:";
                    QStringList tempBuffer = settings->value("MameStart").toStringList();
                    //qInfo() << tempBuffer;
                    while(!tempBuffer.isEmpty()) {
                        if(tempBuffer[0].contains("cmo")) {
                            // open serial port at number (index(4))
                            uint8_t portNum = tempBuffer[0].at(4).digitValue()-1;
                            if(portNum >= 0 && portNum < serialFoundList.count()) {
                                if(!serialPort[portNum].isOpen()) {
                                    serialPort[portNum].open(QIODevice::WriteOnly);
                                    // Just in case Wendies complains:
                                    serialPort[portNum].setDataTerminalReady(true);
                                    qInfo() << "Opened port no" << portNum+1;
                                } else {
                                    qWarning() << "Waaaaait a second... Port" << portNum+1 << "is already open!";
                                }
                            }
                        } else if(tempBuffer[0].contains("cmw")) {
                            uint8_t portNum = tempBuffer[0].at(4).digitValue()-1;
                            if(portNum >= 0 && portNum < serialFoundList.count()) {
                                if(serialPort[portNum].isOpen()) {
                                    serialPort[portNum].write(tempBuffer[0].mid(6).toLocal8Bit());
                                    if(!serialPort[portNum].waitForBytesWritten(2000)) {
                                        qWarning() << "Wrote to port no" << portNum+1 << ", but wasn't sent in time apparently!?";
                                    }
                                } else {
                                    qWarning() << "Requested to write to port no" << portNum+1 << ", but it's not even open yet!";
                                }
                            }
                        }
                        tempBuffer.removeFirst();
                    }
                }
                buffer.removeFirst();
                return true;
            } else {
                gameName.clear();
            }
        }
        buffer.removeFirst();
    }
    return false;
}


bool qhookerMain::GameStarted(QString input)
{
    if(buffer.isEmpty()) {
        buffer = input.split('\r', Qt::SkipEmptyParts);
    }
    while(!buffer.isEmpty()) {
        buffer[0] = buffer[0].trimmed();

        if(verbosity) {
            qInfo() << buffer[0];
        }

        QString func = buffer[0].left(buffer[0].indexOf(' '));

        // purge the current game name if stop signal is sent
        if(func == "mame_stop") {
            qInfo() << "mame_stop signal received, disconnecting.";
            if(!gameName.isEmpty()) {
                gameName.clear();
                if(settings) {
                    delete settings;
                    settingsMap.clear();
                    for(uint8_t i = 0; i < serialFoundList.count(); i++) {
                        if(serialPort[i].isOpen()) {
                            serialPort[i].write("E");
                            if(serialPort[i].waitForBytesWritten(2000)) {
                                qInfo() << "Closed port" << i+1;
                                serialPort[i].close();
                            } else {
                                qInfo() << "Sent close signal to port" << i+1 << ", but wasn't sent in time apparently!?";
                            }
                        }
                    }
                }
            }
            buffer.clear();
            return true;
        // checking if a command for this input channel exists
        } else if(!settingsMap[func].isEmpty()) {
            //qDebug() << "Hey, this one isn't empty!"; // testing
            //qDebug() << settingsMap[func]; // testing
            if(settingsMap[func].contains('|')) {
                if(buffer[0].rightRef(1).toInt()) {
                    // right is for 1. Does not need replacement, but ignore "nul"
                    QStringList action = settingsMap[func].mid(settingsMap[func].indexOf('|')+1).split(',', Qt::SkipEmptyParts);
                    for(uint8_t i = 0; i < action.length(); i++) {
                        if(action[i].contains("cmw")) {
                            uint8_t portNum = action[i].at(action[i].indexOf("cmw")+4).digitValue()-1;
                            if(portNum >= 0 && portNum < serialFoundList.count()) {
                                // if contains %s%, s needs to be replaced by state.
                                // yes, even here, in case of stupid.
                                if(action[i].contains("%s%")) {
                                    action[i] = action[i].replace("%s%", "%1").arg(1);
                                }
                                serialPort[portNum].write(action[i].mid(action[i].indexOf("cmw")+6).toLocal8Bit());
                                if(!serialPort[portNum].waitForBytesWritten(2000)) {
                                    qWarning() << "Wrote to port no" << portNum+1 << ", but wasn't sent in time apparently!?";
                                }
                            }
                        }
                    }
                } else {
                    // left is for 0. Does not need replacement, but ignore "nul"
                    QStringList action = settingsMap[func].left(settingsMap[func].indexOf('|')).split(',', Qt::SkipEmptyParts);
                    for(uint8_t i = 0; i < action.length(); i++) {
                        if(action[i].contains("cmw")) {
                            // we can safely assume that "cmw" on the left side will always be at a set place.
                            uint8_t portNum = action[i].at(action[i].indexOf("cmw")+4).digitValue()-1;
                            if(portNum >= 0 && portNum < serialFoundList.count()) {
                                // if contains %s%, s needs to be replaced by state.
                                // yes, even here, in case of stupid.
                                if(action[i].contains("%s%")) {
                                    action[i] = action[i].replace("%s%", "%1").arg(0);
                                }
                                serialPort[portNum].write(action[i].mid(action[i].indexOf("cmw")+6).toLocal8Bit());
                                if(!serialPort[portNum].waitForBytesWritten(2000)) {
                                    qWarning() << "Wrote to port no" << portNum+1 << ", but wasn't sent in time apparently!?";
                                }
                            }
                        }
                    }
                }
            // %s% wildcards: just replace with the number received
            } else {
                QStringList action = settingsMap[func].split(',', Qt::SkipEmptyParts);
                for(uint8_t i = 0; i < action.length(); i++) {
                    if(action[i].contains("cmw")) {
                        // we can safely assume that "cmw" will always be at a set place.
                        uint8_t portNum = action[i].at(action[i].indexOf("cmw")+4).digitValue()-1;
                        if(portNum >= 0 && portNum < serialFoundList.count()) {
                            // if contains %s%, s needs to be replaced by state.
                            if(action[i].contains("%s%")) {
                                action[i] = action[i].replace("%s%", "%1").arg(buffer[0].mid(buffer[0].indexOf('=')+2).toInt());
                            }
                            serialPort[portNum].write(action[i].mid(action[i].indexOf("cmw")+6).toLocal8Bit());
                            if(!serialPort[portNum].waitForBytesWritten(2000)) {
                                qWarning() << "Wrote to port no" << portNum+1 << ", but wasn't sent in time apparently!?";
                            }
                        }
                    }
                }
            }
        // if setting does not exist, register it
        } else if(!settings->contains(func)) {
            settings->beginGroup("Output");
            settings->setValue(func, "");
            settingsMap[func] = "";
            settings->endGroup();
        }
        // then finally:
        buffer.removeFirst();
    }
    return false;
}


void qhookerMain::ReadyRead()
{
    buffer.clear();
    if(gameName.isEmpty()) {
        // if this returns early as true, then zip straight into the GameStarted function with the remaining buffer.
        if(GameSearching(tcpSocket.readLine())) {
            GameStarted("");
        }
    } else {
        if(GameStarted(tcpSocket.readLine())) {
            GameSearching("");
        }
    }
}


void qhookerMain::LoadConfig(QString path)
{
    settings = new QSettings(path, QSettings::IniFormat);
    if(!settings->contains("MameStart")) {
        qWarning() << "Error loading file at:" << path;
        if(!QFile::exists(path) && !path.contains("__empty")) {
            settings->setValue("MameStart", "");
            settings->setValue("MameStop", "");\
            settings->setValue("StateChange", "");
            settings->setValue("OnRotate", "");
            settings->setValue("OnPause", "");
            settings->setValue("KeyStates/RefreshTime", "");
        }
    } else {
        qInfo() << "Loading:" << path;
    }
    settings->beginGroup("Output");
    QStringList settingsTemp = settings->childKeys();
    for(uint8_t i = 0; i < settingsTemp.length(); i++) {
        // QSettings splits anything with a comma, so we have to stitch the Q-splitted value back together.
        if(settings->value(settingsTemp[i]).type() == QVariant::StringList) {
            settingsMap[settingsTemp[i]] = settings->value(settingsTemp[i]).toStringList().join(",");
        } else {
            settingsMap[settingsTemp[i]] = settings->value(settingsTemp[i]).toString();
        }
    }
    settings->endGroup();
}
