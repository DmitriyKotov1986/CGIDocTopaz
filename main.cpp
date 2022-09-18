//Qt
#include <QCoreApplication>
#include <QSettings>
#include <iostream>

//My
#include "Common/common.h"
#include "tconfig.h"
#include "tdoctopaz.h"

using namespace CGIDocTopaz;

using namespace Common;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("CGIDocTopaz");
    QCoreApplication::setOrganizationName("OOO SA");
    QCoreApplication::setApplicationVersion(QString("Version:0.1a Build: %1 %2").arg(__DATE__).arg(__TIME__));

    setlocale(LC_CTYPE, ""); //настраиваем локаль

    QString configFileName = a.applicationDirPath() +"/CGIDocTopaz.ini";

    CGIDocTopaz::TConfig* cfg = TConfig::config(configFileName);

    if (cfg->isError()) {
        QString errorMsg = "Error load configuration: " + cfg->errorString();
        qCritical() << errorMsg;
        writeLogFile("Error load configuration", errorMsg);
        exit(EXIT_CODE::LOAD_CONFIG_ERR);
    }

    QString buf;
    QTextStream inputStream(stdin);
    while (1) {
        QString tmpStr = inputStream.readLine();
        if (tmpStr != "EOF") {
            buf += tmpStr + "\n";
        }
        else {
            break;
        }
    }

    CGIDocTopaz::TDocTopaz docTopaz(cfg);

    int res = docTopaz.run(buf); //обрабатываем пришедшие данные

    if (res != 0 ) {
        writeLogFile("Error parse XML: " + docTopaz.errorString(), buf);
    }
    else {
        qDebug() << "OK";
    }

    return res;
}
