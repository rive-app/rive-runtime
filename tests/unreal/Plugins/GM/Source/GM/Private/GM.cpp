// Copyright Epic Games, Inc. All Rights Reserved.

#include "GM.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "gms.hpp"
#include "goldens.hpp"

#define LOCTEXT_NAMESPACE "FGMModule"

void FGMModule::StartupModule() {}

void FGMModule::ShutdownModule() {}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGMModule, GM)

DEFINE_LOG_CATEGORY(GM_Log);