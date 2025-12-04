#pragma once

#include "defines.h"
#if defined TARGET_OS_Windows
#include <windows.h>

namespace System {
class System_API WindowsUtilities
{
public:
	// Load a library in a matter compliant with non-ascii path
	// returns the eventual error code
	static void* utf16CompliantLoadLibrary(const char* path, HANDLE file = nullptr, DWORD flags = LOAD_WITH_ALTERED_SEARCH_PATH);

	static BOOL utf16CompliantSetEnvironmentVariable(const char* name, const char* value);

	// Load a library in a matter compliant with non-ascii path
	// returns the eventual error code
	static BOOL utf16CompliantCreateProcess(char* applicationName, char* commandLine, LPSECURITY_ATTRIBUTES processAttributes,
											LPSECURITY_ATTRIBUTES threadAttributes, BOOL inheritHandles, DWORD creationFlags, LPVOID environment,
											char* currentDirectory, LPSTARTUPINFO startupInfo, LPPROCESS_INFORMATION processInformation);

	// Load a library in a matter compliant with non-ascii path
	// returns the eventual error code
	static HINSTANCE utf16CompliantShellExecute(HWND hwnd, LPCTSTR operation, LPCTSTR file, LPCTSTR parameters, LPCTSTR directory, INT nShowCmd);
private:
	WindowsUtilities() = delete;
};
}  // namespace System

#endif // TARGET_OS_Windows
