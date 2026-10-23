#include "overlay_gl.hpp"
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "font_fallback.hpp"

void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}

std::array<GLint,5> unpackState(){
    std::array<GLint,5> values{};
    const GLenum names[]={GL_PIXEL_UNPACK_BUFFER_BINDING,GL_UNPACK_ALIGNMENT,
        GL_UNPACK_ROW_LENGTH,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SKIP_ROWS};
    for(size_t i=0;i<values.size();++i)glGetIntegerv(names[i],&values[i]);
    return values;
}

void checkAtlas(){
    unsigned char* expected;int width,height;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&expected,&width,&height);
    GLuint texture=GLuint(ImGui::GetIO().Fonts->TexID),fbo;
    glBindTexture(GL_TEXTURE_2D,texture);
    GLint actualWidth,actualHeight;
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&actualWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&actualHeight);
    require(actualWidth==width&&actualHeight==height,"Font texture dimensions are invalid");
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Font framebuffer is incomplete");
    std::vector<unsigned char> actual(size_t(width)*height*4);
    glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());
    require(glGetError()==GL_NO_ERROR,"Font texture readback failed");
    require(std::memcmp(actual.data(),expected,actual.size())==0,"Uploaded font pixels do not match the atlas");
    glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&fbo);
}

int main(int argc,char** argv){
    try{
        require(argc>=2,"Pass the project asset root");
        bool desktop=argc>2;
        QSurfaceFormat format;format.setRenderableType(desktop?QSurfaceFormat::OpenGL:QSurfaceFormat::OpenGLES);
        format.setVersion(desktop?4:3,desktop?5:2);QSurfaceFormat::setDefaultFormat(format);
        QGuiApplication app(argc,argv);QOpenGLContext context;context.setFormat(format);
        require(context.create(),"Could not create the test GL context");
        QOffscreenSurface surface;surface.setFormat(context.format());surface.create();
        require(context.makeCurrent(&surface),"Could not make the test context current");
        require(context.isOpenGLES()!=desktop,"Unexpected graphics API");
        ImGui::CreateContext();auto fonts=chaos_ui::setup(argv[1]);auto& io=ImGui::GetIO();
        io.DisplaySize={420,900};io.DeltaTime=1.f/60;
        require(ImGui_ImplOpenGL3_Init(desktop?"#version 330 core":"#version 300 es"),"Backend initialization failed");
        GLuint pbo;glGenBuffers(1,&pbo);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER,1024,nullptr,GL_STREAM_DRAW);
        glPixelStorei(GL_UNPACK_ALIGNMENT,8);glPixelStorei(GL_UNPACK_ROW_LENGTH,2048);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,3);glPixelStorei(GL_UNPACK_SKIP_ROWS,2);
        auto expectedState=unpackState();FontFallback fallback;
        for(int cycle=0;cycle<8;++cycle){
            if(cycle){
                fallback.add(QString::fromUtf8(cycle%2?"Night / 夜 Ω Ж 🦊":"Light / 光 λ Я ⚡🎮"),fonts);
                ImGui_ImplOpenGL3_DestroyFontsTexture();require(io.Fonts->Build(),"Atlas rebuild failed");
            }
            require(glGetError()==GL_NO_ERROR,"Invalid state before font upload");
            chaos_ui::beginOpenGLFrame();
            require(glGetError()==GL_NO_ERROR,"Font upload failed");
            require(unpackState()==expectedState,"Font upload changed the caller's unpack state");
            checkAtlas();
            if(cycle)require(fonts.body->FindGlyphNoFallback(0x591c),"Supported fallback glyph was lost");
            auto texture=io.Fonts->TexID;
            for(int frame=0;frame<3;++frame){
                chaos_ui::beginOpenGLFrame();
                ImGui::NewFrame();ImGui::Begin("Controls");ImGui::TextUnformatted("Chaos HDR");
                ImGui::Button("Example control");ImGui::End();ImGui::Render();
                require(io.Fonts->TexID==texture,"A normal frame replaced the font texture");
                require(unpackState()==expectedState,"A normal frame changed unpack state");
            }
        }
        int sources=io.Fonts->Sources.Size;
        ImFontBuilderIO failingBuilder{};
        failingBuilder.FontBuilder_Build=[](ImFontAtlas* atlas){
            atlas->FontBuilderIO=ImFontAtlasGetBuilderForStbTruetype();
            atlas->ClearTexData();atlas->TexID=0;atlas->TexWidth=atlas->TexHeight=0;return false;
        };
        io.Fonts->FontBuilderIO=&failingBuilder;
        require(fallback.add(QString::fromUtf8("Ψ"),fonts),"Rollback fixture did not attempt a new font");
        require(io.Fonts->Sources.Size==sources,"Failed fallback sources were retained");
        ImGui_ImplOpenGL3_DestroyFontsTexture();chaos_ui::beginOpenGLFrame();checkAtlas();
        require(fonts.body->FindGlyphNoFallback(0x591c),"Rollback lost previously supported glyphs");
        require(unpackState()==expectedState,"Rollback changed the caller's unpack state");
        ImGui_ImplOpenGL3_Shutdown();ImGui::DestroyContext();glDeleteBuffers(1,&pbo);
        require(glGetError()==GL_NO_ERROR,"Unexpected graphics error");
        std::printf("%s: initial upload, 7 rebuilds, exact atlas pixels, state restoration, emoji, rollback and normal frames passed\n",desktop?"OpenGL":"OpenGL ES");
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
