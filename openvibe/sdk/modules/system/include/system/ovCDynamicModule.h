#pragma once

#include "defines.h"

#if defined TARGET_OS_Windows
#include <shlobj.h>
#include <Dbghelp.h>
#elif defined TARGET_OS_Linux
	#include <linux/limits.h>
#elif defined TARGET_OS_MacOS
	#include <sys/syslimits.h>
#endif

#include <string>

namespace System {
class CDynamicModuleSymbolLoader; // forward declare to make function declaration possible

class System_API CDynamicModule final
{
public:
	enum ELogErrorCodes : size_t
	{
		LogErrorCodes_NoError = 0,
		LogErrorCodes_ModuleAlreadyLoaded = 1,
		LogErrorCodes_NoModuleLoaded = 2,
		LogErrorCodes_FilenameEmpty = 3,
		LogErrorCodes_FolderPathInvalid = 4,
		LogErrorCodes_RegistryQueryFailed = 5,
		LogErrorCodes_UnloadModuleFailed = 6,
		LogErrorCodes_FailToLoadModule = 7,
		LogErrorCodes_InvalidSymbol = 8,
		LogErrorCodes_EnvironmentVariableInvalid = 9,
		LogErrorCodes_ModuleNotFound = 10
	};

	CDynamicModule();
	~CDynamicModule();

	/**
	 * \brief Load module from a path.
	 *
	 * \param modulePath Path of the module.
	 * \param symbolNameCheck Symbol to check if it is present in the module. It is optionnal and is nullptr by default.
	 *
	 * \retval true If the module loaded successfully.
	 * \retval false If module loading failed.
	 */
	bool loadFromPath(const char* modulePath, const char* symbolNameCheck = nullptr);

#if defined TARGET_OS_Windows
	/**
	 * \brief Load existing module that was already loaded by the process.
	 *
	 * \param modulePath The path to the module.
	 * \param symbolNameCheck Symbol to check if it is present in the module. It is optionnal and is nullptr by default.
	 *
	 * \retval true If the module loaded successfully.
	 * \retval false If module loading failed.
	 */
	bool loadFromExisting(const char* modulePath, const char* symbolNameCheck = nullptr);
	/**
	 * \brief Load module from known path. Windows only.
	 *
	 * \param standardPath A CSIDL value that identifies the folder whose path is to be retrieved.
	 *		Only real folders are valid. If a virtual folder is specified, this function fails.
	 *		You can force creation of a folder by combining the folder's CSIDL with CSIDL_FLAG_CREATE.
	 * \param modulePath Path of the module to load.
	 * \param symbolNameCheck Symbol to check if it is present in the module. It is optional and is nullptr by default.
	 *
	 * \retval true If the module loaded successfully.
	 * \retval false If module loading failed.
	 */
	bool loadFromKnownPath(int standardPath, const char* modulePath, const char* symbolNameCheck = nullptr);

	/**
	 * \brief Load module from Windows environment. Windows only.
	 *
	 * \param environmentPath Environment path.
	 * \param modulePath Module file path.
	 * \param symbolNameCheck Symbol to check if it is present in the module. It is optionnal and is nullptr by default.
	 *
	 * \retval true If the module loaded successfully.
	 * \retval false If module loading failed.
	 */
	bool loadFromEnvironment(const char* environmentPath, const char* modulePath, const char* symbolNameCheck = nullptr);

	/**
	 * \brief Load module from the registry. Windows only.
	 *
	 * \param key Registry key. Check https://msdn.microsoft.com/en-us/library/windows/desktop/ms724836
	 * \param registryPath Registry path.
	 * \param registryKeyName Key name.
	 * \param samDesired A mask that specifies the desired access rights to the key to be opened.
	 *		The function fails if the security descriptor of the key does not permit the requested access for the calling process
	 *		Check https://msdn.microsoft.com/fr-fr/library/windows/desktop/ms724878
	 * \param modulePath sModulePath Module path.
	 * \param symbolNameCheck Symbol to check if it is present in the module. It is optionnal and is nullptr by default.
	 *
	 * \retval true If the module loaded successfully.
	 * \retval false If module loading failed.
	 */
	bool loadFromRegistry(HKEY key, const char* registryPath, const char* registryKeyName, REGSAM samDesired, const char* modulePath,
						  const char* symbolNameCheck = nullptr);

	/**
	 * \brief Check the module architecture. Windows only.
	 * The architecture type of the computer. An image file can only be run on the specified computer or a system that emulates the specified computer.
	 * This member can be one of the following values.
	 * - x86: 0x014c
	 * - x64: 0x8664
	 * - ia64: 0x0200
	 *
	 * \param filePath Module file path
	 * \param architecture Architecture code
	 *
	 * \retval true If the module architecture is equal to the architecture parameter. 
	 * \retval false If the module is unequal to the architecture parameter.
	 */
	static bool isModuleCompatible(const char* filePath, int architecture);
#endif

	// --------------------------------------

	/**
	 * \brief Unload the module. If setShouldFreeModule(false) is called, the unload() has no effect.
	 *
	 * \retval true In case of success.
	 * \retval false In case of failure.
	 *
	 * \sa setShouldFreeModule
	 * \sa isLoaded
	 */
	bool unload();

	/**
	 * \brief Check if the module is loaded.
	 *
	 * \retval true If the module is loaded.
	 * \retval false If no module are loaded.
	 *
	 * \sa unload
	 * \sa setShouldFreeModule
	 */
	bool isLoaded() const { return m_Handle != nullptr; }

	/**
	 * \brief Get the filename of the module.
	 * \return the file name of the module.
	 */
	const char* getFilename() const { return m_Filename; }

	/**
	 * \brief Should be used to avoid the warning "Missing dll" when loading acquisition server
	 * This can happen when the loaded library needs a second library that is not detected.
	 *
	 * \param errorMode Error mode
	 */
	void setDynamicModuleErrorMode(const size_t errorMode) { m_ErrorMode = errorMode; }

	/**
	 * \brief Set if the module should, or not, be free. By default the module will be free.
	 *
	 * \param shouldFreeModule Set to true to free the module when unload is called. False otherwise.
	 *
	 * \sa unload
	 */
	void setShouldFreeModule(const bool shouldFreeModule) { m_ShouldFreeModule = shouldFreeModule; }

	/** 
	 * \brief Get the last error code.
	 *
	 * \return The error code.
	 */
	size_t getLastError() const;

	/**
	 * \brief Get the error message corresponding to the error code.
	 *
	 * \param errorCode The error code.
	 *
	 * \return the message corresponding to the error code.
	 */
	static const char* getErrorString(size_t errorCode);

	/**
	 * \brief Get the detailed error
	 *
	 * \return A string with detailed information about the last error.
	 */
	const char* getErrorDetails() const;

private:
	void* m_Handle = nullptr;

#if defined TARGET_OS_Windows
	char m_Filename[MAX_PATH];
#elif defined TARGET_OS_Linux || defined TARGET_OS_MacOS
		char m_Filename[PATH_MAX];
#endif

	size_t m_ErrorMode      = 0;
	bool m_ShouldFreeModule = true;
	typedef void (*symbol_t)();

	char m_ErrorDetails[1024];
	mutable ELogErrorCodes m_ErrorCode;

	static const size_t m_ErrorModeNull = 0xffffffff;

	friend class CDynamicModuleSymbolLoader;
	/**
	 * \brief Set the error code and details.
	 *
	 * \param errorCode Error code.
	 * \param details Detailed string error.
	 */
	void setError(ELogErrorCodes errorCode, const std::string& details = std::string());

	/**
	* \brief Get a symbol from the module.
	*
	* \param symbolName Symbol name.
	*
	* \return The symbol.
	*/
	symbol_t getSymbolGeneric(const char* symbolName) const;

#ifdef TARGET_OS_Windows
	/**
	 * \brief Get the image file headers. Windows only.
	 * 
	 * \param filename The file path.
	 * \param headers [out] The header.
	 *
	 * \retval true In case of success.
	 * \retval false In case of failure.
	 */
	static bool getImageFileHeaders(const char* filename, IMAGE_NT_HEADERS& headers);
#endif
};

class CDynamicModuleSymbolLoader
{
public:
	/**
	 * \brief Get a symbol from the module.
	 *
	 * \param dynamicModule
	 * \param symbolName The symbol name.
	 * \param symbol [out] The symbol.
	 *
	 * \retval true If the symbol exists.
	 * \retval false If the symbol does not exist.
	 */
	template <typename T>
	static bool getSymbol(CDynamicModule& dynamicModule, const char* symbolName, T* symbol)
	{
		*symbol = reinterpret_cast<T>(dynamicModule.getSymbolGeneric(symbolName));
		return *symbol != nullptr;
	}
};
}  // namespace System
