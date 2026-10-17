#include "profiles.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <cassert>
#include <iostream>

int main(int argc, char** argv) {
    QCoreApplication app(argc,argv);
    assert(argc==2);QDir().mkpath(QString::fromLocal8Bit(argv[1]));
    QTemporaryDir folder(QString::fromLocal8Bit(argv[1])+"/profiles-XXXXXX");assert(folder.isValid());
    QString live=folder.path()+"/profile.json", error;
    QJsonObject config{{"enabled",true},{"debanding",true},{"middle_gray",65},{"contrast",0},{"saturation",0},
                       {"sharpen_intensity",50},{"ignore_film_grain",0},{"peak_brightness",1500},{"target_display","DP-3"}};
    profiles::Store store(live);
    assert(store.saveCurrent(config,error));
    assert(!store.create(config,"global",error));
    assert(!store.create(config," \t ",error));
    assert(!store.create(config,QString(65,'x'),error));
    assert(!store.create(config,"bad\nname",error));
    assert(store.create(config,QString::fromUtf8("Night / 夜"),error));
    config["middle_gray"]=70;config["sharpen_intensity"]=20;assert(store.saveCurrent(config,error));
    assert(!store.create(config,QString::fromUtf8("night / 夜"),error));
    assert(store.create(config,"Second",error));
    config["contrast"]=12;

    config["target_display"]="HDMI-A-1";config["peak_brightness"]=1300;
    assert(store.select(config,"Global",error));
    assert(config["middle_gray"].toInt()==65 && config["sharpen_intensity"].toInt()==50 && config["contrast"].toInt()==0);
    assert(config["target_display"].toString()=="HDMI-A-1" && config["peak_brightness"].toInt()==1300);
    assert(!store.remove(config,error));
    assert(store.select(config,"Second",error));assert(config["contrast"].toInt()==12);
    assert(store.select(config,QString::fromUtf8("Night / 夜"),error));assert(config["sharpen_intensity"].toInt()==20);
    assert(store.remove(config,error));assert(profiles::active(config)=="Global");
    assert(config["middle_gray"].toInt()==65 && config["sharpen_intensity"].toInt()==50);
    assert(!store.select(config,"missing",error));
    auto invalid=config;invalid["middle_gray"]=65.5;assert(!store.saveCurrent(invalid,error));
    QFile saved(folder.path()+"/profiles.json");assert(saved.open(QIODevice::ReadOnly));
    auto presets=QJsonDocument::fromJson(saved.readAll()).object()["profiles"].toObject();saved.close();
    for(auto value:presets)assert(!value.toObject().contains("target_display")&&!value.toObject().contains("peak_brightness"));

    assert(saved.open(QIODevice::WriteOnly|QIODevice::Truncate));saved.write("{broken");saved.close();
    assert(!store.create(config,"New",error));assert(!store.saveCurrent(config,error));
    assert(saved.open(QIODevice::ReadOnly));assert(saved.readAll()=="{broken");
    std::cout<<"Profiles: switching, autosave, display independence, deletion, names and corrupt-file preservation passed\n";
}
