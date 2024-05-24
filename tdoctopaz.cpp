#include "tdoctopaz.h"

//STL
#include <stdexcept>

//QT
#include <QSqlError>
#include <QSqlQuery>
#include <QXmlStreamReader>

//My
#include "Common/common.h"
#include "tconfig.h"

using namespace CGIDocTopaz;
using namespace Common;

static const QString QUERY_TYPE_QUERY = "Query";
static const QString QUERY_TYPE_SESSION_REPORT = "SessionReport";
static const QString QUERY_TYPE_SESSION_DATA = "SessionData";
static const QString CURRENT_PROTOCOL_VERSION = "0.1";

TDocTopaz::TDocTopaz()
{
}

TDocTopaz::~TDocTopaz()
{
    if (_db.isOpen())
    {
        _db.close();
    }
}

int TDocTopaz::run(const QString& XMLText)
{
    writeDebugLogFile("REQUEST>", XMLText);

    QTextStream errStream(stderr);

    if (XMLText.isEmpty())
    {
        _errorString = "XML is empty";
        errStream << _errorString;

        return EXIT_CODE::XML_EMPTY;
    }

    //парсим XML
    QXmlStreamReader XMLReader(XMLText);
    QString AZSCode;
    QString clientVersion;
    QString protocolVersion;

    TQueryDocuments queryDocuments;
    TQuerySQL querySQL;
    TQuerySessionReports querySessionReports;
    TSessionsData querySessionsData;
    TRequestStatuses queryRequestStatuses;

    try
    {
        while ((!XMLReader.atEnd()) && (!XMLReader.hasError()))
        {
            QXmlStreamReader::TokenType token = XMLReader.readNext();
            if (token == QXmlStreamReader::StartDocument)
            {
                continue;
            }
            else if (token == QXmlStreamReader::EndDocument)
            {
                break;
            }
            else if (token == QXmlStreamReader::StartElement)
            {
                //qDebug() << XMLReader.name().toString();
                if (XMLReader.name().toString()  == "Root")
                {
                    while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
                    {
                        //qDebug() << "Root/" << XMLReader.name().toString();
                        if (XMLReader.name().toString().isEmpty())
                        {
                            continue;
                        }
                        if (XMLReader.name().toString()  == "AZSCode")
                        {
                            AZSCode = XMLReader.readElementText();
                        }
                        else if (XMLReader.name().toString()  == "ClientVersion")
                        {
                            clientVersion = XMLReader.readElementText();
                        }
                        else if (XMLReader.name().toString()  == "ProtocolVersion")
                        {
                            protocolVersion = XMLReader.readElementText();
                        }
                        else if (XMLReader.name().toString()  == "Documents")
                        {
                            queryDocuments.append(parseDocuments(XMLReader));
                        }
                        else if (XMLReader.name().toString()  == "Queries")
                        {
                            querySQL.append(parseQueries(XMLReader));
                        }
                        else if (XMLReader.name().toString()  == "SessionReports")
                        {
                            querySessionReports.append(parseSessionReports(XMLReader));
                        }
                        else if (XMLReader.name().toString()  == "SessionsData")
                        {
                            querySessionsData.append(parseSessionsData(XMLReader));
                        }
                        else if (XMLReader.name().toString()  == "RequestStatuses")
                        {
                            queryRequestStatuses.append(parseRequestStatuses(XMLReader));
                        }
                        else
                        {
                            throw std::runtime_error(QString("Undefine tag in XML (Root/%1)")
                                                        .arg(XMLReader.name().toString()).toStdString());
                        }
                    }
                }
                else
                {
                    throw std::runtime_error(QString("Undefine tag in XML (%1)")
                                                .arg(XMLReader.name().toString()).toStdString());
                }
            }
        }

        if (XMLReader.hasError())
        { //неудалось распарсить пришедшую XML
            throw std::runtime_error(QString("Cannot parse XML query. Message: %1")
                                        .arg(XMLReader.errorString()).toStdString());
        }
        if (AZSCode.isEmpty())
        {
            throw std::runtime_error(QString("Value tag Root/AZSCode cannot be empty").toStdString());
        }
        if (protocolVersion.isEmpty() || protocolVersion != CURRENT_PROTOCOL_VERSION)
        {
            throw std::runtime_error(QString("Value tag Root/ProtocolVersion cannot be empty or protocol version is not support. Value: %1")
                                        .arg(protocolVersion).toStdString());
        }
    }
    catch(std::runtime_error &err)
    {
        _errorString = QString("Error parse XML: %1").arg(err.what());
        errStream << _errorString;

        return EXIT_CODE::XML_PARSE_ERR;
    }

    //Добавляем в список запросов новые запрашиваемые данные
    for (const auto& query: querySQL)
    {
        queryRequestStatuses.push_back({QUERY_TYPE_QUERY, query.id});
    }
    for (const auto& query: querySessionReports)
    {
        queryRequestStatuses.push_back({QUERY_TYPE_SESSION_REPORT, query.id});
    }
    for (const auto& query: querySessionsData)
    {
        queryRequestStatuses.push_back({QUERY_TYPE_SESSION_DATA, query.id});
    }

    //Обрабатываем результаты
    connectToDB();
    Q_ASSERT(_db.isOpen());
    _db.transaction();

    //Queries
    Q_ASSERT(!AZSCode.isEmpty());
    addQuerySQL(querySQL, AZSCode);
    addQuerySessionReports(querySessionReports, AZSCode);
    addQuerySessionData(querySessionsData, AZSCode);

    //Documents

    //формируем XML
    QString XMLStr;
    QXmlStreamWriter XMLWriter(&XMLStr);

    XMLWriter.setAutoFormatting(true);
    XMLWriter.writeStartDocument("1.0");
    XMLWriter.writeStartElement("Root");
    XMLWriter.writeTextElement("ProtocolVersion", "0.1");

    for (const auto& query: queryDocuments)
    {
        addDocuments(XMLWriter, query);
    }
    if (!queryRequestStatuses.isEmpty())
    {
        addRequestStatuses(XMLWriter, queryRequestStatuses);
    }

    XMLWriter.writeEndElement(); // Root
    XMLWriter.writeEndDocument(); // end XML

    DBCommit(_db);

    //отправляем ответ
    QTextStream answerTextStream(stdout);
    answerTextStream << XMLStr;

    writeDebugLogFile("ANSWER>", XMLStr);

    return EXIT_CODE::OK;
}

void TDocTopaz::addQuerySQL(TQuerySQL& queriesInfoList, const QString& createrAZSCode)
{
    Q_ASSERT(_db.isOpen());

    for (auto& queryInfo: queriesInfoList)
    {
        //если встретилось 999 - то заменем список на все доступные АЗС
        if (queryInfo.AZSCodes.indexOf("999", 0) >= 0)
        {
            const QString queryText =
                QString("SELECT [AZSCode] "
                        "FROM [AZSInfo] "
                        "WHERE [IsAZS] <> 0");

            writeDebugLogFile(QString("QUERY TO %1>").arg(_db.connectionName()), queryText);

            QSqlQuery query(_db);
            query.setForwardOnly(true);

            if (!query.exec(queryText))
            {
                errorDBQuery(_db, query);
            }

            queryInfo.AZSCodes.clear();
            while (query.next())
            {
                queryInfo.AZSCodes.append(QString("%1").arg(query.value("AZSCode").toString()));
            }
        }

        for (const auto& AZSCode: queryInfo.AZSCodes)
        {


            const QString queryText =
                QString("INSERT INTO [QueriesToTopaz] ([CreaterAZSCode], [AZSCode], [QueryID], [QueryText]) "
                                "VALUES ('%1', '%2', '%3', '%4') ")
                    .arg(createrAZSCode)
                    .arg(AZSCode)
                    .arg(queryInfo.id)
                    .arg(queryInfo.queryText);

            writeDebugLogFile(QString("QUERY TO %1>").arg(_db.connectionName()), queryText);

            QSqlQuery query(_db);

            if (!query.exec(queryText))
            {
                errorDBQuery(_db, query);
            }
        }
    }
}

void TDocTopaz::addQuerySessionReports(TQuerySessionReports &queriesInfoList, const QString& createrAZSCode)
{
    Q_ASSERT(_db.isOpen());



    for (auto& queryInfo: queriesInfoList)
    {

        const QString queryText =
            QString("INSERT INTO [QueriesSessionReports] ([CreaterAZSCode], [AZSCode], [SessionNum], [QueryID]) "
                            "VALUES ('%1', '%2', %3, '%4') ")
                .arg(createrAZSCode)
                .arg(queryInfo.AZSCode)
                .arg(queryInfo.sessionNum)
                .arg(queryInfo.id);

        writeDebugLogFile(QString("QUERY TO %1>").arg(_db.connectionName()), queryText);

        QSqlQuery query(_db);

        if (!query.exec(queryText))
        {
            errorDBQuery(_db, query);
        }
    }
}

void TDocTopaz::addQuerySessionData(TSessionsData &queriesInfoList, const QString& createrAZSCode)
{
    Q_ASSERT(_db.isOpen());

    for (auto& queryInfo: queriesInfoList)
    {
        const QString queryText =
            QString("INSERT INTO [QueriesSessionsData] ([CreaterAZSCode], [AZSCode], [Count], [QueryID]) "
                            "VALUES ('%1', '%2', '%3', '%4') ")
                .arg(createrAZSCode)
                .arg(queryInfo.AZSCode)
                .arg(queryInfo.count)
                .arg(queryInfo.id);

        writeDebugLogFile(QString("QUERY TO %1>").arg(_db.connectionName()), queryText);

        QSqlQuery query(_db);

        if (!query.exec(queryText))
        {
            errorDBQuery(_db, query);
        }
    }
}

void TDocTopaz::connectToDB()
{
    const auto cnf = TConfig::config();
    Q_CHECK_PTR(cnf);

    //настраиваем подключениек БД
    if (!Common::connectToDB(_db, cnf->db_ConnectionInfo(), "MainDB"))
    {
        QString msg = connectDBErrorString(_db);
        qCritical() << QString("%1 %2").arg(QTime::currentTime().toString(SIMPLY_TIME_FORMAT)).arg(msg);
        Common::writeLogFile("ERR>", msg);

        exit(EXIT_CODE::SQL_NOT_CONNECT);
    }
}

void TDocTopaz::addDocuments(QXmlStreamWriter& XMLWriter, const QueryDocument& queryDocuments)
{
    QString additionAZSCode;
    if (!queryDocuments.AZSCodes.isEmpty())
    {
        additionAZSCode = QString(" AND [AZSCode] IN (%1) ").arg(queryDocuments.AZSCodes.join(','));
    }

    QString additionDocumentTypes;
    if (!queryDocuments.documentsType.isEmpty())
    {
        additionDocumentTypes = QString(" AND [DocumentType] IN (%1) ").arg(queryDocuments.documentsType.join(','));
    }

    QString queryText =
        QString("SELECT TOP (%1) [ID], [AZSCode], [DocumentType], [Body], [QueryID] "
                "FROM [TopazDocuments] "
                "WHERE  [ID] > %2 %3 %4"
                "ORDER BY [ID]")
            .arg(queryDocuments.maxDocumentsCount)
            .arg(queryDocuments.lastId)
            .arg(additionAZSCode)
            .arg(additionDocumentTypes);

    writeDebugLogFile(QString("QUERY TO %1>").arg(_db.connectionName()), queryText);

    QSqlQuery query(_db);
    query.setForwardOnly(true);

    if (!query.exec(queryText))
    {
        errorDBQuery(_db, query);
    }

    XMLWriter.writeStartElement("Documents");

    quint64 lastId = queryDocuments.lastId;
    while (query.next())
    {
        XMLWriter.writeStartElement("Document");

        XMLWriter.writeTextElement("AZSCode", query.value("AZSCode").toString());
        XMLWriter.writeTextElement("DocumentType", query.value("DocumentType").toString());
        XMLWriter.writeTextElement("ID", query.value("ID").toString());
        XMLWriter.writeTextElement("Body", query.value("Body").toString());
        if (!query.value("QueryID").isNull())
        {
            XMLWriter.writeTextElement("QueryID", query.value("QueryID").toString());
        }

        XMLWriter.writeEndElement(); // Document

        lastId = std::max(query.value("ID").toULongLong(), queryDocuments.lastId);
    }
    XMLWriter.writeTextElement("LastID", QString::number(lastId));

    XMLWriter.writeEndElement(); // Documents
}

enum class QueryStatus
{
    UNKNOW,
    NOT_EXIST,
    ACCEPTED_SERVER,
    SENT_TO_AZS,
    COMPLITE
};

QString statusToString(QueryStatus status)
{
    switch (status)
    {
    case QueryStatus::ACCEPTED_SERVER: return "ACCEPTED_SERVER";
    case QueryStatus::NOT_EXIST: return "NOT_EXIST";
    case QueryStatus::SENT_TO_AZS: return "SENT_TO_AZS";
    case QueryStatus::COMPLITE: return "COMPLITE";
    case QueryStatus::UNKNOW:
    default:;
    }

    return "UNKNOW";
}

void TDocTopaz::addRequestStatuses(QXmlStreamWriter &XMLWriter, const TRequestStatuses &queryRequestStatuses)
{
    XMLWriter.writeStartElement("RequestStatuses");

    for (const auto& queryRequest: queryRequestStatuses)
    {
        QString tableName;
        if (queryRequest.type == QUERY_TYPE_QUERY)
        {
            tableName = "QueriesToTopaz";
        }
        else if (queryRequest.type == QUERY_TYPE_SESSION_DATA)
        {
            tableName = "QueriesSessionsData";
        }
        else if (queryRequest.type == QUERY_TYPE_SESSION_REPORT)
        {
            tableName = "QueriesSessionReports";
        }

        Q_ASSERT(!tableName.isEmpty());

        const auto documentsID = getDocumentID(queryRequest.id);

        const QString queryText =
            QString("SELECT [ID], [AZSCode], [AddDateTime], [LoadFromDateTime], [SentToDateTime] "
                    "FROM [%1] "
                    "WHERE [QueryID] = '%2'")
                .arg(tableName)
                .arg(queryRequest.id);

        writeDebugLogFile(QString("QUERY TO %1>").arg(_db.connectionName()), queryText);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        if (!query.exec(queryText))
        {
            errorDBQuery(_db, query);
        }

        bool find = false;
        while (query.next())
        {
            QueryStatus currentStatus = QueryStatus::UNKNOW;
            QDateTime currentStatusTime = QDateTime::currentDateTime();
            if (!query.value("LoadFromDateTime").toString().isEmpty())
            {
                currentStatus = QueryStatus::COMPLITE;
                currentStatusTime = query.value("LoadFromDateTime").toDateTime();
            }
            else if (!query.value("SentToDateTime").toString().isEmpty())
            {
                currentStatus = QueryStatus::SENT_TO_AZS;
                currentStatusTime = query.value("SentToDateTime").toDateTime();
            }
            else if (query.value("SentToDateTime").toString().isEmpty())
            {
                currentStatus = QueryStatus::ACCEPTED_SERVER;
                currentStatusTime = query.value("AddDateTime").toDateTime();
            }

            XMLWriter.writeStartElement("RequestStatus");

            const auto AZSCode = query.value("AZSCode").toString();

            XMLWriter.writeTextElement("Type", queryRequest.type);
            XMLWriter.writeTextElement("ID", queryRequest.id);
            XMLWriter.writeTextElement("AZSCode", AZSCode);
            XMLWriter.writeTextElement("Status", statusToString(currentStatus));
            XMLWriter.writeTextElement("DateTime", currentStatusTime.toString(DATETIME_FORMAT));

            if (currentStatus == QueryStatus::UNKNOW)
            {
                XMLWriter.writeTextElement("ErrorMessage", "Unknow internal error. Status cannot be define");
            }
            else if (currentStatus == QueryStatus::COMPLITE)
            {
                const auto documentsId_it = documentsID.find(QString("%1%2%3").arg(AZSCode).arg(queryRequest.id).arg(query.value("ID").toString()));
                XMLWriter.writeTextElement("DocumentID", QString::number(documentsId_it != documentsID.end() ? documentsId_it.value() : 0));
            }

            XMLWriter.writeEndElement(); // RequestStatus

            find = true;
        }

        if (!find)
        {
            XMLWriter.writeStartElement("RequestStatus");

            XMLWriter.writeTextElement("Type", queryRequest.type);
            XMLWriter.writeTextElement("ID", queryRequest.id);
            XMLWriter.writeTextElement("Status", statusToString(QueryStatus::NOT_EXIST));
            XMLWriter.writeTextElement("ErrorMessage", "Request is not exist in database");

            XMLWriter.writeEndElement(); // RequestStatus
        }
    }
    XMLWriter.writeEndElement(); // RequestStatuses
}

TDocTopaz::TQuerySQL TDocTopaz::parseQueries(QXmlStreamReader &XMLReader)
{
    TQuerySQL result;

    while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
    {
        //qDebug() << "Root/Queries" << XMLReader.name().toString();
        if (XMLReader.name().toString().isEmpty())
        {
            continue;
        }
        else if (XMLReader.name().toString()  == "Query")
        {
            QuerySQL tmp;
            while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
            {
                //qDebug() << "Root/Queries/Query" << XMLReader.name().toString();
                if (XMLReader.name().toString().isEmpty())
                {
                    continue;
                }
                else if (XMLReader.name().toString()  == "AZSCodes")
                {
                    tmp.AZSCodes = parseAZSCodes(XMLReader, "Root/Queries/Query/AZSCodes");
                }
                else if (XMLReader.name().toString()  == "ID")
                {
                    tmp.id = XMLReader.readElementText();
                    if (tmp.id.isEmpty() || tmp.id.length() > 25)
                    {
                         throw std::runtime_error(QString("Incorrect value tag (Root/Queries/Query/%1). Value: %2. Value must be string shorter than 25 chars")
                                .arg(XMLReader.name().toString())
                                .arg(XMLReader.readElementText()).toStdString());
                    }
                }
                else if (XMLReader.name().toString()  == "SQL")
                {
                    tmp.queryText =  XMLReader.readElementText();
                    if (tmp.queryText.isEmpty())
                    {
                        throw std::runtime_error(QString("Value tag (Root/Queries/Query/%1) cannot be empty")
                                                    .arg(XMLReader.name().toString()).toStdString());
                    }
                }
                else
                {
                    throw std::runtime_error(QString("Undefine tag in XML (Root/Queries/Query/%1)")
                                                .arg(XMLReader.name().toString()).toStdString());
                }
            }

            if (!tmp.queryText.isEmpty() && !tmp.id.isEmpty())
            {
                result.push_back(tmp);
            }
            else
            {
                throw std::runtime_error(QString("Insufficient parameters tag in XML (Root/Queries/Query)").toStdString());
            }

        }
        else
        {
            throw std::runtime_error(QString("Undefine tag in XML (Root/Queries/%1)")
                                        .arg(XMLReader.name().toString()).toStdString());
        }
    }

    return result;
}

TDocTopaz::TQuerySessionReports TDocTopaz::parseSessionReports(QXmlStreamReader &XMLReader)
{
    TQuerySessionReports result;

    while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
    {
        //qDebug() << "Root/SessionReports" << XMLReader.name().toString();
        if (XMLReader.name().toString().isEmpty())
        {
            continue;
        }
        else if (XMLReader.name().toString()  == "SessionReport")
        {
            QuerySessionReport tmp;
            while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
            {
                //qDebug() << "Root/SessionReports/SessionReport" << XMLReader.name().toString();
                if (XMLReader.name().toString().isEmpty())
                {
                    continue;
                }
                else if (XMLReader.name().toString()  == "AZSCode")
                {
                    tmp.AZSCode = XMLReader.readElementText();
                    if (tmp.AZSCode.isEmpty() || tmp.AZSCode.length() > 3)
                    {
                         throw std::runtime_error(QString("Value tag Root/SessionReports/SessionReport/%1 cannot be empty or londer 3 chars")
                                                    .arg(XMLReader.name().toString()).toStdString());
                    }
                }
                else if (XMLReader.name().toString()  == "SessionNum")
                {
                    bool ok = false;
                    const auto tmpStr = XMLReader.readElementText();
                    tmp.sessionNum = tmpStr.toULongLong(&ok);
                    if (!ok)
                    {
                        throw std::runtime_error(QString("Incorrect value tag (Root/SessionReports/SessionReport/%1). Value: %2. Value must be number")
                               .arg(XMLReader.name().toString())
                               .arg(tmpStr).toStdString());
                    }

                }
                else if (XMLReader.name().toString()  == "ID")
                {
                    tmp.id = XMLReader.readElementText();
                    if (tmp.id.isEmpty() || tmp.id.length() > 25)
                    {
                         throw std::runtime_error(QString("Incorrect value tag (Root/SessionReports/SessionReport/%1). Value: %2. Value must be string shorter than 25 chars")
                                .arg(XMLReader.name().toString())
                                .arg(tmp.id).toStdString());
                    }

                }
                else
                {
                    throw std::runtime_error(QString("Undefine tag in XML (Root/SessionReports/SessionReport/%1)")
                                                .arg(XMLReader.name().toString()).toStdString());
                }
            }
            if (!tmp.AZSCode.isEmpty() && tmp.sessionNum != 0 && !tmp.id.isEmpty())
            {
                result.push_back(tmp);
            }
            else
            {
                throw std::runtime_error(QString("Insufficient parameters tag in XML (Root/SessionsReports/SessionReport)").toStdString());
            }
        }
        else
        {
            throw std::runtime_error(QString("Undefine tag in XML (Root/SessionsReports/%1)")
                                        .arg(XMLReader.name().toString()).toStdString());
        }
    }

    return result;
}

TDocTopaz::QueryDocument TDocTopaz::parseDocuments(QXmlStreamReader &XMLReader)
{
    QueryDocument result;

    while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
    {
        //qDebug() << "Documents/" << XMLReader.name().toString();
        if (XMLReader.name().toString().isEmpty())
        {
            continue;
        }
        else if (XMLReader.name().toString()  == "LastID")
        {
            bool ok = false;
            result.lastId = XMLReader.readElementText().toULongLong(&ok);
            if (!ok)
            {
                throw std::runtime_error(QString("Incorrect value tag (Root/Documents/%1). Value: %2. Value must be number")
                       .arg(XMLReader.name().toString())
                       .arg(XMLReader.readElementText()).toStdString());
            }
        }
        else if (XMLReader.name().toString()  == "MaxDocumentsCount")
        {
            bool ok = false;
            QString tmp = XMLReader.readElementText();
            result.maxDocumentsCount = tmp.toUInt(&ok);
            if (!ok )
            {
                throw std::runtime_error(QString("Incorrect value tag (Root/Documents/%1). Value: %2. Value must be number")
                       .arg(XMLReader.name().toString())
                       .arg(tmp).toStdString());
            }
        }
        else if (XMLReader.name().toString()  == "AZSCodes")
        {
            result.AZSCodes = parseAZSCodes(XMLReader, "Root/Documents/AZSCodes");
        }
        else if (XMLReader.name().toString()  == "DocumentsType")
        {
            while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
            {
                if (XMLReader.name().toString().isEmpty())
                {
                    continue;
                }
                else if (XMLReader.name().toString()  == "DocumentType")
                {
                    const auto documentType = XMLReader.readElementText();
                    if (documentType.isEmpty())
                    {
                        throw std::runtime_error(QString("Value tag Root/Documents/DocumentsType/%1 cannot be empty")
                                                    .arg(XMLReader.name().toString()).toStdString());
                    }

                    result.documentsType.append(QString("'%1'").arg(documentType));
                }
                else
                {
                    throw std::runtime_error(QString("Undefine tag in XML (Root/Documents/DocumentsType/%1)")
                                                .arg(XMLReader.name().toString()).toStdString());
                }
            }
        }
        else
        {
            throw std::runtime_error(QString("Undefine tag in XML (Root/Documents/%1)")
                                        .arg(XMLReader.name().toString()).toStdString());
        }
    }

    result.isEmpty = false;

    return result;
}

TDocTopaz::TSessionsData TDocTopaz::parseSessionsData(QXmlStreamReader &XMLReader)
{
    TSessionsData result;

    while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
    {
        //qDebug() << "Documents/" << XMLReader.name().toString();
        if (XMLReader.name().toString().isEmpty())
        {
            continue;
        }
        else if (XMLReader.name().toString()  == "SessionData")
        {
            SessionData tmp;
            while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
            {
                if (XMLReader.name().toString().isEmpty())
                {
                    continue;
                }
                else if (XMLReader.name().toString()  == "AZSCode")
                {
                    tmp.AZSCode =  XMLReader.readElementText();
                    if (tmp.AZSCode.isEmpty() || tmp.AZSCode.length() > 3)
                    {
                        throw std::runtime_error(QString("Value tag (Root/SessionsData/SessionData/%2 cannot be empty or longer 3 chars")
                                                 .arg(XMLReader.name().toString()).toStdString());
                    }
                }
                else if (XMLReader.name().toString()  == "Count")
                {
                    bool ok = false;
                    tmp.count = XMLReader.readElementText().toULongLong(&ok);
                    if (!ok)
                    {
                        throw std::runtime_error(QString("Incorrect value tag (Root/SessionsData/SessionData/%1). Value: %2. Value must be number")
                               .arg(XMLReader.name().toString())
                               .arg(XMLReader.readElementText()).toStdString());
                    }

                }
                else if (XMLReader.name().toString()  == "ID")
                {
                    tmp.id = XMLReader.readElementText();
                    if (tmp.id.isEmpty() || tmp.id.length() > 25)
                    {
                         throw std::runtime_error(QString("Incorrect value tag (Root/SessionsData/SessionData/%1). Value: %2. Value must be string shorter than 25 chars")
                                .arg(XMLReader.name().toString())
                                .arg(XMLReader.readElementText()).toStdString());
                    }

                }
                else
                {
                    throw std::runtime_error(QString("Undefine tag in XML (Root/SessionsData/SessionData/%1)")
                                                .arg(XMLReader.name().toString()).toStdString());
                }
            }

            if (!tmp.AZSCode.isEmpty() && tmp.count != 0 && !tmp.id.isEmpty())
            {
                result.push_back(tmp);
            }
            else
            {
                throw std::runtime_error(QString("Insufficient parameters tag in XML (Root/SessionsData/SessionData)").toStdString());
            }

        }
        else
        {
            throw std::runtime_error(QString("Undefine tag in XML (Root/SessionsData/%1)")
                                        .arg(XMLReader.name().toString()).toStdString());
        }
    }

    return result;
}

TDocTopaz::TRequestStatuses TDocTopaz::parseRequestStatuses(QXmlStreamReader &XMLReader)
{
    TRequestStatuses result;

    while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
    {
        //qDebug() << "Documents/" << XMLReader.name().toString();
        if (XMLReader.name().toString().isEmpty())
        {
            continue;
        }
        else if (XMLReader.name().toString()  == "RequestStatus")
        {
            RequestStatus tmp;
            while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
            {
                if (XMLReader.name().toString().isEmpty())
                {
                    continue;
                }
                else if (XMLReader.name().toString() == "Type")
                {
                    tmp.type = XMLReader.readElementText();
                    if (tmp.type.isEmpty() ||
                        !(tmp.type == QUERY_TYPE_QUERY || tmp.type == QUERY_TYPE_SESSION_DATA || tmp.type == QUERY_TYPE_SESSION_REPORT))
                    {
                        throw std::runtime_error(QString("Value tag (Root/RequestStatuses/RequestStatus/%2 cannot be empty or type no exist. Value: %2")
                                                 .arg(XMLReader.name().toString()
                                                 .arg(tmp.type)).toStdString());
                    }
                }
                else if (XMLReader.name().toString() == "ID")
                {
                    tmp.id = XMLReader.readElementText();
                    if (tmp.id.isEmpty() || tmp.id.length() > 25)
                    {
                         throw std::runtime_error(QString("Incorrect value tag (Root/RequestStatuses/RequestStatus/%1). Value: %2. Value must be string shorter than 25 chars")
                                .arg(XMLReader.name().toString())
                                .arg(XMLReader.readElementText()).toStdString());
                    }
                }
                else
                {
                    throw std::runtime_error(QString("Undefine tag in XML (Root/RequestStatuses/RequestStatus/%1)")
                                                .arg(XMLReader.name().toString()).toStdString());
                }
            }

            if (!tmp.type.isEmpty() && !tmp.id.isEmpty())
            {
                result.push_back(tmp);
            }
            else
            {
                throw std::runtime_error(QString("Insufficient parameters tag in XML (Root/RequestStatuses/RequestStatus)").toStdString());
            }

        }
        else
        {
            throw std::runtime_error(QString("Undefine tag in XML (Root/RequestStatuses/%1)")
                                        .arg(XMLReader.name().toString()).toStdString());
        }
    }

    return result;
}

TDocTopaz::DocumentsID TDocTopaz::getDocumentID(const QString& queryID)
{
    DocumentsID result;

    const QString queryText =
        QString("SELECT [ID], [AZSCode], [DocumentNumber]"
                "FROM [TopazDocuments] "
                "WHERE [QueryID] = '%1' ")
            .arg(queryID);

    writeDebugLogFile(QString("QUERY TO %1>").arg(_db.connectionName()), queryText);

    QSqlQuery query(_db);
    query.setForwardOnly(true);

    if (!query.exec(queryText))
    {
        errorDBQuery(_db, query);
    }

    while (query.next())
    {
        result.insert(QString("%1%2%3").arg(query.value("AZSCode").toString()).arg(queryID).arg(query.value("DocumentNumber").toString()), query.value("ID").toULongLong());
    }

    return result;
}

QStringList TDocTopaz::parseAZSCodes(QXmlStreamReader &XMLReader, const QString& tagName)
{
    QStringList result;
    while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
    {
        if (XMLReader.name().toString().isEmpty())
        {
            continue;
        }
        else if (XMLReader.name().toString()  == "AZSCode")
        {
            const auto tmpAZSCode =  XMLReader.readElementText();
            if (tmpAZSCode.isEmpty() || tmpAZSCode.length() > 3)
            {
                throw std::runtime_error(QString("Value tag (%1/%2) cannot be empty or longer 3 chars")
                                         .arg(XMLReader.name().toString()).toStdString());
            }

            result.append(tmpAZSCode);
        }
        else
        {
            throw std::runtime_error(QString("Undefine tag in XML (%1/%2)")
                                        .arg(tagName)
                                        .arg(XMLReader.name().toString()).toStdString());
        }
    }

    return result;
}
