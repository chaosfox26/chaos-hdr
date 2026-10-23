#pragma once
#ifdef TRUEHDR_DEVELOPMENT
#include <epoxy/gl.h>
#include <QJsonArray>
#include <QJsonObject>
#include <array>
#include <chrono>

class OverlayMetrics {
    struct Query {GLuint id[2]{};bool pending=false;};
    std::array<Query,16> queries{};size_t index=0;
    Query* current=nullptr;bool enabled=false;
    std::chrono::steady_clock::time_point started;
    QJsonArray cpu,gpu;
public:
    void reset(bool active){enabled=active;cpu={};gpu={};current=nullptr;for(auto& q:queries)q.pending=false;}
    void begin(){
        if(!enabled)return;
        started=std::chrono::steady_clock::now();
        for(auto& q:queries)if(q.pending){
            GLuint ready=0;glGetQueryObjectuivEXT(q.id[1],GL_QUERY_RESULT_AVAILABLE_EXT,&ready);if(!ready)continue;
            GLuint64 low=0,high=0;glGetQueryObjectui64vEXT(q.id[0],GL_QUERY_RESULT_EXT,&low);glGetQueryObjectui64vEXT(q.id[1],GL_QUERY_RESULT_EXT,&high);
            if(gpu.size()<20000)gpu.append(double(high-low)/1e6);q.pending=false;
        }
        auto& q=queries[index++%queries.size()];current=q.pending?nullptr:&q;
        if(current){if(!q.id[0])glGenQueriesEXT(2,q.id);glQueryCounterEXT(q.id[0],GL_TIMESTAMP_EXT);}
    }
    void end(){
        if(!enabled)return;
        if(current){glQueryCounterEXT(current->id[1],GL_TIMESTAMP_EXT);current->pending=true;}
        if(cpu.size()<20000)cpu.append(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
    }
    QJsonObject result()const{return {{"cpu_ms",cpu},{"gpu_ms",gpu},{"scope","Overlay-only CPU submission and OpenGL timestamp span; measured separately, not input-to-display latency."}};}
    void release(){for(auto& q:queries)if(q.id[0]){glDeleteQueriesEXT(2,q.id);q={};}}
};
#endif
