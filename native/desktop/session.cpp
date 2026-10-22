#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusMessage>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <sys/stat.h>
#include "update_controller.hpp"

namespace {
QJsonObject read(const QString& path){QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(file.readAll()).object();}
QByteArray stamp(const QString& path){
    struct stat s{};if(::stat(path.toUtf8().constData(),&s))return {};
    return QByteArray::number(s.st_ino)+':'+QByteArray::number(s.st_mtim.tv_sec)+':'+QByteArray::number(s.st_mtim.tv_nsec)+':'+QByteArray::number(s.st_size);
}
}

class Session : public QObject {
    QString root,dataRoot,profile,livePath,statusPath,outputConfig,kwinExecutable;
    QFileSystemWatcher files;
    QDBusServiceWatcher kwin;
    QTimer watchdog,debounce;
    QTimer retry;
    QProcess helper;
    QHash<QString,QByteArray> stamps;
    QJsonObject state,previous;
    QLockFile lock;
    bool attachWanted=false,displayWanted=false,attaching=false,pinging=false;
    qint64 latestHeartbeat=0;
    QString targetDisplay;
    quint64 checks=0;
    int failures=0;

    bool connected(){return QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.KWin");}
    void publish(){
        state["watchdog_active"]=watchdog.isActive();state["watchdog_checks"]=double(checks);
        if(state==previous)return;
        previous=state;auto value=state;value["updated"]=QDateTime::currentMSecsSinceEpoch()/1000.;
        QSaveFile file(statusPath);if(file.open(QIODevice::WriteOnly)){file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);file.write(QJsonDocument(value).toJson());file.commit();}
    }
    void rearm(){
        for(const auto& path:stamps.keys())if(QFileInfo::exists(path)&&!files.files().contains(path))files.addPath(path);
    }
    void request(bool attach,bool display){
        attachWanted|=attach;displayWanted|=display;
        if(!debounce.isActive())debounce.start(50);
    }
    void observeLive(){
        auto live=read(livePath);if(live.isEmpty())return;
        latestHeartbeat=qint64(live["updated"].toDouble());
        if(live["stage"].toString()=="unloaded"){
            watchdog.stop();state["ready"]=false;request(true,false);publish();return;
        }
        bool active=live["watchdog_active"].toBool(false);
        if(active&&!watchdog.isActive())watchdog.start(2000);
        if(!active)watchdog.stop();
        auto error=live["error"].toString();
        if(!error.isEmpty())state["effect_error"]=error;else state.remove("effect_error");
        publish();
    }
    void changed(){
        rearm();
        for(auto it=stamps.begin();it!=stamps.end();++it){
            auto current=stamp(it.key());if(current==it.value())continue;it.value()=current;
            if(it.key()==livePath)observeLive();
            else if(it.key()==profile){
                auto target=read(profile)["target_display"].toString();
                if(target!=targetDisplay){targetDisplay=target;request(false,true);}
            }else if(it.key()==outputConfig)request(false,true);
            else{failures=0;request(true,false);}
        }
    }
    void drain(){
        if(helper.state()!=QProcess::NotRunning||!connected())return;
        if(!attachWanted&&!displayWanted)return;
        attaching=attachWanted;
        QStringList args;
        if(attaching){attachWanted=false;args={root+"/desktop.py","--attach-only"};}
        else{displayWanted=false;args={"-c","from desktop import refresh_display; refresh_display()"};}
        helper.setWorkingDirectory(root);helper.start(root+"/.venv/bin/python",args);
    }
    void checkHealth(){
        ++checks;


        if(QDateTime::currentSecsSinceEpoch()-latestHeartbeat>6&&!pinging){
            pinging=true;
            auto message=QDBusMessage::createMethodCall("org.kde.KWin","/TrueHDR","org.truehdr.Desktop","ping");
            auto pending=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message,2000),this);
            connect(pending,&QDBusPendingCallWatcher::finished,this,[this,pending]{
                QDBusPendingReply<bool> reply=*pending;pinging=false;
                if(reply.isError()){
                    state["health_error"]="The HDR component did not answer its health check.";
                    if(reply.error().name().contains("Unknown"))request(true,false);
                }else state.remove("health_error");
                pending->deleteLater();publish();
            });
        }else if(QDateTime::currentSecsSinceEpoch()-latestHeartbeat<=6)state.remove("health_error");
        publish();
    }
public:
    explicit Session(QString path):root(std::move(path)),
        dataRoot(qEnvironmentVariable("TRUEHDR_DESKTOP_TEST_ROOT",root)),
        profile(qEnvironmentVariable("TRUEHDR_PROFILE",root+"/profile.json")),
        livePath(dataRoot+"/output/desktop/live-status.json"),
        statusPath(dataRoot+"/output/desktop/controller-status.json"),
        outputConfig(qEnvironmentVariable("XDG_CONFIG_HOME",QDir::homePath()+"/.config")+"/kwinoutputconfig.json"),
        kwinExecutable(QStandardPaths::findExecutable("kwin_wayland")),
        kwin("org.kde.KWin",QDBusConnection::sessionBus(),QDBusServiceWatcher::WatchForOwnerChange,this),
        lock(dataRoot+"/output/desktop/session-controller.lock") {
        QDir().mkpath(dataRoot+"/output/desktop");lock.setStaleLockTime(0);
        if(!lock.tryLock()){QTimer::singleShot(0,qApp,&QCoreApplication::quit);return;}
        state={{"ready",false},{"route","desktop"},{"event_driven",true}};
        if(qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))new ReleaseController(root,this);
        targetDisplay=read(profile)["target_display"].toString();
        for(const auto& path:{livePath,profile,outputConfig,root+"/build-desktop/install-manifest.json",kwinExecutable}){
            if(path.isEmpty())continue;stamps[path]=stamp(path);
            auto parent=QFileInfo(path).absolutePath();
            if(QFileInfo::exists(parent)&&!files.directories().contains(parent))files.addPath(parent);
        }
        rearm();
        connect(&files,&QFileSystemWatcher::fileChanged,this,[this](const QString&){changed();});
        connect(&files,&QFileSystemWatcher::directoryChanged,this,[this](const QString&){changed();});
        debounce.setSingleShot(true);connect(&debounce,&QTimer::timeout,this,[this]{drain();});
        retry.setSingleShot(true);connect(&retry,&QTimer::timeout,this,[this]{request(true,false);});
        connect(&watchdog,&QTimer::timeout,this,[this]{checkHealth();});
        connect(&kwin,&QDBusServiceWatcher::serviceOwnerChanged,this,[this](const QString&,const QString&,const QString& owner){
            watchdog.stop();state["ready"]=false;publish();
            if(!owner.isEmpty()){failures=0;request(true,true);}
        });
        connect(&helper,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus exit){
            auto error=QString::fromUtf8(helper.readAllStandardError()).trimmed();
            auto result=QString::fromUtf8(helper.readAllStandardOutput()).trimmed();
            if(code==0&&exit==QProcess::NormalExit){
                state.remove("error");
                if(attaching){failures=0;retry.stop();state["ready"]=true;state["effect_id"]=result.section('\n',-1);observeLive();}
            }else{
                auto failure=error.isEmpty()?QString("The HDR controller operation failed."):error.right(1000);
                if(state["error"].toString()!=failure)qWarning().noquote()<<failure;
                state["error"]=failure;
                if(attaching){state["ready"]=false;if(++failures<=6)retry.start(qMin(30000,1000*(1<<failures)));}
            }
            publish();drain();
        });
        connect(&helper,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){state["error"]="Could not start the HDR setup helper.";publish();});
        publish();observeLive();request(true,true);
    }
};

int main(int argc,char** argv){
    QCoreApplication app(argc,argv);if(app.arguments().size()!=2)return 2;
    Session session(app.arguments()[1]);return app.exec();
}
