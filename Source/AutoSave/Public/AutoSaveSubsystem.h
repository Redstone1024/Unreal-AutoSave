#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AutoSaveSubsystem.generated.h"

USTRUCT(BlueprintType)
struct AUTOSAVE_API FSaveStruct { GENERATED_BODY() };

DECLARE_DYNAMIC_DELEGATE_OneParam(FSaveStructLoadDelegate, const FString&, Filename);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSaveStructLoadDelegates, const FString&, Filename);

UENUM(BlueprintType, Category = "AutoSave")
enum class ESaveStructState : uint8
{
	Preload,
	Loading,
	Idle,
	Saving,
};

USTRUCT(BlueprintType)
struct AUTOSAVE_API FSaveStructInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "AutoSave")
	FString Filename;

	UPROPERTY(BlueprintReadWrite, Category = "AutoSave")
	UScriptStruct* Struct;

	UPROPERTY(BlueprintReadWrite, Category = "AutoSave")
	ESaveStructState State;

	UPROPERTY(BlueprintReadWrite, Category = "AutoSave")
	int32 RefConut;

	UPROPERTY(BlueprintReadWrite, Category = "AutoSave")
	int32 LastRefConut;

	UPROPERTY(BlueprintReadWrite, Category = "AutoSave")
	FDateTime LastSaveTime;

	UPROPERTY()
	TArray<uint8> Data;
	// FSaveStruct* Data;

	UPROPERTY()
	FSaveStructLoadDelegates OnLoaded;

};

UCLASS(Config = Engine, DefaultConfig)
class AUTOSAVE_API UAutoSaveSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	friend class UAutoSaveBlueprintLibrary;
	template<typename SaveStructType> friend class FSaveStructPtr;

public:

	UAutoSaveSubsystem(const class FObjectInitializer& ObjectInitializer);

	UPROPERTY(Config, EditAnywhere, Category = "AutoSave")
	int32 MaxThreadNum = 4;

	UPROPERTY(Config, EditAnywhere, Category = "AutoSave")
	FTimespan SaveWaitTime = FTimespan(ETimespan::MaxTicks);
	
	UFUNCTION(BlueprintCallable, Category = "AutoSave")
	void GetSaveStructInfosWithoutData(TArray<FSaveStructInfo>& OutSaveStructInfos) const;

	UFUNCTION(BlueprintPure, Category = "AutoSave")
	int32 GetIdleThreadNum() const;

	FSaveStruct* AddSaveStructRef(const FString& Filename, UScriptStruct* ScriptStruct = nullptr, FSaveStructLoadDelegate OnLoaded = FSaveStructLoadDelegate());

	void RemoveSaveStructRef(const FString& Filename);
	
private:

	UPROPERTY()
	TMap<FString, UScriptStruct*> ScriptStructHooker;

	TMap<FString, TUniquePtr<FSaveStructInfo>> StructInfos;

	class FStructLoadOrSaveTask : public FNonAbandonableTask
	{
		friend class FAsyncTask<FStructLoadOrSaveTask>;

		TArray<uint8> DataCopy;

		FSaveStructInfo* StructInfoPtr;

		FStructLoadOrSaveTask(FSaveStructInfo* InStructInfoPtr);

		~FStructLoadOrSaveTask();

		void DoWork();

		void LoadWork();

		void SaveWork();

		FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FStructLoadOrSaveTask, STATGROUP_ThreadPoolAsyncTasks); }

	};

	TArray<TUniquePtr<FAsyncTask<FStructLoadOrSaveTask>>> TaskThreads;

	void HandleTaskStart();

	void HandleTaskDone();

private:

	//~ Begin USubsystem Interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//~ Begin FTickableGameObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual TStatId GetStatId() const override { return GetStatID(); }
	//~ End FTickableGameObject Interface

};
