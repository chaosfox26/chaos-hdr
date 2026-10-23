#include <epoxy/gl.h>
#include "gpu.hpp"
#include "presentation_metrics.hpp"
#include "overlay_metrics.hpp"
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
#include "pointer_input.h"
#include "keyboard_input.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "overlay_motion.hpp"
#include "overlay_style.hpp"
#include "overlay_gl.hpp"
#include "font_fallback.hpp"
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
#include <QRegion>
#include <QSet>
#include <QFileSystemWatcher>
#include <cmath>
#include <chrono>
#include <array>
#include <tuple>

using namespace KWin;

#ifdef TRUEHDR_DEVELOPMENT
class FixtureInputDevice : public InputDevice {
public:
    QString name()const override{return QStringLiteral("Chaos HDR isolated input fixture");}
    bool isEnabled()const override{return true;}
    void setEnabled(bool)override{}
    bool isKeyboard()const override{return true;}
    bool isPointer()const override{return true;}
    bool isTouchpad()const override{return false;}
    bool isTouch()const override{return false;}
    bool isTabletTool()const override{return false;}
    bool isTabletPad()const override{return false;}
    bool isTabletModeSwitch()const override{return false;}
    bool isLidSwitch()const override{return false;}
};
#endif

class TrueHDRDesktop : public Effect {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.truehdr.Desktop")
    QString root=QString::fromUtf8(TRUEHDR_ROOT),profile;
    QJsonObject config;QString error;QTimer refresh,persistTimer,retryTimer;QProcess pasteProcess;QPointer<QProcess> copyProcess;QTimer pasteTimeout;int retryCount=0;bool settingsDirty=false;ImGuiID pasteField=0;QByteArray clipboardText;
    int displayPeak=1500;bool displayHdr=true;QString displayName,displayError;
    QPointer<LogicalOutput> pictureScreen;QList<QMetaObject::Connection> displayConnections;QFileSystemWatcher settingsWatch;
    QJsonObject releaseStatus;
    QStringList profileNames;QString profileReadError;bool profileListPending=false;
    std::unique_ptr<DesktopGPU> gpu;
    struct RetiredGPU {std::unique_ptr<DesktopGPU> gpu;GraphicsBufferRef buffer;std::shared_ptr<SyncReleasePoint> release;};
    std::unique_ptr<RetiredGPU> retiredGpu;
    QTimer retireTimer,queueWatch;QElapsedTimer retireAge;
    bool gpuBlocked=DesktopGPU::recoveryRequired(),unloading=false;
    static constexpr auto recoveryMessage="HDR GPU recovery requires signing out and back in. Your settings are preserved.";
#ifdef TRUEHDR_DEVELOPMENT
    int gpuCreations=0;
#endif
    std::unique_ptr<GLTexture> source,output;
    std::unique_ptr<GLFramebuffer> inputFbo;
    std::unique_ptr<GLTexture> uiTexture;
    std::unique_ptr<GLFramebuffer> uiFbo;
    std::unique_ptr<GLTexture> lightningTexture;
    QJsonObject colorAudit;std::shared_ptr<ColorDescription> hdrColor;double hdrReference=-1;
    QJsonObject bufferAudit;
    GraphicsBufferRef inFlightBuffer;
    std::shared_ptr<SyncReleasePoint> inFlightRelease;
    bool directActive=false,directDisabled=false,preferDirect=true,outputValid=false,outputHdr=false;
    QString directFallback,processingError;QByteArray failedDescriptor;QMetaObject::Connection damageConnection;
    std::shared_ptr<desktop_metrics::State> presentation;
    QPointer<SurfaceInterface> observedSurface;
    QMetaObject::Connection commitConnection,outputConnection;
    QSize dimensions;bool failed=false,renderingInput=false,visible=false;
    ImGuiContext* ui{};char name[256]{};
    FontFallback fontFallback;QString glyphText;bool glyphPending=false;
    bool statusPending=false;QString pendingStage;
    chaos_ui::Fonts uiFonts;chaos_ui::Motion panelMotion;bool closing=false;float panelProgress=0;
#ifdef TRUEHDR_DEVELOPMENT
    bool previewCapture=false;int previewIndex=0;double previewStart=0,previewLast=-1;QJsonArray previewTimeline;
    std::unique_ptr<FixtureInputDevice> fixtureInput;
    bool testOverlayClip=false;
    QHash<QString,QPair<int,bool>> testDisplays;int testFailures=0;
#endif
    static double uiTime(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
    uint64_t frames=0;double cpuTotal=0;QElapsedTimer statusTimer;
    struct FrameCost {int64_t started,finished;double convertMs,gpuPrepare,gpuEngine,gpuTotal;uint64_t gpuSamples;};
    std::vector<FrameCost> frameCosts;
    bool frameTraceEnabled=false;
#ifdef TRUEHDR_DEVELOPMENT
    OverlayMetrics overlayMetrics;
#endif
    bool testOcclusionEnabled=false;uint64_t testOcclusionFrames=0,testOcclusionFailures=0;
    QJsonObject testOcclusionAudit;
    RectF panelGeometry;
    QRectF availablePanel;int overlayWidth=452;
    QRegion inputRegion;
    Qt::MouseButtons capturedButtons=Qt::NoButton;
    QSet<quint32> capturedKeys;
    bool inputInstalled=false,keyboardFocus=false,cursorOwned=false;
    class SidebarInput final : public InputEventFilter {
        TrueHDRDesktop* owner;
    public:
        explicit SidebarInput(TrueHDRDesktop* effect):InputEventFilter(InputFilterOrder::Effects),owner(effect){}
        bool pointerMotion(PointerMotionEvent* e)override{return owner->routeMotion(e);}
        bool pointerButton(PointerButtonEvent* e)override{return owner->routeButton(e);}
        bool pointerAxis(PointerAxisEvent* e)override{return owner->routeAxis(e);}
        bool keyboardKey(KeyboardKeyEvent* e)override{return owner->routeKey(e);}
    };
    SidebarInput sidebarInput{this};
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
    bool hdrEnabled()const{return config["enabled"].toBool(true)&&config["hdr_enabled"].toBool(true);}
    bool sharpenEnabled()const{return config["enabled"].toBool(true)&&config["sharpen_enabled"].toBool(true);}
    bool hdrProcessing()const{return hdrEnabled()&&displayHdr;}
    bool processingEnabled()const{return hdrProcessing()||(sharpenEnabled()&&config["sharpen_intensity"].toInt()>0);}
    float sharpening()const{return sharpenEnabled()?config["sharpen_intensity"].toInt()/100.f:0.f;}
    bool watchdogActive()const{return !gpuBlocked&&!unloading&&selectedWindow&&processingEnabled();}
    void repaintOverlay(){
        if(!controlsScreen)return;
        auto area=controlsScreen->geometry();
        effects->addRepaint(Rect(area.x(),area.y(),std::min(area.width(),overlayWidth),area.height()));
    }
    void updateActivity(){
        if(visible||watchdogActive()){
            int interval=visible?(config["reduced_motion"].toBool()?50:16):1000;
            if(!refresh.isActive()||refresh.interval()!=interval)refresh.start(interval);
        }else refresh.stop();
    }
    const char* phase()const{
        return gpuBlocked?"gpu-recovery-required":retiredGpu?"gpu-retiring":!processingEnabled()?"disabled":!selectedWindow?"waiting-for-game":
            !directFallback.isEmpty()&&!directActive?"unsupported-window":gpu?"filtering":"waiting-for-frame";
    }
    void saveTargets(){
        QJsonArray classes;for(const auto& value:rememberedClasses)classes.append(value);
        QString e;if(!profiles::write(root+"/window-targets.json",{{"version",1},{"classes",classes},{"legacy_ffxiv",legacyAllowed}},e))error=e;
    }
    void resetWindow(){
        effects->makeOpenGLContextCurrent();release();failed=false;directDisabled=false;retryCount=0;retryTimer.stop();
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
        if(selectedWindow!=w){
            resetWindow();disconnect(outputConnection);disconnect(damageConnection);selectedWindow=w;
            damageConnection=connect(w,&EffectWindow::windowDamaged,this,[this]{
                if(directDisabled&&descriptor()!=failedDescriptor){directDisabled=false;failed=false;retryCount=0;retryTimer.stop();directFallback.clear();}
            });
            outputConnection=connect(w->window(),&Window::outputChanged,this,[this]{updateDisplay();effects->addRepaintFull();});
        }else if(directDisabled||failed){retryCount=0;retryTimer.stop();directDisabled=false;failed=false;directFallback.clear();error.clear();}
        if(remember&&!w->windowClass().trimmed().isEmpty()&&!rememberedClasses.contains(w->windowClass())){
            rememberedClasses.append(w->windowClass());saveTargets();
        }
        updateDisplay();updateActivity();status(phase());effects->addRepaintFull();return true;
    }
    void updateDisplay(){
        auto previous=std::tuple(displayPeak,displayHdr,displayName);
        auto* screen=selectedWindow?selectedWindow->screen():controlsScreen.data();
        if(!screen)screen=effects->screenAt(effects->cursorPos().toPoint());
        if(pictureScreen!=screen){
            for(auto connection:displayConnections)disconnect(connection);
            displayConnections.clear();pictureScreen=screen;
            if(screen){
                displayConnections.append(connect(screen,&LogicalOutput::changed,this,[this]{updateDisplay();}));
                if(auto* backend=screen->backendOutput()){
                    for(auto signal:{&BackendOutput::brightnessMetadataChanged,&BackendOutput::highDynamicRangeChanged,&BackendOutput::changed})
                        displayConnections.append(connect(backend,signal,this,[this]{updateDisplay();}));
                }
            }
        }
        auto* backend=screen?screen->backendOutput():nullptr;
        displayName=screen?screen->name():QString();displayError.clear();
        double peak=backend?backend->maxPeakBrightnessOverride().value_or(backend->advertisedMaxPeakBrightness().value_or(0)):0;
        bool enabled=backend&&backend->highDynamicRange();
#ifdef TRUEHDR_DEVELOPMENT
        if(!qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT")){
            auto state=testDisplays.value(displayName,{config["peak_brightness"].toInt(1500),true});peak=state.first;enabled=state.second;
        }
#endif
        displayPeak=PictureSettings::supportedPeak(peak);displayHdr=enabled&&displayPeak;
        if(!displayHdr)displayError=backend&&backend->highDynamicRange()?"Display peak is outside the supported 400–2000 nit range.":"Enable HDR for this screen in KDE Display Settings to use HDR. Sharpening is still available.";
        updateActivity();
        if(previous!=std::tuple(displayPeak,displayHdr,displayName)){hdrColor.reset();status(phase());effects->addRepaintFull();}
        else if(visible)repaintOverlay();
    }
    void read(){
        if(settingsDirty)return;
        QFile file(profile);if(file.open(QIODevice::ReadOnly)){auto obj=QJsonDocument::fromJson(file.readAll()).object();if(!obj.isEmpty())config=obj;}
        profileNames.clear();profileReadError.clear();updateDisplay();
    }
    void persist(){
        persistTimer.stop();if(!settingsDirty)return;
        QString e;
        if(profiles::Store(profile).saveCurrent(config,e)){settingsDirty=false;error.clear();}
        else error=e;
        status(phase());
    }
    void save(){settingsDirty=true;persistTimer.start(180);updateActivity();effects->addRepaintFull();}
    void profileAction(int action,QString value={}){
        QTimer::singleShot(0,this,[this,action,value]{
            persist();if(settingsDirty)return;
            profiles::Store store(profile);bool ok=false;
            if(action==0)ok=store.select(config,value,error);
            if(action==1)ok=store.create(config,value,error);
            if(action==2)ok=store.remove(config,error);
            if(ok){profileNames.clear();profileReadError.clear();updateDisplay();updateActivity();status(phase());effects->addRepaintFull();}
        });
    }
    QByteArray descriptor()const{
        auto* surface=selectedWindow?selectedWindow->window()->surfaceItem():nullptr;auto* buffer=surface?surface->buffer():nullptr;
        if(!buffer)return {};
        return QByteArray::number(buffer->format())+':'+QByteArray::number(buffer->modifier())+':'+QByteArray::number(buffer->size().width())+':'+QByteArray::number(buffer->size().height());
    }
    void processingFailure(const QString& message){
        processingError=message;failedDescriptor=descriptor();directFallback=message;error=message;directDisabled=true;
        if(retryCount<3){int delay=retryCount==0?250:retryCount==1?1000:5000;++retryCount;retryTimer.start(delay);}
    }
    bool opaqueSurface(SurfaceItem* surface)const{
        return surface&&(!surface->hasAlphaChannel()||surface->opaque().contains(RectF(QPointF(),surface->destinationSize())));
    }
    bool directSurface(SurfaceItem* surface,QSize size)const{
        auto* buffer=surface?surface->buffer():nullptr;auto* dma=buffer?buffer->dmabufAttributes():nullptr;
        return dma&&dma->planeCount==1&&DesktopGPU::supportsFormat(dma->format)&&opaqueSurface(surface)
            &&buffer->size()==size&&surface->childItems().isEmpty()&&surface->bufferTransform().kind()==OutputTransform::Normal
            &&*surface->colorDescription()==*ColorDescription::sRGB&&surface->bufferSourceBox()==RectF(QPointF(),QSizeF(size));
    }
    void dismissRemovedScreen(LogicalOutput* screen){
        if(controlsScreen==screen){
            setVisible(false);closing=false;controlsScreen=nullptr;inputRegion={};keyboardFocus=false;setCursorOwned(false);
            capturedButtons=Qt::NoButton;capturedKeys.clear();updateInputFilter();updateActivity();
        }
        if(pictureScreen==screen){pictureScreen=nullptr;displayHdr=false;QTimer::singleShot(0,this,[this]{updateDisplay();});}
    }
    bool target(EffectWindow* w)const{
        return w&&w==selectedWindow;
    }
    void abandonGpu(){
        retiredGpu.release();DesktopGPU::quarantine();gpuBlocked=true;retireTimer.stop();retryTimer.stop();queueWatch.stop();
        error=recoveryMessage;directFallback=error;directDisabled=true;updateActivity();status(phase());
    }
    void pollRetirement(){
        if(!retiredGpu)return;
        effects->makeOpenGLContextCurrent();
        if(retiredGpu->gpu->retirementReady()){
            retiredGpu.reset();retireTimer.stop();status(phase());effects->addRepaintFull();
        }else if(retireAge.elapsed()>=1000)abandonGpu();
    }
    void release(){
        queueWatch.stop();inputFbo.reset();source.reset();output.reset();dimensions={};directActive=false;outputValid=false;
        if(gpu){
            retiredGpu=std::make_unique<RetiredGPU>(RetiredGPU{std::move(gpu),std::move(inFlightBuffer),std::move(inFlightRelease)});
            retireAge.start();pollRetirement();if(retiredGpu)retireTimer.start(20);
        }
        gpuBlocked=DesktopGPU::recoveryRequired();
        if(gpuBlocked){error=recoveryMessage;directFallback=error;}
    }
#ifdef TRUEHDR_DEVELOPMENT
    static constexpr bool development=true;
#else
    static constexpr bool development=false;
#endif
    void status(const char* stage){
        pendingStage=QString::fromUtf8(stage);if(statusPending)return;statusPending=true;
        QTimer::singleShot(0,this,[this]{statusPending=false;writeStatus(pendingStage.toUtf8().constData());});
    }
    void writeStatus(const char* stage){
        QDir().mkpath(root+"/output/desktop");QString e;
        if constexpr(!development){
            profiles::write(root+"/output/desktop/live-status.json",{{"stage",stage},{"updated",QDateTime::currentSecsSinceEpoch()},
                {"watchdog_active",watchdogActive()},{"enabled",processingEnabled()},{"hdr_enabled",hdrEnabled()},{"sharpen_enabled",sharpenEnabled()},{"overlay_visible",visible},
                {"display_peak",displayPeak},{"display_hdr",displayHdr},{"display",displayName},{"direct_input",directActive},{"gpu_recovery_required",gpuBlocked},{"error",gpuBlocked?QString(recoveryMessage):error}},e);
            return;
        }
        profiles::write(root+"/output/desktop/live-status.json",{{"stage",stage},{"pid",QCoreApplication::applicationPid()},
            {"updated",QDateTime::currentSecsSinceEpoch()},{"frames",double(frames)},{"target",selectedWindow?selectedWindow->windowClass():QString()},
            {"selection","KDE window identity; no executable path"},{"fast_path_required",preferDirect&&!fixtureCapture},
            {"gpu_recovery_required",gpuBlocked},{"gpu_retiring",bool(retiredGpu)},
            {"target_window_type",selectedWindow?selectedWindow->windowTypeInt():-1},
            {"target_transient",selectedWindow&&selectedWindow->transientFor()},{"target_modal",selectedWindow&&selectedWindow->isModal()},
            {"session_only_target",selectedWindow&&selectedWindow->windowClass().trimmed().isEmpty()},
            {"watchdog_active",watchdogActive()},{"settings_monitor","file-notifications"},
            {"enabled",processingEnabled()},{"hdr_enabled",hdrEnabled()},{"sharpen_enabled",sharpenEnabled()},{"overlay_visible",visible},{"error",error},{"width",dimensions.width()},{"height",dimensions.height()},
            {"display_peak",displayPeak},{"display_hdr",displayHdr},{"display",displayName},
            {"color_pipeline",colorAudit},
            {"source_buffer",bufferAudit},{"direct_input",directActive},{"direct_fallback",directFallback},
            {"input",inputAudit},{"controls",controlRects},
            {"gpu_timing",gpuAudit},
            {"test_occlusion",testOcclusionAudit},
            {"presentation_timing",presentation?(presentation->traceMode?QJsonObject{{"enabled",presentation->enabled},{"trace_mode",true},{"presented_frames",double(presentation->samples.size())}}:presentation->result()):QJsonObject{}},
            {"cpu_submit_mean_ms",frames?cpuTotal/frames:0},{"transport",!gpu||!processingEnabled()?"unfiltered":directActive?"submitted DMA-BUF directly into Vulkan TrueHDR":"OpenGL capture diagnostic"}},e);
    }
    void uiContext(){
        if(ui){ImGui::SetCurrentContext(ui);return;}
        ui=ImGui::CreateContext();uiFonts=chaos_ui::setup(QString::fromUtf8(TRUEHDR_ASSET_ROOT).toStdString());
        QImage background(QString::fromUtf8(TRUEHDR_ASSET_ROOT)+"/assets/chaos-lightning.png");
        if(!background.isNull()){lightningTexture=GLTexture::upload(background);if(lightningTexture){lightningTexture->setFilter(GL_LINEAR);lightningTexture->setWrapMode(GL_CLAMP_TO_EDGE);lightningTexture->bind();lightningTexture->unbind();}}
        auto& io=ImGui::GetIO();io.ClipboardUserData=this;
        io.GetClipboardTextFn=[](void* data){return static_cast<TrueHDRDesktop*>(data)->clipboardText.constData();};
        io.SetClipboardTextFn=[](void* data,const char* text){static_cast<TrueHDRDesktop*>(data)->copyText(QByteArray(text));};
        if(!ImGui_ImplOpenGL3_Init("#version 300 es"))throw std::runtime_error("Initialize desktop controls");
    }
    void copyText(QByteArray text){
        QTimer::singleShot(0,this,[this,text]{
            if(copyProcess)copyProcess->kill();
            auto* process=new QProcess(this);copyProcess=process;
            connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),process,&QObject::deleteLater);
            auto env=QProcessEnvironment::systemEnvironment();env.insert("QT_QPA_PLATFORM","wayland");process->setProcessEnvironment(env);
            process->start(QString::fromUtf8(TRUEHDR_ROOT)+"/build/truehdr-panel",{profile,"--clipboard-write"});
            process->write(text);process->closeWriteChannel();
        });
    }
    void pasteText(){
        if(pasteProcess.state()!=QProcess::NotRunning||!ImGui::GetIO().WantTextInput)return;
        pasteField=ImGui::GetActiveID();
        auto env=QProcessEnvironment::systemEnvironment();env.insert("QT_QPA_PLATFORM","wayland");pasteProcess.setProcessEnvironment(env);
        pasteProcess.start(QString::fromUtf8(TRUEHDR_ROOT)+"/build/truehdr-panel",{profile,"--clipboard-read"});pasteTimeout.start(2000);
    }
    Timing* beginTiming(){
        if(!development||!timingEnabled)return nullptr;
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
        if(profileNames.isEmpty()&&!profileListPending&&profileReadError.isEmpty()){profileListPending=true;QTimer::singleShot(0,this,[this]{profileListPending=false;profileNames=profiles::Store(profile).names(config,profileReadError);});}
        const auto& names=profileNames;const auto& profileError=profileReadError;
        float width=std::min(420.f,float(availablePanel.width())),height=availablePanel.height();
        float x=availablePanel.x()-(width+24)*(1-panelProgress),y=availablePanel.y();
        ImGui::SetNextWindowPos({x,y},ImGuiCond_Always);ImGui::SetNextWindowSize({width,height},ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,panelProgress);
        auto flags=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
        if(!visible)flags|=ImGuiWindowFlags_NoInputs;
        if(ImGui::Begin("Chaos HDR",nullptr,flags)){
            if(lightningTexture){
                auto* backdrop=ImGui::GetWindowDrawList();double time=config["reduced_motion"].toBool()?0:uiTime();
                auto alpha=[&](float y){return float(.16+.055*std::sin(time*1.5-y*8)+.025*std::sin(time*6.3-y*17));};
                for(int band=0;band<24;++band){
                    float top=band/24.f,bottom=(band+1)/24.f;
                    backdrop->AddImage(ImTextureID(lightningTexture->texture()),{x,y+top*height},{x+width,y+bottom*height},{0,top},{1,bottom});
                    auto* vertices=backdrop->VtxBuffer.Data+backdrop->VtxBuffer.Size-4;
                    vertices[0].col=vertices[1].col=ImGui::GetColorU32({1,1,1,alpha(top)});
                    vertices[2].col=vertices[3].col=ImGui::GetColorU32({1,1,1,alpha(bottom)});
                }
            }
            chaos_ui::wordmark(uiFonts.brand,config["reduced_motion"].toBool()?0:ImGui::GetTime(),width-82);
            auto afterLogo=ImGui::GetCursorPos();ImGui::SetCursorPos({width-54,23});
            bool close=ImGui::Button("##collapse",{28,28});item("close");
            auto cp=ImGui::GetItemRectMin();auto* draw=ImGui::GetWindowDrawList();
            draw->AddLine({cp.x+17,cp.y+8},{cp.x+10,cp.y+14},ImGui::GetColorU32(chaos_ui::white),1.5f);
            draw->AddLine({cp.x+10,cp.y+14},{cp.x+17,cp.y+20},ImGui::GetColorU32(chaos_ui::white),1.5f);
            if(ImGui::IsItemHovered())ImGui::SetTooltip("Collapse sidebar (Alt+Z or Esc)");
            if(close)setVisible(false);
            ImGui::SetCursorPos(afterLogo);chaos_ui::hint(uiFonts.small,"Your SDR picture, transformed.");
            ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
            bool changed=false,enabled=processingEnabled();
            chaos_ui::hint(uiFonts.small,!enabled?"Off - original picture":!selectedWindow?"Choose and allow an application to begin":!directFallback.isEmpty()?"This window cannot be processed":directActive?"Active on your selected window":"Ready - waiting for a compatible frame");
            ImGui::Spacing();
            if(ImGui::BeginChild("Picture controls",{0,-90},ImGuiChildFlags_None,visible?0:ImGuiWindowFlags_NoInputs)){
                chaos_ui::heading(uiFonts.bold,"Application");
                auto caption=candidateWindow?candidateWindow->caption().toUtf8():QByteArray("Choose a running application");
                ImGui::SetNextItemWidth(-1);
                if(ImGui::BeginCombo("##application",caption.constData())){
                    int index=0;
                    for(auto* w:effects->stackingOrder())if(applicationWindow(w)){
                        ImGui::PushID(index++);
                        if(ImGui::Selectable(w->caption().toUtf8().constData(),w==candidateWindow)){
                            candidateWindow=w;if(knownWindow(w))selectWindow(w,false);
                        }
                        if constexpr(development)item(("application_option_"+w->windowClass()+"_"+w->caption()).toUtf8().constData());
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",w->windowClass().toUtf8().constData());
                        ImGui::PopID();
                    }
                    if(!index)ImGui::TextDisabled("Open an application, then return here.");
                    ImGui::EndCombo();
                }item("application");
                if(candidateWindow){
                    if(knownWindow(candidateWindow)||candidateWindow==selectedWindow){
                        chaos_ui::hint(uiFonts.small,candidateWindow->windowClass().trimmed().isEmpty()?"Allowed for this session only.":"Allowed. You can remove this permission anytime.");
                        if(chaos_ui::button("Remove application",{-1,34})){
                            rememberedClasses.removeAll(candidateWindow->windowClass());
                            if(candidateWindow->windowClass().contains(targetClass,Qt::CaseInsensitive))legacyAllowed=false;
                            saveTargets();if(selectedWindow&&selectedWindow->windowClass()==candidateWindow->windowClass()){resetWindow();selectedWindow=nullptr;}
                            updateActivity();status(phase());effects->addRepaintFull();
                        }item("remove_application");
                    }else{
                        chaos_ui::hint(uiFonts.small,"Your picture stays unchanged until you allow it.");
                        if(chaos_ui::button("Allow application",{-1,36},true))selectWindow(candidateWindow,true);
                        item("allow_application");
                    }
                }else chaos_ui::hint(uiFonts.small,"Open an SDR application and select its window.");
                if(selectedWindow&&candidateWindow!=selectedWindow)ImGui::TextWrapped("Currently adjusting: %s",selectedWindow->caption().toUtf8().constData());
                if(gpuBlocked)ImGui::TextWrapped("%s",recoveryMessage);
                if(!gpuBlocked&&candidateWindow==selectedWindow&&!directFallback.isEmpty()){
                    ImGui::TextWrapped("This window is unchanged.");
                    chaos_ui::hint(uiFonts.small,directFallback.toUtf8().constData());
                    if(directDisabled){if(chaos_ui::button("Retry processing"))selectWindow(selectedWindow,false);item("retry_processing");}
                }
                ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
                bool profileOpen=ImGui::CollapsingHeader(("Profile: "+profiles::active(config)+"##profile_section").toUtf8().constData());item("profile_section");
                if(profileOpen){
                auto selected=profiles::active(config).toUtf8();ImGui::SetNextItemWidth(-1);
                if(ImGui::BeginCombo("##profile",selected.constData())){
                    for(auto preset:names)if(ImGui::Selectable(preset.toUtf8().constData(),preset==profiles::active(config)))
                        profileAction(0,preset);
                    ImGui::EndCombo();
                }item("profile");
                float half=(ImGui::GetContentRegionAvail().x-10)*.5f;
                if(chaos_ui::button("Save as...",{half,32})){name[0]=0;ImGui::OpenPopup("Save profile");}item("save_profile");
                ImGui::SameLine();ImGui::BeginDisabled(profiles::active(config)=="Global");
                if(chaos_ui::button("Delete profile",{-1,32}))ImGui::OpenPopup("Delete profile?");item("delete_profile");ImGui::EndDisabled();
                if(ImGui::BeginPopupModal("Save profile",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
                    chaos_ui::hint(uiFonts.small,"Save the current picture settings under a new name.");
                    if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();
                    bool enter=ImGui::InputText("Name",name,sizeof(name),ImGuiInputTextFlags_EnterReturnsTrue);item("profile_name");
                    if(chaos_ui::button("Save",{},true)||enter){profileAction(1,QString::fromUtf8(name));ImGui::CloseCurrentPopup();}
                    ImGui::SameLine();if(chaos_ui::button("Cancel"))ImGui::CloseCurrentPopup();
                    if(!error.isEmpty())ImGui::TextWrapped("%s",error.toUtf8().constData());ImGui::EndPopup();
                }
                if(ImGui::BeginPopupModal("Delete profile?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
                    ImGui::TextUnformatted("Delete this profile and restore Global?");
                    if(chaos_ui::button("Delete")){profileAction(2);ImGui::CloseCurrentPopup();}
                    ImGui::SameLine();if(chaos_ui::button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
                }
                if(!profileError.isEmpty())chaos_ui::hint(uiFonts.small,profileError.toUtf8().constData());
                }
                ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
                chaos_ui::heading(uiFonts.bold,"HDR");ImGui::SameLine(ImGui::GetWindowContentRegionMax().x-42);
                bool hdr=hdrEnabled();if(chaos_ui::toggle("##hdr_enabled",hdr)){config["hdr_enabled"]=hdr;if(hdr)config["enabled"]=true;changed=true;}item("hdr_enabled");chaos_ui::tooltip("Convert SDR highlights and tones to HDR. Sharpening can stay on independently.");
                ImGui::BeginDisabled(!hdr);
                ImGui::Text("Display peak");ImGui::SameLine();ImGui::TextColored(chaos_ui::blue,"%d nits - Auto",displayPeak);
                if(!displayError.isEmpty())chaos_ui::hint(uiFonts.small,displayError.toUtf8().constData());
                chaos_ui::tooltip("Uses the HDR brightness set in KDE Display Settings. Chaos HDR follows that display setting automatically.");
                auto slider=[&](const char* label,const char* key,int low,int high,int fallback,const char* format,const char* help){
                    ImGui::Spacing();int value=config[key].toInt(fallback);
                    chaos_ui::heading(uiFonts.bold,label);chaos_ui::tooltip(help);
                    char number[64];std::snprintf(number,sizeof(number),format,value);
                    ImGui::SameLine(std::max(0.f,ImGui::GetWindowContentRegionMax().x-ImGui::CalcTextSize(number).x-2));
                    ImGui::TextColored(chaos_ui::white,"%s",number);
                    auto id=std::string("##")+key;
                    if(chaos_ui::slider(id.c_str(),value,low,high)){config[key]=value;changed=true;}item(key);
                    chaos_ui::tooltip(help);
                };
                slider("Middle greys","middle_gray",10,100,50,"%d nits","Brightness of everyday tones, without changing the display peak.");
                slider("Contrast","contrast",-100,100,0,"%d%%","Separation between bright and dark areas.");
                slider("Saturation","saturation",-100,100,0,"%d%%","Colour intensity. Zero keeps the default balance.");
                ImGui::EndDisabled();ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
                chaos_ui::heading(uiFonts.bold,"Sharpening");ImGui::SameLine(ImGui::GetWindowContentRegionMax().x-42);
                bool sharp=sharpenEnabled();if(chaos_ui::toggle("##sharpen_enabled",sharp)){config["sharpen_enabled"]=sharp;if(sharp)config["enabled"]=true;changed=true;}item("sharpen_enabled");chaos_ui::tooltip("Enhance detail independently of HDR. Turning this off keeps your saved intensity.");
                ImGui::BeginDisabled(!sharp);
                slider("Intensity","sharpen_intensity",0,100,0,"%d%%","Enhances fine edges. Zero turns sharpening off.");
                slider("Ignore film grain","ignore_film_grain",0,100,0,"%d%%","Reduces sharpening of grain and small texture variations.");
                ImGui::EndDisabled();
                ImGui::Spacing();
                if(!error.isEmpty())ImGui::TextWrapped("%s",error.toUtf8().constData());
            }ImGui::EndChild();
            ImGui::Separator();
            bool updateAvailable=releaseStatus["available"].isObject();
            if(chaos_ui::button(updateAvailable?"Update available":"Updates & repair",{ImGui::GetContentRegionAvail().x-80,34},updateAvailable)){
                setVisible(false);QProcess::startDetached(QString::fromUtf8(TRUEHDR_ROOT)+"/build/truehdr-panel",{profile,"--updates"});
            }item("updates");
            ImGui::SameLine();if(chaos_ui::button("More",{-1,34}))ImGui::OpenPopup("More options");
            if(ImGui::BeginPopup("More options")){
                bool reduced=config["reduced_motion"].toBool();
                if(ImGui::Checkbox("Reduce motion",&reduced)){config["reduced_motion"]=reduced;changed=true;}
                if(ImGui::MenuItem("Accessible controls")){setVisible(false);QProcess::startDetached(QString::fromUtf8(TRUEHDR_ROOT)+"/build/truehdr-panel",{profile,"--accessible"});}
                ImGui::EndPopup();
            }
            ImGui::PushFont(uiFonts.small);ImGui::TextColored(chaos_ui::muted,"Settings save automatically");ImGui::SameLine();ImGui::TextColored(chaos_ui::blue,"Alt+Z to close");ImGui::PopFont();
            if(changed)save();
        }ImGui::End();ImGui::PopStyleVar();
    }
    void setCursorOwned(bool own){
        own=own&&controlsScreen&&controlsScreen->geometryF().contains(effects->cursorPos());
        if(cursorOwned==own)return;
        cursorOwned=own;
        if(own)effects->hideCursor();
        else effects->showCursor();
        if(ui){ImGui::SetCurrentContext(ui);ImGui::GetIO().MouseDrawCursor=own;}
    }
    bool desktopUiHasPriority()const{
        auto* window=effects->activeWindow();
        return window&&window->isFullScreen()&&window->windowClass().split(' ').contains("org.kde.spectacle");
    }
    void updateInputFilter(){
        bool needed=visible||capturedButtons!=Qt::NoButton||!capturedKeys.isEmpty();
        if(needed==inputInstalled)return;
        inputInstalled=needed;
        if(needed)input()->installInputEventFilter(&sidebarInput);
        else input()->uninstallInputEventFilter(&sidebarInput);
    }
    bool routeMotion(PointerMotionEvent* e){
        if(!visible||desktopUiHasPriority()){setCursorOwned(false);return capturedButtons!=Qt::NoButton;}
        bool owned=capturedButtons!=Qt::NoButton||(e->buttons==Qt::NoButton&&inputRegion.contains(e->position.toPoint()));
        pointerMotion(e);setCursorOwned(owned);
        return owned;
    }
    bool routeButton(PointerButtonEvent* e){
        bool owned=capturedButtons.testFlag(e->button);
        if(e->state==PointerButtonState::Pressed){
            owned=visible&&!desktopUiHasPriority()&&(capturedButtons!=Qt::NoButton||inputRegion.contains(e->position.toPoint()));
            keyboardFocus=owned;
            if(owned)capturedButtons|=e->button;
            else if(ui){ImGui::SetCurrentContext(ui);ImGui::GetIO().ClearInputKeys();}
        }else capturedButtons&=~e->button;
        if(owned&&visible)pointerButton(e);
        updateInputFilter();return owned;
    }
    bool routeAxis(PointerAxisEvent* e){
        if(!visible||desktopUiHasPriority()||!inputRegion.contains(e->position.toPoint()))return false;
        pointerAxis(e);return true;
    }
    bool routeKey(KeyboardKeyEvent* e){
        bool released=e->state==KeyboardKeyState::Released;
        bool owned=capturedKeys.contains(e->nativeScanCode);
        if(released)capturedKeys.remove(e->nativeScanCode);
        else if(visible&&!desktopUiHasPriority()&&keyboardFocus&&e->key!=Qt::Key_Print&&e->key!=Qt::Key_Meta&&!e->modifiers.testFlag(Qt::MetaModifier)){
            owned=true;capturedKeys.insert(e->nativeScanCode);
        }
        if(owned&&visible){
            QKeyEvent key(released?QEvent::KeyRelease:QEvent::KeyPress,e->key,e->modifiers,e->text,e->state==KeyboardKeyState::Repeated);
            grabbedKeyboardEvent(&key);
        }
        updateInputFilter();return owned;
    }
    void setVisible(bool on){
        if(visible==on)return;
        if(!on)clipboardText.clear();
        if(on){
            profileNames.clear();
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


            effects->makeOpenGLContextCurrent();uiContext();auto& io=ImGui::GetIO();io.ClearEventsQueue();io.ClearInputKeys();
            auto point=effects->cursorPos();io.AddMousePosEvent(point.x()-panelGeometry.x(),point.y()-panelGeometry.y());
            for(int n=0;n<3;++n)io.AddMouseButtonEvent(n,false);
            io.MouseDrawCursor=false;
            keyboardFocus=true;
            input()->pointer()->setEnableConstraints(false);
        }
        panelMotion.set(on,uiTime(),config["reduced_motion"].toBool());visible=on;closing=!on&&panelMotion.value(uiTime())>0;
        updateActivity();
        if(!on){
            keyboardFocus=false;inputRegion={};setCursorOwned(false);input()->pointer()->setEnableConstraints(true);
            if(ui){ImGui::SetCurrentContext(ui);ImGui::GetIO().ClearEventsQueue();ImGui::GetIO().ClearInputKeys();}
            if(!closing)controlsScreen=nullptr;
        }
        updateInputFilter();
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
        pasteTimeout.setSingleShot(true);connect(&pasteTimeout,&QTimer::timeout,this,[this]{pasteProcess.kill();});
        connect(&pasteProcess,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus state){
            pasteTimeout.stop();auto text=pasteProcess.readAllStandardOutput();
            if(code||state!=QProcess::NormalExit||text.size()>4096||!visible||!ui||!keyboardFocus)return;
            ImGui::SetCurrentContext(ui);
            if(ImGui::GetActiveID()==pasteField&&ImGui::GetIO().WantTextInput){
                clipboardText=text;auto& io=ImGui::GetIO();bool control=io.KeyCtrl;
                io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_V,true);io.AddKeyEvent(ImGuiKey_V,false);io.AddKeyEvent(ImGuiMod_Ctrl,control);repaintOverlay();
            }
        });
        connect(&retireTimer,&QTimer::timeout,this,[this]{pollRetirement();});
        queueWatch.setSingleShot(true);connect(&queueWatch,&QTimer::timeout,this,[this]{
            if(!gpu)return;
            try{if(gpu->ready()){effects->addRepaintFull();return;}}catch(const std::exception&){}
            effects->makeOpenGLContextCurrent();release();status(phase());
        });
        retryTimer.setSingleShot(true);connect(&retryTimer,&QTimer::timeout,this,[this]{directDisabled=false;failed=false;directFallback.clear();effects->addRepaintFull();});
        connect(effects,&EffectsHandler::screenRemoved,this,[this](LogicalOutput* screen){dismissRemovedScreen(screen);});
        persistTimer.setSingleShot(true);connect(&persistTimer,&QTimer::timeout,this,[this]{persist();});
        QString recoveryError;if(!profiles::Store(profile).recover(recoveryError))error=recoveryError;
        read();statusTimer.start();toggleStamp=QFileInfo(root+"/overlay.toggle").lastModified().toMSecsSinceEpoch();
        auto readRelease=[this]{QFile file(root+"/output/updates/status.json");if(file.open(QIODevice::ReadOnly))releaseStatus=QJsonDocument::fromJson(file.readAll()).object();};
        readRelease();QDir().mkpath(root+"/output/updates");settingsWatch.addPath(root+"/output/updates");
        connect(&settingsWatch,&QFileSystemWatcher::directoryChanged,this,[this,readRelease](const QString& path){if(path==root+"/output/updates"){readRelease();if(visible)repaintOverlay();}});
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
            if(visible)repaintOverlay();
            if(statusTimer.elapsed()>=1000){
                if(gpu&&!queueWatch.isActive()){
                    bool ready=false;try{ready=gpu->ready();}catch(const std::exception&){}
                    if(!ready)queueWatch.start(1000);
                }
                status(phase());statusTimer.restart();
            }
        });updateActivity();
        QDBusConnection::sessionBus().registerObject("/TrueHDR",this,QDBusConnection::ExportAllSlots);
        connect(effects,&EffectsHandler::windowAdded,this,[this](EffectWindow* w){if(!selectedWindow&&knownWindow(w))selectWindow(w,false);});
        connect(effects,&EffectsHandler::windowActivated,this,[this](EffectWindow* w){
            keyboardFocus=false;
            if(ui){ImGui::SetCurrentContext(ui);ImGui::GetIO().ClearInputKeys();}
            if(knownWindow(w))selectWindow(w,false);
        });
        connect(effects,&EffectsHandler::windowDeleted,this,[this](EffectWindow* w){if(target(w)){resetWindow();selectedWindow=nullptr;updateActivity();status("waiting-for-game");}});
        connect(effects,&EffectsHandler::activeFullScreenEffectChanged,this,[this]{if(effects->activeFullScreenEffect())setVisible(false);});
        connect(effects,&EffectsHandler::screenLockingChanged,this,[this](bool locked){if(locked){setVisible(false);closing=false;controlsScreen=nullptr;capturedButtons=Qt::NoButton;capturedKeys.clear();updateInputFilter();}});
        status("loaded");effects->addRepaintFull();
    }
    ~TrueHDRDesktop()override{
        persist();setVisible(false);QDBusConnection::sessionBus().unregisterObject("/TrueHDR");
        if(inputInstalled)input()->uninstallInputEventFilter(&sidebarInput);
        effects->makeOpenGLContextCurrent();release();pollRetirement();if(retiredGpu)abandonGpu();if(ui){ImGui::SetCurrentContext(ui);ImGui_ImplOpenGL3_Shutdown();ImGui::DestroyContext(ui);}writeStatus("unloaded");
        for(auto& sample:timings)if(sample.queries[0])glDeleteQueriesEXT(4,sample.queries);
#ifdef TRUEHDR_DEVELOPMENT
        overlayMetrics.release();
        if(fixtureInput)input()->removeInputDevice(fixtureInput.get());
#endif
    }
    bool isActive()const override{return true;}
    void prePaintScreen(ScreenPrePaintData& data)override{
        if((visible||closing)&&controlsScreen==data.screen){
            auto area=controlsScreen->geometry();data.paint|=Rect(area.x(),area.y(),std::min(area.width(),overlayWidth),area.height());
        }
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
    bool blocksDirectScanout()const override{
        if(visible||closing)return true;
        if(gpuBlocked||retiredGpu||unloading)return false;
        auto* w=selectedWindow.data();if(!w||w!=effects->activeWindow()||!processingEnabled()||directDisabled||failed)return false;
        return w->screen()&&directSurface(w->window()->surfaceItem(),w->clientGeometry().size().toSize()*w->screen()->scale());
    }
    bool drawWindow(const RenderTarget& targetBuffer,const RenderViewport& viewport,EffectWindow* window,int mask,const Region& region,WindowPaintData& data)override{
        if(target(window)&&pictureScreen!=window->screen())updateDisplay();
        if(renderingInput||failed||gpuBlocked||retiredGpu||unloading||!target(window)||!processingEnabled())return effects->drawWindow(targetBuffer,viewport,window,mask,region,data);
        if(region.isEmpty())return true;

        auto surface=window->window()->surfaceItem();
        if(surface&&surface->colorDescription()->transferFunction().type==TransferFunction::PerceptualQuantizer){directFallback="This window already supplies HDR.";return effects->drawWindow(targetBuffer,viewport,window,mask,region,data);}
        if(!opaqueSurface(surface)){directFallback="Transparent surfaces are left unchanged to preserve their alpha channel.";return effects->drawWindow(targetBuffer,viewport,window,mask,region,data);}
        auto start=std::chrono::steady_clock::now();
        try{
            auto geometry=window->clientGeometry();QSize size=geometry.size().toSize()*viewport.scale();
            if(size.isEmpty())return effects->drawWindow(targetBuffer,viewport,window,mask,region,data);
            auto buffer=surface?surface->buffer():nullptr;
            auto dma=buffer?buffer->dmabufAttributes():nullptr;
            bool direct=preferDirect&&!directDisabled&&directSurface(surface,size);
            if(development&&buffer)bufferAudit={{"dma_buf",bool(dma)},{"planes",dma?dma->planeCount:0},{"opaque",opaqueSurface(surface)},
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
                release();if(retiredGpu||gpuBlocked)return effects->drawWindow(targetBuffer,viewport,window,mask,region,data);
                try{gpu=std::make_unique<DesktopGPU>(size.width(),size.height(),direct);}
                catch(const std::exception& e){if(!direct)throw;processingFailure(QString::fromUtf8(e.what()));
                    if(!fixtureCapture){release();return effects->drawWindow(targetBuffer,viewport,window,mask,region,data);}
                    direct=false;gpu=std::make_unique<DesktopGPU>(size.width(),size.height());}
#ifdef TRUEHDR_DEVELOPMENT
                ++gpuCreations;
#endif
                dimensions=size;directActive=direct;
                if(!direct)source=GLTexture::createNonOwningWrapper(gpu->inputTexture(),GL_RGBA8,size);
                output=GLTexture::createNonOwningWrapper(gpu->outputTexture(),GL_RGB10_A2,size);
                if(direct)output->setContentTransform(OutputTransform::FlipY);
                if(!direct){inputFbo=std::make_unique<GLFramebuffer>(source.get());
                    if(!inputFbo->valid())throw std::runtime_error("Desktop input framebuffer is invalid");}
            }
            bool process=gpu->ready();
            if(!process){if(!queueWatch.isActive())queueWatch.start(1000);}else if(queueWatch.isActive())queueWatch.stop();
            DesktopGPU::DmaInput input;
            if(direct&&process){
                input.width=dma->width;input.height=dma->height;input.format=dma->format;input.planes=dma->planeCount;input.modifier=dma->modifier;input.pitch=dma->pitch;input.offset=dma->offset;input.acquireFd=surface->syncFd().get();
                for(int n=0;n<dma->planeCount;++n)input.fd[n]=dma->fd[n].get();
            }
            if(!process&&!outputValid)return effects->drawWindow(targetBuffer,viewport,window,mask,region,data);
            if(process)gpu->setSettings(PictureSettings::from(config,displayPeak));
            auto timing=process?beginTiming():nullptr;
            const auto convertStarted=process&&development&&frameTraceEnabled?desktop_metrics::now():0;
            if(direct&&process){
                if(outputValid)gpu->releaseOutput();
                if(timing)glQueryCounterEXT(timing->queries[1],GL_TIMESTAMP_EXT);
                try{
#ifdef TRUEHDR_DEVELOPMENT
                    if(testFailures>0){--testFailures;throw std::runtime_error("Isolated fixture import failure");}
#endif
                    inFlightBuffer=buffer;
                    auto wayland=qobject_cast<SurfaceItemWayland*>(surface);
                    inFlightRelease=wayland&&wayland->surface()?wayland->surface()->bufferReleasePoint():nullptr;
                    gpu->convertDirect(input,sharpening(),config["ignore_film_grain"].toInt()/100.f,timingEnabled,hdrProcessing());}
                catch(const std::exception& e){processingFailure(QString::fromUtf8(e.what()));release();return drawWindow(targetBuffer,viewport,window,mask,region,data);}



                if(error==processingError)error.clear();processingError.clear();retryCount=0;
            }else if(process){
            RenderTarget inputTarget(inputFbo.get(),ColorDescription::sRGB);RenderViewport inputViewport(geometry,viewport.scale(),inputTarget,QPoint());
            GLFramebuffer::pushFramebuffer(inputFbo.get());glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
            WindowPaintData inputData;inputData.setOpacity(1);renderingInput=true;
            bool ok=effects->drawWindow(inputTarget,inputViewport,window,PAINT_WINDOW_TRANSFORMED|PAINT_WINDOW_TRANSLUCENT,Region::infinite(),inputData);
            renderingInput=false;GLFramebuffer::popFramebuffer();
            if(!ok)return false;
            if(timing)glQueryCounterEXT(timing->queries[1],GL_TIMESTAMP_EXT);
            gpu->convert(sharpening(),config["ignore_film_grain"].toInt()/100.f,timingEnabled,hdrProcessing());
            }
            if(process){outputValid=true;outputHdr=hdrProcessing();}
            const auto convertFinished=process&&development&&frameTraceEnabled?desktop_metrics::now():0;
            if(timing)glQueryCounterEXT(timing->queries[2],GL_TIMESTAMP_EXT);
            if(process&&timingEnabled){auto values=gpu->timings();gpuAudit["vulkan_prepare_ms"]=values.prepare;gpuAudit["vulkan_engine_ms"]=values.engine;gpuAudit["vulkan_total_ms"]=values.total;gpuAudit["vulkan_samples"]=double(values.samples);}
            ShaderBinder binder(ShaderTrait::MapTexture|ShaderTrait::TransformColorspace|ShaderTrait::Modulate);
            auto shader=ShaderManager::instance()->getBoundShader();QMatrix4x4 mvp=viewport.projectionMatrix();
            mvp.translate(std::round(geometry.x()*viewport.scale()),std::round(geometry.y()*viewport.scale()));
            shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix,mvp*data.toMatrix(viewport.scale()));
            shader->setUniform(GLShader::Mat4Uniform::TextureMatrix,output->matrix(NormalizedCoordinates));
            shader->setUniform(GLShader::Vec4Uniform::ModulationConstant,QVector4D(data.opacity(),data.opacity(),data.opacity(),data.opacity()));
            auto backend=window->screen()?window->screen()->backendOutput():nullptr;




            double reference=backend?backend->referenceLuminance():targetBuffer.colorDescription()->referenceLuminance();
            if(outputHdr&&(!hdrColor||hdrReference!=reference)){
                hdrColor=std::make_shared<ColorDescription>(Colorimetry::BT2020,TransferFunction(TransferFunction::PerceptualQuantizer,0,10000),reference,0,displayPeak,displayPeak);hdrReference=reference;
            }
            shader->setUniform(GLShader::IntUniform::Sampler,0);shader->setColorspaceUniforms(outputHdr?hdrColor:ColorDescription::sRGB,targetBuffer.colorDescription(),outputHdr?RenderingIntent::AbsoluteColorimetricNoAdaptation:RenderingIntent::Perceptual);
            if constexpr(development)colorAudit={{"hdr_source_peak",displayPeak},{"hdr_processing",outputHdr},{"hdr_source_reference",reference},{"target_reference",targetBuffer.colorDescription()->referenceLuminance()},
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
            if(timing){glQueryCounterEXT(timing->queries[3],GL_TIMESTAMP_EXT);timing->pending=true;}
            const auto finished=development?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
            if constexpr(development){if(process){++frames;cpuTotal+=std::chrono::duration<double,std::milli>(finished-start).count();}}
            if(process&&development&&frameTraceEnabled&&frameCosts.size()<20000){const auto previous=gpu->timings();frameCosts.push_back({
                std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count(),
                std::chrono::duration_cast<std::chrono::nanoseconds>(finished.time_since_epoch()).count(),
                (convertFinished-convertStarted)/1e6,previous.lastPrepare,previous.lastEngine,previous.lastTotal,previous.samples});}
            if(statusTimer.elapsed()>1000){status("filtering");statusTimer.restart();}
            return true;
        }catch(const std::exception& e){renderingInput=false;processingFailure(QString::fromUtf8(e.what()));release();failed=true;status("error");
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
        if(!ok||(!visible&&!closing))return ok;
        if(controlsScreen!=screen)return ok;
        if(desktopUiHasPriority()){inputRegion={};setCursorOwned(false);return ok;}
        auto animationTime=uiTime();panelProgress=panelMotion.value(animationTime);
        if(!visible&&!panelMotion.moving(animationTime)){
#ifdef TRUEHDR_DEVELOPMENT
            if(previewCapture){
                QImage empty(viewport.deviceSize(),QImage::Format_RGBA8888);empty.fill(Qt::transparent);
                QString file=QString("frame-%1.png").arg(previewIndex++,4,10,QChar('0'));empty.save(root+"/motion-preview/"+file);
                previewTimeline.append(QJsonObject{{"file",file},{"time",uiTime()-previewStart},{"progress",0},{"open",false}});
            }
#endif
            closing=false;controlsScreen=nullptr;return ok;
        }
        if(panelMotion.moving(animationTime))repaintOverlay();
#ifdef TRUEHDR_DEVELOPMENT
        overlayMetrics.begin();
#endif
        uiContext();auto& io=ImGui::GetIO();auto rect=viewport.renderRect();panelGeometry=rect;
        availablePanel=QRectF(0,0,rect.width(),rect.height());
        io.DisplaySize={float(rect.width()),float(rect.height())};io.DisplayFramebufferScale={float(viewport.scale()),float(viewport.scale())};
        auto now=std::chrono::steady_clock::now();io.DeltaTime=std::max(.001f,std::chrono::duration<float>(now-previous).count());previous=now;
        QString text=profileNames.join(' ')+profiles::active(config)+QString::fromUtf8(name);
        for(auto* window:effects->stackingOrder())if(applicationWindow(window))text+=' '+window->caption();
        if(text!=glyphText&&!glyphPending){glyphPending=true;QTimer::singleShot(0,this,[this,text]{
            glyphPending=false;glyphText=text;if(!ui)return;effects->makeOpenGLContextCurrent();ImGui::SetCurrentContext(ui);
            if(fontFallback.add(text,uiFonts)){ImGui_ImplOpenGL3_DestroyFontsTexture();repaintOverlay();}
        });}
        chaos_ui::beginOpenGLFrame();ImGui::NewFrame();controls();
        inputRegion={};
        if(visible)for(auto* window:ImGui::GetCurrentContext()->Windows){
            if(!window->Active||window->Hidden||(window->Flags&ImGuiWindowFlags_NoMouseInputs))continue;
            auto bounds=window->OuterRectClipped;
            inputRegion+=QRectF(rect.x()+bounds.Min.x,rect.y()+bounds.Min.y,bounds.GetWidth(),bounds.GetHeight()).toAlignedRect();
        }
        setCursorOwned(visible&&(capturedButtons!=Qt::NoButton||(input()->pointer()->buttons()==Qt::NoButton&&inputRegion.contains(effects->cursorPos().toPoint()))));
        ImGui::Render();
        if constexpr(development)inputAudit={{"screen",screen->name()},{"origin_x",rect.x()},{"origin_y",rect.y()},
            {"mouse_x",io.MousePos.x},{"mouse_y",io.MousePos.y},{"want_mouse",io.WantCaptureMouse},
            {"mouse_down",io.MouseDown[0]},{"overlay_cursor",cursorOwned},{"regional_input",true},
            {"fullscreen_input_owned",effects->activeFullScreenEffect()==this},{"panel_progress",panelProgress},{"panel_closing",closing}};



        float right=452;
        for(auto* window:ImGui::GetCurrentContext()->Windows)if(window->Active&&!window->Hidden)
            right=std::max(right,window->Pos.x+window->Size.x+24);
        if(cursorOwned)right=std::max(right,io.MousePos.x+32);
        int width=std::min(int(rect.width()),int(std::ceil(right/64))*64);
        if(width!=overlayWidth){int previousWidth=overlayWidth;overlayWidth=std::max(width,previousWidth);repaintOverlay();overlayWidth=width;}
        QSize textureSize(qCeil(width*viewport.scale()),viewport.deviceSize().height());
        if(!uiTexture||uiTexture->size()!=textureSize){
            uiFbo.reset();uiTexture=GLTexture::allocate(GL_RGBA8,textureSize);uiFbo=std::make_unique<GLFramebuffer>(uiTexture.get());
        }
#ifdef TRUEHDR_DEVELOPMENT
        if(testOverlayClip){glEnable(GL_SCISSOR_TEST);glScissor(0,0,8,8);}
#endif
        const bool scissorEnabled=glIsEnabled(GL_SCISSOR_TEST),blendEnabled=glIsEnabled(GL_BLEND);
        GLint srcRgb,dstRgb,srcAlpha,dstAlpha;
        glGetIntegerv(GL_BLEND_SRC_RGB,&srcRgb);glGetIntegerv(GL_BLEND_DST_RGB,&dstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
        glDisable(GL_SCISSOR_TEST);
        GLFramebuffer::pushFramebuffer(uiFbo.get());glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
        auto* drawData=ImGui::GetDrawData();auto displaySize=drawData->DisplaySize;
        drawData->DisplaySize.x=textureSize.width()/float(viewport.scale());
        ImGui_ImplOpenGL3_RenderDrawData(drawData);drawData->DisplaySize=displaySize;GLFramebuffer::popFramebuffer();
#ifdef TRUEHDR_DEVELOPMENT
        if(previewCapture&&previewIndex<300&&(previewLast<0||uiTime()-previewLast>=1.0/60)){
            previewLast=uiTime();QString file=QString("frame-%1.png").arg(previewIndex++,4,10,QChar('0'));
            uiTexture->toImage().mirrored(false,true).save(root+"/motion-preview/"+file);
            previewTimeline.append(QJsonObject{{"file",file},{"time",previewLast-previewStart},{"progress",panelProgress},{"open",visible}});
        }
#endif
        ShaderBinder binder(ShaderTrait::MapTexture|ShaderTrait::TransformColorspace|ShaderTrait::Modulate);
        auto shader=ShaderManager::instance()->getBoundShader();auto mvp=viewport.projectionMatrix();
        mvp.translate(rect.x()*viewport.scale(),rect.y()*viewport.scale());
        shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix,mvp);
        shader->setUniform(GLShader::IntUniform::Sampler,0);shader->setUniform(GLShader::Vec4Uniform::ModulationConstant,QVector4D(1,1,1,1));
        shader->setColorspaceUniforms(ColorDescription::sRGB,targetBuffer.colorDescription(),RenderingIntent::Perceptual);
        glActiveTexture(GL_TEXTURE0);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        uiTexture->bind();uiTexture->render(uiTexture->size());uiTexture->unbind();
        glBlendFuncSeparate(srcRgb,dstRgb,srcAlpha,dstAlpha);
        if(!blendEnabled)glDisable(GL_BLEND);
        if(scissorEnabled)glEnable(GL_SCISSOR_TEST);
#ifdef TRUEHDR_DEVELOPMENT
        overlayMetrics.end();
#endif
        return true;
    }
    void pointerMotion(PointerMotionEvent* e)override{if(ui&&visible){ImGui::SetCurrentContext(ui);ImGui::GetIO().AddMousePosEvent(e->position.x()-panelGeometry.x(),e->position.y()-panelGeometry.y());repaintOverlay();}}
    void pointerButton(PointerButtonEvent* e)override{if(ui&&visible){ImGui::SetCurrentContext(ui);auto& io=ImGui::GetIO();io.AddMousePosEvent(e->position.x()-panelGeometry.x(),e->position.y()-panelGeometry.y());int button=e->button==Qt::LeftButton?0:e->button==Qt::RightButton?1:2;io.AddMouseButtonEvent(button,e->state==PointerButtonState::Pressed);repaintOverlay();}}
    void pointerAxis(PointerAxisEvent* e)override{
        if(!ui||!visible)return;ImGui::SetCurrentContext(ui);
        float amount=e->deltaV120?float(e->deltaV120)/120.f:float(e->delta)/15.f;
        ImGui::GetIO().AddMouseWheelEvent(e->orientation==Qt::Horizontal?-amount:0,e->orientation==Qt::Vertical?-amount:0);repaintOverlay();
    }
    void grabbedKeyboardEvent(QKeyEvent* e)override{
        if(!ui||!visible)return;ImGui::SetCurrentContext(ui);auto& io=ImGui::GetIO();bool down=e->type()==QEvent::KeyPress;
        if(down&&(e->key()==Qt::Key_Escape||(e->key()==Qt::Key_Z&&e->modifiers().testFlag(Qt::AltModifier)))){setVisible(false);return;}
        if(down&&e->key()==Qt::Key_V&&e->modifiers().testFlag(Qt::ControlModifier)){pasteText();return;}
        ImGuiKey key=ImGuiKey_None;
        if(e->key()>=Qt::Key_A&&e->key()<=Qt::Key_Z)key=ImGuiKey(ImGuiKey_A+e->key()-Qt::Key_A);
        switch(e->key()){case Qt::Key_PageUp:key=ImGuiKey_PageUp;break;case Qt::Key_PageDown:key=ImGuiKey_PageDown;break;case Qt::Key_Tab:key=ImGuiKey_Tab;break;case Qt::Key_Space:key=ImGuiKey_Space;break;case Qt::Key_Up:key=ImGuiKey_UpArrow;break;case Qt::Key_Down:key=ImGuiKey_DownArrow;break;case Qt::Key_Home:key=ImGuiKey_Home;break;case Qt::Key_End:key=ImGuiKey_End;break;case Qt::Key_Backspace:key=ImGuiKey_Backspace;break;case Qt::Key_Return:case Qt::Key_Enter:key=ImGuiKey_Enter;break;case Qt::Key_Left:key=ImGuiKey_LeftArrow;break;case Qt::Key_Right:key=ImGuiKey_RightArrow;break;case Qt::Key_Delete:key=ImGuiKey_Delete;break;default:break;}
        auto modifiers=e->modifiers();
        if(e->key()==Qt::Key_Control)modifiers.setFlag(Qt::ControlModifier,down);
        if(e->key()==Qt::Key_Shift)modifiers.setFlag(Qt::ShiftModifier,down);
        if(e->key()==Qt::Key_Alt)modifiers.setFlag(Qt::AltModifier,down);
        io.AddKeyEvent(ImGuiMod_Ctrl,modifiers.testFlag(Qt::ControlModifier));io.AddKeyEvent(ImGuiMod_Shift,modifiers.testFlag(Qt::ShiftModifier));io.AddKeyEvent(ImGuiMod_Alt,modifiers.testFlag(Qt::AltModifier));
        if(key!=ImGuiKey_None)io.AddKeyEvent(key,down);
        if(down&&!e->modifiers().testFlag(Qt::ControlModifier)&&!e->modifiers().testFlag(Qt::AltModifier)&&!e->text().isEmpty()&&e->text().at(0).isPrint())io.AddInputCharactersUTF8(e->text().toUtf8().constData());effects->addRepaintFull();
    }
public Q_SLOTS:
    bool prepareUnload(){unloading=true;setVisible(false);effects->makeOpenGLContextCurrent();release();pollRetirement();return !retiredGpu;}
    void testGpuRetirement(int mode,bool retire){
#ifdef TRUEHDR_DEVELOPMENT
        if(qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))return;
        if(auto context=gpu?gpu.get():retiredGpu?retiredGpu->gpu.get():nullptr)context->testCompletion(mode);
        if(retire){effects->makeOpenGLContextCurrent();release();}
#endif
    }
    QString testGpuState(){
#ifdef TRUEHDR_DEVELOPMENT
        if(!qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))return QString::fromUtf8(QJsonDocument(QJsonObject{
            {"active",bool(gpu)},{"retiring",bool(retiredGpu)},{"buffer_retained",retiredGpu&&bool(retiredGpu->buffer)},
            {"blocked",gpuBlocked},{"creations",gpuCreations},{"retire_timer",retireTimer.isActive()},{"queue_timer",queueWatch.isActive()}}).toJson());
#endif
        return "{}";
    }
    bool ping(){return true;}
    QString applications(){
        QJsonArray result;for(auto* window:effects->stackingOrder())if(applicationWindow(window))
            result.append(QJsonObject{{"id",window->window()->internalId().toString()},{"title",window->caption()}});
        return QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
    }
    bool setApplicationAllowed(QString id,bool allowed){
        for(auto* window:effects->stackingOrder())if(applicationWindow(window)&&window->window()->internalId().toString()==id){
            if(allowed)return selectWindow(window,true);
            rememberedClasses.removeAll(window->windowClass());
            if(window->windowClass().contains(targetClass,Qt::CaseInsensitive))legacyAllowed=false;
            saveTargets();if(selectedWindow==window){resetWindow();selectedWindow=nullptr;}
            updateActivity();status(phase());return true;
        }
        return false;
    }
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
    void testPreviewCapture(bool enabled){
#ifdef TRUEHDR_DEVELOPMENT
        if(qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))return;
        previewCapture=enabled;
        if(enabled){previewIndex=0;previewStart=uiTime();previewLast=-1;previewTimeline={};QDir().mkpath(root+"/motion-preview");}
        else{QString error;profiles::write(root+"/motion-preview/timing.json",{{"frames",previewTimeline}},error);}
#endif
    }
    void setEnabled(bool enabled){config["enabled"]=enabled;failed=false;save();status(enabled?"enabled":"disabled");}
    void setTiming(bool enabled){if constexpr(development)timingEnabled=enabled;}
    void setFrameTrace(bool enabled){
        if constexpr(!development)return;
        frameTraceEnabled=enabled;timingEnabled=enabled;
#ifdef TRUEHDR_DEVELOPMENT
        overlayMetrics.reset(enabled);
#endif
        frameCosts.clear();
        if(enabled){frameCosts.reserve(20000);presentation=std::make_shared<desktop_metrics::State>();presentation->traceMode=true;}
        else{if(presentation&&presentation->traceMode)presentation.reset();frameCosts.shrink_to_fit();}
        status("measurement");
    }
    QString frameTrace(){
        if constexpr(!development)return "{}";
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
#ifdef TRUEHDR_DEVELOPMENT
            {"overlay",overlayMetrics.result()},
#endif
            {"scope","Optional bounded diagnostics only. Commit times are KDE-observed receipt, which can itself be delayed if its event loop stalls. CPU conversion includes submission and waits, not isolated GPU execution. GPU per-frame values describe the previously completed HDR job. Frame generation is not distinguished."}
        }).toJson(QJsonDocument::Compact));
    }
    void setPresentationTiming(bool enabled){if constexpr(!development)return;if(enabled)presentation=std::make_shared<desktop_metrics::State>();else if(presentation)presentation->enabled=false;status("measurement");effects->addRepaintFull();}
    QString presentationTiming(){if constexpr(!development)return "{}";return QString::fromUtf8(QJsonDocument(presentation?presentation->result():QJsonObject{}).toJson(QJsonDocument::Compact));}
    void setDirect(bool enabled){if constexpr(!development)return;effects->makeOpenGLContextCurrent();release();preferDirect=enabled;directDisabled=false;directFallback.clear();timingSamples=0;for(auto& total:gpuTotals)total=0;for(auto& sample:timings)sample.pending=false;gpuAudit={};effects->addRepaintFull();}
    void testPointer(double x,double y,int state){
        if(!development||qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))return;
        PointerMotionEvent motion{};motion.position={x,y};pointerMotion(&motion);
        if(state>=0){PointerButtonEvent button{};button.position={x,y};button.button=Qt::LeftButton;button.state=state?PointerButtonState::Pressed:PointerButtonState::Released;pointerButton(&button);}
    }
    void testSystemPointer(double x,double y,int state){
#ifdef TRUEHDR_DEVELOPMENT
        if(qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))return;
        auto now=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
        input()->pointer()->processMotionAbsolute({x,y},now);
        if(state>=0)input()->pointer()->processButton(272,state?PointerButtonState::Pressed:PointerButtonState::Released,now);
        input()->pointer()->processFrame();
#endif
    }
    void testImportFailures(int count){
#ifdef TRUEHDR_DEVELOPMENT
        if(qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))return;
        testFailures=std::clamp(count,0,10);retryCount=0;directDisabled=false;failed=false;effects->addRepaintFull();
#endif
    }
    QString testRecoveryState(){
#ifdef TRUEHDR_DEVELOPMENT
        if(!qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))return QString::fromUtf8(QJsonDocument(QJsonObject{{"remaining",testFailures},{"retries",retryCount},{"disabled",directDisabled},{"pending",retryTimer.isActive()},{"frames",double(frames)}}).toJson());
#endif
        return "{}";
    }
    void testDisplay(int index,int peak,bool hdr,bool move){
#ifdef TRUEHDR_DEVELOPMENT
        if(qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))return;
        auto screens=effects->screens();if(index<0||index>=screens.size())return;
        auto* screen=screens[index];testDisplays[screen->name()]={peak,hdr};
        if(move&&selectedWindow)selectedWindow->window()->sendToOutput(screen);
        updateDisplay();status(phase());effects->addRepaintFull();
#endif
    }
    QString testTextState(){
#ifdef TRUEHDR_DEVELOPMENT
        if(qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT")||!ui)return "{}";
        ImGui::SetCurrentContext(ui);QJsonArray missing;
        for(auto code:QString::fromUtf8(name).toUcs4())if(!uiFonts.body->FindGlyphNoFallback(code))missing.append(int(code));
        return QString::fromUtf8(QJsonDocument(QJsonObject{{"text",QString::fromUtf8(name)},{"missing",missing}}).toJson());
#else
        return "{}";
#endif
    }
    void testInputDevice(){
#ifdef TRUEHDR_DEVELOPMENT
        if(qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT")||fixtureInput)return;
        fixtureInput=std::make_unique<FixtureInputDevice>();input()->addInputDevice(fixtureInput.get());
#endif
    }
    QString testInputState(){
#ifdef TRUEHDR_DEVELOPMENT
        if(!qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))return QString::fromUtf8(QJsonDocument(QJsonObject{
            {"active",effects->activeWindow()?effects->activeWindow()->windowClass():QString()},
            {"keyboard_focus",keyboardFocus},{"captured_buttons",int(capturedButtons)},{"input_installed",inputInstalled},
            {"overlay_cursor",cursorOwned},{"native_cursor_hidden",effects->isCursorHidden()}}).toJson());
#endif
        return "{}";
    }
    void testOverlayClipping(bool enabled){
#ifdef TRUEHDR_DEVELOPMENT
        if(!qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT")){testOverlayClip=enabled;effects->addRepaintFull();}
#endif
    }
    void testSystemKey(uint scan,bool pressed){
#ifdef TRUEHDR_DEVELOPMENT
        if(qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))return;
        auto now=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
        input()->keyboard()->processKey(scan,pressed?KeyboardKeyState::Pressed:KeyboardKeyState::Released,now);
#endif
    }
    void testScroll(double amount){
        if(!development||qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT")||!visible||!ui)return;
        ImGui::SetCurrentContext(ui);ImGui::GetIO().AddMouseWheelEvent(0,float(amount));effects->addRepaintFull();
    }
    void testKey(int key,bool pressed){
        if(!development||qEnvironmentVariableIsEmpty("TRUEHDR_DESKTOP_TEST_ROOT"))return;
        QKeyEvent event(pressed?QEvent::KeyPress:QEvent::KeyRelease,key,Qt::NoModifier);grabbedKeyboardEvent(&event);
    }
};

KWIN_EFFECT_FACTORY_SUPPORTED(TrueHDRDesktop,"effect.json",return effects->compositingType()==OpenGLCompositing;)
#include "effect.moc"
