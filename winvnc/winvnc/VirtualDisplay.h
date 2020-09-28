#pragma once
#include <iostream>
#include <Windows.h>
#include <stdio.h>
#include <vector>
#include <list>
#include <swdevice.h>
#include <aclapi.h>

const static LPCSTR g_szIPC = ("Global\\{4A77E11C-B0B4-40F9-AA8B-D249116A76FE}");

typedef struct _SUPPORTEDMONITORS
{
	int counter;
	int w[200];
	int h[200];
}SUPPORTEDMONITORS;

typedef struct _DISPLAYINFO
{
	DEVMODE dm;
	CHAR* naam;
}DISPLAYINFO;

typedef struct _VIRTUALDISPLAY
{
	HSWDEVICE hDevice;
	HANDLE hEvent;
}VIRTUALDISPLAY;

class VirtualDisplay
{
private:
	LPVOID FileView;
	HANDLE hFileMap;
	SUPPORTEDMONITORS* pbuff;
	std::list<DISPLAYINFO> diplayInfoList;
	std::list <VIRTUALDISPLAY> virtualDisplayList;
	bool initialized;
	bool restoreNeeded;
public:
	VirtualDisplay();
	~VirtualDisplay();
	void SetVirtualMonitorsSize(int height, int width);
	void AddVirtualMonitors();
	bool AddVirtualDisplay(HSWDEVICE& hSwDevice, HANDLE& hEvent, WCHAR* name);
	static bool InstallDriver();
};

