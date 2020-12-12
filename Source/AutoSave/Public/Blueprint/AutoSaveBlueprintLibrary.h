#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AutoSaveBlueprintLibrary.generated.h"

UCLASS()
class AUTOSAVE_API UAutoSaveBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "AutoSave", meta = (WorldContext = "WorldContextObject"))
	static bool AddSaveStructRef(UObject* WorldContextObject, const FString& Filename, UScriptStruct* ScriptStruct);

	UFUNCTION(BlueprintCallable, Category = "AutoSave", meta = (WorldContext = "WorldContextObject"))
	static void RemoveSaveStructRef(UObject* WorldContextObject, const FString& Filename);

};
