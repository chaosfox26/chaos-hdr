#pragma once
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStringList>



namespace profiles {
inline QJsonObject picture(const QJsonObject& config) {
    QJsonObject result;
    for (auto key : {"enabled", "debanding", "middle_gray", "contrast", "saturation", "sharpen_intensity", "ignore_film_grain"})
        if (config.contains(key)) result[key] = config[key];
    result["hdr_enabled"]=config.contains("hdr_enabled")?config["hdr_enabled"]:QJsonValue(true);
    result["sharpen_enabled"]=config.contains("sharpen_enabled")?config["sharpen_enabled"]:QJsonValue(true);
    return result;
}
inline bool valid(const QJsonObject& value) {
    for (auto key : {"enabled", "debanding"}) if (!value[key].isBool()) return false;
    for (auto key : {"hdr_enabled", "sharpen_enabled"}) if (value.contains(key)&&!value[key].isBool()) return false;
    struct Range { const char* key; int low, high; };
    for (auto range : {Range{"middle_gray",10,100}, Range{"contrast",-100,100}, Range{"saturation",-100,100},
                       Range{"sharpen_intensity",0,100}, Range{"ignore_film_grain",0,100}}) {
        auto number = value[range.key];
        if (!number.isDouble() || number.toDouble() != number.toInt() || number.toInt() < range.low || number.toInt() > range.high) return false;
    }
    return true;
}
inline QString active(const QJsonObject& config) { return config["profile_name"].toString("Global"); }
inline bool write(const QString& path, const QJsonObject& value, QString& error) {
    QSaveFile file(path); auto bytes = QJsonDocument(value).toJson();
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner) || file.write(bytes) != bytes.size() || !file.commit()) {
        error = "Could not save profiles: " + file.errorString(); return false;
    }
    return true;
}
class Store {
    QString path,live,journal;
public:
    explicit Store(const QString& livePath) : path(QFileInfo(livePath).absolutePath()+"/profiles.json"),live(livePath),journal(livePath+".pending") {}
    bool recover(QString& error)const{
        QFile file(journal);if(!file.exists())return true;
        if(!file.open(QIODevice::ReadOnly)){error="Could not read pending settings.";return false;}
        auto saved=QJsonDocument::fromJson(file.readAll()).object();file.close();
        if(saved["version"].toInt()!=1||!valid(picture(saved["config"].toObject()))||!saved["presets"].isObject()){
            error="Pending settings are invalid; the saved file has been preserved.";return false;
        }
        auto presets=saved["presets"].toObject();
        if(!presets.contains("Global")){error="Pending settings are missing Global.";return false;}
        for(auto value:presets)if(!value.isObject()||!valid(value.toObject())){error="Pending profile settings are invalid.";return false;}
        if(!persist(presets,error)||!write(live,saved["config"].toObject(),error))return false;
        if(!QFile::remove(journal)){error="Could not finish saving settings.";return false;}
        return true;
    }
    bool commit(const QJsonObject& config,const QJsonObject& presets,QString& error)const{
        if(QFile::exists(journal)&&!recover(error))return false;
        if(!write(journal,{{"version",1},{"config",config},{"presets",presets}},error))return false;
        return recover(error);
    }
    bool read(const QJsonObject& config, QJsonObject& presets, QString& error) const {
        QFile file(path);
        if (!file.exists()) { presets = {{"Global", picture(config)}}; return true; }
        if (!file.open(QIODevice::ReadOnly)) { error="Could not read saved profiles."; return false; }
        QJsonParseError parse;
        auto document=QJsonDocument::fromJson(file.readAll(), &parse);
        if (parse.error!=QJsonParseError::NoError || !document.isObject() || document.object()["version"].toInt()!=1) {
            error="Saved profiles could not be read; your file has been preserved."; return false;
        }
        presets=document.object()["profiles"].toObject();
        if (!presets.contains("Global")) { error="Saved profiles are missing Global."; return false; }
        for (auto it=presets.begin();it!=presets.end();++it)
            if (!it.value().isObject() || !valid(it.value().toObject())) { error="A saved profile has invalid settings."; return false; }
        return true;
    }
    bool persist(const QJsonObject& presets, QString& error) const {
        return write(path, {{"version",1},{"profiles",presets}}, error);
    }
    QStringList names(const QJsonObject& config, QString& error) const {
        QJsonObject presets; if (!read(config,presets,error)) return {};
        auto list=presets.keys(); list.removeAll("Global"); list.sort(Qt::CaseInsensitive); list.prepend("Global"); return list;
    }
    bool saveCurrent(const QJsonObject& config, QString& error) const {
        QJsonObject presets; if (!read(config,presets,error)) return false;
        if (!valid(picture(config))) { error="Current settings are invalid."; return false; }
        auto name=active(config);
        if (!presets.contains(name)) { error="The selected profile no longer exists."; return false; }
        presets[name]=picture(config); return commit(config,presets,error);
    }
    bool create(QJsonObject& config, QString name, QString& error) const {
        name=name.trimmed();
        if (name.isEmpty() || name.size()>64) { error="Use a profile name from 1 to 64 characters."; return false; }
        for (auto c:name) if (!c.isPrint()) { error="Use a profile name without control characters."; return false; }
        QJsonObject presets; if (!read(config,presets,error)) return false;
        for (auto existing:presets.keys()) if (existing.compare(name,Qt::CaseInsensitive)==0) {
            error="That profile name already exists."; return false;
        }
        if (!valid(picture(config))) { error="Current settings are invalid."; return false; }
        presets[active(config)]=picture(config); presets[name]=picture(config);
        auto updated=config;updated["profile_name"]=name;
        if(!commit(updated,presets,error))return false;
        config=updated;return true;
    }
    bool select(QJsonObject& config, const QString& name, QString& error) const {
        QJsonObject presets; if (!read(config,presets,error)) return false;
        if (!presets.contains(name)) { error="That profile no longer exists."; return false; }
        if (!valid(picture(config))) { error="Current settings are invalid."; return false; }
        presets[active(config)]=picture(config);
        auto updated=config;auto selected=picture(presets[name].toObject());
        for(auto it=selected.begin();it!=selected.end();++it)updated[it.key()]=it.value();
        updated["profile_name"]=name;if(!commit(updated,presets,error))return false;
        config=updated;return true;
    }
    bool remove(QJsonObject& config, QString& error) const {
        auto name=active(config);
        if (name=="Global") { error="Global cannot be deleted."; return false; }
        QJsonObject presets; if (!read(config,presets,error)) return false;
        presets.remove(name);auto updated=config;auto global=picture(presets["Global"].toObject());
        for(auto it=global.begin();it!=global.end();++it)updated[it.key()]=it.value();
        updated["profile_name"]="Global";if(!commit(updated,presets,error))return false;
        config=updated;return true;
    }
};
}
