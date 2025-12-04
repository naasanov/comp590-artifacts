#include "OVMaster.h"

#include <QQmlContext>
#include <QQmlEngine>

#include <system/ovCTime.h>
#include <vector>

#include "ovasIDriver.h"
#include "ovasCAcquisitionServer.h"

#include <parameters.h>
#include <parametersObject.h>

// Simulation drivers
#include "generic-oscillator/ovasCDriverGenericOscillator.h"

/*
#include "generic-sawtooth/ovasCDriverGenericSawTooth.h"
#include "generic-time-signal/ovasCDriverGenericTimeSignal.h"
#include "simulated-deviator/ovasCDriverSimulatedDeviator.h"

#include "biosemi-activetwo/ovasCDriverBioSemiActiveTwo.h"
#include "brainproducts-actichamp/ovasCDriverBrainProductsActiCHamp.h"
#include "brainproducts-brainampseries/ovasCDriverBrainProductsBrainampSeries.h"
#include "brainproducts-vamp/ovasCDriverBrainProductsVAmp.h"
#include "brainproducts-liveamp/ovasCDriverBrainProductsLiveAmp.h"
#include "egi-ampserver/ovasCDriverEGIAmpServer.h"
#include "emotiv-epoc/ovasCDriverEmotivEPOC.h"
#include "labstreaminglayer/ovasCDriverLabStreamingLayer.h"
#include "micromed-systemplusevolution/ovasCDriverMicromedSystemPlusEvolution.h"
#include "mindmedia-nexus32b/ovasCDriverMindMediaNeXus32B.h"
#include "mcs-nvx/ovasCDriverMCSNVXDriver.h"
#include "neuroelectrics-enobio3g/ovasCDriverEnobio3G.h"
#include "neuroservo/ovasCDriverNeuroServoHid.h"
#include "neurosky-mindset/ovasCDriverNeuroskyMindset.h"
#include "tmsi/ovasCDriverTMSi.h"
#include "tmsi-refa32b/ovasCDriverTMSiRefa32B.h"

#include "mensia-acquisition/ovasCDriverMensiaAcquisition.h"

#include "shimmer-gsr/ovasCDriverShimmerGSR.hpp"
*/

namespace OpenViBE {
namespace AcquisitionServer {

struct OVMasterPrivate {

    OVMasterPrivate(const Kernel::IKernelContext& kernelCtx, OVMaster *q);
	~OVMasterPrivate();

    void initDrivers();
    void launchThread();
	QJSValue toJSParams(const ParameterObjects& params);

    std::vector<IDriver*> m_drivers;
    CAcquisitionServer* m_acquisitionServer = nullptr;
	OVMaster *q = nullptr;

	ParameterIntObject m_param_identifier;
	ParameterIntObject m_param_age;
	ParameterListStringObject m_param_gender;
	ParameterStringObject m_param_hostname;
	ParameterIntObject m_param_port;
	ParameterIntObject m_param_min_samples;
	ParameterObjects m_device_params;
	ParameterObjects m_driver_params;
};

OVMasterPrivate::OVMasterPrivate(const Kernel::IKernelContext& kernelCtx, OVMaster *q)
	: m_acquisitionServer(new CAcquisitionServer(kernelCtx)),
	 q(q),
	 m_param_identifier("Identifier", "Identifier", 0, 0, 10000000),
	 m_param_age("Age", "Age", 18, 0, 115),
	 m_param_gender("Gender", "the gender", {"Other", "Male", "Female"}),
	 m_param_hostname("Hostname", "the hostname", "Localhost"),
	 m_param_port("Port", "the port", 1979, 0, 65535),
	 m_param_min_samples("MinSamples", "the minimum number of samples", 1, 1, 10000000 )
{
	m_device_params.push_back(&m_param_identifier);
	m_device_params.push_back(&m_param_age);
	m_device_params.push_back(&m_param_gender);
	m_device_params.push_back(&m_param_hostname);
	m_device_params.push_back(&m_param_port);
	m_device_params.push_back(&m_param_min_samples);
}

OVMasterPrivate::~OVMasterPrivate()
{
	delete m_acquisitionServer;
	m_acquisitionServer = nullptr;

	for(auto paramObj: m_driver_params) {
		delete paramObj;
	}
}

void OVMasterPrivate::initDrivers()
{

    m_drivers.push_back(new CDriverGenericOscillator(m_acquisitionServer->getDriverContext()));
    //m_drivers.push_back(new CDriverGenericSawTooth(m_acquisitionServer->getDriverContext()));
    //m_drivers.push_back(new CDriverGenericTimeSignal(m_acquisitionServer->getDriverContext()));
}

void OVMasterPrivate::launchThread()
{

}

QJSValue OVMasterPrivate::toJSParams(const ParameterObjects& params)
{
	QQmlEngine *js_engine = nullptr;
	QQmlContext *context = QQmlEngine::contextForObject(q);
	if (!context) {
		qWarning() << Q_FUNC_INFO << " - Could not obtain QQmlContext for object";
		return QJSValue();
	} else {
		js_engine = context->engine();
	}

	QJSValue js_params = js_engine->newArray(params.size());
	int nb_params = params.size();
	for (int i = 0; i < nb_params; ++i) {
		js_params.setProperty(i, js_engine->newQObject(params[i]));
	}

	return js_params;
}

OVMaster::OVMaster() {
    OpenViBE::CKernelLoader kernelLoader;

	std::cout << "[  INF  ] Created kernel loader, trying to load kernel module" << std::endl;
	OpenViBE::CString error;

	OpenViBE::CString kernelFile = OpenViBE::Directories::getLib("kernel");

	if (!kernelLoader.load(kernelFile, &error)) { 
        std::cout << "[ FAILED ] Error loading kernel from [" << kernelFile << "]: " << error << "\n"; 
    }
	else {
		std::cout << "[  INF  ] Kernel module loaded, trying to get kernel descriptor" << std::endl;
		OpenViBE::Kernel::IKernelDesc* kernelDesc = nullptr;
		kernelLoader.initialize();
		kernelLoader.getKernelDesc(kernelDesc);
		if (!kernelDesc) { std::cout << "[ FAILED ] No kernel descriptor" << std::endl; }
		else {
			std::cout << "[  INF  ] Got kernel descriptor, trying to create kernel" << std::endl;


			OpenViBE::CString configFile = OpenViBE::CString(OpenViBE::Directories::getDataDir() + "/kernel/openvibe.conf");

			OpenViBE::Kernel::IKernelContext* kernelCtx = kernelDesc->createKernel("acquisition-server", configFile);
			if (!kernelCtx) { std::cout << "[ FAILED ] No kernel created by kernel descriptor" << std::endl; }
			else {
				kernelCtx->initialize();

				OpenViBE::Kernel::IConfigurationManager& configManager = kernelCtx->getConfigurationManager();

				// @FIXME CERT silent fail if missing file is provided
				configManager.addConfigurationFromFile(configManager.expand("${Path_Data}/applications/acquisition-server/acquisition-server-defaults.conf"));

				// User configuration mods
				configManager.addConfigurationFromFile(configManager.expand("${Path_UserData}/openvibe-acquisition-server.conf"));

				kernelCtx->getPluginManager().addPluginsFromFiles(configManager.expand("${AcquisitionServer_Plugins}"));

                /*
                Commented out cause config is loaded from argc/argv in legacy, which is not in place here
				for (auto itr = config.tokens.begin(); itr != config.tokens.end(); ++itr) {
					kernelCtx->getLogManager() << OpenViBE::Kernel::LogLevel_Trace << "Adding command line configuration token ["
							<< (*itr).first.c_str() << " = " << (*itr).second.c_str() << "]\n";
					configManager.addOrReplaceConfigurationToken((*itr).first.c_str(), (*itr).second.c_str());
				}   */

				// Check the clock
				if (!System::Time::isClockSteady()) {
					kernelCtx->getLogManager() << OpenViBE::Kernel::LogLevel_Warning
							<< "The system does not seem to have a steady clock. This may affect the acquisition time precision.\n";
				}


                d = new OVMasterPrivate(*kernelCtx, this);
                d->initDrivers();
                d->launchThread();
                emit driverChanged();
            }
        }
    }


}

QStringList OVMaster::getDrivers() {
    QStringList drivers;
    for (const auto driver: d->m_drivers) {
        std::cout << Q_FUNC_INFO << " - Adding driver: " << driver->getName() << std::endl;
        drivers.append(driver->getName());
    }
    return drivers;
}

OVMaster::~OVMaster() {
    delete d;

}


QJSValue OVMaster::driverParameters() {
	auto * driver = d->m_acquisitionServer->getDriver();
	if(!driver) {
		qWarning() << Q_FUNC_INFO << " - No driver set";
		return QJSValue();
	}
	Parameters params = driver->parameters();

	//TODO can be optimized by reusing the same objects
	//clear old params
	for(auto paramObj: d->m_driver_params) {
		delete paramObj;
	}
	d->m_driver_params.clear();

	for (auto param: params) {
		d->m_driver_params.push_back(ParameterObject::fromParameter(param));
	}
	return d->toJSParams(d->m_driver_params);
}

QJSValue OVMaster::deviceParameters()
{
	return d->toJSParams(d->m_device_params);
}

void OVMaster::setCurrentDriver(const QString& driverName)
{
	std::cout << Q_FUNC_INFO << " - Setting current driver: " << driverName.toStdString() << std::endl;
	for (const auto driver: d->m_drivers) {
		if(driver->getName() == driverName) {
			std::cout << Q_FUNC_INFO << " - Setting current driver: " << driverName.toStdString() << std::endl;
			
			d->m_acquisitionServer->setDriver(driver);
			emit driverParametersChanged();
			return;
		}
	}
	std::cout << Q_FUNC_INFO << " - Driver not found: " << driverName.toStdString() << std::endl;

}

}  // namespace AcquisitionServer
}  // namespace OpenViBE