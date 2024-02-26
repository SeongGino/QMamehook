#include "qhookermain.h"
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
    qDebug() << "Main app is running!";
    tcpSocket = new QTcpSocket();
    connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(readyRead()));

    SerialInit();

    qInfo() << "Waiting for MAME Network Output @ localhost:8000 ...";

    for(;;) {
        switch(tcpSocket->state()) { // oh, bite me QT designer--these two are the only ones I need. :/
        case QAbstractSocket::UnconnectedState:
            tcpSocket->connectToHost("localhost", 8000);
            if(tcpSocket->waitForConnected(5000)) {
                qInfo() << "Connected to MAME instance!";
            } else {
                QThread::sleep(1);
            }
            break;
        case QAbstractSocket::ConnectedState:
            while(tcpSocket->state() == QAbstractSocket::ConnectedState) {
                if(!tcpSocket->waitForReadyRead(-1)) {
                    // mame has disconnected, so exit out.
                    qInfo() << "MAME exiting, disconnecting...";
                    tcpSocket->abort();
                    qDebug() << "Unloading config file.";
                    gameName.clear();
                    if(settings) {
                        delete settings;
                        settingsMap.clear();
                    }
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
                    break;
                }
            }
            break;
        }
    }
    // required to exit.
    quit();
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
            // Detect GUN4ALL and GUN4IR guns (are we the only ones that support this?)
            if(serialFoundList[i].vendorIdentifier() == 2336 ||
               serialFoundList[i].vendorIdentifier() == 9025) {
                qInfo() << "Found device @" << serialFoundList[i].systemLocation();
            } else {
                qDebug() << "Deleting dummy device" << serialFoundList[i].systemLocation();
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


void qhookerMain::readyRead()
{
    buffer.clear();
    QString input = tcpSocket->readLine();
    if(input.contains("mame_start")) {
        qInfo() << "Detected game name!";
        input.remove(" ");
        input = input.trimmed();
        gameName = input.remove(0, input.indexOf("=")+1);
        qDebug() << gameName;
        LoadConfig(QString(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/QMamehook/ini/" + gameName + ".ini"));
        if(settings->contains("MameStart")) {
            //qInfo() << "Detected start statement:";
            buffer = settings->value("MameStart").toStringList();
            //qInfo() << buffer;
            while(!buffer.isEmpty()) {
                if(buffer[0].contains("cmo")) {
                    // open serial port at number (index(4))
                    uint8_t portNum = buffer[0].at(4).digitValue()-1;
                    if(portNum >= 0 && portNum < serialFoundList.count()) {
                        if(!serialPort[portNum].isOpen()) {
                            serialPort[portNum].open(QIODevice::WriteOnly);
                            // Just in case Wendies complains:
                            serialPort[portNum].setDataTerminalReady(true);
                            qInfo() << "Opened port no" << portNum+1;
                        } else {
                            qInfo() << "Waaaaait a second... Port" << portNum+1 << "is already open!";
                        }
                    }
                } else if(buffer[0].contains("cmw")) {
                    uint8_t portNum = buffer[0].at(4).digitValue()-1;
                    if(portNum >= 0 && portNum < serialFoundList.count()) {
                        if(serialPort[portNum].isOpen()) {
                            serialPort[portNum].write(buffer[0].mid(6).toLocal8Bit());
                            if(serialPort[portNum].waitForBytesWritten(2000)) {
                                qInfo() << "Wrote" << buffer[0].mid(6).toLocal8Bit() << "to port" << portNum+1;
                            } else {
                                qInfo() << "Wrote to port no" << portNum+1 << ", but wasn't sent in time apparently!?";
                            }
                        } else {
                            qInfo() << "Requested to write to port no" << portNum+1 << ", but it's not even open yet!";
                        }
                    }
                }
                buffer.removeFirst();
            }
        }
    } else {
        buffer = input.split('\r');
        buffer.removeLast();
        while(!buffer.isEmpty()) {
            buffer[0] = buffer[0].trimmed();
            QString func = buffer[0].left(buffer[0].indexOf(" "));
            // do some comparisons with the settings here.
            if(!settingsMap[func].isEmpty()) {
                //qDebug() << "Hey, this one isn't empty!"; // testing
                //qDebug() << settingsMap[func]; // testing
                if(buffer[0].rightRef(1).toInt()) {
                    // Section One.
                    // if contains |, it's a two-function switch.
                    if(settingsMap[func].contains("|")) {
                        // left is for 0, right is for 1. Does not need replacement, but ignore "nul"
                        if(settingsMap[func].leftRef(settingsMap[func].indexOf("|")).contains("cmw")) {
                            // we can safely assume that "cmw" on the left side will always be at a set place.
                            uint8_t portNum = settingsMap[func].at(4).digitValue()-1;
                            if(portNum >= 0 && portNum < serialFoundList.count()) {
                                serialPort[portNum].write(settingsMap[func].mid(6, settingsMap[func].indexOf("|")).toLocal8Bit());
                                if(serialPort[portNum].waitForBytesWritten(2000)) {
                                //    qInfo() << "Wrote" << buffer[0].mid(6).toLocal8Bit() << "to port" << portNum+1;
                                } else {
                                    qWarning() << "Wrote to port no" << portNum+1 << ", but wasn't sent in time apparently!?";
                                }
                            }
                        }
                    // if contains %s%, s needs to be replaced by state.
                    } else if(settingsMap[func].contains("%s%", Qt::CaseInsensitive)) {
                        if(settingsMap[func].contains("cmw")) {
                            // we can safely assume that "cmw" will always be at a set place.
                            uint8_t portNum = settingsMap[func].at(4).digitValue()-1;
                            if(portNum >= 0 && portNum < serialFoundList.count()) {
                                QString temp = settingsMap[func];
                                temp = temp.replace("%s%", "%1", Qt::CaseInsensitive).arg(1);
                                serialPort[portNum].write(temp.mid(6).toLocal8Bit());
                                if(serialPort[portNum].waitForBytesWritten(2000)) {
                                    //    qInfo() << "Wrote" << buffer[0].mid(6).toLocal8Bit() << "to port" << portNum+1;
                                } else {
                                    qWarning() << "Wrote to port no" << portNum+1 << ", but wasn't sent in time apparently!?";
                                }
                            }
                        }
                    // ...or it has neither. Their funeral.
                    } else {
                        if(settingsMap[func].contains("cmw")) {
                            // we can safely assume that "cmw" will always be at a set place.
                            uint8_t portNum = settingsMap[func].at(4).digitValue()-1;
                            if(portNum >= 0 && portNum < serialFoundList.count()) {
                                serialPort[portNum].write(settingsMap[func].mid(6).toLocal8Bit());
                                if(serialPort[portNum].waitForBytesWritten(2000)) {
                                    //    qInfo() << "Wrote" << buffer[0].mid(6).toLocal8Bit() << "to port" << portNum+1;
                                } else {
                                    qWarning() << "Wrote to port no" << portNum+1 << ", but wasn't sent in time apparently!?";
                                }
                            }
                        }
                    }
                } else {
                    // Section Zero.
                    // if contains |, it's a two-function switch.
                    if(settingsMap[func].contains("|")) {
                        // right is for 1. Does not need replacement, but ignore "nul"
                        if(settingsMap[func].midRef(settingsMap[func].lastIndexOf("|")).contains("cmw")) {
                            uint8_t portNum = settingsMap[func].at(settingsMap[func].lastIndexOf("cmw")+4).digitValue()-1;
                            if(portNum >= 0 && portNum < serialFoundList.count()) {
                                serialPort[portNum].write(settingsMap[func].mid(settingsMap[func].lastIndexOf("cmw")).toLocal8Bit());
                                if(serialPort[portNum].waitForBytesWritten(2000)) {
                                    //qInfo() << "Wrote" << buffer[0].mid(6).toLocal8Bit() << "to port" << portNum+1;
                                } else {
                                    qWarning() << "Wrote to port no" << portNum+1 << ", but wasn't sent in time apparently!?";
                                }
                            }
                        }
                    // if contains %s%, s needs to be replaced by state.
                    } else if(settingsMap[func].contains("%s%", Qt::CaseInsensitive)) {
                        if(settingsMap[func].contains("cmw")) {
                            // we can safely assume that "cmw" will always be at a set place.
                            uint8_t portNum = settingsMap[func].at(4).digitValue()-1;
                            if(portNum >= 0 && portNum < serialFoundList.count()) {
                                QString temp = settingsMap[func];
                                temp = temp.replace("%s%", "%1", Qt::CaseInsensitive).arg(0);
                                serialPort[portNum].write(temp.mid(6).toLocal8Bit());
                                if(serialPort[portNum].waitForBytesWritten(2000)) {
                                    //    qInfo() << "Wrote" << buffer[0].mid(6).toLocal8Bit() << "to port" << portNum+1;
                                } else {
                                    qWarning() << "Wrote to port no" << portNum+1 << ", but wasn't sent in time apparently!?";
                                }
                            }
                        }
                    // ...or it has neither. Their funeral.
                    } else {
                        if(settingsMap[func].contains("cmw")) {
                            // we can safely assume that "cmw" will always be at a set place.
                            uint8_t portNum = settingsMap[func].at(4).digitValue()-1;
                            if(portNum >= 0 && portNum < serialFoundList.count()) {
                                serialPort[portNum].write(settingsMap[func].mid(6).toLocal8Bit());
                                if(serialPort[portNum].waitForBytesWritten(2000)) {
                                    //    qInfo() << "Wrote" << buffer[0].mid(6).toLocal8Bit() << "to port" << portNum+1;
                                } else {
                                    qWarning() << "Wrote to port no" << portNum+1 << ", but wasn't sent in time apparently!?";
                                }
                            }
                        }
                    }
                }
            }
            // then finally:
            buffer.removeFirst();
        }
    }
}

void qhookerMain::LoadConfig(QString path)
{
    settings = new QSettings(path, QSettings::IniFormat);
    if(!settings->contains("MameStart")) {
        qWarning() << "Error loading file at:" << path;
    } else {
        qInfo() << "Loading:" << path;
    }
    //qDebug() << settings->allKeys();
    settings->beginGroup("Output");
    QStringList settingsTemp = settings->childKeys();
    for(uint8_t i = 0; i < settingsTemp.length(); i++) {
        settingsMap[settingsTemp[i]] = settings->value(settingsTemp[i]).toString();
    }
    //qDebug() << settingsMap;
    settings->endGroup();
}
