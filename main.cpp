#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>
#include <QInputDialog>
#include <string>
#include <QSettings>


void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if(MainWindow::s_textEdit == 0)
    {
        QByteArray localMsg = msg.toLocal8Bit();
        switch (type) {
        case QtDebugMsg:
            fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
            break;
        case QtWarningMsg:
            fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
            break;
        case QtCriticalMsg:
            fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
            break;
        case QtFatalMsg:
            fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
            abort();
        }
    }
    else
    {
        MainWindow::s_textEdit->log_with_signal(msg);
        MainWindow::s_textEdit->update();
    }
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(myMessageOutput);
    QApplication a(argc, argv);

    QSettings setting("TexasSolver", "Setting");
    setting.beginGroup("solver");
    QString language_str = setting.value("language").toString();

    if(language_str.isEmpty()){
        QStringList languages;
        languages << "English" << QString::fromUtf8("简体中文");
        QString lang = QInputDialog::getItem(NULL,"select language","language",languages,0,false);

        if(lang == "English"){
            language_str = "EN";
        }else if(lang == QString::fromUtf8("简体中文")){
            language_str = "CN";
        }
        setting.setValue("language",language_str);
    }

    QTranslator trans;
    if(language_str == "EN"){
        trans.load(":/lang_en.qm");
    }else if(language_str == "CN"){
        trans.load(":/lang_cn.qm");
    }

    if (!language_str.isEmpty()) {
        a.installTranslator(&trans);
    }

    int dump_round = setting.value("dump_round").toInt();
    if(dump_round == 0){
        setting.setValue("dump_round",2);
    }

    MainWindow w;
    w.show();

    return a.exec();
}
