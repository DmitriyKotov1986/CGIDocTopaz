#include "tconfig.h"

//QT
#include <QSettings>
#include <QFileInfo>
#include <QDebug>
#include <QDir>

using namespace CGIDocTopaz;
using namespace Common;

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

    qDebug() << QString("%1 %2").arg(QTime::currentTime().toString(SIMPLY_TIME_FORMAT)).arg("Reading configuration from " +  _configFileName);

    QSettings ini(_configFileName, QSettings::IniFormat);

    //Database
    ini.beginGroup("DATABASE");

    _db_ConnectionInfo.db_Driver = ini.value("Driver", "QODBC").toString();
    if (_db_ConnectionInfo.db_Driver.isEmpty())
    {
        _errorString = "Driver for database is undefine. Check [DATABASE]/Driver";

        return;
    }
    _db_ConnectionInfo.db_DBName = ini.value("DataBase", "SystemMonitorDB").toString();
    if (_db_ConnectionInfo.db_Driver.isEmpty())
    {
        _errorString = "Database is undefine. Check [DATABASE]/DataBase";

        return;
    }
    _db_ConnectionInfo.db_UserName = ini.value("UID", "").toString();
    _db_ConnectionInfo.db_Password = ini.value("PWD", "").toString();
    _db_ConnectionInfo.db_ConnectOptions = ini.value("ConnectionOptions", "").toString();
    _db_ConnectionInfo.db_Port = ini.value("Port", "").toUInt();
    _db_ConnectionInfo.db_Host = ini.value("Host", "localhost").toString();

    ini.endGroup();
}

