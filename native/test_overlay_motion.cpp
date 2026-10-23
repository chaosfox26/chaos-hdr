#include "desktop/overlay_motion.hpp"
#include <cassert>
#include <cmath>

int main(){
    chaos_ui::Motion motion;
    assert(motion.value(0)==0&&!motion.moving(0));
    motion.set(true,10);assert(motion.value(10)==0);
    float last=0;
    for(int i=0;i<=240;++i){
        auto value=motion.value(10+i*.001);
        assert(value>=last&&value>=0&&value<=1);last=value;
    }
    assert(motion.value(11)==1&&!motion.moving(11));
    motion.set(false,11);assert(motion.value(11)==1);
    last=1;
    for(int i=0;i<=200;++i){
        auto value=motion.value(11+i*.001);
        assert(value<=last&&value>=0&&value<=1);last=value;
    }
    assert(motion.value(12)==0&&!motion.moving(12));
    motion.set(true,12);float before=motion.value(12.08);
    motion.set(false,12.08);assert(std::abs(motion.value(12.08)-before)<.000001f);
    assert(motion.value(12.1)<before);
    before=motion.value(12.11);motion.set(true,12.11);
    assert(std::abs(motion.value(12.11)-before)<.000001f);
    assert(motion.value(12.12)>before&&motion.value(13)==1);
    for(int i=0;i<1000;++i){double now=20+i*.003;float at=motion.value(now);motion.set(i%2,now);assert(std::isfinite(motion.value(now)));assert(std::abs(motion.value(now)-at)<.000001f);}
    motion.set(false,30);assert(motion.value(31)==0&&!motion.moving(31));
    motion.set(false,32);assert(motion.value(32)==0&&!motion.moving(32));
}
