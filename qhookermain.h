#ifndef QHOOKERMAIN_H
#define QHOOKERMAIN_H

#include <QCoreApplication>
#include <QObject>
#include <QSettings>
#include <QTcpSocket>
#include <QTextStream>
#include <QSerialPort>
#include <QSerialPortInfo>

class qhookerMain : public QObject
{
    Q_OBJECT

private:
    QCoreApplication *mainApp;

    QSettings *settings;

    QTcpSocket tcpSocket;

    QSerialPort *serialPort;

    QStringList buffer;

    QByteArray gameName;

    QList<QSerialPortInfo> serialFoundList;

    QList<QSerialPortInfo> validDevices;

    QHash<QString, QString> settingsMap;

    void LoadConfig(QString name);

    void SerialInit();

    bool GameSearching(QString input);

    bool GameStarted(QString input);

    void ReadyRead();

    QMap<int, QSerialPort*> serialPortMap;

public:
    explicit qhookerMain(QObject *parent = 0);

    bool verbosity = false;

    bool customPathSet = false;

    bool closeOnDisconnect = false;

    QString customPath;

    void PrintDeviceInfo(const QList<QSerialPortInfo> &devices);

    void quit();

signals:
    void finished();

public slots:
    void run();

    void aboutToQuitApp();

//private slots:

};

#endif // QHOOKERMAIN_H
