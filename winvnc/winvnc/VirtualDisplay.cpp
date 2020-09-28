#include "VirtualDisplay.h"
#include "versionhelpers.h"
#pragma comment(lib, "swdevice.lib")

VOID WINAPI
CreationCallback(
	_In_ HSWDEVICE hSwDevice,
	_In_ HRESULT hrCreateResult,
	_In_opt_ PVOID pContext,
	_In_opt_ PCWSTR pszDeviceInstanceId
)
{
	HANDLE hEvent = *(HANDLE*)pContext;

	SetEvent(hEvent);
	UNREFERENCED_PARAMETER(hSwDevice);
	UNREFERENCED_PARAMETER(hrCreateResult);
	UNREFERENCED_PARAMETER(pszDeviceInstanceId);
}



VirtualDisplay::VirtualDisplay()
{
	FileView = NULL;
	hFileMap = NULL;
	pbuff = NULL;
	initialized = false;
	restoreNeeded = false;

	hFileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SUPPORTEDMONITORS), g_szIPC);
	SetSecurityInfo(hFileMap, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, 0, 0, (PACL)NULL, NULL);
	if (hFileMap) {
		FileView = MapViewOfFile(hFileMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
		pbuff = (SUPPORTEDMONITORS*)FileView;
	}
	if (!pbuff)
		return;
	initialized = true;
	pbuff->counter = 0;

	DISPLAY_DEVICE dd;
	ZeroMemory(&dd, sizeof(dd));
	dd.cb = sizeof(dd);
	DWORD dev = 0;

	while (EnumDisplayDevices(0, dev, &dd, 0))
	{
		if (!(dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) && (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) {
			DISPLAY_DEVICE ddMon;
			ZeroMemory(&ddMon, sizeof(ddMon));
			ddMon.cb = sizeof(ddMon);
			DWORD devMon = 0;

			while (EnumDisplayDevices(dd.DeviceName, devMon, &ddMon, 0)) {
				if (ddMon.StateFlags & DISPLAY_DEVICE_ACTIVE)
					break;
				devMon++;
			}

			DEVMODE dm;
			ZeroMemory(&dm, sizeof(dm));
			dm.dmSize = sizeof(dm);
			dm.dmDriverExtra = 0;
			if (EnumDisplaySettingsEx(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm, 0) == FALSE)
				EnumDisplaySettingsEx(dd.DeviceName, ENUM_REGISTRY_SETTINGS, &dm, 0);
			DISPLAYINFO di;
			di.dm = dm;
			di.naam = dd.DeviceName;
			diplayInfoList.push_back(di);
		}
		ZeroMemory(&dd, sizeof(dd));
		dd.cb = sizeof(dd);
		dev++;
	}
}

VirtualDisplay::~VirtualDisplay()
{
	if (FileView)
		UnmapViewOfFile((LPVOID)FileView);
	if (hFileMap)
		CloseHandle(hFileMap);

	if (!initialized)
		return;
	if (restoreNeeded) {
		std::list<DISPLAYINFO>::iterator resIter;
		resIter = diplayInfoList.begin();
		while (resIter != diplayInfoList.end())
		{
			ChangeDisplaySettingsEx((*resIter).naam, &(*resIter).dm, NULL, (CDS_UPDATEREGISTRY | CDS_NORESET), NULL);
			resIter++;
		}
		ChangeDisplaySettingsEx(NULL, NULL, NULL, 0, NULL);
	}

	std::list<VIRTUALDISPLAY>::iterator virtualDisplayIter;
	virtualDisplayIter = virtualDisplayList.begin();
	while (virtualDisplayIter != virtualDisplayList.end())
	{
		if ((*virtualDisplayIter).hDevice)
			SwDeviceClose((*virtualDisplayIter).hDevice);
		if ((*virtualDisplayIter).hEvent)
			CloseHandle((*virtualDisplayIter).hEvent);
		virtualDisplayIter++;
	}

}

void VirtualDisplay::SetVirtualMonitorsSize(int height, int width)
{
	if (!initialized)
		return;
	pbuff->w[pbuff->counter] = width;
	pbuff->h[pbuff->counter] = height;
	pbuff->counter++;
}

void VirtualDisplay::AddVirtualMonitors()
{
	if (!initialized)
		return;
	HSWDEVICE hSwDevice = NULL;
	HANDLE hEvent = NULL;
	WCHAR name[256];
	wcscpy_s(name, L"UVncVirtualDisplay");
	WCHAR nummer[256];
	swprintf_s(nummer, L"%d", virtualDisplayList.size());
	wcscat_s(name, nummer);

	if (AddVirtualDisplay(hSwDevice, hEvent, name))
	{
		VIRTUALDISPLAY virtualDisplay;
		virtualDisplay.hDevice = hSwDevice;
		virtualDisplay.hEvent = hEvent;
		virtualDisplayList.push_back(virtualDisplay);
	}
}

bool VirtualDisplay::AddVirtualDisplay(HSWDEVICE& hSwDevice, HANDLE& hEvent, WCHAR* name)
{
	if (!initialized)
		return false;
	PCWSTR description = name;
	PCWSTR instanceId = name;
	PCWSTR hardwareIds = L"UVncVirtualDisplay\0\0";
	PCWSTR compatibleIds = L"UVncVirtualDisplay\0\0";
	SW_DEVICE_CREATE_INFO createInfo = { 0 };
	hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	createInfo.cbSize = sizeof(createInfo);
	createInfo.pszzCompatibleIds = compatibleIds;
	createInfo.pszInstanceId = instanceId;
	createInfo.pszzHardwareIds = hardwareIds;
	createInfo.pszDeviceDescription = description;
	createInfo.CapabilityFlags = SWDeviceCapabilitiesRemovable |
		SWDeviceCapabilitiesSilentInstall |
		SWDeviceCapabilitiesDriverRequired;
	HRESULT hr = SwDeviceCreate(name, L"HTREE\\ROOT\\0", &createInfo, 0,
		nullptr, CreationCallback, &hEvent, &hSwDevice);

	if (FAILED(hr))
		return false;
	DWORD waitResult = WaitForSingleObject(hEvent, 10 * 1000);
	if (waitResult != WAIT_OBJECT_0)
		return false;
	return true;
}

typedef void (WINAPI* RtlGetVersion_FUNC)(OSVERSIONINFOEXW*);
BOOL GetVersion2(OSVERSIONINFOEX* os) {
	HMODULE hMod;
	RtlGetVersion_FUNC func;
#ifdef UNICODE
	OSVERSIONINFOEXW* osw = os;
#else
	OSVERSIONINFOEXW o;
	OSVERSIONINFOEXW* osw = &o;
#endif
	hMod = LoadLibrary(TEXT("ntdll.dll"));
	if (hMod) {
		func = (RtlGetVersion_FUNC)GetProcAddress(hMod, "RtlGetVersion");
		if (func == 0) {
			FreeLibrary(hMod);
			return FALSE;
		}
		ZeroMemory(osw, sizeof(*osw));
		osw->dwOSVersionInfoSize = sizeof(*osw);
		func(osw);
#ifndef UNICODE
		os->dwBuildNumber = osw->dwBuildNumber;
		os->dwMajorVersion = osw->dwMajorVersion;
		os->dwMinorVersion = osw->dwMinorVersion;
		os->dwPlatformId = osw->dwPlatformId;
		os->dwOSVersionInfoSize = sizeof(*os);
		DWORD sz = sizeof(os->szCSDVersion);
		WCHAR* src = osw->szCSDVersion;
		unsigned char* dtc = (unsigned char*)os->szCSDVersion;
		while (*src)
			*dtc++ = (unsigned char)*src++;
		*dtc = '\0';
#endif

	}
	else
		return FALSE;
	FreeLibrary(hMod);
	return TRUE;
}

#include <newdev.h>
#pragma comment(lib, "Newdev.lib")
bool VirtualDisplay::InstallDriver()
{
	OSVERSIONINFOEX os;
	if (GetVersion2(&os) == TRUE && os.dwMajorVersion == 10 && os.dwBuildNumber >= 18632) {
		CHAR szdriverPath[MAX_PATH];
		if (GetModuleFileName(NULL, szdriverPath, MAX_PATH)) {
			char* p = strrchr(szdriverPath, '\\');
			if (p == NULL)
				return 0;
			*p = '\0';
#ifdef _X64
			strcat_s(szdriverPath, "\\UVncVirtualDisplay64\\UVncVirtualDisplay.inf");
#else
			strcat_s(szdriverPath, "\\UVncVirtualDisplay\\UVncVirtualDisplay.inf");
#endif
		}

		PBOOL restart = new BOOL();
		BOOL status = DiInstallDriverA(NULL, szdriverPath, NULL/*DIIRFLAG_FORCE_INF*/, restart);
		DWORD errorMessageID = ::GetLastError();
		LPSTR messageBuffer = nullptr;
		if (status == 0) {
			size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
		}
		return status;
	}
	return 0;
}