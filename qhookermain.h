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

    QString gameName;

    QList<QSerialPortInfo> serialFoundList;

    QHash<QString, QString> settingsMap;

    void LoadConfig(QString name);

    void SerialInit();

    void GameSearching(QString input);

    void GameStarted(QString input);

    void ReadyRead();

public:
    explicit qhookerMain(QObject *parent = 0);

    bool verbosity = false;

    void quit();

signals:
    void finished();

public slots:
    void run();

    void aboutToQuitApp();

//private slots:

};

#endif // QHOOKERMAIN_H
