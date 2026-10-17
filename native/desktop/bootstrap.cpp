#include <QQmlExtensionPlugin>
#include <QQmlEngine>
#include <QCoreApplication>





class PathRegistration : public QObject {
    Q_OBJECT
public:
    explicit PathRegistration(QObject* parent=nullptr):QObject(parent){
        QCoreApplication::addLibraryPath(QString::fromUtf8(TRUEHDR_PLUGIN_ROOT));
    }
};
class Bootstrap : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    void registerTypes(const char* uri)override{qmlRegisterType<PathRegistration>(uri,1,0,"PathRegistration");}
};
#include "bootstrap.moc"
