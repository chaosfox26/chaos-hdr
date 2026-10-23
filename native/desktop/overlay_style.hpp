#pragma once
#include "imgui.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>

namespace chaos_ui {
inline const ImVec4 white{.96f,.97f,1.f,1.f};
inline const ImVec4 muted{.76f,.78f,.85f,1.f};
inline const ImVec4 pink{1.f,.24f,.71f,1.f};
inline const ImVec4 blue{.23f,.65f,1.f,1.f};
inline const ImVec4 panel{.025f,.027f,.041f,.74f};
struct Fonts {ImFont *body{},*small{},*bold{},*brand{};};

inline Fonts setup(const std::string& root){
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    io.FontGlobalScale=1.f;
    auto load=[&](const char* file,float size){
        auto path=root+"/vendor/fonts/"+file;
        if(!std::filesystem::is_regular_file(path))return io.Fonts->AddFontDefault();
        ImFontConfig config;config.OversampleH=3;config.OversampleV=2;
        return io.Fonts->AddFontFromFileTTF(path.c_str(),size,&config);
    };
    Fonts fonts{load("Barlow-Regular.ttf",18),load("Barlow-Regular.ttf",15),
                load("Barlow-SemiBold.ttf",18),load("Audiowide-Regular.ttf",52)};
    io.FontDefault=fonts.body;
    ImGui::StyleColorsDark();auto& s=ImGui::GetStyle();
    s.WindowPadding={24,18};s.FramePadding={12,7};s.ItemSpacing={10,7};s.ItemInnerSpacing={8,5};
    s.WindowRounding=0;s.ChildRounding=0;s.FrameRounding=7;s.PopupRounding=9;
    s.ScrollbarSize=6;s.ScrollbarRounding=3;s.GrabMinSize=16;s.GrabRounding=8;
    s.WindowBorderSize=0;s.ChildBorderSize=0;s.PopupBorderSize=1;s.FrameBorderSize=0;
    s.AntiAliasedLines=true;s.AntiAliasedFill=true;
    auto* c=s.Colors;
    c[ImGuiCol_Text]=white;c[ImGuiCol_TextDisabled]=muted;
    c[ImGuiCol_WindowBg]=panel;c[ImGuiCol_ChildBg]={0,0,0,0};c[ImGuiCol_PopupBg]={.045f,.047f,.07f,.995f};
    c[ImGuiCol_Border]={.20f,.22f,.30f,1};
    c[ImGuiCol_FrameBg]={.075f,.08f,.115f,1};c[ImGuiCol_FrameBgHovered]={.13f,.12f,.19f,1};c[ImGuiCol_FrameBgActive]={.19f,.10f,.20f,1};
    c[ImGuiCol_Button]={.10f,.105f,.15f,1};c[ImGuiCol_ButtonHovered]={.20f,.13f,.24f,1};c[ImGuiCol_ButtonActive]={.27f,.14f,.30f,1};
    c[ImGuiCol_Header]={.20f,.09f,.20f,1};c[ImGuiCol_HeaderHovered]={.25f,.12f,.27f,1};c[ImGuiCol_HeaderActive]={.32f,.14f,.32f,1};
    c[ImGuiCol_SliderGrab]=white;c[ImGuiCol_SliderGrabActive]=pink;c[ImGuiCol_CheckMark]=pink;
    c[ImGuiCol_Separator]={.15f,.16f,.22f,1};c[ImGuiCol_SeparatorHovered]=blue;c[ImGuiCol_SeparatorActive]=pink;
    c[ImGuiCol_ScrollbarBg]={0,0,0,0};c[ImGuiCol_ScrollbarGrab]={.23f,.24f,.33f,.8f};
    c[ImGuiCol_ScrollbarGrabHovered]=blue;c[ImGuiCol_ScrollbarGrabActive]=pink;
    c[ImGuiCol_NavCursor]=blue;c[ImGuiCol_TextSelectedBg]={.38f,.13f,.36f,.8f};
    c[ImGuiCol_ModalWindowDimBg]={0,0,0,.55f};
    return fonts;
}

inline void hint(ImFont* font,const char* text){
    ImGui::PushFont(font);ImGui::PushStyleColor(ImGuiCol_Text,muted);
    ImGui::PushTextWrapPos(0);ImGui::TextUnformatted(text);ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();ImGui::PopFont();
}
inline void heading(ImFont* font,const char* text){
    ImGui::PushFont(font);ImGui::TextUnformatted(text);ImGui::PopFont();
}
inline void wordmark(ImFont* font,double time,float available){
    auto origin=ImGui::GetCursorScreenPos();auto* draw=ImGui::GetWindowDrawList();
    float size=52,gap=9;
    float first=font->CalcTextSizeA(size,10000,0,"CHAOS").x;
    float width=first+gap+font->CalcTextSizeA(size,10000,0,"HDR").x;
    float scale=std::max(1.f,available-16)/width;size*=scale;gap*=scale;first*=scale;width*=scale;
    ImGui::Dummy({width,58});
    float phase=std::fmod(time,3.4);
    auto pulse=[&](float center,float spread){float d=(phase-center)/spread;return std::exp(-d*d);};
    float burst=std::min(1.f,pulse(.36f,.13f)+.8f*pulse(.73f,.09f)+.65f*pulse(2.28f,.15f));
    float tick=std::floor(time*17);
    float jitter=std::sin(tick*7.13f),vertical=std::sin(tick*11.79f);
    ImVec2 position{origin.x+burst*jitter*2.4f,origin.y+burst*vertical*1.2f};
    draw->PushClipRect({origin.x-7,origin.y-2},{origin.x+available,origin.y+58},true);
    auto letters=[&](ImVec2 at,float alpha,int channel,float core=0.f){
        ImVec4 a=channel==2?blue:pink,b=channel==1?pink:blue;a.w=b.w=alpha;
        for(auto* color:{&a,&b}){
            color->x+=(1.f-color->x)*core;
            color->y+=(1.f-color->y)*core;
            color->z+=(1.f-color->z)*core;
        }
        draw->AddText(font,size,at,ImGui::GetColorU32(a),"CHAOS");
        draw->AddText(font,size,{at.x+first+gap,at.y},ImGui::GetColorU32(b),"HDR");
    };
    float energy=1.f-.4f*burst*(.5f+.5f*std::sin(time*9));
    constexpr std::array<ImVec2,8> directions{{{1,0},{.7071f,.7071f},{0,1},{-.7071f,.7071f},
                                             {-1,0},{-.7071f,-.7071f},{0,-1},{.7071f,-.7071f}}};
    for(auto direction:directions){
        letters({position.x+direction.x*4.f,position.y+direction.y*4.f},.065f*energy,0);
        letters({position.x+direction.x*1.6f,position.y+direction.y*1.6f},.19f*energy,0);
    }
    if(burst>.015f){
        letters({position.x-5.5f*burst,position.y+burst},.60f*burst,1);
        letters({position.x+5.5f*burst,position.y-burst},.60f*burst,2);
    }
    letters(position,energy,0,.72f);
    if(burst>.12f)for(int slice=0;slice<3;++slice){
        float top=origin.y+size*(.23f+.24f*slice)+vertical;
        draw->PushClipRect({origin.x-7,top},{origin.x+available,top+2+4*burst},true);
        letters({position.x+(slice%2?1:-1)*(3+8*burst)*jitter,position.y},.85f*burst,slice%2+1);
        draw->PopClipRect();
    }
    draw->PopClipRect();
}
inline void tooltip(const char* text){
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)||(ImGui::GetIO().NavVisible&&ImGui::IsItemFocused())){
        ImGui::BeginTooltip();ImGui::PushTextWrapPos(290);ImGui::TextUnformatted(text);ImGui::PopTextWrapPos();ImGui::EndTooltip();
    }
}
inline bool button(const char* text,ImVec2 size={},bool primary=false){
    if(primary){
        ImGui::PushStyleColor(ImGuiCol_Button,{.54f,.09f,.36f,1});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{.66f,.12f,.45f,1});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,{.44f,.07f,.30f,1});
    }
    bool pressed=ImGui::Button(text,size);
    if(primary)ImGui::PopStyleColor(3);
    return pressed;
}
inline bool toggle(const char* id,bool& value){
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0,0});
    bool pressed=ImGui::Button(id,{42,24});ImGui::PopStyleVar();
    if(pressed)value=!value;
    auto p=ImGui::GetItemRectMin();auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(p,{p.x+42,p.y+24},ImGui::GetColorU32(value?pink:ImVec4{.22f,.24f,.31f,1}),12);
    draw->AddCircleFilled({p.x+(value?30:12),p.y+12},8,ImGui::GetColorU32(white));
    return pressed;
}
inline bool slider(const char* id,int& value,int low,int high){
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0,5});
    for(auto color:{ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive,ImGuiCol_SliderGrab,ImGuiCol_SliderGrabActive})
        ImGui::PushStyleColor(color,{0,0,0,0});
    ImGui::SetNextItemWidth(-1);
    bool changed=ImGui::SliderInt(id,&value,low,high,"",ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoInput);
    ImGui::PopStyleColor(5);ImGui::PopStyleVar();
    auto p=ImGui::GetItemRectMin(),end=ImGui::GetItemRectMax();auto* draw=ImGui::GetWindowDrawList();
    float left=p.x+10,right=end.x-10,y=(p.y+end.y)*.5f;
    float x=left+(right-left)*std::clamp(float(value-low)/float(high-low),0.f,1.f);
    draw->AddRectFilled({left,y-2},{right,y+2},ImGui::GetColorU32({.21f,.23f,.31f,1}),2);
    if(x>left)draw->AddRectFilledMultiColor({left,y-2},{x,y+2},ImGui::GetColorU32(pink),ImGui::GetColorU32(blue),ImGui::GetColorU32(blue),ImGui::GetColorU32(pink));
    if(ImGui::IsItemHovered()||ImGui::IsItemActive())draw->AddCircleFilled({x,y},11,ImGui::GetColorU32({pink.x,pink.y,pink.z,.16f}));
    draw->AddCircleFilled({x,y},7,ImGui::GetColorU32(white));
    draw->AddCircleFilled({x,y},3,ImGui::GetColorU32(ImGui::IsItemActive()?pink:blue));
    return changed;
}

}
