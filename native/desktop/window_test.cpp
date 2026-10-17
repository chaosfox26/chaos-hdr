#include <epoxy/gl.h>
#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QWindow>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <memory>
class Fixture : public QWidget {
    int frame=0;
    void paintEvent(QPaintEvent*)override{
        QPainter painter(this);QLinearGradient gradient(0,0,width(),0);gradient.setColorAt(0,Qt::black);gradient.setColorAt(1,Qt::white);painter.fillRect(rect(),gradient);
        for(int n=0;n<6;++n)painter.fillRect(n*width()/6,height()/2,width()/6,height()/2,QColor::fromHsv(n*60,255,200));
        painter.setPen(Qt::white);painter.drawText(30,40,QString("Chaos HDR existing-window fixture · %1").arg(++frame));
    }
public:Fixture(){resize(800,600);setWindowTitle("Chaos HDR existing-window fixture");auto timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{update();});timer->start(16);}
};
int main(int argc,char** argv){
    QApplication app(argc,argv);auto identity=qEnvironmentVariable("TRUEHDR_TEST_WINDOW_CLASS","truehdrtest");app.setApplicationName(identity);app.setDesktopFileName(identity);
    if(qEnvironmentVariableIsSet("TRUEHDR_TEST_STATIC_MENU")){
        QWidget menu;menu.setWindowFlags(Qt::Tool|Qt::WindowStaysOnTopHint);
        menu.setWindowTitle("Chaos HDR static overlapping-window regression");
        auto palette=menu.palette();palette.setColor(QPalette::Window,QColor(255,0,255));
        menu.setPalette(palette);menu.setAutoFillBackground(true);menu.resize(220,180);menu.show();
        QTimer::singleShot(120000,&app,&QApplication::quit);return app.exec();
    }
    std::unique_ptr<Fixture> raster;std::unique_ptr<QWindow> gpu;std::unique_ptr<QOpenGLContext> context;QTimer timer;
    if(qEnvironmentVariableIsSet("TRUEHDR_TEST_GPU_WINDOW")){
        QSurfaceFormat format;format.setRenderableType(QSurfaceFormat::OpenGLES);format.setVersion(3,2);
        gpu=std::make_unique<QWindow>();if(qEnvironmentVariable("TRUEHDR_TEST_WINDOW_TYPE")=="dialog")gpu->setFlags(Qt::Dialog);
        gpu->setSurfaceType(QSurface::OpenGLSurface);gpu->setFormat(format);gpu->resize(800,600);gpu->setTitle("Chaos HDR submitted-GPU-buffer fixture");gpu->showFullScreen();
        context=std::make_unique<QOpenGLContext>();context->setFormat(format);if(!context->create())return 3;
        QObject::connect(&timer,&QTimer::timeout,&app,[&]{
            if(!gpu->isExposed()||!context->makeCurrent(gpu.get()))return;
            const int w=gpu->width()*gpu->devicePixelRatio(),h=gpu->height()*gpu->devicePixelRatio();
            glViewport(0,0,w,h);glEnable(GL_SCISSOR_TEST);
            for(int n=0;n<64;++n){float value=n/63.f;glScissor(n*w/64,0,(n+1)*w/64-n*w/64,h);glClearColor(value,value,value,1);glClear(GL_COLOR_BUFFER_BIT);}
            for(int n=0;n<6;++n){QColor color=QColor::fromHsv(n*60,255,200);glScissor(n*w/6,0,(n+1)*w/6-n*w/6,h/2);glClearColor(color.redF(),color.greenF(),color.blueF(),1);glClear(GL_COLOR_BUFFER_BIT);}
            glDisable(GL_SCISSOR_TEST);context->swapBuffers(gpu.get());
        });timer.start(16);
    }else{raster=std::make_unique<Fixture>();raster->showFullScreen();}
    QTimer::singleShot(120000,&app,&QApplication::quit);return app.exec();
}
