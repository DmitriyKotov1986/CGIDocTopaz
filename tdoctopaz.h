#ifndef TDOCTOPAZ_H
#define TDOCTOPAZ_H

//Qt
#include <QtSql/QSqlDatabase>
#include <QString>
#include <QStringList>

namespace CGIDocTopaz
{

class TDocTopaz
{
private:
    struct QueryInfo
    {
        QStringList AZSCodes;
        quint64 id;
        QString queryText;
    };

    typedef QList<QueryInfo> TQueriesInfoList;

public:
    explicit TDocTopaz();
    ~TDocTopaz();

    int run(const QString& XMLText);
    void addQueryToDB(QSqlDatabase& db, QSqlQuery& query, TQueriesInfoList& queriesInfoList);
    QString errorString() const { return _errorString; }

private:
    QSqlDatabase _db;
    QString _errorString;

};

} //namespace CGIDocTopaz

#endif // TDOCTOPAZ_H
