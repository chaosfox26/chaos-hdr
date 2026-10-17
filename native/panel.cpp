#include <QApplication>
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
#include <QShortcut>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QDateTime>
#include <QLineEdit>
#include <QVBoxLayout>
#include <KGlobalAccel>
#include <QInputDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include "profiles.hpp"
#include <iostream>

#ifdef TRUEHDR_DEVELOPMENT
class Panel : public QWidget {
    QString path;
    QJsonObject config;
    QLabel* notice;
    QTimer debounce;
    QTimer displayTimer;
    QProcess displayProbe;
    QComboBox* profileChoice;
    QPushButton* deleteProfile;
    QCheckBox* enabledControl;
    QMap<QString,QSpinBox*> controlValues;
    bool dirty=false;
public:
    explicit Panel(QString profile) : path(std::move(profile)) {
        setWindowTitle("Chaos HDR");
        setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
        setFixedWidth(400);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(24, 22, 24, 22); layout->setSpacing(14);
        auto* heading = new QLabel("Chaos HDR");
        heading->setStyleSheet("font-size:27px;font-weight:700;"); layout->addWidget(heading);
        auto* subtitle = new QLabel("Global profile · SDR to HDR");
        subtitle->setStyleSheet("color:#a5adb9;"); layout->addWidget(subtitle);
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) config = QJsonDocument::fromJson(file.readAll()).object();
        if (config.isEmpty()) config = {{"version",1},{"enabled",true},{"peak_brightness",1500},
            {"middle_gray",50},{"contrast",0},{"saturation",0},{"debanding",true},{"monitor","ASUS PG27AQWP-W"}};
        if (!config.contains("sharpen_intensity")) config["sharpen_intensity"] = config["sharpness"].toInt(0);
        if (!config.contains("ignore_film_grain")) config["ignore_film_grain"] = 0;
        if (!config.contains("debanding")) config["debanding"] = true;
        config.remove("sharpness");
        profileChoice=new QComboBox;layout->addWidget(profileChoice);
        auto* profileButtons=new QHBoxLayout;auto* newProfile=new QPushButton("Save as…");deleteProfile=new QPushButton("Delete profile");
        profileButtons->addWidget(newProfile);profileButtons->addWidget(deleteProfile);layout->addLayout(profileButtons);
        connect(profileChoice,&QComboBox::activated,this,[this](int){
            save();QString error;
            if(profiles::Store(path).select(config,profileChoice->currentText(),error)&&profiles::write(path,config,error))refreshControls();
            else notice->setText(error);
            refreshProfiles();
        });
        connect(newProfile,&QPushButton::clicked,this,[this]{
            bool accepted;auto name=QInputDialog::getText(this,"Save profile","Profile name",QLineEdit::Normal,{},&accepted);
            if(!accepted)return;
            save();QString error;
            if(!profiles::Store(path).create(config,name,error)||!profiles::write(path,config,error))notice->setText(error);
            refreshProfiles();
        });
        connect(deleteProfile,&QPushButton::clicked,this,[this]{
            if(QMessageBox::question(this,"Delete profile","Delete "+profiles::active(config)+"? Global settings will be restored.")!=QMessageBox::Yes)return;
            debounce.stop();QString error;
            if(profiles::Store(path).remove(config,error)&&profiles::write(path,config,error)){dirty=false;refreshControls();}
            else notice->setText(error);
            refreshProfiles();
        });
        auto* enabled = new QCheckBox("Enable Chaos HDR");
        enabledControl=enabled;
        enabled->setChecked(config["enabled"].toBool(true)); layout->addWidget(enabled);
        connect(enabled, &QCheckBox::toggled, this, [this](bool x) { config["enabled"]=x; saveSoon(); });
        auto slider = [&](QString label, QString key, int low, int high, QString suffix) {
            auto* row = new QWidget; auto* vertical = new QVBoxLayout(row);
            vertical->setContentsMargins(0,0,0,0); vertical->setSpacing(5);
            auto* top = new QHBoxLayout; top->addWidget(new QLabel(label)); top->addStretch();
            auto* spin = new QSpinBox; spin->setRange(low, high); spin->setSuffix(suffix);
            controlValues[key]=spin;
            spin->setValue(config[key].toInt()); top->addWidget(spin); vertical->addLayout(top);
            auto* bar = new QSlider(Qt::Horizontal); bar->setRange(low,high); bar->setValue(spin->value());
            vertical->addWidget(bar); layout->addWidget(row);
            connect(bar,&QSlider::valueChanged,spin,&QSpinBox::setValue);
            connect(spin,&QSpinBox::valueChanged,bar,&QSlider::setValue);
            connect(spin,&QSpinBox::valueChanged,this,[this,key](int x) { config[key]=x; saveSoon(); });
            return spin;
        };
        auto* peakRow = new QHBoxLayout;
        peakRow->addWidget(new QLabel("Peak brightness · Auto")); peakRow->addStretch();
        auto* peak = new QLabel(QString::number(config["peak_brightness"].toInt(1500))+" nits");
        peakRow->addWidget(peak); layout->addLayout(peakRow);
        peak->setToolTip("Uses the HDR peak brightness configured in KDE Display Configuration.");
        slider("Middle greys", "middle_gray",10,100," nits");
        slider("Contrast", "contrast",-100,100," %");
        slider("Saturation", "saturation",-100,100," %");
        auto* sharpenHeading = new QLabel("Sharpen");
        sharpenHeading->setStyleSheet("font-size:17px;font-weight:600;"); layout->addWidget(sharpenHeading);
        slider("Intensity", "sharpen_intensity",0,100," %");
        slider("Ignore Film Grain", "ignore_film_grain",0,100," %");
        notice = new QLabel("Preview controls ready · game connection pending.");
        notice->setWordWrap(true); notice->setStyleSheet("color:#a5adb9;font-size:12px;"); layout->addWidget(notice);
        refreshProfiles();
        auto* buttons = new QHBoxLayout;
        auto* hide = new QPushButton("Done"); buttons->addWidget(hide);
        connect(hide,&QPushButton::clicked,this,&QWidget::hide);
        layout->addLayout(buttons);
        setStyleSheet("QWidget {background:#171a21;color:#eef0f5;font-size:14px;} QComboBox,QSpinBox,QPushButton {background:#252b36;border:1px solid #3b4452;border-radius:6px;padding:6px;} QSlider::groove:horizontal {height:6px;background:#333c48;border-radius:3px;} QSlider::handle:horizontal {background:#8dd45b;width:16px;margin:-5px 0;border-radius:8px;} QCheckBox::indicator {width:18px;height:18px;}");
        debounce.setSingleShot(true); debounce.setInterval(80);
        connect(&debounce,&QTimer::timeout,this,[this] { save(); });
        auto* close = new QShortcut(QKeySequence(Qt::Key_Escape),this);
        connect(close,&QShortcut::activated,this,&QWidget::hide);
        auto* toggle = new QAction("Show Chaos HDR controls",this); toggle->setObjectName("toggle-controls");
        auto keySequence = QKeySequence("Alt+Z");
        if (!KGlobalAccel::isGlobalShortcutAvailable(keySequence,"truehdr-linux"))
            keySequence = QKeySequence("Ctrl+Alt+H");
        if (KGlobalAccel::isGlobalShortcutAvailable(keySequence,"truehdr-linux"))
            KGlobalAccel::setGlobalShortcut(toggle,keySequence);
        connect(toggle,&QAction::triggered,this,[this] { if (isVisible()) this->hide(); else { show(); raise(); activateWindow(); } });
        auto shortcuts = KGlobalAccel::self()->shortcut(toggle);
        QString key = shortcuts.isEmpty() ? "No shortcut registered" : shortcuts.front().toString();
        layout->addWidget(new QLabel(key + " · show or hide controls"));
        std::cout << "Controls shortcut: " << key.toStdString() << std::endl;
        connect(qApp,&QApplication::aboutToQuit,this,[this] { save(); });
        connect(&displayProbe,&QProcess::finished,this,[this,peak](int code,QProcess::ExitStatus status) {
            if (code || status != QProcess::NormalExit) {
                notice->setText("Display settings unavailable · using saved brightness"); return;
            }
            auto displays = QJsonDocument::fromJson(displayProbe.readAllStandardOutput()).array();
            QJsonObject target;
            auto connector = config["target_display"].toString();
            for (const auto& value : displays) {
                auto display = value.toObject();
                if (connector.isEmpty() || display["connector"].toString() == connector) { target=display; break; }
            }
            if (target.isEmpty()) { notice->setText("Target HDR display unavailable · using saved brightness"); return; }
            int nits = target["peak_brightness"].toInt();
            if (nits != config["peak_brightness"].toInt() || connector.isEmpty()) {
                config["target_display"] = target["connector"];
                config["peak_brightness"] = nits;
                peak->setText(QString::number(nits)+" nits"); saveSoon();
            }
            if (nits < 400 || nits > 2000) notice->setText("Display peak is outside the engine's supported 400–2000 nits.");
        });
        connect(&displayTimer,&QTimer::timeout,this,[this] {
            if (displayProbe.state() == QProcess::NotRunning)
                displayProbe.start("python3",{QApplication::applicationDirPath()+"/../displays.py"});
        });
        displayTimer.setInterval(1000); displayTimer.start();
        displayProbe.start("python3",{QApplication::applicationDirPath()+"/../displays.py"});
    }
    void refreshProfiles(){
        QString error;auto names=profiles::Store(path).names(config,error);QSignalBlocker block(profileChoice);
        profileChoice->clear();profileChoice->addItems(names);profileChoice->setCurrentText(profiles::active(config));
        deleteProfile->setEnabled(profiles::active(config)!="Global");if(!error.isEmpty())notice->setText(error);
    }
    void refreshControls(){
        {QSignalBlocker block(enabledControl);enabledControl->setChecked(config["enabled"].toBool(true));}
        for(auto it=controlValues.begin();it!=controlValues.end();++it){
            QSignalBlocker block(it.value());it.value()->setValue(config[it.key()].toInt());

            for(auto* bar:it.value()->parentWidget()->findChildren<QSlider*>()){QSignalBlocker barBlock(bar);bar->setValue(it.value()->value());}
        }
        notice->setText("Profile applied · changes save automatically.");
    }
    void saveSoon() { dirty=true;debounce.start(); }
    void save() {
        debounce.stop();if(!dirty)return;QString error;
        if(profiles::Store(path).saveCurrent(config,error)&&profiles::write(path,config,error)){
            dirty=false;notice->setText("Saved · changes apply live.");
        }else notice->setText(error);
    }
    void closeEvent(QCloseEvent* event) override { save(); hide(); event->ignore(); }
};

#endif

int main(int argc,char** argv) {
    QApplication app(argc,argv); app.setApplicationName("truehdr-linux");
    app.setApplicationDisplayName("Chaos HDR"); app.setQuitOnLastWindowClosed(false);
    if(app.arguments().contains("--unregister-shortcut")){
        KGlobalAccel::cleanComponent("truehdr-linux");
        return 0;
    }
    QString profile = argc>1 ? QString::fromLocal8Bit(argv[1]) : "profile.json";
    if(app.arguments().contains("--overlay-service")){
        QAction toggle("Show Chaos HDR controls",&app);toggle.setObjectName("toggle-controls");
        KGlobalAccel::setGlobalShortcut(&toggle,QKeySequence("Alt+Z"));
        if(!KGlobalAccel::self()->shortcut(&toggle).contains(QKeySequence("Alt+Z"))){
            std::cerr<<"Alt+Z could not be registered"<<std::endl;return 1;
        }
        QObject::connect(&toggle,&QAction::triggered,&app,[profile]{
            QSaveFile file(QFileInfo(profile).absolutePath()+"/overlay.toggle");
            if(file.open(QIODevice::WriteOnly)){file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);file.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));file.commit();}
        });
        std::cout<<"Alt+Z routes to the in-game overlay; no separate controls window"<<std::endl;
        return app.exec();
    }
#ifndef TRUEHDR_DEVELOPMENT
    auto root=QFileInfo(QApplication::applicationDirPath()+"/..").absoluteFilePath();
    return QProcess::startDetached(root+"/.venv/bin/python",{root+"/desktop.py","--controls"})?0:1;
#else
    if(!app.arguments().contains("--preview-controls") && !app.arguments().contains("--test-screenshot")){
        auto root=QFileInfo(QApplication::applicationDirPath()+"/..").absoluteFilePath();
        return QProcess::startDetached(root+"/.venv/bin/python",{root+"/desktop.py","--controls"})?0:1;
    }
    Panel panel(profile);
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
