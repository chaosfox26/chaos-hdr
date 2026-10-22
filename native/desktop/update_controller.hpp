#pragma once
#include <QObject>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>

class ReleaseController : public QObject {
    QString root,folder;
    QProcess check;
    QTimer timer;
    QJsonObject read(const QString& path){QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(file.readAll()).object();}
    void schedule(){
        auto state=read(folder+"/status.json");auto last=state["last_attempt"].toDouble();
        if(last<=0||state["busy"].toBool()){timer.start(86400000);return;}
        auto delay=qMax(1000.,(last+86400-QDateTime::currentSecsSinceEpoch())*1000.);
        timer.start(int(qMin(delay,86400000.)));
    }
    void startCheck(){
        if(check.state()!=QProcess::NotRunning)return;
        check.setWorkingDirectory(root);check.start(root+"/.venv/bin/python",{root+"/updates.py","--check"});
    }
public:
    explicit ReleaseController(QString path,QObject* parent):QObject(parent),root(std::move(path)),folder(root+"/output/updates"){
        QDir().mkpath(folder);
        timer.setSingleShot(true);connect(&timer,&QTimer::timeout,this,[this]{startCheck();});
        connect(&check,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code){
            if(code)timer.start(86400000);else schedule();
        });
        connect(&check,&QProcess::errorOccurred,this,[this]{timer.start(86400000);});
        timer.start(10000);
    }
};
