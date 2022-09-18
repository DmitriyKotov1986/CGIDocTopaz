#include "tdoctopaz.h"

//QT
#include <QSqlError>
#include <QSqlQuery>
#include <QXmlStreamReader>

//My
#include "Common/common.h"

using namespace CGIDocTopaz;

using namespace Common;

TDocTopaz::TDocTopaz(TConfig* cfg)
{
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

    if (XMLText.isEmpty()) {
        return EXIT_CODE::XML_EMPTY;
    }

    if (!_db.open()) {
        _errorString = "Cannot connet to DB. Error: " + _db.lastError().text();
        return EXIT_CODE::SQL_NOT_OPEN_DB;
    }

    QTextStream textStream(stdout);

    //парсим XML
    QXmlStreamReader XMLReader(XMLText);
    QString AZSCode = "n/a";
    QString clientVersion = "n/a";
    QString protocolVersion = "n/a";
    qulonglong lastId = 0;
    quint32 maxDocumentCount = 1;

    while ((!XMLReader.atEnd()) && (!XMLReader.hasError())) {
        QXmlStreamReader::TokenType token = XMLReader.readNext();
        if (token == QXmlStreamReader::StartDocument) continue;
        else if (token == QXmlStreamReader::EndDocument) break;
        else if (token == QXmlStreamReader::StartElement) {
            //qDebug() << XMLReader.name().toString();
            if (XMLReader.name().toString()  == "Root") {
                while ((XMLReader.readNext() != QXmlStreamReader::EndElement) || XMLReader.atEnd() || XMLReader.hasError()) {
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
                    else if (XMLReader.name().toString()  == "LastID")
                    {
                        lastId = XMLReader.readElementText().toULongLong();
                    }
                    else if (XMLReader.name().toString()  == "MaxDocumentCount")
                    {
                        maxDocumentCount = XMLReader.readElementText().toUInt();
                    }
                    else {
                        _errorString = "Undefine tag in XML (Root/" + XMLReader.name().toString() + ")";
                        textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
                        return EXIT_CODE::XML_UNDEFINE_TOCKEN;
                    }
                }
            }
        }
        else {
            _errorString = "Undefine token in XML";
            textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
            return EXIT_CODE::XML_UNDEFINE_TOCKEN;
        }
    }

    if (XMLReader.hasError()) { //неудалось распарсить пришедшую XML
        _errorString = "Cannot parse XML query. Message: " + XMLReader.errorString();
        textStream << _errorString; //выводим сообщение об ошибке в cin для отправки клиенту
        return EXIT_CODE::XML_PARSE_ERR;
    }

    QSqlQuery query(_db);
    query.setForwardOnly(true);
    _db.transaction();

    QString queryText = QString("SELECT TOP (%1) [ID], [AZSCode], [DocumentType], [Body] "
                                "FROM [TopazDocuments] "
                                "WHERE  [ID] > %2 "
                                "ORDER BY [ID]")
                                .arg(maxDocumentCount).arg(lastId);

    if (!query.exec(queryText))
    {
        Common::errorDBQuery(_db, query);
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
          XMLWriter.writeTextElement("Body", query.value("Body").toString());
          XMLWriter.writeEndElement(); // Document
          lastId = std::max(query.value("ID").toULongLong(), lastId);
    }
    XMLWriter.writeEndElement(); // Documents
    XMLWriter.writeTextElement("LastID", QString::number(lastId));
    XMLWriter.writeEndElement(); // Root
    XMLWriter.writeEndDocument(); // end XML

    //отправляем ответ
    textStream << XMLStr;

    writeDebugLogFile("ANSWER>", XMLStr);

    return 0;
}
