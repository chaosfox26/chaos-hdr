#pragma once
#include <fontconfig/fontconfig.h>
#include <QString>
#include <map>
#include <set>
#include <deque>
#include <string_view>
#include "overlay_style.hpp"
#include "imgui_internal.h"

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
            FcPatternAddBool(pattern,FC_OUTLINE,FcTrue);FcPatternAddBool(pattern,FC_COLOR,FcFalse);
            FcConfigSubstitute(nullptr,pattern,FcMatchPattern);FcDefaultSubstitute(pattern);FcResult result;
            auto* match=FcFontMatch(nullptr,pattern,&result);
            FcChar8 *file=nullptr,*format=nullptr;int index=0;FcBool outline=FcFalse;FcCharSet* available=nullptr;
            if(match&&FcPatternGetBool(match,FC_OUTLINE,0,&outline)==FcResultMatch&&outline&&
               FcPatternGetString(match,FC_FONTFORMAT,0,&format)==FcResultMatch&&
               (std::string_view(reinterpret_cast<char*>(format))=="TrueType"||std::string_view(reinterpret_cast<char*>(format))=="CFF")&&
               FcPatternGetString(match,FC_FILE,0,&file)==FcResultMatch&&FcPatternGetCharSet(match,FC_CHARSET,0,&available)==FcResultMatch&&FcCharSetHasChar(available,code)){
                FcPatternGetInteger(match,FC_INDEX,0,&index);groups[{reinterpret_cast<const char*>(file),index}].push_back(ImWchar(code));
            }
            if(match)FcPatternDestroy(match);FcCharSetDestroy(charset);FcPatternDestroy(pattern);
        }
        auto& io=ImGui::GetIO();auto& atlas=*io.Fonts;int sourceCount=atlas.Sources.Size;
        for(auto& [face,codes]:groups){
            if(face.second<0||face.second>0xffff)continue;
            std::sort(codes.begin(),codes.end());ranges.emplace_back();auto& range=ranges.back();
            for(auto code:codes){range.push_back(code);range.push_back(code);}range.push_back(0);
            for(auto* font:{fonts.body,fonts.small,fonts.bold}){
                ImFontConfig config;config.MergeMode=true;config.DstFont=font;config.FontNo=face.second;config.OversampleH=2;
                io.Fonts->AddFontFromFileTTF(face.first.c_str(),font==fonts.small?15:18,&config,range.Data);
            }
        }
        if(atlas.Sources.Size==sourceCount)return false;
        if(!atlas.Build()){
            for(int i=sourceCount;i<atlas.Sources.Size;++i)
                if(atlas.Sources[i].FontDataOwnedByAtlas)IM_FREE(atlas.Sources[i].FontData);
            atlas.Sources.resize(sourceCount);ImFontAtlasUpdateSourcesPointers(&atlas);atlas.Build();
        }
        return true;
    }
};
