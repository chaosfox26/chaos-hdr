#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <KSystemClipboard>
#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QProcess>
#include <QJsonArray>
#include <QSaveFile>
#include <QSpinBox>
#include <QTimer>
#include <QDateTime>
#include <QLineEdit>
#include <QVBoxLayout>
#include <KGlobalAccel>
#include <QInputDialog>
#include <QMessageBox>
#include "profiles.hpp"
#include <iostream>
#include <QDir>
#include "update_dialog.hpp"
#include "accessible_controls.hpp"

int main(int argc,char** argv) {
    QApplication app(argc,argv); app.setApplicationName("truehdr-linux");
    app.setDesktopFileName("truehdr-linux");
    app.setApplicationDisplayName("Chaos HDR"); app.setQuitOnLastWindowClosed(false);
    if(app.arguments().contains("--unregister-shortcut")){
        KGlobalAccel::cleanComponent("truehdr-linux");
        return 0;
    }
    QString profile = argc>1 ? QString::fromLocal8Bit(argv[1]) : "profile.json";
    auto root=QFileInfo(profile).absolutePath();
    app.setApplicationVersion(updateRead(root+"/release.json")["tag"].toString());
    if(app.arguments().contains("--clipboard-read")){
        auto* clipboard=KSystemClipboard::instance();
        QTimer::singleShot(150,&app,[&]{auto text=clipboard->text(QClipboard::Clipboard);if(text.toUtf8().size()<=4096)std::cout<<text.toUtf8().constData();QTimer::singleShot(0,&app,&QCoreApplication::quit);});return app.exec();
    }
    if(app.arguments().contains("--clipboard-write")){
        QFile input;if(!input.open(stdin,QIODevice::ReadOnly))return 2;auto text=QString::fromUtf8(input.read(4097));if(text.toUtf8().size()>4096)return 2;
        auto* clipboard=KSystemClipboard::instance();bool installed=false;
        QTimer::singleShot(100,&app,[&]{auto mime=new QMimeData;mime->setText(text);clipboard->setMimeData(mime,QClipboard::Clipboard);installed=true;});
        QObject::connect(clipboard,&KSystemClipboard::changed,&app,[&](QClipboard::Mode mode){if(installed&&mode==QClipboard::Clipboard&&!clipboard->ownsClipboard())app.quit();});
        return app.exec();
    }
    if(app.arguments().contains("--accessible")){app.setQuitOnLastWindowClosed(true);AccessibleControls controls(profile);controls.show();return app.exec();}
    UpdateDialog* recovery=nullptr;
    auto showRecovery=[&]{if(!recovery)recovery=new UpdateDialog(root);recovery->show();recovery->raise();recovery->activateWindow();};
    if(app.arguments().contains("--updates")){app.setQuitOnLastWindowClosed(true);showRecovery();return app.exec();}
    if(app.arguments().contains("--overlay-service")){
        QAction toggle("Show Chaos HDR controls",&app);toggle.setObjectName("toggle-controls");
        KGlobalAccel::setGlobalShortcut(&toggle,QKeySequence("Alt+Z"));
        if(!KGlobalAccel::self()->shortcut(&toggle).contains(QKeySequence("Alt+Z"))){
            std::cerr<<"Alt+Z could not be registered"<<std::endl;return 1;
        }
        QObject::connect(&toggle,&QAction::triggered,&app,[profile,&showRecovery]{
            QDBusInterface effect("org.kde.KWin","/TrueHDR","org.truehdr.Desktop");effect.setTimeout(1000);
            QDBusReply<bool> ready=effect.call("ping");
            if(!ready.isValid()||!ready.value()){showRecovery();return;}
            QSaveFile file(QFileInfo(profile).absolutePath()+"/overlay.toggle");
            if(file.open(QIODevice::WriteOnly)){file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);file.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));file.commit();}
        });
        std::cout<<"Alt+Z routes to the in-game overlay; no separate controls window"<<std::endl;
        return app.exec();
    }
#ifndef TRUEHDR_DEVELOPMENT
    QDBusInterface effect("org.kde.KWin","/TrueHDR","org.truehdr.Desktop");effect.setTimeout(1000);
    QDBusReply<bool> ready=effect.call("ping");
    if(!ready.isValid()||!ready.value()){app.setQuitOnLastWindowClosed(true);showRecovery();return app.exec();}
    return QProcess::startDetached(root+"/.venv/bin/python",{root+"/desktop.py","--controls"})?0:1;
#else
    if(!app.arguments().contains("--preview-controls") && !app.arguments().contains("--test-screenshot")){
        return QProcess::startDetached(root+"/.venv/bin/python",{root+"/desktop.py","--controls"})?0:1;
    }
    AccessibleControls panel(profile);
    if (!app.arguments().contains("--hidden")) panel.show();
    if (app.arguments().contains("--test-screenshot")) {
        QTimer::singleShot(300,[&] {
            panel.grab().save(QFileInfo(profile).absolutePath()+"/output/control-panel.png");
            app.quit();
        });
    }
    return app.exec();
#endif
}
