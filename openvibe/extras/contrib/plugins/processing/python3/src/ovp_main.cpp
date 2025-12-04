#if defined TARGET_HAS_ThirdPartyPython3 && !(defined(WIN32) && defined(TARGET_BUILDTYPE_Debug))
// Windows debug build doesn't typically link as most people don't have the python debug library.

#include "box-algorithms/ovpCBoxAlgorithmPython3.h"

#if defined(PY_MAJOR_VERSION) && (PY_MAJOR_VERSION == 3)

#ifdef TARGET_OS_Windows
#include "windows.h"
#endif

#include <string>
#include <iostream>

//-------------------------------------------------------------------------------------------------
class CPython3Initializer
{
public:
	CPython3Initializer();
	~CPython3Initializer();
	bool isPython3Available() const { return m_pythonAvailable; }
private:
#ifdef TARGET_OS_Windows
	static bool checkPython3Path();
#endif
	// PyThreadState *m_pMainPyThreadState;
	bool m_pythonAvailable;
};

#ifdef TARGET_OS_Windows
bool CPython3Initializer::checkPython3Path()
{
	std::wstring ws(Py_GetPath());
	std::string path(ws.begin(), ws.end());

	size_t found = path.find_first_of(';');
	while (found != std::string::npos)
	{
		if (found > 0)
		{
			std::string filename = path.substr(0, found);
			const bool exists    = (_access(filename.c_str(), 0) == 0);
			if (exists) { return true; }
		}
		path  = path.substr(found + 1);
		found = path.find_first_of(';');
	}

	std::cout << "Python directory not found. You probably have a corrupted python installation!" << std::endl;
	std::cout << "The tried path from Py_GetPath() was [" << Py_GetPath() << "]\n";

	return false;
}
#endif

CPython3Initializer::CPython3Initializer() : m_pythonAvailable(false)
{
#ifdef TARGET_OS_Windows
	__try
	{
		// We do not care about the last file, since it is the OpenViBE runtime path
		if (!Py_IsInitialized())
		{
			Py_Initialize();
			if (checkPython3Path()) {
				m_pythonAvailable = true;
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { }
#else
		if (!Py_IsInitialized())
		{
			Py_Initialize();
			m_pythonAvailable = true;
		}
#endif
}

CPython3Initializer::~CPython3Initializer()
{
	if (m_pythonAvailable)
	{
		m_pythonAvailable = false;
		Py_Finalize();
	}
}
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
namespace OpenViBE {
namespace Plugins {
namespace Python {

OVP_Declare_Begin()
	static CPython3Initializer python3Init;
	if (python3Init.isPython3Available()) { OVP_Declare_New(CBoxAlgorithmPython3Desc) }
OVP_Declare_End()

}  // namespace Python
}  // namespace Plugins
}  // namespace OpenViBE
//-------------------------------------------------------------------------------------------------

#else
#pragma message ("WARNING: Python 3.x headers are required to build the Python plugin, different includes found, skipped")
#endif // #if defined(PY_MAJOR_VERSION) && (PY_MAJOR_VERSION == 2)
#endif // TARGET_HAS_ThirdPartyPython3
