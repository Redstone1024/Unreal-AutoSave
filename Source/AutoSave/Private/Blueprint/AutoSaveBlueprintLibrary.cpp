#include "Blueprint/AutoSaveBlueprintLibrary.h"

#include "AutoSaveSubsystem.h"
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
