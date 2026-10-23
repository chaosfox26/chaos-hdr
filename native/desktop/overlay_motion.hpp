#pragma once
#include <algorithm>
#include <cmath>

namespace chaos_ui {
class Motion {
    double started=0, duration=0;
    float from=0, target=0;
public:
    float value(double now) const {
        if(duration<=0)return target;
        auto t=std::clamp((now-started)/duration,0.0,1.0);
        auto eased=1.0-std::pow(1.0-t,3.0);
        return float(from+(target-from)*eased);
    }
    void set(bool open,double now,bool reduced=false){
        from=value(now);target=open?1.f:0.f;started=now;
        duration=reduced?0:std::abs(target-from)*(open?.24:.20);
    }
    bool moving(double now)const{return duration>0&&now<started+duration;}
};
}
