#ifndef _XBOX_CONTROLLER_H_
#define _XBOX_CONTROLLER_H_

// No MFC
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <XInput.h>

// XInput Library
//#pragma comment(lib, "XInput.lib")    // Windows 8 and higher
#pragma comment(lib, "XINPUT9_1_0.lib") // Windows 7

class XInputUtils
{
private:
	XINPUT_STATE _controllerState;
	int _controllerNum;
public:
	XInputUtils(int playerNumber);
	XINPUT_STATE GetState();
	bool IsConnected();
};

#endif