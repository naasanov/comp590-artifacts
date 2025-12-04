#include "system/WindowsUtilities.h"
#if defined TARGET_OS_Windows
#include "m_ConverterUtf8.h"
#include <ShellAPI.h>

#ifndef UNICODE
#define UNICODE
#endif

namespace System {

// Load a library in a matter compliant with non-ascii path
// returns the eventual error code
void* WindowsUtilities::utf16CompliantLoadLibrary(const char* path, const HANDLE file, const DWORD flags)
{
	//const HMODULE hModule = ::LoadLibraryEx(path, file, flags); // LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_DEFAULT_DIRS);
	return ::LoadLibraryEx(path, file, flags); // LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_DEFAULT_DIRS);
}

BOOL WindowsUtilities::utf16CompliantSetEnvironmentVariable(const char* name, const char* value) { return SetEnvironmentVariable(name, value); }

// Load a library in a matter compliant with non-ascii path
// returns the eventual error code
BOOL WindowsUtilities::utf16CompliantCreateProcess(char* applicationName, char* commandLine, LPSECURITY_ATTRIBUTES processAttributes,
												   LPSECURITY_ATTRIBUTES threadAttributes, const BOOL inheritHandles, const DWORD creationFlags,
												   LPVOID environment, char* currentDirectory, LPSTARTUPINFO startupInfo,
												   LPPROCESS_INFORMATION processInformation)
{
	return CreateProcess(applicationName, const_cast<char*>(commandLine), processAttributes, threadAttributes,
						 inheritHandles, creationFlags, environment, currentDirectory, startupInfo, processInformation);
}

// Load a library in a matter compliant with non-ascii path
// returns the eventual error code
HINSTANCE WindowsUtilities::utf16CompliantShellExecute(HWND hwnd, LPCTSTR operation, LPCTSTR file, LPCTSTR parameters, LPCTSTR directory, const INT nShowCmd)
{
	return ShellExecute(hwnd, operation, file, parameters, directory, nShowCmd);
}

}  // namespace System
#endif // TARGET_OS_Windows
