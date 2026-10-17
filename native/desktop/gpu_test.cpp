#include <epoxy/gl.h>
#include "gpu.hpp"
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <vector>
#include <cstdio>
#include <stdexcept>
#include <cstdlib>
int main(int argc,char** argv){
    QSurfaceFormat format;format.setRenderableType(QSurfaceFormat::OpenGLES);format.setVersion(3,2);QSurfaceFormat::setDefaultFormat(format);
    QGuiApplication app(argc,argv);
    if(argc<2||argc>5)return 2;
    float strength=argc>2?std::strtof(argv[2],nullptr)/100.f:.5f;
    float grain=argc>3?std::strtof(argv[3],nullptr)/100.f:0.f;
    bool grainPattern=argc>4&&std::string(argv[4])=="grain";
    QString directory=QString::fromLocal8Bit(argv[1]);QDir().mkpath(directory);
    try{
        QOpenGLContext context;context.setFormat(format);if(!context.create())throw std::runtime_error("Create test OpenGL context");
        QOffscreenSurface surface;surface.setFormat(context.format());surface.create();if(!context.makeCurrent(&surface))throw std::runtime_error("Make test context current");
        std::fprintf(stderr,"Desktop test GL: %s; %s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
        constexpr int width=2560,height=1440;
        std::vector<unsigned char> input(width*height*4);
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){auto i=4*(y*width+x);input[i]=x*255/(width-1);input[i+1]=y*255/(height-1);input[i+2]=((x/32+y/32)%2)?220:30;input[i+3]=255;}
        if(grainPattern)for(int y=0;y<height;++y)for(int x=0;x<width;++x){
            uint32_t hash=uint32_t(x+y*width)*747796405u+2891336453u;hash=((hash>>((hash>>28u)+4u))^hash)*277803737u;hash=(hash>>22u)^hash;
            int noise=int(hash%9)-4;unsigned char value=(x<width/2?80:170)+noise;auto i=4*(y*width+x);
            input[i]=input[i+1]=input[i+2]=value;input[i+3]=255;
        }
        {
            DesktopGPU engine(width,height);
            glBindTexture(GL_TEXTURE_2D,engine.inputTexture());glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,input.data());glBindTexture(GL_TEXTURE_2D,0);
            for(int n=0;n<3;++n)engine.convert(strength,grain);
            GLuint fbo;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,engine.outputTexture(),0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Readback framebuffer incomplete");
            std::vector<uint32_t> pixels(width*height);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_INT_2_10_10_10_REV,pixels.data());glFinish();
            auto error=glGetError();if(error)throw std::runtime_error("Output readback failed: "+std::to_string(error));
            QFile source(directory+"/input.rgba8"),out(directory+"/output.rgb10a2");
            if(!source.open(QIODevice::WriteOnly)||!out.open(QIODevice::WriteOnly))throw std::runtime_error("Write test captures");
            source.write(reinterpret_cast<const char*>(input.data()),input.size());out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()*4);
            glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&fbo);
        }
        QFile result(directory+"/gpu-test.json");if(!result.open(QIODevice::WriteOnly))throw std::runtime_error("Write GPU test result");
        result.write(QJsonDocument(QJsonObject{{"gpu_sharing_passed",true},{"width",width},{"height",height},{"frames",3},{"sharpen",strength},{"ignore_grain",grain},{"grain_pattern",grainPattern},{"renderer",reinterpret_cast<const char*>(glGetString(GL_RENDERER))},{"api",reinterpret_cast<const char*>(glGetString(GL_VERSION))}}).toJson());
        std::puts("Desktop OpenGL/Vulkan GPU sharing and TrueHDR processing passed");
    }catch(const std::exception& e){std::fprintf(stderr,"Desktop GPU test failed: %s\n",e.what());return 1;}
}
