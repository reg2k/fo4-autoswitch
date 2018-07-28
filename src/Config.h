#pragma once
#include "f4se_common/f4se_version.h"

//-----------------------
// Plugin Information
//-----------------------
#define PLUGIN_VERSION              12
#define PLUGIN_VERSION_STRING       "1.5.5"
#define PLUGIN_NAME_SHORT           "autoswitch"
#define PLUGIN_NAME_LONG            "Autoswitch"
#define SUPPORTED_RUNTIME_VERSION   CURRENT_RELEASE_RUNTIME
#define MINIMUM_RUNTIME_VERSION     RUNTIME_VERSION_1_10_26
#define COMPATIBLE(runtimeVersion)  (runtimeVersion == SUPPORTED_RUNTIME_VERSION)

//-------------
// Addresses
//-------------
// [Count]  Location
// [2]      Autoswitch.cpp
