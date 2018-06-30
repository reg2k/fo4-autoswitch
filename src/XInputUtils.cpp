#include "XInputUtils.h"

XInputUtils::XInputUtils(int playerNumber)
{
	// Set the controller number
	_controllerNum = playerNumber - 1;
}

XINPUT_STATE XInputUtils::GetState()
{
	ZeroMemory(&_controllerState, sizeof(XINPUT_STATE));

	XInputGetState(_controllerNum, &_controllerState);

	return _controllerState;
}

bool XInputUtils::IsConnected()
{
	ZeroMemory(&_controllerState, sizeof(XINPUT_STATE));

	// Get the state
	DWORD Result = XInputGetState(_controllerNum, &_controllerState);

	if(Result == ERROR_SUCCESS)
	{
		return true;
	}
	else
	{
		return false;
	}
}