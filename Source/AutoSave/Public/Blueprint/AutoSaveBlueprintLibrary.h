#pragma once

#include "CoreMinimal.h"
#include "SaveStruct.h"
#include "AutoSaveSubsystem.h"
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
	
	UFUNCTION(BlueprintCallable, Category = "AutoSave", meta = (WorldContext = "WorldContextObject", CustomStructureParam = "Value"), CustomThunk)
	static void TryGetSaveStruct(UObject* WorldContextObject, const FString& Filename, int32& Value, bool& bSuccess) { checkNoEntry(); }
	static bool Generic_TryGetSaveStruct(UObject* WorldContextObject, const FString& Filename, UScriptStruct* ScriptStruct, void* Value);
	DECLARE_FUNCTION(execTryGetSaveStruct) 
	{
		P_GET_OBJECT(UObject, WorldContextObject);
		P_GET_PROPERTY_REF(FStrProperty, Filename);

		Stack.Step(Stack.Object, nullptr);
		void* StructPtr = Stack.MostRecentPropertyAddress;
		FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);

		P_GET_UBOOL_REF(bSuccess);

		P_FINISH;

		P_NATIVE_BEGIN;
		bSuccess = Generic_TryGetSaveStruct(WorldContextObject, Filename, StructProperty ? StructProperty->Struct : nullptr, StructPtr);
		P_NATIVE_END;
	}

	UFUNCTION(BlueprintCallable, Category = "AutoSave", meta = (WorldContext = "WorldContextObject", CustomStructureParam = "Value"), CustomThunk)
	static void TrySetSaveStruct(UObject* WorldContextObject, const FString& Filename, const int32& Value, bool& bSuccess) { checkNoEntry(); }
	static bool Generic_TrySetSaveStruct(UObject* WorldContextObject, const FString& Filename, UScriptStruct* ScriptStruct, void* Value);
	DECLARE_FUNCTION(execTrySetSaveStruct)
	{
		P_GET_OBJECT(UObject, WorldContextObject);
		P_GET_PROPERTY_REF(FStrProperty, Filename);

		Stack.Step(Stack.Object, nullptr);
		void* StructPtr = Stack.MostRecentPropertyAddress;
		FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);

		P_GET_UBOOL_REF(bSuccess);

		P_FINISH;

		P_NATIVE_BEGIN;
		bSuccess = Generic_TrySetSaveStruct(WorldContextObject, Filename, StructProperty ? StructProperty->Struct : nullptr, StructPtr);
		P_NATIVE_END;
	}
};
