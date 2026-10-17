#pragma once
#include "core/renderbackend.h"
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>

namespace desktop_metrics {
inline int64_t now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct Sample {int64_t committed,submitted,presented;};
struct State {
    bool enabled=true;
    bool traceMode=false;
    uint64_t serial=1,lastQueued=0;
    int64_t latestCommit=now();
    std::vector<Sample> samples;
    State(){samples.reserve(20000);}
    QJsonArray trace(int64_t origin)const{
        QJsonArray rows;
        for(const auto& s:samples)rows.append(QJsonArray{(s.committed-origin)/1e6,(s.submitted-origin)/1e6,(s.presented-origin)/1e6});
        return rows;
    }
    QJsonObject result()const{
        std::vector<double> intervals,latency,queue;


        for(size_t n=5;n<samples.size();++n){auto& s=samples[n];
            if(s.presented>=s.committed)latency.push_back((s.presented-s.committed)/1e6);
            if(s.presented>=s.submitted)queue.push_back((s.presented-s.submitted)/1e6);
            auto delta=s.presented-samples[n-1].presented;
            if(delta>0)intervals.push_back(delta/1e6);
        }
        auto stats=[](std::vector<double> values){
            if(values.empty())return QJsonObject{};
            double total=0;for(double v:values)total+=v;std::sort(values.begin(),values.end());
            return QJsonObject{{"mean_ms",total/values.size()},{"median_ms",values[values.size()/2]},
                {"p95_ms",values[std::min(values.size()-1,size_t(values.size()*.95))]},
                {"p99_ms",values[std::min(values.size()-1,size_t(values.size()*.99))]},
                {"max_ms",values.back()},{"samples",double(values.size())}};
        };
        auto frame=stats(intervals);double mean=frame["mean_ms"].toDouble();
        return {{"enabled",enabled},{"presented_frames",double(samples.size())},{"displayed_frame_interval",frame},
            {"displayed_fps",mean>0?1000/mean:0},{"kde_commit_to_presentation",stats(latency)},
            {"kde_prepaint_to_presentation",stats(queue)},
            {"scope","Actual KWin presentation feedback for distinct observed target-surface commits; excludes game input and rendering before KDE receives the commit. Not isolated HDR overhead."}};
    }
};
class Feedback final : public KWin::PresentationFeedback {
    std::weak_ptr<State> state;
    int64_t commit,submitted;
public:
    Feedback(const std::shared_ptr<State>& s):state(s),commit(s->latestCommit),submitted(now()){}
    void presented(KWin::OutputFrame*,std::chrono::nanoseconds timestamp,KWin::PresentationMode,KWin::PresentationFeedbackFlags)override{
        if(auto s=state.lock();s&&s->enabled&&s->samples.size()<20000)s->samples.push_back({commit,submitted,timestamp.count()});
    }
};
}
