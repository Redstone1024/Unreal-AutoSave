// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoSave.h"
#include "AutoSaveSubsystem.h"
#include "Developer/Settings/Public/ISettingsModule.h"

#define LOCTEXT_NAMESPACE "FAutoSaveModule"

void FAutoSaveModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	if (ISettingsModule* SettingModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingModule->RegisterSettings("Project", "Plugins", "Auto Save",
			LOCTEXT("RuntimeSettingsName", "Auto Save"),
			LOCTEXT("RuntimeSettingsDescription", "Configure the Auto Save plugin"),
			GetMutableDefault<UAutoSaveSubsystem>()
		);
	}
}

void FAutoSaveModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Auto Save");
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAutoSaveModule, AutoSave)