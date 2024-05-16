#ifndef TCONFIG_H
#define TCONFIG_H

//QT
#include <QString>
#include <QFile>

//My
#include "Common/common.h"

namespace CGIDocTopaz
{

class TConfig final
{
public:
    static TConfig* config(const QString& configFileName = "");
    static void deleteConfig();

private:
    explicit TConfig(const QString& configFileName);

public:
    //[DATABASE]
    const Common::DBConnectionInfo& db_ConnectionInfo() const { return _db_ConnectionInfo; }

    const QString& errorString() const { return _errorString; }
    bool isError() const { return !_errorString.isEmpty(); }

private:
    const QString _configFileName;

    QString _errorString;

    //[DATABASE]
    Common::DBConnectionInfo _db_ConnectionInfo;

};

} //namespace CGITopaz

#endif // TCONFIG_H
