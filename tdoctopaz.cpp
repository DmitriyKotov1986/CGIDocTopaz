#include "tdoctopaz.h"

//QT
#include <QSqlError>
#include <QSqlQuery>
#include <QXmlStreamReader>

//My
#include "Common/common.h"
#include "tconfig.h"

using namespace CGIDocTopaz;
using namespace Common;

TDocTopaz::TDocTopaz()
{
    const auto cfg = TConfig::config();

    Q_CHECK_PTR(cfg);

    //настраиваем БД
    _db = QSqlDatabase::addDatabase(cfg->db_Driver(), "MainDB");
    _db.setDatabaseName(cfg->db_DBName());
    _db.setUserName(cfg->db_UserName());
    _db.setPassword(cfg->db_Password());
    _db.setConnectOptions(cfg->db_ConnectOptions());
    _db.setPort(cfg->db_Port());
    _db.setHostName(cfg->db_Host());
}

TDocTopaz::~TDocTopaz()
{
    if (_db.isOpen()) {
        _db.close();
    }
}

int TDocTopaz::run(const QString& XMLText)
{
    writeDebugLogFile("REQUEST>", XMLText);

    QTextStream textStream(stderr);

    if (XMLText.isEmpty())
    {
        _errorString = "XML is empty";
        textStream << _errorString;

        return EXIT_CODE::XML_EMPTY;
    }

    if (!_db.open()) {
        _errorString = "Cannot connet to DB. Error: " + _db.lastError().text();
        return EXIT_CODE::SQL_NOT_OPEN_DB;
    }

    //парсим XML
    QXmlStreamReader XMLReader(XMLText);
    QString AZSCode = "n/a";
    QString clientVersion = "n/a";
    QString protocolVersion = "n/a";
    qulonglong lastId = 0;
    quint32 maxDocumentCount = 1;
    QStringList documentsAZSCodes;
    QStringList documentsType;

    TQueriesInfoList queriesInfoList;

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
                        while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
                        {
                            //qDebug() << "Documents/" << XMLReader.name().toString();
                            if (XMLReader.name().toString().isEmpty())
                            {
                                continue;
                            }
                            else if (XMLReader.name().toString()  == "LastID")
                            {
                                lastId = XMLReader.readElementText().toULongLong();
                            }
                            else if (XMLReader.name().toString()  == "MaxDocumentCount")
                            {
                                maxDocumentCount = XMLReader.readElementText().toUInt();
                            }
                            else if (XMLReader.name().toString()  == "AZSCodes")
                            {
                                while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
                                {
                                    if (XMLReader.name().toString().isEmpty())
                                    {
                                        continue;
                                    }
                                    else if (XMLReader.name().toString()  == "AZSCode")
                                    {
                                        const auto tmpAZSCode =  XMLReader.readElementText();
                                        if (tmpAZSCode.isEmpty())
                                        {
                                            _errorString = "Value tag Root/Documents/AZSCodes/AZSCode cannot be empty";
                                            textStream << _errorString;

                                            return EXIT_CODE::XML_EMPTY;
                                        }
                                        documentsAZSCodes.append(QString("'%1'").arg(tmpAZSCode));
                                    }
                                    else
                                    {
                                        _errorString = "Undefine tag in XML (Root/Documents/AZSCodes" + XMLReader.name().toString() + ")";
                                        textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту

                                        return EXIT_CODE::XML_UNDEFINE_TOCKEN;
                                    }
                                }
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
                                            _errorString = "Value tag Root/Documents/DocumentsType/DocumentType cannot be empty";
                                            textStream << _errorString;

                                            return EXIT_CODE::XML_EMPTY;
                                        }

                                        documentsType.append(QString("'%1'").arg(documentType));
                                    }
                                    else
                                    {
                                        _errorString = "Undefine tag in XML (Root/Documents/DocumentsType" + XMLReader.name().toString() + ")";
                                        textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту

                                        return EXIT_CODE::XML_UNDEFINE_TOCKEN;
                                    }
                                }
                            }
                            else
                            {
                                _errorString = "Undefine tag in XML (Root/Documents" + XMLReader.name().toString() + ")";
                                textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту

                                return EXIT_CODE::XML_UNDEFINE_TOCKEN;
                            }
                        }
                    }
                    else if (XMLReader.name().toString()  == "Queries")
                    {
                        while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
                        {
                            //qDebug() << "Root/Queries" << XMLReader.name().toString();
                            if (XMLReader.name().toString().isEmpty())
                            {
                                continue;
                            }
                            else if (XMLReader.name().toString()  == "Query")
                            {
                                QueryInfo tmp;
                                while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
                                {
                                    //qDebug() << "Root/Queries/Query" << XMLReader.name().toString();
                                    if (XMLReader.name().toString().isEmpty())
                                    {
                                        continue;
                                    }
                                    else if (XMLReader.name().toString()  == "AZSCodes")
                                    {
                                        while ((XMLReader.readNext() != QXmlStreamReader::EndElement) && !XMLReader.atEnd() && !XMLReader.hasError())
                                        {
                                            if (XMLReader.name().toString().isEmpty())
                                            {
                                                continue;
                                            }
                                            else if (XMLReader.name().toString()  == "AZSCode")
                                            {
                                                const auto tmpAZSCode =  XMLReader.readElementText();
                                                if (tmpAZSCode.isEmpty())
                                                {
                                                    _errorString = "Value tag Root/Queries/Query/AZSCodes/AZSCode cannot be empty";
                                                    textStream << _errorString;

                                                    return EXIT_CODE::XML_EMPTY;
                                                }
                                               tmp.AZSCodes.append(QString("%1").arg(tmpAZSCode));
                                            }
                                            else
                                            {
                                                _errorString = "Undefine tag in XML (Root/Queries/Query/AZSCodes" + XMLReader.name().toString() + ")";
                                                textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту

                                                return EXIT_CODE::XML_UNDEFINE_TOCKEN;
                                            }
                                        }
                                    }
                                    else if (XMLReader.name().toString()  == "ID")
                                    {
                                        bool ok = false;
                                        tmp.id = XMLReader.readElementText().toULongLong(&ok);
                                        if (!ok)
                                        {
                                            _errorString =  QString("Incorrect value tag (Root/Queries/Query/%1). Value: %2. Value must be number.").arg(XMLReader.name().toString());
                                            textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту

                                            return EXIT_CODE::XML_PARSE_ERR;
                                        }

                                    }
                                    else if (XMLReader.name().toString()  == "SQL")
                                    {
                                        const auto queryText =  XMLReader.readElementText();
                                        if (AZSCode.isEmpty())
                                        {
                                            _errorString = "Value tag Root/Queries/Query/SQL cannot be empty";
                                            textStream << _errorString;

                                            return EXIT_CODE::XML_EMPTY;
                                        }
                                       tmp.queryText = queryText;
                                    }
                                    else
                                    {
                                        _errorString = "Undefine tag in XML (Root/Queries/Query" + XMLReader.name().toString() + ")";
                                        textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту

                                        return EXIT_CODE::XML_UNDEFINE_TOCKEN;
                                    }
                                }

                                queriesInfoList.push_back(tmp);
                            }
                            else
                            {
                                _errorString = "Undefine tag in XML (Root/Queries" + XMLReader.name().toString() + ")";
                                textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту

                                return EXIT_CODE::XML_UNDEFINE_TOCKEN;
                            }

                        }
                    }
                }
            }
            else
            {
                _errorString = "Undefine token in XML";
                textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту

                return EXIT_CODE::XML_UNDEFINE_TOCKEN;
            }
        }
    }

    if (XMLReader.hasError()) { //неудалось распарсить пришедшую XML
        _errorString = "Cannot parse XML query. Message: " + XMLReader.errorString();
        textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
        return EXIT_CODE::XML_PARSE_ERR;
    }

    //Обрабатываем результаты
    _db.transaction();
    QSqlQuery query(_db);
    query.setForwardOnly(true);

    //Queries
    addQueryToDB(_db, query, queriesInfoList);

    //Documents
    QString additionAZSCode;
    if (!documentsAZSCodes.isEmpty())
    {
        additionAZSCode = QString(" AND [AZSCode] IN (%1) ").arg(documentsAZSCodes.join(','));
    }

    QString additionDocumentTypes;
    if (!documentsType.isEmpty())
    {
        additionDocumentTypes = QString(" AND [DocumentType] IN (%1) ").arg(documentsType.join(','));
    }

    QString queryText =
        QString("SELECT TOP (%1) [ID], [AZSCode], [DocumentType], [Body] "
                "FROM [TopazDocuments] "
                "WHERE  [ID] > %2 %3 %4"
                "ORDER BY [ID]")
            .arg(maxDocumentCount)
            .arg(lastId)
            .arg(additionAZSCode)
            .arg(additionDocumentTypes);

    writeDebugLogFile(QString("QUERY TO %1>").arg(_db.connectionName()), queryText);

    if (!query.exec(queryText))
    {
        errorDBQuery(_db, query);
    }

    //формируем XML
    QString XMLStr;
    QXmlStreamWriter XMLWriter(&XMLStr);

    XMLWriter.setAutoFormatting(true);
    XMLWriter.writeStartDocument("1.0");
    XMLWriter.writeStartElement("Root");
    XMLWriter.writeTextElement("ProtocolVersion", "0.1");

    XMLWriter.writeStartElement("Documents");
    while (query.next())
    {
          XMLWriter.writeStartElement("Document");
          XMLWriter.writeTextElement("AZSCode", query.value("AZSCode").toString());
          XMLWriter.writeTextElement("DocumentType", query.value("DocumentType").toString());
          XMLWriter.writeTextElement("ID", query.value("ID").toString());
          XMLWriter.writeTextElement("Body", query.value("Body").toString());
          XMLWriter.writeEndElement(); // Document
          lastId = std::max(query.value("ID").toULongLong(), lastId);
    }
    XMLWriter.writeEndElement(); // Documents
    XMLWriter.writeTextElement("LastID", QString::number(lastId));

    XMLWriter.writeEndElement(); // Root
    XMLWriter.writeEndDocument(); // end XML

    DBCommit(_db);

    //отправляем ответ
    QTextStream answerTextStream(stdout);
    answerTextStream << XMLStr;

    writeDebugLogFile("ANSWER>", XMLStr);

    return 0;
}

void TDocTopaz::addQueryToDB(QSqlDatabase& db, QSqlQuery& query, TQueriesInfoList& queriesInfoList)
{
    Q_ASSERT(db.isOpen());

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

            if (!query.exec(queryText))
            {
                errorDBQuery(db, query);
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
                QString("INSERT INTO [QueriesToTopaz] ([AZSCode], [QueryID], [QueryText]) "
                                "VALUES ('%1', %2, '%3') ")
                    .arg(AZSCode)
                    .arg(queryInfo.id)
                    .arg(queryInfo.queryText);

            if (!query.exec(queryText))
            {
                errorDBQuery(db, query);
            }
        }
    }
}
