#pragma once
#include <QDialog>
#include <QDateTime>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFileSystemWatcher>
#include <QRegularExpression>

inline QJsonObject updateRead(const QString& path){
    QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(file.readAll()).object();
}
inline bool updateRequest(const QString& root,const QString& action){
    QStringList args{"--user","--collect","--no-block","--unit=chaos-hdr-"+QString(action=="check"?"release-check":"update"),
        "--property=WorkingDirectory="+root,root+"/.venv/bin/python",root+"/updates.py","--"+action};
    if(action=="check")args<<"--manual";
    QProcess launcher;launcher.start("systemd-run",args);
    return launcher.waitForFinished(3000)&&launcher.exitCode()==0;
}

class UpdateDialog : public QDialog {
    QString root;
    QString installedVersion;
    bool sessionKnown=false,sessionMatches=false;
    QFileSystemWatcher files;
    QLabel *summary,*details,*last,*desktop;
    QPushButton *check,*install,*rebuild;
    void refresh(){
        auto state=updateRead(root+"/output/updates/status.json");
        auto current=updateRead(root+"/output/updates/installed-release.json");
        if(current.isEmpty())current=updateRead(root+"/build-desktop/install-manifest.json")["release"].toObject();
        if(current.isEmpty())current=updateRead(root+"/release.json");
        auto available=state["available"].toObject()["tag"].toString();
        bool busy=state["busy"].toBool()||state["status"]=="checking";
        summary->setText(state["message"].toString("Check for new Chaos HDR releases."));
        details->setText(state["error"].toString());details->setVisible(!details->text().isEmpty());
        auto stamp=qint64(state["last_attempt"].toDouble());
        last->setText(QString("Installed: %1%2\nLast check: %3\nAutomatic checks: once every 24 hours").arg(current["tag"].toString("unknown"),
            current["prerelease"].toBool()?" (experimental channel)":" (stable channel)",
            stamp?QDateTime::fromSecsSinceEpoch(stamp).toLocalTime().toString("MMM d, h:mm AP"):"Not checked yet"));
        check->setEnabled(!busy);check->setText(state["status"]=="checking"?"Checking...":"Check for updates");
        install->setVisible(!available.isEmpty());install->setEnabled(!busy);install->setText("Install "+available);
        rebuild->setEnabled(!busy);
        auto manifest=updateRead(root+"/build-desktop/install-manifest.json");
        bool built=manifest["kwin_version"].toString()=="kwin "+installedVersion;
        desktop->setText(!sessionKnown?"Could not read the desktop version. Check the Chaos HDR service log before trying to rebuild.":
            !sessionMatches?"Your desktop session needs to catch up with the installed version. Sign out and back in, then rebuild Chaos HDR.":
            !built?"Your desktop was updated. Rebuild Chaos HDR for this version to restore HDR and Alt+Z.":
            "Chaos HDR matches this desktop. Rebuild here if HDR or Alt+Z stops working after a desktop update. No new release is required.");
    }
public:
    explicit UpdateDialog(QString path):root(std::move(path)){
        setWindowTitle("Chaos HDR - Updates & repair");setMinimumWidth(420);setMaximumWidth(540);
        auto layout=new QVBoxLayout(this);layout->setContentsMargins(24,24,24,24);layout->setSpacing(14);
        auto heading=new QLabel("Updates & repair");auto font=heading->font();font.setPointSize(font.pointSize()+6);font.setBold(true);heading->setFont(font);layout->addWidget(heading);
        summary=new QLabel;summary->setWordWrap(true);layout->addWidget(summary);
        details=new QLabel;details->setWordWrap(true);layout->addWidget(details);
        last=new QLabel;last->setWordWrap(true);layout->addWidget(last);
        auto row=new QHBoxLayout;check=new QPushButton("Check for updates");install=new QPushButton;row->addWidget(check);row->addWidget(install);layout->addLayout(row);
        auto line=new QFrame;line->setFrameShape(QFrame::HLine);layout->addWidget(line);
        auto repairHeading=new QLabel("Desktop compatibility");auto bold=repairHeading->font();bold.setBold(true);repairHeading->setFont(bold);layout->addWidget(repairHeading);
        desktop=new QLabel;desktop->setWordWrap(true);layout->addWidget(desktop);
        QProcess process;process.start("kwin_wayland",{"--version"});process.waitForFinished(3000);
        installedVersion=QString::fromUtf8(process.readAllStandardOutput()).trimmed().section(' ',-1);
        QDBusInterface kwin("org.kde.KWin","/KWin","org.kde.KWin");kwin.setTimeout(2000);
        QDBusReply<QString> support=kwin.call("supportInformation");
        auto match=QRegularExpression("KWin version:\\s*(\\S+)").match(support.value());
        sessionKnown=match.hasMatch()&&!installedVersion.isEmpty();sessionMatches=sessionKnown&&match.captured(1)==installedVersion;
        rebuild=new QPushButton("Rebuild for this desktop");layout->addWidget(rebuild);
        auto note=new QLabel("Saved profiles and allowed applications are preserved. Setup may ask for system authentication. It never restarts your desktop.");note->setWordWrap(true);layout->addWidget(note);
        auto done=new QPushButton("Close");layout->addWidget(done);
        connect(done,&QPushButton::clicked,this,&QDialog::close);
        connect(check,&QPushButton::clicked,this,[this]{if(updateRequest(root,"check")){check->setEnabled(false);summary->setText("Checking GitHub releases...");}
            else QMessageBox::information(this,"Could not start the check","Another check may be running. Wait a moment and try again.");});
        connect(install,&QPushButton::clicked,this,[this]{
            if(QMessageBox::question(this,"Install Chaos HDR update?","Download and install the newer release? HDR controls may close briefly while setup runs. Your saved settings are preserved.")!=QMessageBox::Yes)return;
            if(updateRequest(root,"install"))install->setEnabled(false);
            else QMessageBox::information(this,"Could not start setup","Another update may be running. Wait a moment and try again.");
        });
        connect(rebuild,&QPushButton::clicked,this,[this]{
            if(!sessionKnown){QMessageBox::information(this,"Desktop status unavailable","Could not confirm the running desktop version. Check the Chaos HDR service log, then try again.");return;}
            if(!sessionMatches){QMessageBox::information(this,"Sign in again first","Sign out and back in so the running desktop matches the installed version, then try Rebuild.");return;}
            if(QMessageBox::question(this,"Rebuild Chaos HDR?","Rebuild and restart only Chaos HDR? Your saved settings are preserved. Your desktop and other applications stay open.")!=QMessageBox::Yes)return;
            if(updateRequest(root,"rebuild"))rebuild->setEnabled(false);
            else QMessageBox::information(this,"Could not start setup","Another update may be running. Wait a moment and try again.");
        });
        QDir().mkpath(root+"/output/updates");files.addPath(root+"/output/updates");
        connect(&files,&QFileSystemWatcher::directoryChanged,this,[this]{refresh();});
        refresh();
    }
};
