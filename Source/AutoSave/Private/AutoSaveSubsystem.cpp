#include "AutoSaveSubsystem.h"

#include "AutoSaveLog.h"
#include "Engine/UserDefinedStruct.h"
#include "Serialization/MemoryReader.h"	
#include "Serialization/MemoryWriter.h"	

UAutoSaveSubsystem::UAutoSaveSubsystem(const class FObjectInitializer & ObjectInitializer)
{
}

FString UAutoSaveSubsystem::GetSaveStructDebugString() const
{
	FString Result;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	for (const TPair<FString, TUniquePtr<FSaveStructInfo>>& Info : StructInfos)
	{
		Result.Append(Info.Value->Filename);

		Result.Append(TEXT(" - "));

		Result.Append(Info.Value->Struct->GetName());

		Result.Append(TEXT(" - "));

		switch (Info.Value->State)
		{
		case ESaveStructState::Preload:
			Result.Append(TEXT("Preload"));
			break;
		case ESaveStructState::Loading:
			Result.Append(TEXT("Loading"));
			break;
		case ESaveStructState::Idle:
			Result.Append(TEXT("Idle"));
			break;
		case ESaveStructState::Saving:
			Result.Append(TEXT("Saving"));
			break;
		default: checkNoEntry();
		}

		Result.Append(TEXT(" - "));

		Result.Append(FString::Printf(TEXT("%d"), Info.Value->RefConut));

		Result.Append(TEXT(" - "));

		Result.Append(FString::Printf(TEXT("%d"), Info.Value->LastRefConut));

		Result.Append(TEXT(" - "));

		Result.Append(FString::Printf(TEXT("%f"), (FDateTime::Now() - Info.Value->LastSaveTime).GetTotalSeconds()));

		Result.Append(TEXT("\n"));
	}

#endif

	return Result;
}

int32 UAutoSaveSubsystem::GetIdleThreadNum() const
{
	int32 Result = 0;

	for (const TUniquePtr<FAsyncTask<FStructLoadOrSaveTask>>& Task : TaskThreads)
	{
		if (!Task) ++Result;
	}

	return Result;
}

FSaveStruct * UAutoSaveSubsystem::AddSaveStructRef(const FString& Filename, UScriptStruct * ScriptStruct)
{
	if (StructInfos.Contains(Filename))
	{
		FSaveStructInfo* StructInfo = StructInfos[Filename].Get();

		if (ScriptStruct && ScriptStruct != StructInfo->Struct)
		{
			UE_LOG(LogAutoSave, Warning, TEXT("The requested Save Struct '%s' type conflicts with the existing."), *Filename);
			return nullptr;
		}

		// Increase the reference count of SaveStruct by one, and then decrease it accordingly in UAutoSaveSubsystem::RemoveSaveStructRef
		StructInfo->RefConut++;

		return (FSaveStruct*)StructInfo->Data.GetData();
	}

	if (!ScriptStruct) return nullptr;

	const bool bIsCppStruct = ScriptStruct->IsChildOf(FSaveStruct::StaticStruct());
	const bool bIsBlueprintStruct = ScriptStruct->GetClass() == UUserDefinedStruct::StaticClass();

	if (!bIsCppStruct && !bIsBlueprintStruct)
		return nullptr;

	TUniquePtr<FSaveStructInfo> NewStructInfo(new FSaveStructInfo());

	if (FPaths::FileExists(Filename))
	{
		NewStructInfo->Filename = Filename;
		NewStructInfo->Struct = ScriptStruct;
		NewStructInfo->State = ESaveStructState::Preload;
		NewStructInfo->RefConut = 1;
		NewStructInfo->LastRefConut = 0;
		NewStructInfo->LastSaveTime = FDateTime::Now();
		NewStructInfo->Data.SetNumUninitialized(ScriptStruct->GetStructureSize());
		ScriptStruct->InitializeStruct(NewStructInfo->Data.GetData());
	}
	else
	{
		// Check if the target is writable
		if (!FFileHelper::SaveStringToFile(TEXT(""), *Filename))
			return nullptr;

		NewStructInfo->Filename = Filename;
		NewStructInfo->Struct = ScriptStruct;
		NewStructInfo->State = ESaveStructState::Idle;
		NewStructInfo->RefConut = 1;
		NewStructInfo->LastRefConut = 0;
		NewStructInfo->LastSaveTime = FDateTime::Now();
		NewStructInfo->Data.SetNumUninitialized(ScriptStruct->GetStructureSize());
		ScriptStruct->InitializeStruct(NewStructInfo->Data.GetData());
	}

	ScriptStructHooker.Add(Filename, ScriptStruct);

	StructInfos.Add(Filename, nullptr);
	StructInfos[Filename].Reset(NewStructInfo.Release());

	return (FSaveStruct*)StructInfos[Filename]->Data.GetData();
}

FSaveStruct * UAutoSaveSubsystem::AddSaveStructRef(const FString & Filename, UScriptStruct * ScriptStruct, FSaveStructLoadDelegate LoadCallback)
{
	FSaveStruct* Result = AddSaveStructRef(Filename, ScriptStruct);

	if (!LoadCallback.IsBound()) return Result;

	if (!Result) return nullptr;

	if (!LoadDelegates.Contains(Filename))
	{
		LoadDelegates.Add(Filename);
	}

	LoadDelegates[Filename].Add(LoadCallback);

	return Result;
}

FSaveStruct * UAutoSaveSubsystem::AddSaveStructRef(const FString & Filename, UScriptStruct * ScriptStruct, FSaveStructLoadDynamicDelegate LoadCallback)
{
	FSaveStruct* Result = AddSaveStructRef(Filename, ScriptStruct);

	if (!LoadCallback.IsBound()) return Result;

	if (!Result) return nullptr;

	if (!LoadDynamicDelegates.Contains(Filename))
	{
		LoadDynamicDelegates.Add(Filename);
	}

	LoadDynamicDelegates[Filename].Add(LoadCallback);

	return Result;
}

void UAutoSaveSubsystem::RemoveSaveStructRef(const FString& Filename)
{
	if (StructInfos.Contains(Filename))
	{
		if (StructInfos[Filename]->RefConut > 0)
		{
			// Decrement the reference count of SaveStruct by one, and increase it accordingly in UAutoSaveSubsystem::AddSaveStructRef
			StructInfos[Filename]->RefConut--;
		}
		else
		{
			UE_LOG(LogAutoSave, Warning, TEXT("Save Struct '%s' reference is negative, But was tried to remove the reference."), *Filename);
		}
	}
	else
	{
		UE_LOG(LogAutoSave, Warning, TEXT("Save Struct '%s' is invalid, But was tried to remove the reference."), *Filename);
	}
}

UAutoSaveSubsystem::FStructLoadOrSaveTask::FStructLoadOrSaveTask(FSaveStructInfo * InStructInfoPtr)
	: StructInfoPtr(InStructInfoPtr)
{
	StructInfoPtr->LastRefConut = StructInfoPtr->RefConut;
	StructInfoPtr->LastSaveTime = FDateTime::Now();

	switch (StructInfoPtr->State)
	{
	case ESaveStructState::Preload:
		StructInfoPtr->State = ESaveStructState::Loading;
		DataCopy = StructInfoPtr->Data;
		break;

	case ESaveStructState::Idle:
		StructInfoPtr->State = ESaveStructState::Saving;
		DataCopy = StructInfoPtr->Data;
		break;

	default: checkNoEntry()
	}
}

UAutoSaveSubsystem::FStructLoadOrSaveTask::~FStructLoadOrSaveTask()
{
	switch (StructInfoPtr->State)
	{
	case ESaveStructState::Loading:
		StructInfoPtr->State = ESaveStructState::Idle;
		StructInfoPtr->Data = DataCopy;
		break;

	case ESaveStructState::Saving:
		StructInfoPtr->State = ESaveStructState::Idle;
		break;

	default: checkNoEntry()
	}
}

void UAutoSaveSubsystem::FStructLoadOrSaveTask::DoWork()
{
	switch (StructInfoPtr->State)
	{
	case ESaveStructState::Loading:
		LoadWork();
		break;

	case ESaveStructState::Saving:
		SaveWork();
		break;

	default: checkNoEntry()
	}
}

void UAutoSaveSubsystem::FStructLoadOrSaveTask::LoadWork()
{
	TArray<uint8> DataBuffer;

	const bool bSuccessful = FFileHelper::LoadFileToArray(DataBuffer, *StructInfoPtr->Filename);

	check(bSuccessful);

	FMemoryReader MemoryReader(DataBuffer);

	UScriptStruct* Struct = StructInfoPtr->Struct;

	check(Struct);

	Struct->SerializeItem(MemoryReader, DataCopy.GetData(), nullptr);
}

void UAutoSaveSubsystem::FStructLoadOrSaveTask::SaveWork()
{
	TArray<uint8> DataBuffer;
	FMemoryWriter MemoryWriter(DataBuffer);

	UScriptStruct* Struct = StructInfoPtr->Struct;

	check(Struct);

	Struct->SerializeItem(MemoryWriter, DataCopy.GetData(), nullptr);

	const bool bSuccessful = FFileHelper::SaveArrayToFile(DataBuffer, *StructInfoPtr->Filename);

	check(bSuccessful);
}

void UAutoSaveSubsystem::HandleTaskStart()
{
	const FDateTime NowTime = FDateTime::Now();

	TFunction<FSaveStructInfo*(void)> FindPreHandleStruct = [&]() -> FSaveStructInfo*
	{
		FSaveStructInfo* PreHandleStruct = nullptr;

		for (const TPair<FString, TUniquePtr<FSaveStructInfo>>& Info : StructInfos)
		{
			check(Info.Value);

			if (Info.Value->State == ESaveStructState::Preload)
			{
				PreHandleStruct = Info.Value.Get();
				break;
			}

			if (Info.Value->State != ESaveStructState::Idle) continue;
			
			if (Info.Value->RefConut == 0)
			{
				PreHandleStruct = Info.Value.Get();
				break;
			}

			const bool bTimeout = PreHandleStruct
				? PreHandleStruct->LastSaveTime > Info.Value->LastSaveTime
				: NowTime - Info.Value->LastSaveTime > SaveWaitTime;

			if (bTimeout)
			{
				PreHandleStruct = Info.Value.Get();
			}
		}

		return PreHandleStruct;
	};

	if (MaxThreadNum <= 0)
	{
		FSaveStructInfo* PreHandleStruct = FindPreHandleStruct();

		if (PreHandleStruct) 
		{
			FAsyncTask<FStructLoadOrSaveTask> Task(PreHandleStruct);
			Task.StartSynchronousTask();
		}
	}

	for (TUniquePtr<FAsyncTask<FStructLoadOrSaveTask>>& Task : TaskThreads)
	{
		if (Task) continue;

		FSaveStructInfo* PreHandleStruct = FindPreHandleStruct();

		if (!PreHandleStruct) break;

		Task.Reset(new FAsyncTask<FStructLoadOrSaveTask>(PreHandleStruct));
		Task->StartBackgroundTask();
	}

}

void UAutoSaveSubsystem::HandleTaskDone()
{
	for (TUniquePtr<FAsyncTask<FStructLoadOrSaveTask>>& Task : TaskThreads)
	{
		if (!Task) continue;
		if (!Task->IsDone()) continue;
		Task = nullptr;
	}

	TArray<FString> StructToRemove;

	for (const TPair<FString, TUniquePtr<FSaveStructInfo>>& Info : StructInfos)
	{
		check(Info.Value);

		if (Info.Value->State != ESaveStructState::Idle) continue;

		if (Info.Value->RefConut <= 0 && Info.Value->LastRefConut <= 0)
		{
			StructToRemove.Add(Info.Value->Filename);
		}
	}

	for (const FString& Filename : StructToRemove)
	{
		StructInfos.Remove(Filename);
		ScriptStructHooker.Remove(Filename);
	}
}

void UAutoSaveSubsystem::HandleLoadDelegates()
{
	// Delegates
	{
		TArray<FString> DelegatesToRemove;

		for (const TPair<FString, FSaveStructLoadDelegates>& Delegates : LoadDelegates)
		{
			if (!StructInfos.Contains(Delegates.Key))
			{
				DelegatesToRemove.Add(Delegates.Key);
				continue;
			}

			if (StructInfos[Delegates.Key]->State == ESaveStructState::Idle || StructInfos[Delegates.Key]->State == ESaveStructState::Saving)
			{
				Delegates.Value.Broadcast(Delegates.Key);
				DelegatesToRemove.Add(Delegates.Key);
			}
		}

		for (const FString& Filename : DelegatesToRemove)
		{
			LoadDelegates.Remove(Filename);
		}
	}

	// DynamicDelegates
	{
		TArray<FString> DynamicDelegatesToRemove;

		for (const TPair<FString, FSaveStructLoadDynamicDelegates>& Delegates : LoadDynamicDelegates)
		{
			if (!StructInfos.Contains(Delegates.Key))
			{
				DynamicDelegatesToRemove.Add(Delegates.Key);
				continue;
			}

			if (StructInfos[Delegates.Key]->State == ESaveStructState::Idle || StructInfos[Delegates.Key]->State == ESaveStructState::Saving)
			{
				Delegates.Value.Broadcast(Delegates.Key);
				DynamicDelegatesToRemove.Add(Delegates.Key);
			}
		}

		for (const FString& Filename : DynamicDelegatesToRemove)
		{
			LoadDynamicDelegates.Remove(Filename);
		}
	}
}

void UAutoSaveSubsystem::Initialize(FSubsystemCollectionBase & Collection)
{
	if (MaxThreadNum > 0)
		TaskThreads.SetNum(MaxThreadNum);
}

void UAutoSaveSubsystem::Deinitialize()
{
	// Make sure the tasks are completed
	for (TUniquePtr<FAsyncTask<FStructLoadOrSaveTask>>& Task : TaskThreads)
	{
		if (!Task) continue;
		Task->EnsureCompletion();
		Task = nullptr;
	}

	// Make sure objects are saved
	for (const TPair<FString, TUniquePtr<FSaveStructInfo>>& Info : StructInfos)
	{
		// Skip objects that are not loaded
		if (Info.Value->State == ESaveStructState::Preload) continue;

		check(Info.Value->State == ESaveStructState::Idle);

		if (Info.Value->RefConut > 0)
		{
			UE_LOG(LogAutoSave, Warning, TEXT("The subsystem deinitialize, but '%s' still has references."), *Info.Value->Filename);
		}

		FAsyncTask<FStructLoadOrSaveTask> Task(Info.Value.Get());
		Task.StartSynchronousTask();
	}
}

void UAutoSaveSubsystem::Tick(float DeltaTime)
{
	HandleTaskDone();
	HandleTaskStart();
	HandleLoadDelegates();
}
