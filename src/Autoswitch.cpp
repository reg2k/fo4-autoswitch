#include <shlobj.h>

#include "f4se/PluginAPI.h"
#include "f4se/GameInput.h"

#include <Xinput.h>
#include "XInputUtils.h"

#include "f4se_common/f4se_version.h"
#include "f4se_common/BranchTrampoline.h"

#include "xbyak/xbyak.h"

#include "Config.h"
#include "rva/RVA.h"

#define INI_LOCATION "./Data/F4SE/autoswitch.ini"

IDebugLog gLog;
PluginHandle g_pluginHandle = kPluginHandle_Invalid;
F4SEMessagingInterface  *g_messaging = NULL;

//--------------------
// Addresses [2]
//--------------------

// v1.9.4   : 37D5EE0
// v1.10.20	: 38642C0
// v1.10.26	: 38642C0
// cmp     cs:g_gamepadEnabled, 0
RVA <unsigned char> g_gamepadEnabled({
    { RUNTIME_VERSION_1_10_75, 0x0387E340 },
    { RUNTIME_VERSION_1_10_64, 0x0387E340 },
    { RUNTIME_VERSION_1_10_50, 0x0387D340 },
    { RUNTIME_VERSION_1_10_40, 0x0387F340 },
    { RUNTIME_VERSION_1_10_26, 0x038642C0 }
}, "80 3D ? ? ? ? ? 74 1A 80 3D ? ? ? ? ?", 0, 2, 7);

// v1.9.4   : 1B140F0
// v1.10.20	: 1B2FC70
// v1.10.26	: 1B2FD30
typedef void(*_GamepadUpdate)(void * unk1, void * unk2, void * unk3, void * unk4);
RVA <_GamepadUpdate> GamepadUpdate({
    { RUNTIME_VERSION_1_10_75, 0x01B31B30 },
    { RUNTIME_VERSION_1_10_64, 0x01B31B70 },
    { RUNTIME_VERSION_1_10_50, 0x01B31750 },
    { RUNTIME_VERSION_1_10_40, 0x01B31690 },
    { RUNTIME_VERSION_1_10_26, 0x01B2FD30 }
}, "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 54 41 56 41 57 48 81 EC ? ? ? ? 45 33 F6");
_GamepadUpdate GamepadUpdate_Original = nullptr;

// XInput
XInputUtils* xinputUtils;

// State
bool gamepadButtonPressed = false;	// whether or not a button is being pressed on the gamepad.

void checkGamepadState() {
	if (!*g_gamepadEnabled || gamepadButtonPressed) {
		XINPUT_STATE state;
		ZeroMemory(&state, sizeof(XINPUT_STATE));
		DWORD result = XInputGetState(0, &state);
		if (result != ERROR_SUCCESS) return;

		if (state.Gamepad.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
			state.Gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
			state.Gamepad.sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
			state.Gamepad.sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
			state.Gamepad.sThumbRX > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE ||
			state.Gamepad.sThumbRX < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE ||
			state.Gamepad.sThumbRY > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE ||
			state.Gamepad.sThumbRY < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE ||
			state.Gamepad.wButtons > 0 ||
			state.Gamepad.bLeftTrigger > 0 ||
			state.Gamepad.bRightTrigger > 0) {
			// Controller active; switch to it.
			gamepadButtonPressed = true;
			if (!*g_gamepadEnabled) {
				(*g_gamepadEnabled) = 1;
				_MESSAGE("Switched to gamepad.");
			}
		} else {
			// Not pressed.
			gamepadButtonPressed = false;
		}
	}
}

class F4SEInputHandler : public BSInputEventUser
{
public:
	F4SEInputHandler() : BSInputEventUser(true) { }

	virtual void OnMouseMoveEvent(MouseMoveEvent * inputEvent) {
		if (*g_gamepadEnabled && !gamepadButtonPressed) {
			(*g_gamepadEnabled) = 0;
			_MESSAGE("Switched to KB/M.");
		};
	}

	virtual void OnButtonEvent(ButtonEvent * inputEvent)
	{
		UInt32	deviceType = inputEvent->deviceType;

		float timer  = inputEvent->timer;
		bool  isDown = inputEvent->isDown == 1.0f && timer == 0.0f;
		bool  isUp   = inputEvent->isDown == 0.0f && timer != 0.0f;

		// Keyboard
		if (deviceType == InputEvent::kDeviceType_Keyboard && isDown && *g_gamepadEnabled && !gamepadButtonPressed) {
			(*g_gamepadEnabled) = 0;
			_MESSAGE("Switched to KB/M.");
		}
	}
};

F4SEInputHandler g_inputHandler;

// Event Handling
void onF4SEMessage(F4SEMessagingInterface::Message* msg) {
    switch (msg->type) {
        case F4SEMessagingInterface::kMessage_GameLoaded:
            // TODO: this is not version independent
            (*g_menuControls)->inputEvents.Push(&g_inputHandler);
            _MESSAGE("Registered for input events.");
            break;
    }
}

void GamepadUpdate_Hook(void * unk1, void * unk2, void * unk3, void * unk4) {
	checkGamepadState();
	GamepadUpdate_Original(unk1, unk2, unk3, unk4);
}

extern "C"
{

bool F4SEPlugin_Query(const F4SEInterface * f4se, PluginInfo * info)
{
	gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Fallout4\\F4SE\\autoswitch.log");

	_MESSAGE("autoswitch v%s", PLUGIN_VERSION_STRING);
	_MESSAGE("autoswitch query");

	// populate info structure
	info->infoVersion =	PluginInfo::kInfoVersion;
	info->name        = "autoswitch";
	info->version     = PLUGIN_VERSION;

	// store plugin handle so we can identify ourselves later
	g_pluginHandle = f4se->GetPluginHandle();

	// Check user setting
	BOOL bEnable = GetPrivateProfileInt("Autoswitch", "bEnable", 1, INI_LOCATION);
	if (bEnable) {
		_MESSAGE("autoswitch is enabled.");
	} else {
		_MESSAGE("autoswitch is not enabled and will not be loaded.");
		return false;
	}

	// Check game version
    if (!COMPATIBLE(f4se->runtimeVersion)) {
        char str[512];
        sprintf_s(str, sizeof(str), "Your game version: v%d.%d.%d.%d\nExpected version: v%d.%d.%d.%d\n%s will be disabled.",
            GET_EXE_VERSION_MAJOR(f4se->runtimeVersion),
            GET_EXE_VERSION_MINOR(f4se->runtimeVersion),
            GET_EXE_VERSION_BUILD(f4se->runtimeVersion),
            GET_EXE_VERSION_SUB(f4se->runtimeVersion),
            GET_EXE_VERSION_MAJOR(SUPPORTED_RUNTIME_VERSION),
            GET_EXE_VERSION_MINOR(SUPPORTED_RUNTIME_VERSION),
            GET_EXE_VERSION_BUILD(SUPPORTED_RUNTIME_VERSION),
            GET_EXE_VERSION_SUB(SUPPORTED_RUNTIME_VERSION),
            PLUGIN_NAME_LONG
        );

        MessageBox(NULL, str, PLUGIN_NAME_LONG, MB_OK | MB_ICONEXCLAMATION);
        return false;
    }

    if (f4se->runtimeVersion != SUPPORTED_RUNTIME_VERSION) {
        _MESSAGE("INFO: Game version (%08X), target (%08X).", f4se->runtimeVersion, SUPPORTED_RUNTIME_VERSION);
    }

	// Get messaging interface
	g_messaging = (F4SEMessagingInterface *)f4se->QueryInterface(kInterface_Messaging);

	// Get gamepad input.
	xinputUtils = new XInputUtils(1);
	_MESSAGE("Gamepad connected: %d", xinputUtils->IsConnected());

	if (!xinputUtils->IsConnected() && GetPrivateProfileInt("Autoswitch", "bRequireGamepadOnStartup", 0, INI_LOCATION)) {
		_MESSAGE("Gamepad not connected. Plugin is disabled.");
		delete xinputUtils;
		return false;
	}

	return true;
}

bool F4SEPlugin_Load(const F4SEInterface *f4se)
{
	_MESSAGE("autoswitch load");

    RVAManager::UpdateAddresses(f4se->runtimeVersion);

	// Register for F4SE messages
	g_messaging->RegisterListener(g_pluginHandle, "F4SE", onF4SEMessage);

	if (!g_localTrampoline.Create(1024 * 64, nullptr)) {
		_ERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
		return false;
	}

	if (!g_branchTrampoline.Create(1024 * 64)) {
		_ERROR("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
		return false;
	}

	// Hook gamepad update
	{
		struct GamepadUpdate_Code : Xbyak::CodeGenerator {
			GamepadUpdate_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
			{
				Xbyak::Label retnLabel;

				mov(ptr[rsp + 0x8], rbx);

				jmp(ptr[rip + retnLabel]);

				L(retnLabel);
				dq(GamepadUpdate.GetUIntPtr() + 5);
			}
		};

		void * codeBuf = g_localTrampoline.StartAlloc();
		GamepadUpdate_Code code(codeBuf);
		g_localTrampoline.EndAlloc(code.getCurr());

		GamepadUpdate_Original = (_GamepadUpdate)codeBuf;

		g_branchTrampoline.Write5Branch(GamepadUpdate.GetUIntPtr(), (uintptr_t)GamepadUpdate_Hook);
		_MESSAGE("Hook complete.");
	}

	return true;
}

};
