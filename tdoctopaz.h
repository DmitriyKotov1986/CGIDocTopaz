#ifndef TDOCTOPAZ_H
#define TDOCTOPAZ_H

//Qt
#include <QtSql/QSqlDatabase>
#include <QString>
#include <QHash>
#include <QStringList>
#include <QXmlStreamWriter>
#include <QDateTime>

namespace CGIDocTopaz
{

class TDocTopaz
{
private:
    struct QuerySQL
    {
        QString CreaterAZSCode;
        QStringList AZSCodes;
        QString id;
        QString queryText;
    };

    using TQuerySQL = QList<QuerySQL>;

    struct QuerySessionReport
    {
        QString AZSCode;
        QString id;
        quint64 sessionNum;
    };

    using TQuerySessionReports = QList<QuerySessionReport>;

    struct QueryDocument
    {
        bool isEmpty = true;
        quint64 lastId = 0;
        quint32 maxDocumentsCount = 1;
        QStringList AZSCodes;
        QStringList documentsType;
    };

    using TQueryDocuments = QList<QueryDocument>;

    struct SessionData
    {
        QString AZSCode;
        QString id;
        quint32 count = 0;
    };

    using TSessionsData = QList<SessionData>;

    struct RequestStatus
    {
        QString type;
        QString id;
    };

    using TRequestStatuses = QList<RequestStatus>;

    using DocumentsID = QHash<QString, quint64>;

public:
    explicit TDocTopaz();
    ~TDocTopaz();

    int run(const QString& XMLText);
    QString errorString() const { return _errorString; }

private:
    void addQuerySQL(TQuerySQL& queriesInfoList, const QString& createrAZSCode);
    void addQuerySessionReports(TQuerySessionReports& queriesInfoList, const QString& createrAZSCode);
    void addQuerySessionData(TSessionsData& queriesInfoList, const QString& createrAZSCode);

    void connectToDB();

    void addDocuments(QXmlStreamWriter& XMLWriter, const QueryDocument& queryDocuments);
    void addRequestStatuses(QXmlStreamWriter& XMLWriter, const TRequestStatuses& queryRequestStatuses);

    TQuerySQL parseQueries(QXmlStreamReader& XMLReader);
    TQuerySessionReports parseSessionReports(QXmlStreamReader& XMLReader);
    QueryDocument parseDocuments(QXmlStreamReader& XMLReader);
    TSessionsData parseSessionsData(QXmlStreamReader& XMLReader);
    TRequestStatuses parseRequestStatuses(QXmlStreamReader& XMLReader);

    DocumentsID getDocumentID(const QString& queryID);

    QStringList parseAZSCodes(QXmlStreamReader& XMLReader, const QString& tagName);

private:
    QSqlDatabase _db;
    QString _errorString;

};

} //namespace CGIDocTopaz

#endif // TDOCTOPAZ_H
