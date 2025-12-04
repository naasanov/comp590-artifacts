#include "system/ovCDynamicModule.h"

#if defined TARGET_OS_Windows
#include <system/WindowsUtilities.h> // Allowed to use utf8_to_utf16 function for os that use utf16
#elif defined TARGET_OS_Linux || defined TARGET_OS_MacOS
	#include <dlfcn.h>
#endif

#include <map>
#include <vector>
#include <cstring>

namespace System {

static const std::map<CDynamicModule::ELogErrorCodes, std::string> ERROR_MAP =
{
	{ CDynamicModule::LogErrorCodes_ModuleAlreadyLoaded, "A module is already loaded." },
	{ CDynamicModule::LogErrorCodes_NoModuleLoaded, "No module loaded." },
	{ CDynamicModule::LogErrorCodes_FilenameEmpty, "The filename is empty." },
	{ CDynamicModule::LogErrorCodes_FolderPathInvalid, "The folder path is invalid." },
	{ CDynamicModule::LogErrorCodes_RegistryQueryFailed, "The registry query is invalid." },
	{ CDynamicModule::LogErrorCodes_UnloadModuleFailed, "Fail to unload the module." },
	{ CDynamicModule::LogErrorCodes_FailToLoadModule, "Fail to load the module." },
	{ CDynamicModule::LogErrorCodes_InvalidSymbol, "The symbol is invalid." },
	{ CDynamicModule::LogErrorCodes_ModuleNotFound, "Module not found." }
};

#if defined TARGET_OS_Windows
static std::vector<std::string> split(char* str, const char* delim)
{
	char* token = strtok(str, delim);

	std::vector<std::string> result;

	while (token != nullptr)
	{
		result.push_back(token);
		token = strtok(nullptr, delim);
	}

	return result;
}

static std::string formatWindowsError(const DWORD code)
{
	LPTSTR text;

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |				// use system message tables to retrieve error text
				  FORMAT_MESSAGE_ALLOCATE_BUFFER |			// allocate buffer on local heap for error text
				  FORMAT_MESSAGE_IGNORE_INSERTS,			// Important! will fail otherwise, since we're not (and CANNOT) pass insertion parameters
				  nullptr,									// unused with FORMAT_MESSAGE_FROM_SYSTEM
				  code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				  LPTSTR(&text),							// output
				  0,										// minimum size for output buffer
				  nullptr
	);														// arguments - see note

	return std::string(text);
}
#endif

const char* CDynamicModule::getErrorString(size_t errorCode)
{
	if (ERROR_MAP.count(ELogErrorCodes(errorCode)) == 0) { return "Invalid error code"; }
	return ERROR_MAP.at(ELogErrorCodes(errorCode)).c_str();
}

const char* CDynamicModule::getErrorDetails() const { return &m_ErrorDetails[0]; }
size_t CDynamicModule::getLastError() const { return m_ErrorCode; }

CDynamicModule::CDynamicModule()
	: m_ErrorMode(m_ErrorModeNull), m_ErrorCode(LogErrorCodes_NoError)
{
	strcpy(m_ErrorDetails, "");
	strcpy(m_Filename, "");
}

CDynamicModule::~CDynamicModule() {}

// --------------------------------------

#if defined TARGET_OS_Windows
bool CDynamicModule::loadFromExisting(const char* modulePath, const char* symbolNameCheck)
{
	if (m_Handle)
	{
		this->setError(LogErrorCodes_ModuleAlreadyLoaded, "Module [" + std::string(m_Filename) + "] is already loaded");
		return false;
	}

	m_Handle = ::GetModuleHandle(modulePath);

	if (m_Handle == nullptr)
	{
		this->setError(LogErrorCodes_FailToLoadModule, "Windows error: " + formatWindowsError(GetLastError()));
		return false;
	}

	if (symbolNameCheck != nullptr)
	{
		if (GetProcAddress(HMODULE(m_Handle), symbolNameCheck) == nullptr)
		{
			this->unload();
			this->setError(LogErrorCodes_InvalidSymbol, "Windows error: " + formatWindowsError(GetLastError()));
			return false;
		}
	}

	strcpy(m_Filename, modulePath);

	return true;
}
#endif

bool CDynamicModule::loadFromPath(const char* modulePath, const char* symbolNameCheck)
{
	if (m_Handle)
	{
		this->setError(LogErrorCodes_ModuleAlreadyLoaded, "Module [" + std::string(m_Filename) + "] is already loaded");
		return false;
	}

	// Verify empty filename
	if (modulePath == nullptr || (modulePath != nullptr && modulePath[0] == '\0'))
	{
		this->setError(LogErrorCodes_FilenameEmpty);
		return false;
	}

#if defined TARGET_OS_Windows

	if (m_ErrorMode == m_ErrorModeNull)
	{
		const UINT mode = SetErrorMode(UINT(m_ErrorModeNull));
		SetErrorMode(mode);
	}
	else { SetErrorMode(UINT(m_ErrorMode)); }

	m_Handle = WindowsUtilities::utf16CompliantLoadLibrary(modulePath, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);

	if (m_Handle == nullptr)
	{
		this->setError(LogErrorCodes_FailToLoadModule, "Fail to load [" + std::string(modulePath) + "]. Windows error:" + formatWindowsError(GetLastError()));
		return false;
	}

	if (symbolNameCheck != nullptr)
	{
		if (GetProcAddress(HMODULE(m_Handle), symbolNameCheck) == nullptr)
		{
			this->unload();
			this->setError(LogErrorCodes_InvalidSymbol,
						   "Symbol invalid: [" + std::string(symbolNameCheck) + "]. Windows error: " + formatWindowsError(GetLastError()));
			return false;
		}
	}
#elif defined TARGET_OS_Linux || defined TARGET_OS_MacOS
	m_Handle = ::dlopen(modulePath, RTLD_LAZY|RTLD_GLOBAL);

	if (m_Handle == nullptr)
	{
		this->setError(LogErrorCodes_FailToLoadModule);
		return false;
	}

	if(symbolNameCheck != nullptr)
	{
		if(::dlsym(m_Handle, symbolNameCheck) == nullptr)
		{
			char* error = ::dlerror();
			
			if(error) { this->setError(LogErrorCodes_InvalidSymbol, "Error: " + std::string(error)); }
			else { this->setError(LogErrorCodes_InvalidSymbol); }

			::dlclose(m_Handle);
			m_Handle = NULL;
			
			return false;
		}
	}
#endif

	strcpy(m_Filename, modulePath);

	return true;
}

#if defined TARGET_OS_Windows
bool CDynamicModule::loadFromKnownPath(const int standardPath, const char* modulePath, const char* symbolNameCheck)
{
	if (m_Handle)
	{
		this->setError(LogErrorCodes_ModuleAlreadyLoaded, "Module [" + std::string(m_Filename) + "] is already loaded");
		return false;
	}

	char dllPath[MAX_PATH];

	const HRESULT result = ::SHGetFolderPath(nullptr, standardPath, nullptr, SHGFP_TYPE_CURRENT, dllPath);

	if (result != S_OK)
	{
		this->setError(LogErrorCodes_FolderPathInvalid, "Windows error code: " + std::to_string(result));
		return false;
	}

	strcat(dllPath, "\\");
	strcat(dllPath, modulePath);
	return loadFromPath(dllPath, symbolNameCheck); // Error set in the loadFromPath function
}

bool CDynamicModule::loadFromEnvironment(const char* environmentPath, const char* modulePath, const char* symbolNameCheck)
{
	if (m_Handle)
	{
		this->setError(LogErrorCodes_ModuleAlreadyLoaded, "Module [" + std::string(m_Filename) + "] is already loaded");
		return false;
	}

	char* str = getenv(environmentPath);

	if (str == nullptr)
	{
		this->setError(LogErrorCodes_EnvironmentVariableInvalid);
		return false;
	}

	std::vector<std::string> paths = split(str, ";");

	for (const std::string& path : paths) { if (loadFromPath((path + "\\" + modulePath).c_str(), symbolNameCheck)) { return true; } }

	this->setError(LogErrorCodes_ModuleNotFound);
	return false;
}

bool CDynamicModule::loadFromRegistry(HKEY key, const char* registryPath, const char* registryKeyName, REGSAM samDesired, const char* modulePath,
									  const char* symbolNameCheck)
{
	char dllPath[MAX_PATH];
	DWORD size = sizeof(dllPath);
	dllPath[0] = '\0';

	HKEY lKey = nullptr;

	LONG result = RegOpenKeyEx(key, TEXT(registryPath), NULL, samDesired, &lKey);

	if (result != ERROR_SUCCESS)
	{
		this->setError(LogErrorCodes_RegistryQueryFailed, "Fail to open registry key. Windows error code: " + std::to_string(result));
		RegCloseKey(lKey);
		return false;
	}

	result = ::RegQueryValueEx(lKey, registryKeyName, nullptr, nullptr, reinterpret_cast<unsigned char*>(dllPath), &size);

	if (result == ERROR_SUCCESS)
	{
		strcat(dllPath, modulePath);
		return loadFromPath(dllPath, symbolNameCheck); // Error set in the loadFromPath function
	}
	this->setError(LogErrorCodes_RegistryQueryFailed, "Fail to query registry value. Windows error code: " + std::to_string(result));
	return false;
}

bool CDynamicModule::isModuleCompatible(const char* filePath, const int architecture)
{
	IMAGE_NT_HEADERS headers;
	if (!getImageFileHeaders(filePath, headers)) { return false; } // Error set in the getImageFileHeaders function
	return headers.FileHeader.Machine == architecture;
}
#endif

// --------------------------------------

bool CDynamicModule::unload()
{
	if (!m_Handle)
	{
		this->setError(LogErrorCodes_NoModuleLoaded);
		return false;
	}

	// If the flag m_shouldFreeModule, set to true per default, is set to false,
	// the module is not unloaded.
	// This flag was first set for Enobio3G driver which dll freezes when unloaded
	if (!m_ShouldFreeModule) { return true; }

#if defined TARGET_OS_Windows
	if (::FreeModule(reinterpret_cast<HMODULE>(m_Handle)) == 0)
	{
		this->setError(LogErrorCodes_UnloadModuleFailed, "Windows error code: " + formatWindowsError(GetLastError()));
		return false;
	}
#elif defined TARGET_OS_Linux || defined TARGET_OS_MacOS
	if(::dlclose(m_Handle) != 0)
	{
		char* error = ::dlerror();
		if(error) { this->setError(LogErrorCodes_UnloadModuleFailed, "Error: " + std::string(error)); }
		else { this->setError(LogErrorCodes_UnloadModuleFailed); }
		return false;
	}
#else
#endif

	strcpy(m_Filename, "");
	m_Handle = nullptr;

	return true;
}

CDynamicModule::symbol_t CDynamicModule::getSymbolGeneric(const char* symbolName) const
{
	symbol_t res = nullptr;

	if (!m_Handle)
	{
		m_ErrorCode = LogErrorCodes_NoModuleLoaded;
		return res;
	}

	if (m_Handle)
	{
#if defined TARGET_OS_Windows
		res = symbol_t(GetProcAddress(reinterpret_cast<HMODULE>(m_Handle), symbolName));

		if (!res)
		{
			m_ErrorCode = LogErrorCodes_InvalidSymbol;
			return res;
		}

#elif defined TARGET_OS_Linux || defined TARGET_OS_MacOS
		res = (CDynamicModule::symbol_t)::dlsym(m_Handle, symbolName);

		if (!res)
		{
			m_ErrorCode = LogErrorCodes_InvalidSymbol;
			return res;
		}
#else
#endif
	}

	return res;
}

#ifdef TARGET_OS_Windows
bool CDynamicModule::getImageFileHeaders(const char* filename, IMAGE_NT_HEADERS& headers)
{
	const HANDLE fileHandle = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (fileHandle == INVALID_HANDLE_VALUE) { return false; }

	const HANDLE imageHandle = CreateFileMapping(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);

	if (imageHandle == nullptr)
	{
		CloseHandle(fileHandle);
		return false;
	}

	void* imagePtr = MapViewOfFile(imageHandle, FILE_MAP_READ, 0, 0, 0);

	if (imagePtr == nullptr)
	{
		CloseHandle(imageHandle);
		CloseHandle(fileHandle);
		return false;
	}

	const PIMAGE_NT_HEADERS headersPtr = ImageNtHeader(imagePtr);

	if (headersPtr == nullptr)
	{
		UnmapViewOfFile(imagePtr);
		CloseHandle(imageHandle);
		CloseHandle(fileHandle);
		return false;
	}

	headers = *headersPtr;

	UnmapViewOfFile(imagePtr);
	CloseHandle(imageHandle);
	CloseHandle(fileHandle);

	return true;
}
#endif

void CDynamicModule::setError(const ELogErrorCodes errorCode, const std::string& details)
{
	m_ErrorCode = errorCode;
	strcpy(m_ErrorDetails, details.c_str());
}
}  // namespace System
