#include <QtCore>
#include <QtQml/qqmlregistration.h>
#include <QJSValue>


namespace OpenViBE {
namespace AcquisitionServer {

class OVMaster : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    
    Q_PROPERTY(QStringList drivers READ getDrivers NOTIFY driverChanged)
    Q_PROPERTY(QJSValue driverParameters READ driverParameters NOTIFY driverParametersChanged)
    Q_PROPERTY(QJSValue deviceParameters READ deviceParameters NOTIFY deviceParametersChanged)

public:
    OVMaster();
    ~OVMaster();

    QStringList getDrivers();
    QJSValue deviceParameters();
    QJSValue driverParameters();

    Q_INVOKABLE void setCurrentDriver(const QString& driverName);

signals:
    void deviceParametersChanged(void);
    void driverChanged();
    void driverParametersChanged(void);

protected:
    struct OVMasterPrivate *d = nullptr;
};

}  // namespace AcquisitionServer
}  // namespace OpenViBE