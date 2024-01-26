#include "tconfig.h"

#include <QSettings>
#include <QFileInfo>
#include <QDebug>
#include <QDir>

using namespace CGIDocTopaz;

static TConfig* configPtr = nullptr;

TConfig* TConfig::config(const QString& configFileName)
{
    if (configPtr == nullptr)
    {
        configPtr = new TConfig(configFileName);
    }

    return configPtr;
};

void TConfig::deleteConfig()
{
    Q_CHECK_PTR(configPtr);

    delete configPtr;

    configPtr = nullptr;
}

TConfig::TConfig(const QString& configFileName) :
    _configFileName(configFileName)
{
    if (_configFileName.isEmpty()) {
        _errorString = "Configuration file name cannot be empty";
        return;
    }
    if (!QFileInfo(_configFileName).exists()) {
        _errorString = "Configuration file not exist. File name: " + _configFileName;
        return;
    }

    qDebug() << QString("%1 %2").arg(QTime::currentTime().toString("hh:mm:ss")).arg("Reading configuration from " +  _configFileName);

    QSettings ini(_configFileName, QSettings::IniFormat);

    //Database
    ini.beginGroup("DATABASE");
    _db_Driver = ini.value("Driver", "QODBC").toString();
    _db_DBName = ini.value("DataBase", "SystemMonitorDB").toString();
    _db_UserName = ini.value("UID", "").toString();
    _db_Password = ini.value("PWD", "").toString();
    _db_ConnectOptions = ini.value("ConnectionOprions", "").toString();
    _db_Port = ini.value("Port", "").toUInt();
    _db_Host = ini.value("Host", "localhost").toString();
    ini.endGroup();
}

