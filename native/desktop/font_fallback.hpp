#pragma once
#include <fontconfig/fontconfig.h>
#include <QString>
#include <map>
#include <set>
#include <deque>
#include "overlay_style.hpp"

class FontFallback {
    std::set<char32_t> requested;
    std::deque<ImVector<ImWchar>> ranges;
public:
    bool add(const QString& text,const chaos_ui::Fonts& fonts){
        std::map<std::pair<std::string,int>,std::vector<ImWchar>> groups;
        for(auto code:text.toUcs4()){
            if(code<0x100||!requested.insert(code).second)continue;
            auto* pattern=FcPatternCreate();auto* charset=FcCharSetCreate();FcCharSetAddChar(charset,code);
            FcPatternAddCharSet(pattern,FC_CHARSET,charset);FcPatternAddBool(pattern,FC_SCALABLE,FcTrue);
            FcConfigSubstitute(nullptr,pattern,FcMatchPattern);FcDefaultSubstitute(pattern);FcResult result;
            auto* match=FcFontMatch(nullptr,pattern,&result);
            FcChar8* file=nullptr;int index=0;FcCharSet* available=nullptr;
            if(match&&FcPatternGetString(match,FC_FILE,0,&file)==FcResultMatch&&FcPatternGetCharSet(match,FC_CHARSET,0,&available)==FcResultMatch&&FcCharSetHasChar(available,code)){
                FcPatternGetInteger(match,FC_INDEX,0,&index);groups[{reinterpret_cast<const char*>(file),index}].push_back(ImWchar(code));
            }
            if(match)FcPatternDestroy(match);FcCharSetDestroy(charset);FcPatternDestroy(pattern);
        }
        auto& io=ImGui::GetIO();
        for(auto& [face,codes]:groups){
            std::sort(codes.begin(),codes.end());ranges.emplace_back();auto& range=ranges.back();
            for(auto code:codes){range.push_back(code);range.push_back(code);}range.push_back(0);
            for(auto* font:{fonts.body,fonts.small,fonts.bold}){
                ImFontConfig config;config.MergeMode=true;config.DstFont=font;config.FontNo=face.second;config.OversampleH=2;
                io.Fonts->AddFontFromFileTTF(face.first.c_str(),font==fonts.small?15:18,&config,range.Data);
            }
        }
        return !groups.empty();
    }
};
