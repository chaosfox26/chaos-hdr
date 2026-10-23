#pragma once
#include <algorithm>
#include <cmath>
#include <QJsonObject>

struct PictureSettings {
    int peak=1500,middle=50,contrast=0,saturation=0;
    bool debanding=true;
    static int supportedPeak(double peak){return std::isfinite(peak)&&peak>=400&&peak<=2000?int(std::round(peak)):0;}
    bool operator==(const PictureSettings&)const=default;
    static PictureSettings from(const QJsonObject& config,int peak){
        return {std::clamp(peak,400,2000),std::clamp(config["middle_gray"].toInt(50),10,100),
                std::clamp(config["contrast"].toInt(),-100,100),std::clamp(config["saturation"].toInt(),-100,100),
                config["debanding"].toBool(true)};
    }
};
