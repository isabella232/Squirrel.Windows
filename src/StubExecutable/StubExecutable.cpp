// StubExecutable.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "StubExecutable.h"

#include "semver200.h"

using namespace std;

wchar_t* FindRootAppDir() 
{
	wchar_t* ourDirectory = new wchar_t[MAX_PATH];

	GetModuleFileName(GetModuleHandle(NULL), ourDirectory, MAX_PATH);
	wchar_t* lastSlash = wcsrchr(ourDirectory, L'\\');
	if (!lastSlash) {
		delete[] ourDirectory;
		return NULL;
	}

	// Null-terminate the string at the slash so now it's a directory
	*lastSlash = 0x0;
	return ourDirectory;
}

wchar_t* FindOwnExecutableName() 
{
	wchar_t* ourDirectory = new wchar_t[MAX_PATH];

	GetModuleFileName(GetModuleHandle(NULL), ourDirectory, MAX_PATH);
	wchar_t* lastSlash = wcsrchr(ourDirectory, L'\\');
	if (!lastSlash) {
		delete[] ourDirectory;
		return NULL;
	}

	wchar_t* ret = _wcsdup(lastSlash + 1);
	delete[] ourDirectory;
	return ret;
}

// Credit to https://stackoverflow.com/a/35717/3193009
LONG GetStringRegKey(HKEY hKey, const std::wstring &strValueName, std::wstring &strValue)
{
    WCHAR szBuffer[512];
    DWORD dwBufferSize = sizeof(szBuffer);
    ULONG nError;
    nError = RegQueryValueExW(hKey, strValueName.c_str(), 0, NULL, (LPBYTE)szBuffer, &dwBufferSize);
    if (ERROR_SUCCESS == nError)
    {
        strValue = szBuffer;
    }
    return nError;
}

// Credit to https://stackoverflow.com/a/6218957/3193009
BOOL FileExists(LPCTSTR szPath)
{
  DWORD dwAttrib = GetFileAttributes(szPath);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
         !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

std::wstring GetBraveCorePath(HKEY hKey, LPCTSTR lpSubKey)
{
	HKEY hBraveKey;
	std::wstring fullPath;
	if (ERROR_SUCCESS == RegOpenKeyExW(hKey,
		lpSubKey, 0, KEY_READ, &hBraveKey))
	{
		if (ERROR_SUCCESS == GetStringRegKey(hBraveKey, L"path", fullPath))
		{
			size_t path_index = fullPath.find(L"Update\\BraveUpdate.exe");
			if (path_index != std::string::npos)
			{
				fullPath = fullPath.substr(0, path_index).append(L"Brave-Browser\\Application");
				std::wstring exePath(fullPath + L"\\brave.exe");
				if (FileExists(exePath.c_str()))
				{
					return fullPath;
				}
			}
		}
		RegCloseKey(hBraveKey);
	}

	return std::wstring(L"");
}

std::wstring FindBraveCoreInstall()
{
	std::wstring fullPath;

	// 1) Check for user-specific install
	// ex: `C:\Users\bsclifton\AppData\Local\BraveSoftware\Brave-Browser\Application\brave.exe`
	fullPath.assign(GetBraveCorePath(HKEY_CURRENT_USER, L"Software\\BraveSoftware\\Update"));
	if (fullPath.length() > 0) {
		return fullPath;
	}

	// 2) Check for multi-user install (install would prompt w/ UAC)
	// 64-bit; ex: `C:\Program Files (x86)\BraveSoftware\Brave-Browser\Application\brave.exe`
	fullPath.assign(GetBraveCorePath(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\BraveSoftware\\Update"));
	if (fullPath.length() > 0) {
		return fullPath;
	}

	// 32-bit; ex: `C:\Program Files\BraveSoftware\Brave-Browser\Application\brave.exe`
	fullPath.assign(GetBraveCorePath(HKEY_LOCAL_MACHINE, L"SOFTWARE\\BraveSoftware\\Update"));

	return fullPath;
}

std::wstring FindLatestAppDir() 
{
	std::wstring ourDir;
	ourDir.assign(FindRootAppDir());

	ourDir += L"\\app-*";

	WIN32_FIND_DATA fileInfo = { 0 };
	HANDLE hFile = FindFirstFile(ourDir.c_str(), &fileInfo);
	if (hFile == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	version::Semver200_version acc("0.0.0");
	std::wstring acc_s;

	do {
		std::wstring appVer = fileInfo.cFileName;
		appVer = appVer.substr(4);   // Skip 'app-'
		if (!(fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			continue;
		}

		std::string s(appVer.begin(), appVer.end());

		version::Semver200_version thisVer(s);

		if (thisVer > acc) {
			acc = thisVer;
			acc_s = appVer;
		}
	} while (FindNextFile(hFile, &fileInfo));

	if (acc == version::Semver200_version("0.0.0")) {
		return NULL;
	}

	ourDir.assign(FindRootAppDir());
	std::wstringstream ret;
	ret << ourDir << L"\\app-" << acc_s;

	FindClose(hFile);
	return ret.str();
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	std::wstring fullPath;
	std::wstring workingDir;
	// Brave Software specific logic
	std::wstring braveCore(FindBraveCoreInstall());
	if (braveCore.length() > 0) {
		// Logic specifically for launching detected brave-core
		workingDir.assign(braveCore);
		fullPath = workingDir + L"\\brave.exe";
	} else {
		// Original stub installer logic
		std::wstring appName;
		appName.assign(FindOwnExecutableName());
		workingDir.assign(FindLatestAppDir());
		fullPath = workingDir + L"\\" + appName;
	}

	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = nCmdShow;

	std::wstring cmdLine(L"\"");
	cmdLine += fullPath;
	cmdLine += L"\" ";
	cmdLine += lpCmdLine;

	wchar_t* lpCommandLine = wcsdup(cmdLine.c_str());
	wchar_t* lpCurrentDirectory = wcsdup(workingDir.c_str());
	if (!CreateProcess(NULL, lpCommandLine, NULL, NULL, true, 0, NULL, lpCurrentDirectory, &si, &pi)) {
		return -1;
	}

	AllowSetForegroundWindow(pi.dwProcessId);
	WaitForInputIdle(pi.hProcess, 5 * 1000);
	return 0;
}
