#include "qhookermain.h"
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>

// TODO: core app stuff isn't really needed, looks very messy.

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

    printf("Waiting for MAME-compatible Network Output @ localhost:8000 ...\n");

    for(;;) {
        switch(tcpSocket.state()) { // oh, bite me QT designer--these two are the only ones I need. :/
        case QAbstractSocket::UnconnectedState:
            tcpSocket.connectToHost("localhost", 8000);

            if(tcpSocket.waitForConnected(5000))
                qInfo() << "Connected to output server instance!\n";
            else QThread::sleep(1);

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
                    while(!tcpSocket.atEnd())
                        ReadyRead();

                // Apparently wendies maybe possibly might make false positives here,
                // so check if the error is actually the host being closed, to at least stop it from ending early.
                } else if(tcpSocket.error() == QAbstractSocket::RemoteHostClosedError) {
                    printf("Server closing, disconnecting...\n");
                    tcpSocket.abort();

                    if(!gameName.isEmpty()) {
                        gameName.clear();
                        if(settings) {
                            delete settings;
                            settingsMap.clear();
                        }
                    }

                    if (closeOnDisconnect) {
                        printf("Application closing due to -c argument.\n");
                        quit();
                        return;
                    }

                    // in case we exit without connecting to a game (*coughFLYCASTcough*)
                    for(uint8_t i = 0; i < validDevices.count(); i++) {
                        if(serialPort[i].isOpen()) {
                            serialPort[i].write("E");
                            if(serialPort[i].waitForBytesWritten(2000)) {
                                printf("Closed port %d (%s)\n", i+1, serialPort[i].portName().toLocal8Bit().constData());
                                serialPort[i].close();
                            } else {
                                printf("Sent close signal to port %d, but wasn't sent in time apparently!?\n", i+1);
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

    if (serialFoundList.isEmpty()) {
        printf("No devices found! COM devices need to be found at start time.\n");
        quit();
    } else {
        // Filter devices based on Vendor IDs (JB = 9025, Props3D = 13939, OpenFIRE = 0xF143)
        for (const QSerialPortInfo &info : serialFoundList) {
            if(info.vendorIdentifier() == 9025  ||  // JB
               info.vendorIdentifier() == 13939 ||  // Props3D
               info.vendorIdentifier() == 0xF143)   // OpenFIRE
                validDevices.append(info);
            else if(!info.portName().startsWith("ttyS"))
                printf("Unknown device found: %s\n", info.portName().toLocal8Bit().constData());
        }

        // Print all device information
        PrintDeviceInfo(validDevices);

        if (validDevices.isEmpty()) {
            printf("No VALID devices found! COM devices need to be found at start time.\n");
            quit();
        } else {
            // Sort valid devices by Product ID ascending
            std::sort(validDevices.begin(), validDevices.end(),
                      [](const QSerialPortInfo &a, const QSerialPortInfo &b) {
                          return a.productIdentifier() < b.productIdentifier();
                      });

            // Create our array of QSerialPorts, sized to the number of valid devices
            serialPort = new QSerialPort[validDevices.size()];

            // Keep track of assigned PIDs and check for duplicates
            QSet<quint16> assignedPids;
            bool duplicateProductIds = false;

            // Assign indices (ports) in sorted order (lowest PID → highest PID)
            for (int i = 0; i < validDevices.size(); ++i) {
                const QSerialPortInfo &info = validDevices[i];

                // Check for duplicates
                if(assignedPids.contains(info.productIdentifier())) {
                   duplicateProductIds = true;
                   printf("Duplicate Product ID %04X found on device %s\n",
                          info.productIdentifier(), info.portName().toLocal8Bit().constData());
                } else assignedPids.insert(info.productIdentifier());

                // Now simply assign i as the index for this device
                // (port #1 for i=0, port #2 for i=1, etc.)
                serialPort[i].setPort(info);
                serialPort[i].setBaudRate(QSerialPort::Baud9600);
                serialPort[i].setDataBits(QSerialPort::Data8);
                serialPort[i].setParity(QSerialPort::NoParity);
                serialPort[i].setStopBits(QSerialPort::OneStop);
                serialPort[i].setFlowControl(QSerialPort::NoFlowControl);

                printf("Assigning %s (%04X:%04X) to port no. %d\n",
                       info.portName().toLocal8Bit().constData(), info.vendorIdentifier(), info.productIdentifier(), i+1);
            }

            if (duplicateProductIds) {
                printf("Matching identifiers detected.\n"
                       "To get consistent port allocations, each gun should have differentiating Product IDs.\n");
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
            printf("Detected game name:");

            // flycast (standalone) ALSO doesn't disconnect at any point,
            // so we terminate and unload any existing settings if a new gameStart is found while a game is already loaded.
            if(!gameName.isEmpty()) {
                gameName.clear();
                if(settings) {
                    delete settings;
                    settingsMap.clear();
                }
            }

            gameName = buffer[0].mid(input.indexOf('=')+2).trimmed().toLocal8Bit();
            printf("%s\n", gameName.constData());

            if(gameName != "___empty") {
                if(customPathSet) {
                    LoadConfig(customPath + gameName + ".ini");
                } else {
                // TODO: there might be a better path for this? Trying to prevent "../QMamehook/QMamehook/ini" on Windows here.
                #ifdef Q_OS_WIN
                    LoadConfig(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/ini/" + gameName + ".ini");
                #else
                    LoadConfig(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/QMamehook/ini/" + gameName + ".ini");
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
                                    printf("Opened port no. %d (%s)\n", portNum+1, serialPort[portNum].portName().toLocal8Bit().constData());
                                } else {
                                    printf("Waaaaait a second... Port %d is already open!\n", portNum+1);
                                }
                            }
                        } else if(tempBuffer[0].contains("cmw")) {
                            uint8_t portNum = tempBuffer[0].at(4).digitValue()-1;
                            if(portNum >= 0 && portNum < serialFoundList.count()) {
                                if(serialPort[portNum].isOpen()) {
                                    serialPort[portNum].write(tempBuffer[0].mid(6).toLocal8Bit());
                                    if(!serialPort[portNum].waitForBytesWritten(2000))
                                        printf("Wrote to port no. %d, but wasn't sent in time apparently!?\n", portNum+1);
                                } else  printf("Requested to write to port no. %d, but it's not even open yet!\n", portNum+1);
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
    if(buffer.isEmpty())
        buffer = input.split('\r', Qt::SkipEmptyParts);

    while(!buffer.isEmpty()) {
        buffer[0] = buffer[0].trimmed();

        if(verbosity)
            printf("%s\n", buffer[0].toLocal8Bit().constData());

        QString func = buffer[0].left(buffer[0].indexOf(' '));

        // purge the current game name if stop signal is sent
        if(func == "mame_stop") {
            printf("mame_stop signal received, disconnecting.\n");

            if(!gameName.isEmpty()) {
                gameName.clear();

                if(settings) {
                    delete settings;
                    settingsMap.clear();
                    for(uint8_t i = 0; i < validDevices.count(); i++) {
                        if(serialPort[i].isOpen()) {
                            serialPort[i].write("E");
                            if(serialPort[i].waitForBytesWritten(2000)) {
                                printf("Closed port %d\n", i+1);
                                serialPort[i].close();
                            } else printf("Sent close signal to port %d, but wasn't sent in time apparently!?\n", i+1);
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
                if(buffer[0].right(1).toInt()) {
                    // right is for 1. Does not need replacement, but ignore "nul"
                    QStringList action = settingsMap[func].mid(settingsMap[func].indexOf('|')+1).split(',', Qt::SkipEmptyParts);
                    for(uint8_t i = 0; i < action.length(); i++) {
                        if(action[i].contains("cmw")) {
                            uint8_t portNum = action[i].at(action[i].indexOf("cmw")+4).digitValue()-1;
                            if(portNum >= 0 && portNum < serialFoundList.count()) {
                                // if contains %s%, s needs to be replaced by state.
                                // yes, even here, in case of stupid.
                                if(action[i].contains("%s%"))
                                    action[i] = action[i].replace("%s%", "%1").arg(1);

                                serialPort[portNum].write(action[i].mid(action[i].indexOf("cmw")+6).toLocal8Bit());
                                if(!serialPort[portNum].waitForBytesWritten(2000))
                                    printf("Wrote to port no. %d, but wasn't sent in time apparently!?\n", portNum+1);
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
                                if(action[i].contains("%s%"))
                                    action[i] = action[i].replace("%s%", "%1").arg(0);

                                serialPort[portNum].write(action[i].mid(action[i].indexOf("cmw")+6).toLocal8Bit());
                                if(!serialPort[portNum].waitForBytesWritten(2000))
                                    printf("Wrote to port no. %d, but wasn't sent in time apparently!?\n", portNum+1);
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
                            if(action[i].contains("%s%"))
                                action[i] = action[i].replace("%s%", "%1").arg(buffer[0].mid(buffer[0].indexOf('=')+2).toInt());

                            serialPort[portNum].write(action[i].mid(action[i].indexOf("cmw")+6).toLocal8Bit());
                            if(!serialPort[portNum].waitForBytesWritten(2000))
                                printf("Wrote to port no. %d, but wasn't sent in time apparently!?\n", portNum+1);
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
        printf("Error loading file at: %s\n", path.toLocal8Bit().constData());
        if(!QFile::exists(path) && !path.contains("__empty")) {
            settings->setValue("MameStart", "");
            settings->setValue("MameStop", "");
            settings->setValue("StateChange", "");
            settings->setValue("OnRotate", "");
            settings->setValue("OnPause", "");
            settings->setValue("KeyStates/RefreshTime", "");
        }
    } else printf("Loading: %s\n", path.toLocal8Bit().constData());

    settings->beginGroup("Output");

    QStringList settingsTemp = settings->childKeys();
    for(uint8_t i = 0; i < settingsTemp.length(); i++) {
        // QSettings splits anything with a comma, so we have to stitch the Q-splitted value back together.
        if(settings->value(settingsTemp[i]).type() == QVariant::StringList)
             settingsMap[settingsTemp[i]] = settings->value(settingsTemp[i]).toStringList().join(",");
        else settingsMap[settingsTemp[i]] = settings->value(settingsTemp[i]).toString();
    }
    settings->endGroup();
}

void qhookerMain::PrintDeviceInfo(const QList<QSerialPortInfo> &devices)
{
    for (const QSerialPortInfo &info : devices) {
        printf("========================================\n");
        printf("Port Name: %s\n", info.portName().toLocal8Bit().constData());
        printf("Vendor Identifier:  ");
        if(info.hasVendorIdentifier()) {
            printf("%04X", info.vendorIdentifier());
            switch(info.vendorIdentifier()) {
            case 9025:
                printf(" (GUN4IR Lightgun)\n");
                break;
            case 13939:
                printf(" (Blamcon Lightgun)\n");
                break;
            case 0xF143:
                printf(" (OpenFIRE Lightgun)\n");
                break;
            default:
                // unlikely to happen due to whitelisting, but just in case.
                printf("\n");
                break;
            }
        } else printf("N/A\n");

        printf("Product Identifier: ");
        if(info.hasProductIdentifier())
            printf("%04X\n", info.productIdentifier());
        else printf("N/A\n");

        if(!info.manufacturer().isEmpty() && !info.description().isEmpty())
            printf("Device: %s %s\n", info.manufacturer().toLocal8Bit().constData(), info.description().toLocal8Bit().constData());
        printf("========================================\n");
    }
}
