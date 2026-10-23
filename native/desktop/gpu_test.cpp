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
#include <algorithm>
#include <cmath>
#include <unistd.h>
#include <chrono>
#include <thread>
int main(int argc,char** argv){
    if(argc==2&&std::string(argv[1])=="--readiness"){
        int pipefd[2];if(pipe(pipefd))return 1;
        DesktopGPU::DmaInput input;input.acquireFd=pipefd[0];auto start=std::chrono::steady_clock::now();
        for(int n=0;n<1000;++n)if(DesktopGPU::producerReady(input))return 2;
        if(std::chrono::steady_clock::now()-start>std::chrono::milliseconds(100))return 3;
        char data=1;if(write(pipefd[1],&data,1)!=1||!DesktopGPU::producerReady(input))return 4;
        close(pipefd[0]);close(pipefd[1]);if(DesktopGPU::producerReady(input))return 5;
        std::puts("Producer readiness: unready, ready and invalid descriptors passed without waiting.");return 0;
    }
    QSurfaceFormat format;format.setRenderableType(QSurfaceFormat::OpenGLES);format.setVersion(3,2);QSurfaceFormat::setDefaultFormat(format);
    QGuiApplication app(argc,argv);
    if(argc<2||argc>6)return 2;
    float strength=argc>2?std::strtof(argv[2],nullptr)/100.f:.5f;
    float grain=argc>3?std::strtof(argv[3],nullptr)/100.f:0.f;
    bool grainPattern=argc>4&&std::string(argv[4])=="grain";
    bool hdr=argc<6||std::string(argv[5])!="sdr";
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
            for(int n=0;n<3;++n)engine.convert(strength,grain,false,hdr);
            GLuint fbo;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,engine.outputTexture(),0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Readback framebuffer incomplete");
            std::vector<uint32_t> pixels(width*height);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_INT_2_10_10_10_REV,pixels.data());glFinish();
            auto error=glGetError();if(error)throw std::runtime_error("Output readback failed: "+std::to_string(error));
            if(!hdr){
                int maximum=0;
                for(int y=0;y<height;++y)for(int x=0;x<width;++x)for(int channel=0;channel<3;++channel){
                    float value=input[4*(y*width+x)+channel]/255.f;
                    if(strength>0){
                        float sum=0;int count=0;
                        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){int px=x+dx,py=y+dy;if(px>=0&&px<width&&py>=0&&py<height){sum+=input[4*(py*width+px)+channel]/255.f;++count;}}
                        float detail=value-sum/count;detail=std::copysign(std::max(std::abs(detail)-grain*(8.f/255.f),0.f),detail);
                        value=std::clamp(value+std::clamp(detail,-.05f,.05f)*strength*3.f,0.f,1.f);
                    }
                    int actual=(pixels[y*width+x]>>(channel*10))&1023;
                    maximum=std::max(maximum,std::abs(actual-int(std::lround(value*1023))));
                }
                if(maximum>2)throw std::runtime_error("Sharpen-only SDR output differs from the reference: "+std::to_string(maximum));
                std::printf("SDR-only output verified against every source pixel; maximum 10-bit difference: %d\n",maximum);
            }
            QFile source(directory+"/input.rgba8"),out(directory+"/output.rgb10a2");
            if(!source.open(QIODevice::WriteOnly)||!out.open(QIODevice::WriteOnly))throw std::runtime_error("Write test captures");
            source.write(reinterpret_cast<const char*>(input.data()),input.size());out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()*4);
            glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&fbo);
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
            while(!engine.retirementReady()){
                if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("GPU test cleanup did not finish");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        QFile result(directory+"/gpu-test.json");if(!result.open(QIODevice::WriteOnly))throw std::runtime_error("Write GPU test result");
        result.write(QJsonDocument(QJsonObject{{"gpu_sharing_passed",true},{"width",width},{"height",height},{"frames",3},{"sharpen",strength},{"ignore_grain",grain},{"grain_pattern",grainPattern},{"renderer",reinterpret_cast<const char*>(glGetString(GL_RENDERER))},{"api",reinterpret_cast<const char*>(glGetString(GL_VERSION))}}).toJson());
        std::puts("Desktop OpenGL/Vulkan GPU sharing and TrueHDR processing passed");
    }catch(const std::exception& e){std::fprintf(stderr,"Desktop GPU test failed: %s\n",e.what());return 1;}
}
