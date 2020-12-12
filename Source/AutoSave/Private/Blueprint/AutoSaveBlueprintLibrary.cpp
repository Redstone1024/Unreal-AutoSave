#include "Blueprint/AutoSaveBlueprintLibrary.h"

#include "Kismet/GameplayStatics.h"

bool UAutoSaveBlueprintLibrary::AddSaveStructRef(UObject * WorldContextObject, const FString & Filename, UScriptStruct * ScriptStruct)
{
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);

	if (!GameInstance) return false;

	UAutoSaveSubsystem* AutoSaveSubsystem = GameInstance->GetSubsystem<UAutoSaveSubsystem>();
	
	if (!AutoSaveSubsystem) return false;

	return AutoSaveSubsystem->AddSaveStructRef(Filename, ScriptStruct) != nullptr;
}

void UAutoSaveBlueprintLibrary::RemoveSaveStructRef(UObject * WorldContextObject, const FString & Filename)
{
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);

	if (!GameInstance) return;

	UAutoSaveSubsystem* AutoSaveSubsystem = GameInstance->GetSubsystem<UAutoSaveSubsystem>();

	if (!AutoSaveSubsystem) return;

	AutoSaveSubsystem->RemoveSaveStructRef(Filename);
}

bool UAutoSaveBlueprintLibrary::Generic_TryGetSaveStruct(UObject * WorldContextObject, const FString & Filename, UScriptStruct * ScriptStruct, void * Value)
{
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);

	if (!GameInstance) return false;

	UAutoSaveSubsystem* AutoSaveSubsystem = GameInstance->GetSubsystem<UAutoSaveSubsystem>();

	if (!AutoSaveSubsystem) return false;

	if (!AutoSaveSubsystem->StructInfos.Contains(Filename)) return false;

	FSaveStructInfo* Info = AutoSaveSubsystem->StructInfos[Filename].Get();

	if (Info->State == ESaveStructState::Preload || Info->State == ESaveStructState::Loading) return false;

	if (Info->Struct != ScriptStruct) return false;

	ScriptStruct->CopyScriptStruct(Value, Info->Data.GetData());

	return true;
}

bool UAutoSaveBlueprintLibrary::Generic_TrySetSaveStruct(UObject * WorldContextObject, const FString & Filename, UScriptStruct * ScriptStruct, void * Value)
{
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);

	if (!GameInstance) return false;

	UAutoSaveSubsystem* AutoSaveSubsystem = GameInstance->GetSubsystem<UAutoSaveSubsystem>();

	if (!AutoSaveSubsystem) return false;

	if (!AutoSaveSubsystem->StructInfos.Contains(Filename)) return false;

	FSaveStructInfo* Info = AutoSaveSubsystem->StructInfos[Filename].Get();

	if (Info->State == ESaveStructState::Preload || Info->State == ESaveStructState::Loading) return false;

	if (Info->Struct != ScriptStruct) return false;

	ScriptStruct->CopyScriptStruct(Info->Data.GetData(), Value);

	return true;
}
