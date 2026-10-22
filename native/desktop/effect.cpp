#include <epoxy/gl.h>
#include "gpu.hpp"
#include "presentation_metrics.hpp"
#include "../profiles.hpp"
#include "effect/effect.h"
#include "effect/effecthandler.h"
#include "effect/effectwindow.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "core/backendoutput.h"
#include "opengl/gltexture.h"
#include "opengl/glframebuffer.h"
#include "opengl/glshadermanager.h"
#include "opengl/glshader.h"
#include "window.h"
#include "scene/surfaceitem.h"
#include "scene/surfaceitem_wayland.h"
#include "wayland/surface.h"
#include "input_event.h"
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include <QTimer>
#include <QKeyEvent>
#include <QDateTime>
#include <QDir>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QElapsedTimer>
#include <QPointer>
#include <QJsonArray>
#include <QImage>
#include <QProcess>
#include <QFileSystemWatcher>
#include <cmath>
#include <chrono>
#include <array>

using namespace KWin;

class TrueHDRDesktop : public Effect {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.truehdr.Desktop")
    QString root=QString::fromUtf8(TRUEHDR_ROOT),profile;
    QJsonObject config;QString error;QTimer refresh;QFileSystemWatcher settingsWatch;
    QJsonObject releaseStatus;
    std::unique_ptr<DesktopGPU> gpu;
    std::unique_ptr<GLTexture> source,output;
    std::unique_ptr<GLFramebuffer> inputFbo;
    std::unique_ptr<GLTexture> uiTexture;
    std::unique_ptr<GLFramebuffer> uiFbo;
    QJsonObject colorAudit;
    QJsonObject bufferAudit;
    GraphicsBufferRef inFlightBuffer;
    std::shared_ptr<SyncReleasePoint> inFlightRelease;
    bool directActive=false,directDisabled=false,preferDirect=true;
    QString directFallback;
    std::shared_ptr<desktop_metrics::State> presentation;
    QPointer<SurfaceInterface> observedSurface;
    QMetaObject::Connection commitConnection;
    QSize dimensions;bool failed=false,renderingInput=false,visible=false;
    ImGuiContext* ui{};char name[256]{};
    uint64_t frames=0;double cpuTotal=0;QElapsedTimer statusTimer;
    struct FrameCost {int64_t started,finished;double convertMs,gpuPrepare,gpuEngine,gpuTotal;uint64_t gpuSamples;};
    std::vector<FrameCost> frameCosts;
    bool frameTraceEnabled=false;
    bool testOcclusionEnabled=false;uint64_t testOcclusionFrames=0,testOcclusionFailures=0;
    QJsonObject testOcclusionAudit;
    RectF panelGeometry;
    QPointer<LogicalOutput> controlsScreen;
    QJsonArray controlRects;
    QJsonObject inputAudit;
    struct Timing {GLuint queries[4]{};bool pending=false;};
    std::array<Timing,16> timings{};size_t timingIndex=0;
    bool timingEnabled=false;uint64_t timingSamples=0;double gpuTotals[3]{};QJsonObject gpuAudit;
    qint64 toggleStamp=0;
    std::chrono::steady_clock::time_point previous=std::chrono::steady_clock::now();
    QString targetClass="ffxiv";
    QPointer<EffectWindow> selectedWindow;
    QPointer<EffectWindow> candidateWindow;
    QStringList rememberedClasses;
    bool fixtureCapture=false,legacyAllowed=false;
    bool applicationWindow(EffectWindow* w)const{
        return w&&(w->isNormalWindow()||w->isDialog()||w->isUtility()||w->isToolbar());
    }
    bool watchdogActive()const{return selectedWindow&&config["enabled"].toBool(true);}
    void updateActivity(){
        if(visible||watchdogActive()){
            int interval=visible?250:1000;
            if(!refresh.isActive()||refresh.interval()!=interval)refresh.start(interval);
        }else refresh.stop();
    }
    const char* phase()const{
        return !config["enabled"].toBool(true)?"disabled":!selectedWindow?"waiting-for-game":
            !directFallback.isEmpty()&&!directActive?"unsupported-window":gpu?"filtering":"waiting-for-frame";
    }
    void saveTargets(){
        QJsonArray classes;for(const auto& value:rememberedClasses)classes.append(value);
        QString e;if(!profiles::write(root+"/window-targets.json",{{"version",1},{"classes",classes},{"legacy_ffxiv",legacyAllowed}},e))error=e;
    }
    void resetWindow(){
        effects->makeOpenGLContextCurrent();release();failed=false;directDisabled=false;
        directFallback.clear();colorAudit={};bufferAudit={};gpuAudit={};
        timingSamples=0;for(auto& total:gpuTotals)total=0;
        disconnect(commitConnection);observedSurface=nullptr;
        if(presentation)presentation=std::make_shared<desktop_metrics::State>();
    }
    bool knownWindow(EffectWindow* w)const{
        return applicationWindow(w)&&!w->windowClass().trimmed().isEmpty()&&(rememberedClasses.contains(w->windowClass())
            ||(legacyAllowed&&w->windowClass().contains(targetClass,Qt::CaseInsensitive)));
    }
    bool selectWindow(EffectWindow* w,bool remember){
        if(!applicationWindow(w))return false;
        if(selectedWindow!=w){resetWindow();selectedWindow=w;}
        if(remember&&!w->windowClass().trimmed().isEmpty()&&!rememberedClasses.contains(w->windowClass())){
            rememberedClasses.append(w->windowClass());saveTargets();
        }
        updateActivity();status(phase());effects->addRepaintFull();return true;
    }
    void read(){
        QFile file(profile);if(file.open(QIODevice::ReadOnly)){auto obj=QJsonDocument::fromJson(file.readAll()).object();if(!obj.isEmpty())config=obj;}
    }
    void save(){QString e;if(profiles::Store(profile).saveCurrent(config,e)&&profiles::write(profile,config,e))error.clear();else error=e;updateActivity();status(phase());effects->addRepaintFull();}
    bool target(EffectWindow* w)const{
        return w&&w==selectedWindow;
    }
    void release(){inputFbo.reset();source.reset();output.reset();gpu.reset();inFlightBuffer.reset();inFlightRelease.reset();dimensions={};directActive=false;}
#ifdef TRUEHDR_DEVELOPMENT
    static constexpr bool development=true;
#else
    static constexpr bool development=false;
#endif
    void status(const char* stage){
        QDir().mkpath(root+"/output/desktop");QString e;
        if constexpr(!development){
            profiles::write(root+"/output/desktop/live-status.json",{{"stage",stage},{"updated",QDateTime::currentSecsSinceEpoch()},
                {"watchdog_active",watchdogActive()},{"enabled",config["enabled"]},{"overlay_visible",visible},
                {"direct_input",directActive},{"error",error}},e);
            return;
        }
        profiles::write(root+"/output/desktop/live-status.json",{{"stage",stage},{"pid",QCoreApplication::applicationPid()},
            {"updated",QDateTime::currentSecsSinceEpoch()},{"frames",double(frames)},{"target",selectedWindow?selectedWindow->windowClass():QString()},
            {"selection","KDE window identity; no executable path"},{"fast_path_required",preferDirect&&!fixtureCapture},
            {"target_window_type",selectedWindow?selectedWindow->windowTypeInt():-1},
            {"session_only_target",selectedWindow&&selectedWindow->windowClass().trimmed().isEmpty()},
            {"watchdog_active",watchdogActive()},{"settings_monitor","file-notifications"},
            {"enabled",config["enabled"]},{"overlay_visible",visible},{"error",error},{"width",dimensions.width()},{"height",dimensions.height()},
            {"color_pipeline",colorAudit},
            {"source_buffer",bufferAudit},{"direct_input",directActive},{"direct_fallback",directFallback},
            {"input",inputAudit},{"controls",controlRects},
            {"gpu_timing",gpuAudit},
            {"test_occlusion",testOcclusionAudit},
            {"presentation_timing",presentation?(presentation->traceMode?QJsonObject{{"enabled",presentation->enabled},{"trace_mode",true},{"presented_frames",double(presentation->samples.size())}}:presentation->result()):QJsonObject{}},
            {"cpu_submit_mean_ms",frames?cpuTotal/frames:0},{"transport",!gpu||!config["enabled"].toBool(true)?"unfiltered":directActive?"submitted DMA-BUF directly into Vulkan TrueHDR":"OpenGL capture diagnostic"}},e);
    }
    void uiContext(){
        if(ui){ImGui::SetCurrentContext(ui);return;}
        ui=ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
        io.FontGlobalScale=1.15f;ImGui::StyleColorsDark();
        if(!ImGui_ImplOpenGL3_Init("#version 300 es"))throw std::runtime_error("Initialize desktop controls");
    }
    Timing* beginTiming(){
        if(!timingEnabled)return nullptr;
        for(auto& sample:timings)if(sample.pending){
            GLuint available=0;glGetQueryObjectuivEXT(sample.queries[3],GL_QUERY_RESULT_AVAILABLE_EXT,&available);
            if(!available)continue;
            GLuint64 ticks[4]{};for(int n=0;n<4;++n)glGetQueryObjectui64vEXT(sample.queries[n],GL_QUERY_RESULT_EXT,&ticks[n]);
            for(int n=0;n<3;++n)gpuTotals[n]+=(ticks[n+1]-ticks[n])/1e6;
            ++timingSamples;sample.pending=false;
        }
        if(timingSamples)gpuAudit={{"samples",double(timingSamples)},{"capture_ms",gpuTotals[0]/timingSamples},
            {"gl_handoff_segment_ms",gpuTotals[1]/timingSamples},{"draw_ms",gpuTotals[2]/timingSamples},
            {"gl_context_span_ms",(gpuTotals[0]+gpuTotals[1]+gpuTotals[2])/timingSamples},
            {"scope","OpenGL-context timestamps may exclude cross-API waits; not total processing, added overhead, or input latency"}};
        auto& sample=timings[timingIndex++%timings.size()];if(sample.pending)return nullptr;
        if(!sample.queries[0])glGenQueriesEXT(4,sample.queries);
        glQueryCounterEXT(sample.queries[0],GL_TIMESTAMP_EXT);return &sample;
    }
    void controls(){
        controlRects={};
        auto item=[&](const char* key){if constexpr(!development)return;auto low=ImGui::GetItemRectMin(),high=ImGui::GetItemRectMax();controlRects.append(QJsonObject{{"key",key},{"x",low.x},{"y",low.y},{"width",high.x-low.x},{"height",high.y-low.y}});};
        profiles::Store store(profile);QString e;auto names=store.names(config,e);
        ImGui::SetNextWindowPos({24,24},ImGuiCond_Always);ImGui::SetNextWindowSize({390,0},ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints({390,0},{390,std::max(120.f,ImGui::GetIO().DisplaySize.y-48)});
        if(ImGui::Begin("Chaos HDR",nullptr,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize)){
            auto caption=candidateWindow?candidateWindow->caption().toUtf8():QByteArray("Choose a running application");
            ImGui::TextUnformatted("Application");
            ImGui::SetNextItemWidth(-1);
            if(ImGui::BeginCombo("##application",caption.constData())){
                int index=0;
                for(auto* w:effects->stackingOrder())if(applicationWindow(w)){
                    ImGui::PushID(index++);
                    if(ImGui::Selectable(w->caption().toUtf8().constData(),w==candidateWindow)){
                        candidateWindow=w;if(knownWindow(w))selectWindow(w,false);
                    }
                    item(("application_option_"+w->windowClass()).toUtf8().constData());
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",w->windowClass().toUtf8().constData());
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }item("application");
            if(candidateWindow){
                if(knownWindow(candidateWindow)||candidateWindow==selectedWindow){
                    ImGui::TextDisabled(candidateWindow->windowClass().trimmed().isEmpty()?"Allowed for this session":"Allowed application");
                    if(ImGui::Button("Remove application")){
                        rememberedClasses.removeAll(candidateWindow->windowClass());
                        if(candidateWindow->windowClass().contains(targetClass,Qt::CaseInsensitive))legacyAllowed=false;
                        saveTargets();if(selectedWindow&&selectedWindow->windowClass()==candidateWindow->windowClass()){resetWindow();selectedWindow=nullptr;}
                        updateActivity();status(phase());effects->addRepaintFull();
                    }item("remove_application");
                }else{
                    ImGui::TextDisabled("Not allowed; its image is unchanged");
                    if(ImGui::Button("Allow application"))selectWindow(candidateWindow,true);
                    item("allow_application");
                }
            }
            if(selectedWindow&&candidateWindow!=selectedWindow)ImGui::TextWrapped("Adjusting: %s",selectedWindow->caption().toUtf8().constData());
            if(candidateWindow==selectedWindow&&!directFallback.isEmpty()){
                ImGui::TextWrapped("Fast path unavailable. This window is unchanged.");
                ImGui::TextWrapped("%s",directFallback.toUtf8().constData());
            }
            ImGui::Separator();
            auto selected=profiles::active(config).toUtf8();
            if(ImGui::BeginCombo("Profile",selected.constData())){
                for(auto preset:names)if(ImGui::Selectable(preset.toUtf8().constData(),preset==profiles::active(config)))
                    if(store.select(config,preset,error))save();
                ImGui::EndCombo();
            }
            if(ImGui::Button("Save as...")){name[0]=0;ImGui::OpenPopup("Save profile");}
            ImGui::SameLine();ImGui::BeginDisabled(profiles::active(config)=="Global");
            if(ImGui::Button("Delete"))ImGui::OpenPopup("Delete profile?");ImGui::EndDisabled();
            if(ImGui::BeginPopupModal("Save profile",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
                if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();
                bool enter=ImGui::InputText("Name",name,sizeof(name),ImGuiInputTextFlags_EnterReturnsTrue);
                if(ImGui::Button("Save")||enter)if(store.create(config,QString::fromUtf8(name),error)){save();ImGui::CloseCurrentPopup();}
                ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
                if(!error.isEmpty())ImGui::TextWrapped("%s",error.toUtf8().constData());ImGui::EndPopup();
            }
            if(ImGui::BeginPopupModal("Delete profile?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
                ImGui::TextUnformatted("Delete this profile and restore Global?");
                if(ImGui::Button("Delete"))if(store.remove(config,error)){save();ImGui::CloseCurrentPopup();}
                ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
            }
            bool changed=false;bool enabled=config["enabled"].toBool(true);
            if(ImGui::Checkbox("Enable Chaos HDR",&enabled)){config["enabled"]=enabled;changed=true;}item("enabled");
            ImGui::Text("Peak brightness (Auto): %d nits",config["peak_brightness"].toInt(1500));
            auto slider=[&](const char* label,const char* key,int low,int high,int fallback,const char* format){
                int value=config[key].toInt(fallback);ImGui::TextUnformatted(label);ImGui::SetNextItemWidth(-1);
                auto id=std::string("##")+key;if(ImGui::SliderInt(id.c_str(),&value,low,high,format)){config[key]=value;changed=true;}
                item(key);
            };
            slider("Middle greys","middle_gray",10,100,50,"%d nits");slider("Contrast","contrast",-100,100,0,"%d%%");slider("Saturation","saturation",-100,100,0,"%d%%");
            ImGui::SeparatorText("Sharpen");slider("Intensity","sharpen_intensity",0,100,0,"%d%%");slider("Ignore Film Grain","ignore_film_grain",0,100,0,"%d%%");
            if(!error.isEmpty())ImGui::TextWrapped("%s",error.toUtf8().constData());
            ImGui::TextDisabled("Saved automatically");if(ImGui::Button("Done",{-1,0}))setVisible(false);item("done");
            ImGui::SeparatorText("Updates");
            ImGui::TextWrapped("%s",releaseStatus["message"].toString("Daily release checks and desktop repair").toUtf8().constData());
            if(ImGui::Button("Updates & repair...",{-1,0})){
                setVisible(false);QProcess::startDetached(root+"/build/truehdr-panel",{profile,"--updates"});
            }item("updates");
            if(changed)save();
        }ImGui::End();
    }
    void setVisible(bool on){
        if(visible==on)return;
        if(on){
            if(effects->activeFullScreenEffect()&&effects->activeFullScreenEffect()!=this)return;
            controlsScreen=nullptr;
            auto active=effects->activeWindow();
            if(applicationWindow(active))candidateWindow=active;
            else candidateWindow=selectedWindow;
            if(knownWindow(active))selectWindow(active,false);
            if(target(active))controlsScreen=active->screen();
            if(!controlsScreen)for(auto* w:effects->stackingOrder())if(target(w))controlsScreen=w->screen();
            if(!controlsScreen)controlsScreen=effects->screenAt(effects->cursorPos().toPoint());
            if(!controlsScreen)return;
            panelGeometry=controlsScreen->geometryF();


            effects->setActiveFullScreenEffect(this);
            if(!effects->grabKeyboard(this)){effects->setActiveFullScreenEffect(nullptr);return;}
            effects->makeOpenGLContextCurrent();uiContext();auto& io=ImGui::GetIO();io.ClearEventsQueue();io.ClearInputKeys();
            auto point=effects->cursorPos();io.AddMousePosEvent(point.x()-panelGeometry.x(),point.y()-panelGeometry.y());
            for(int n=0;n<3;++n)io.AddMouseButtonEvent(n,false);
            io.MouseDrawCursor=true;
        }
        visible=on;
        updateActivity();
        if(on){effects->startMouseInterception(this,Qt::BlankCursor);}
        else{effects->stopMouseInterception(this);effects->ungrabKeyboard();if(effects->activeFullScreenEffect()==this)effects->setActiveFullScreenEffect(nullptr);controlsScreen=nullptr;}
        status(visible?"controls":phase());
        effects->addRepaintFull();
    }
public:
    TrueHDRDesktop(){
        auto p=qEnvironmentVariable("TRUEHDR_PROFILE");profile=p.isEmpty()?root+"/profile.json":p;
        if(development&&!qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))root=qEnvironmentVariable("TRUEHDR_DESKTOP_TEST_ROOT");
        if(development&&!qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_CLASS"))targetClass=qEnvironmentVariable("TRUEHDR_DESKTOP_TEST_CLASS");
        fixtureCapture=development&&!qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT")&&qEnvironmentVariableIsSet("TRUEHDR_DESKTOP_TEST_CAPTURE");
        legacyAllowed=development;
        QFile targets(root+"/window-targets.json");
        if(targets.open(QIODevice::ReadOnly)){
            auto saved=QJsonDocument::fromJson(targets.readAll()).object();legacyAllowed=saved["legacy_ffxiv"].toBool(development);
            for(const auto& value:saved["classes"].toArray())if(value.isString()&&!value.toString().trimmed().isEmpty())rememberedClasses.append(value.toString());
        }
        if(knownWindow(effects->activeWindow()))selectWindow(effects->activeWindow(),false);
        if(!selectedWindow)for(auto* w:effects->stackingOrder())if(knownWindow(w))selectWindow(w,false);
        read();statusTimer.start();toggleStamp=QFileInfo(root+"/overlay.toggle").lastModified().toMSecsSinceEpoch();
        auto readRelease=[this]{QFile file(root+"/output/updates/status.json");if(file.open(QIODevice::ReadOnly))releaseStatus=QJsonDocument::fromJson(file.readAll()).object();};
        readRelease();QDir().mkpath(root+"/output/updates");settingsWatch.addPath(root+"/output/updates");
        connect(&settingsWatch,&QFileSystemWatcher::directoryChanged,this,[this,readRelease](const QString& path){if(path==root+"/output/updates"){readRelease();if(visible)effects->addRepaintFull();}});
        auto settingsChanged=[this]{
            for(const auto& path:{profile,root+"/overlay.toggle"})
                if(QFileInfo::exists(path)&&!settingsWatch.files().contains(path))settingsWatch.addPath(path);
            auto before=config;read();auto stamp=QFileInfo(root+"/overlay.toggle").lastModified().toMSecsSinceEpoch();
            if(stamp&&stamp!=toggleStamp){toggleStamp=stamp;setVisible(!visible);status(visible?"controls":phase());}
            if(before!=config){updateActivity();status(phase());effects->addRepaintFull();}
        };
        settingsWatch.addPath(QFileInfo(profile).absolutePath());
        if(QFileInfo(profile).absolutePath()!=root)settingsWatch.addPath(root);
        for(const auto& path:{profile,root+"/overlay.toggle"})if(QFileInfo::exists(path))settingsWatch.addPath(path);
        connect(&settingsWatch,&QFileSystemWatcher::fileChanged,this,[settingsChanged](const QString&){settingsChanged();});
        connect(&settingsWatch,&QFileSystemWatcher::directoryChanged,this,[settingsChanged](const QString&){settingsChanged();});
        connect(&refresh,&QTimer::timeout,this,[this]{
            if(visible)effects->addRepaintFull();
            if(statusTimer.elapsed()>=1000){status(phase());statusTimer.restart();}
        });updateActivity();
        QDBusConnection::sessionBus().registerObject("/TrueHDR",this,QDBusConnection::ExportAllSlots);
        connect(effects,&EffectsHandler::windowAdded,this,[this](EffectWindow* w){if(!selectedWindow&&knownWindow(w))selectWindow(w,false);});
        connect(effects,&EffectsHandler::windowActivated,this,[this](EffectWindow* w){if(knownWindow(w))selectWindow(w,false);});
        connect(effects,&EffectsHandler::windowDeleted,this,[this](EffectWindow* w){if(target(w)){resetWindow();selectedWindow=nullptr;updateActivity();status("waiting-for-game");}});
        status("loaded");effects->addRepaintFull();
    }
    ~TrueHDRDesktop()override{
        setVisible(false);QDBusConnection::sessionBus().unregisterObject("/TrueHDR");
        effects->makeOpenGLContextCurrent();release();if(ui){ImGui::SetCurrentContext(ui);ImGui_ImplOpenGL3_Shutdown();ImGui::DestroyContext(ui);}status("unloaded");
        for(auto& sample:timings)if(sample.queries[0])glDeleteQueriesEXT(4,sample.queries);
    }
    bool isActive()const override{return true;}
    void prePaintScreen(ScreenPrePaintData& data)override{
        effects->prePaintScreen(data);
        if(!presentation||!presentation->enabled||!data.frame)return;
        for(auto* window:effects->stackingOrder())if(target(window)&&window->screen()==data.screen&&window->isOnCurrentDesktop()&&!window->isMinimized()){
            auto item=qobject_cast<SurfaceItemWayland*>(window->window()->surfaceItem());
            if(!item||!item->surface())return;
            if(observedSurface!=item->surface()){
                disconnect(commitConnection);observedSurface=item->surface();
                commitConnection=connect(observedSurface,&SurfaceInterface::committed,this,[this]{if(presentation&&presentation->enabled){++presentation->serial;presentation->latestCommit=desktop_metrics::now();}});
            }
            if(presentation->serial!=presentation->lastQueued){
                data.frame->addFeedback(std::make_shared<desktop_metrics::Feedback>(presentation),{});
                presentation->lastQueued=presentation->serial;
            }
            break;
        }
    }
    bool blocksDirectScanout()const override{return visible||(target(effects->activeWindow())&&config["enabled"].toBool(true));}
    bool drawWindow(const RenderTarget& targetBuffer,const RenderViewport& viewport,EffectWindow* window,int mask,const Region& region,WindowPaintData& data)override{
        if(renderingInput||failed||!target(window)||!config["enabled"].toBool(true))return effects->drawWindow(targetBuffer,viewport,window,mask,region,data);
        if(region.isEmpty())return true;

        auto surface=window->window()->surfaceItem();
        if(surface&&surface->colorDescription()->transferFunction().type==TransferFunction::PerceptualQuantizer){directFallback="This window already supplies HDR.";return effects->drawWindow(targetBuffer,viewport,window,mask,region,data);}
        auto start=std::chrono::steady_clock::now();
        try{
            auto geometry=window->clientGeometry();QSize size=geometry.size().toSize()*viewport.scale();
            if(size.isEmpty())return effects->drawWindow(targetBuffer,viewport,window,mask,region,data);
            auto buffer=surface?surface->buffer():nullptr;
            auto dma=buffer?buffer->dmabufAttributes():nullptr;
            bool direct=preferDirect&&!directDisabled&&dma&&dma->planeCount==1&&buffer->size()==size
                &&surface->bufferTransform().kind()==OutputTransform::Normal&&surface->childItems().isEmpty()
                &&*surface->colorDescription()==*ColorDescription::sRGB
                &&surface->bufferSourceBox()==RectF(QPointF(),QSizeF(size));
            if(development&&buffer)bufferAudit={{"dma_buf",bool(dma)},{"planes",dma?dma->planeCount:0},
                {"format",QString::number(buffer->format(),16)},{"modifier",QString::number(buffer->modifier(),16)},
                {"width",buffer->size().width()},{"height",buffer->size().height()},
                {"explicit_acquire",surface->syncFd().isValid()},{"sdr_identity",*surface->colorDescription()==*ColorDescription::sRGB},
                {"transform",int(surface->bufferTransform().kind())},{"children",surface->childItems().size()}};


            if(preferDirect&&!fixtureCapture&&!direct){
                if(directFallback.isEmpty())directFallback="The submitted buffer's format, scaling or color description is not supported by the fast path.";
                if(gpu)release();
                return effects->drawWindow(targetBuffer,viewport,window,mask,region,data);
            }
            if(direct)directFallback.clear();
            if(size!=dimensions||direct!=directActive){
                release();
                try{gpu=std::make_unique<DesktopGPU>(size.width(),size.height(),direct);}
                catch(const std::exception& e){if(!direct)throw;directFallback=QString::fromUtf8(e.what());directDisabled=true;
                    if(!fixtureCapture){release();return effects->drawWindow(targetBuffer,viewport,window,mask,region,data);}
                    direct=false;gpu=std::make_unique<DesktopGPU>(size.width(),size.height());}
                dimensions=size;directActive=direct;
                source=GLTexture::createNonOwningWrapper(gpu->inputTexture(),GL_RGBA8,size);
                output=GLTexture::createNonOwningWrapper(gpu->outputTexture(),GL_RGB10_A2,size);
                if(direct)output->setContentTransform(OutputTransform::FlipY);
                inputFbo=std::make_unique<GLFramebuffer>(source.get());
                if(!inputFbo->valid())throw std::runtime_error("Desktop input framebuffer is invalid");
            }
            auto timing=beginTiming();
            const auto convertStarted=frameTraceEnabled?desktop_metrics::now():0;
            if(direct){
                DesktopGPU::DmaInput input;input.width=dma->width;input.height=dma->height;input.format=dma->format;input.planes=dma->planeCount;input.modifier=dma->modifier;input.pitch=dma->pitch;input.offset=dma->offset;input.acquireFd=surface->syncFd().get();
                for(int n=0;n<dma->planeCount;++n)input.fd[n]=dma->fd[n].get();
                if(timing)glQueryCounterEXT(timing->queries[1],GL_TIMESTAMP_EXT);
                try{gpu->convertDirect(input,config["sharpen_intensity"].toInt()/100.f,config["ignore_film_grain"].toInt()/100.f,timingEnabled);}
                catch(const std::exception& e){directFallback=QString::fromUtf8(e.what());directDisabled=true;release();return drawWindow(targetBuffer,viewport,window,mask,region,data);}



                inFlightBuffer=buffer;
                auto wayland=qobject_cast<SurfaceItemWayland*>(surface);
                inFlightRelease=wayland&&wayland->surface()?wayland->surface()->bufferReleasePoint():nullptr;
            }else{
            RenderTarget inputTarget(inputFbo.get(),ColorDescription::sRGB);RenderViewport inputViewport(geometry,viewport.scale(),inputTarget,QPoint());
            GLFramebuffer::pushFramebuffer(inputFbo.get());glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
            WindowPaintData inputData;inputData.setOpacity(1);renderingInput=true;
            bool ok=effects->drawWindow(inputTarget,inputViewport,window,PAINT_WINDOW_TRANSFORMED|PAINT_WINDOW_TRANSLUCENT,Region::infinite(),inputData);
            renderingInput=false;GLFramebuffer::popFramebuffer();
            if(!ok)return false;
            if(timing)glQueryCounterEXT(timing->queries[1],GL_TIMESTAMP_EXT);
            gpu->convert(config["sharpen_intensity"].toInt()/100.f,config["ignore_film_grain"].toInt()/100.f,timingEnabled);
            }
            const auto convertFinished=frameTraceEnabled?desktop_metrics::now():0;
            if(timing)glQueryCounterEXT(timing->queries[2],GL_TIMESTAMP_EXT);
            if(timingEnabled){auto values=gpu->timings();gpuAudit["vulkan_prepare_ms"]=values.prepare;gpuAudit["vulkan_engine_ms"]=values.engine;gpuAudit["vulkan_total_ms"]=values.total;gpuAudit["vulkan_samples"]=double(values.samples);}
            ShaderBinder binder(ShaderTrait::MapTexture|ShaderTrait::TransformColorspace|ShaderTrait::Modulate);
            auto shader=ShaderManager::instance()->getBoundShader();QMatrix4x4 mvp=viewport.projectionMatrix();
            mvp.translate(std::round(geometry.x()*viewport.scale()),std::round(geometry.y()*viewport.scale()));
            shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix,mvp*data.toMatrix(viewport.scale()));
            shader->setUniform(GLShader::Mat4Uniform::TextureMatrix,output->matrix(NormalizedCoordinates));
            shader->setUniform(GLShader::Vec4Uniform::ModulationConstant,QVector4D(data.opacity(),data.opacity(),data.opacity(),data.opacity()));
            auto backend=window->screen()?window->screen()->backendOutput():nullptr;




            double reference=backend?backend->referenceLuminance():targetBuffer.colorDescription()->referenceLuminance();
            auto hdr=std::make_shared<ColorDescription>(Colorimetry::BT2020,TransferFunction(TransferFunction::PerceptualQuantizer,0,10000),reference,0,config["peak_brightness"].toInt(1500),config["peak_brightness"].toInt(1500));
            shader->setUniform(GLShader::IntUniform::Sampler,0);shader->setColorspaceUniforms(hdr,targetBuffer.colorDescription(),RenderingIntent::AbsoluteColorimetricNoAdaptation);
            if constexpr(development)colorAudit={{"hdr_source_reference",reference},{"target_reference",targetBuffer.colorDescription()->referenceLuminance()},
                {"target_transfer",targetBuffer.colorDescription()->transferFunction().type},{"target_min",targetBuffer.colorDescription()->transferFunction().minLuminance},
                {"target_max",targetBuffer.colorDescription()->transferFunction().maxLuminance},{"game_pid",window->pid()}};
            glActiveTexture(GL_TEXTURE0);output->bind();glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);




            const bool clipped=region!=Region::infinite()||!viewport.renderOffset().isNull();
            const QSize offset=targetBuffer.transform().map(QSize(viewport.renderOffset().x(),viewport.renderOffset().y()));
            Region clip=Rect(QPoint(offset.width(),offset.height()),targetBuffer.size()-2*offset);
            if(clipped){
                clip&=viewport.transform().map(region&targetBuffer.transformedRect(),targetBuffer.transformedSize());
                glEnable(GL_SCISSOR_TEST);
            }
            output->render(clip,geometry.size()*viewport.scale(),clipped);
            if(clipped)glDisable(GL_SCISSOR_TEST);
            output->unbind();glDisable(GL_BLEND);
            if(direct)gpu->releaseOutput();
            if(timing){glQueryCounterEXT(timing->queries[3],GL_TIMESTAMP_EXT);timing->pending=true;}
            const auto finished=development?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
            if constexpr(development){++frames;cpuTotal+=std::chrono::duration<double,std::milli>(finished-start).count();}
            if(frameTraceEnabled&&frameCosts.size()<20000){const auto previous=gpu->timings();frameCosts.push_back({
                std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count(),
                std::chrono::duration_cast<std::chrono::nanoseconds>(finished.time_since_epoch()).count(),
                (convertFinished-convertStarted)/1e6,previous.lastPrepare,previous.lastEngine,previous.lastTotal,previous.samples});}
            if(statusTimer.elapsed()>1000){status("filtering");statusTimer.restart();}
            return true;
        }catch(const std::exception& e){renderingInput=false;error=QString::fromUtf8(e.what());failed=true;status("error");
            return effects->drawWindow(targetBuffer,viewport,window,mask,region,data);}
    }
    bool paintScreen(const RenderTarget& targetBuffer,const RenderViewport& viewport,int mask,const Region& region,LogicalOutput* screen)override{
        bool ok=effects->paintScreen(targetBuffer,viewport,mask,region,screen);


#ifdef TRUEHDR_DEVELOPMENT
        if(ok&&testOcclusionEnabled){
            for(auto* w:effects->stackingOrder())if(w->screen()==screen&&w->windowClass().contains("truehdr-menu-fixture")){
                auto point=viewport.mapToRenderTarget(w->clientGeometry().center());
                unsigned char pixel[4]{};glReadPixels(qRound(point.x()),targetBuffer.size().height()-qRound(point.y())-1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
                ++testOcclusionFrames;bool preserved=pixel[0]>240&&pixel[1]<15&&pixel[2]>240;
                if(!preserved)++testOcclusionFailures;
                testOcclusionAudit={{"frames",double(testOcclusionFrames)},{"failures",double(testOcclusionFailures)},
                    {"red",pixel[0]},{"green",pixel[1]},{"blue",pixel[2]}};
                break;
            }
        }
#endif
        if(!ok||!visible)return ok;
        if(controlsScreen!=screen)return ok;
        uiContext();auto& io=ImGui::GetIO();auto rect=viewport.renderRect();panelGeometry=rect;
        io.DisplaySize={float(rect.width()),float(rect.height())};io.DisplayFramebufferScale={float(viewport.scale()),float(viewport.scale())};
        auto now=std::chrono::steady_clock::now();io.DeltaTime=std::max(.001f,std::chrono::duration<float>(now-previous).count());previous=now;
        ImGui_ImplOpenGL3_NewFrame();ImGui::NewFrame();controls();ImGui::Render();
        if constexpr(development)inputAudit={{"screen",screen->name()},{"origin_x",rect.x()},{"origin_y",rect.y()},
            {"mouse_x",io.MousePos.x},{"mouse_y",io.MousePos.y},{"want_mouse",io.WantCaptureMouse},
            {"mouse_down",io.MouseDown[0]},{"overlay_cursor",io.MouseDrawCursor},
            {"fullscreen_input_owned",effects->activeFullScreenEffect()==this}};



        if(!uiTexture||uiTexture->size()!=viewport.deviceSize()){
            uiFbo.reset();uiTexture=GLTexture::allocate(GL_RGBA8,viewport.deviceSize());uiFbo=std::make_unique<GLFramebuffer>(uiTexture.get());
        }
        GLFramebuffer::pushFramebuffer(uiFbo.get());glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());GLFramebuffer::popFramebuffer();
        ShaderBinder binder(ShaderTrait::MapTexture|ShaderTrait::TransformColorspace|ShaderTrait::Modulate);
        auto shader=ShaderManager::instance()->getBoundShader();auto mvp=viewport.projectionMatrix();
        mvp.translate(rect.x()*viewport.scale(),rect.y()*viewport.scale());
        shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix,mvp);
        shader->setUniform(GLShader::IntUniform::Sampler,0);shader->setUniform(GLShader::Vec4Uniform::ModulationConstant,QVector4D(1,1,1,1));
        shader->setColorspaceUniforms(ColorDescription::sRGB,targetBuffer.colorDescription(),RenderingIntent::Perceptual);
        glActiveTexture(GL_TEXTURE0);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        uiTexture->bind();uiTexture->render(viewport.deviceSize());uiTexture->unbind();glDisable(GL_BLEND);return true;
    }
    void pointerMotion(PointerMotionEvent* e)override{if(ui&&visible){ImGui::SetCurrentContext(ui);ImGui::GetIO().AddMousePosEvent(e->position.x()-panelGeometry.x(),e->position.y()-panelGeometry.y());effects->addRepaintFull();}}
    void pointerButton(PointerButtonEvent* e)override{if(ui&&visible){ImGui::SetCurrentContext(ui);auto& io=ImGui::GetIO();io.AddMousePosEvent(e->position.x()-panelGeometry.x(),e->position.y()-panelGeometry.y());int button=e->button==Qt::LeftButton?0:e->button==Qt::RightButton?1:2;io.AddMouseButtonEvent(button,e->state==PointerButtonState::Pressed);effects->addRepaintFull();}}
    void grabbedKeyboardEvent(QKeyEvent* e)override{
        if(!ui||!visible)return;ImGui::SetCurrentContext(ui);auto& io=ImGui::GetIO();bool down=e->type()==QEvent::KeyPress;
        if(down&&(e->key()==Qt::Key_Escape||(e->key()==Qt::Key_Z&&e->modifiers().testFlag(Qt::AltModifier)))){setVisible(false);return;}
        ImGuiKey key=ImGuiKey_None;switch(e->key()){case Qt::Key_Backspace:key=ImGuiKey_Backspace;break;case Qt::Key_Return:case Qt::Key_Enter:key=ImGuiKey_Enter;break;case Qt::Key_Left:key=ImGuiKey_LeftArrow;break;case Qt::Key_Right:key=ImGuiKey_RightArrow;break;case Qt::Key_Delete:key=ImGuiKey_Delete;break;default:break;}
        if(key!=ImGuiKey_None)io.AddKeyEvent(key,down);
        if(down&&!e->text().isEmpty()&&e->text().at(0).isPrint())io.AddInputCharactersUTF8(e->text().toUtf8().constData());effects->addRepaintFull();
    }
public Q_SLOTS:
    bool ping(){return true;}
    void testOcclusion(bool enabled){
        if(!development||qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))return;
        testOcclusionEnabled=enabled;testOcclusionFrames=0;testOcclusionFailures=0;testOcclusionAudit={};
    }
    void toggleControls(){setVisible(!visible);status(visible?"controls":"loaded");}
    bool selectFocusedWindow(){auto w=effects->activeWindow();return knownWindow(w)&&selectWindow(w,false);}
    bool allowFocusedWindow(){return selectWindow(effects->activeWindow(),true);}
    bool testCaptureControls(){
        if(!development||qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT")||!uiTexture)return false;
        effects->makeOpenGLContextCurrent();return uiTexture->toImage().mirrored(false,true).save(root+"/controls.png");
    }
    void setEnabled(bool enabled){config["enabled"]=enabled;failed=false;save();status(enabled?"enabled":"disabled");}
    void setTiming(bool enabled){if constexpr(development)timingEnabled=enabled;}
    void setFrameTrace(bool enabled){
        if constexpr(!development)return;
        frameTraceEnabled=enabled;timingEnabled=enabled;
        frameCosts.clear();
        if(enabled){frameCosts.reserve(20000);presentation=std::make_shared<desktop_metrics::State>();presentation->traceMode=true;}
        else{if(presentation&&presentation->traceMode)presentation.reset();frameCosts.shrink_to_fit();}
        status("measurement");
    }
    QString frameTrace(){
        if(!presentation||!presentation->traceMode)return "{}";
        const auto origin=!frameCosts.empty()?frameCosts.front().started:desktop_metrics::now();
        QJsonArray costs;for(const auto& frame:frameCosts)costs.append(QJsonArray{
            (frame.started-origin)/1e6,(frame.finished-origin)/1e6,frame.convertMs,frame.gpuPrepare,frame.gpuEngine,frame.gpuTotal,double(frame.gpuSamples)});
        return QString::fromUtf8(QJsonDocument(QJsonObject{
            {"summary",presentation->result()},{"origin_monotonic_ns",QString::number(origin)},
            {"presentation_columns",QJsonArray{"commit_ms","prepaint_ms","present_ms"}},
            {"presentation",presentation->trace(origin)},
            {"processing_columns",QJsonArray{"start_ms","finish_ms","conversion_cpu_ms","previous_gpu_prepare_ms","previous_gpu_engine_ms","previous_gpu_total_ms","gpu_samples"}},{"processing",costs},
            {"gpu_averages",gpuAudit},
            {"scope","Optional bounded diagnostics only. Commit times are KDE-observed receipt, which can itself be delayed if its event loop stalls. CPU conversion includes submission and waits, not isolated GPU execution. GPU per-frame values describe the previously completed HDR job. Frame generation is not distinguished."}
        }).toJson(QJsonDocument::Compact));
    }
    void setPresentationTiming(bool enabled){if constexpr(!development)return;if(enabled)presentation=std::make_shared<desktop_metrics::State>();else if(presentation)presentation->enabled=false;status("measurement");effects->addRepaintFull();}
    QString presentationTiming(){return QString::fromUtf8(QJsonDocument(presentation?presentation->result():QJsonObject{}).toJson(QJsonDocument::Compact));}
    void setDirect(bool enabled){if constexpr(!development)return;effects->makeOpenGLContextCurrent();release();preferDirect=enabled;directDisabled=false;directFallback.clear();timingSamples=0;for(auto& total:gpuTotals)total=0;for(auto& sample:timings)sample.pending=false;gpuAudit={};effects->addRepaintFull();}
    void testPointer(double x,double y,int state){
        if(!development||qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))return;
        PointerMotionEvent motion{};motion.position={x,y};pointerMotion(&motion);
        if(state>=0){PointerButtonEvent button{};button.position={x,y};button.button=Qt::LeftButton;button.state=state?PointerButtonState::Pressed:PointerButtonState::Released;pointerButton(&button);}
    }
};

KWIN_EFFECT_FACTORY_SUPPORTED(TrueHDRDesktop,"effect.json",return effects->compositingType()==OpenGLCompositing;)
#include "effect.moc"
