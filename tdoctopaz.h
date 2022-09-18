#ifndef TSYNC_H
#define TSYNC_H

#include <QObject>
#include <QtSql/QSqlDatabase>
#include <QDateTime>
#include <QList>

#include "tconfig.h"

namespace CGIDocTopaz
{

class TDocTopaz
{
public:
    explicit TDocTopaz(TConfig* cfg);
    ~TDocTopaz();

public:
    int run(const QString& XMLText);
    QString errorString() const { return _errorString; }

private:
    QSqlDatabase _db;
    QString _errorString;

};

} //namespace CGIDocTopaz

#endif // TSYNC_H
