#include "AutoSaveSubsystem.h"

#include "AutoSaveLog.h"
#include "Engine/UserDefinedStruct.h"
#include "Serialization/MemoryReader.h"	
#include "Serialization/MemoryWriter.h"	

UAutoSaveSubsystem::UAutoSaveSubsystem(const class FObjectInitializer & ObjectInitializer)
{
}

void UAutoSaveSubsystem::GetSaveStructInfosWithoutData(TArray<FSaveStructInfo>& OutSaveStructInfos) const
{
	OutSaveStructInfos.SetNum(StructInfos.Num());

	int32 Index = 0;

	for (const TPair<FString, TUniquePtr<FSaveStructInfo>>& Info : StructInfos)
	{
		OutSaveStructInfos[Index].Filename     = Info.Value->Filename;
		OutSaveStructInfos[Index].Struct       = Info.Value->Struct;
		OutSaveStructInfos[Index].State        = Info.Value->State;
		OutSaveStructInfos[Index].RefConut     = Info.Value->RefConut;
		OutSaveStructInfos[Index].LastRefConut = Info.Value->LastRefConut;
		OutSaveStructInfos[Index].LastSaveTime = Info.Value->LastSaveTime;

		++Index;
	}
}

FSaveStruct * UAutoSaveSubsystem::AddSaveStructRef(const FString& Filename, UScriptStruct * ScriptStruct)
{
	const bool bIsCppStruct = ScriptStruct->IsChildOf(FSaveStruct::StaticStruct());
	const bool bIsBlueprintStruct = ScriptStruct->GetClass() == UUserDefinedStruct::StaticClass();

	if (!bIsCppStruct && !bIsBlueprintStruct)
		return nullptr;

	if (StructInfos.Contains(Filename))
	{
		FSaveStructInfo* StructInfo = StructInfos[Filename].Get();

		// Increase the reference count of SaveStruct by one, and then decrease it accordingly in UAutoSaveSubsystem::RemoveSaveStructRef
		StructInfo->RefConut++;

		return (FSaveStruct*)StructInfo->Data.GetData();
	}

	TUniquePtr<FSaveStructInfo> NewStructInfo(new FSaveStructInfo());

	if (FPaths::FileExists(Filename))
	{
		NewStructInfo->Filename = Filename;
		NewStructInfo->Struct = ScriptStruct;
		NewStructInfo->Data.SetNumUninitialized(ScriptStruct->GetStructureSize());
		NewStructInfo->State = ESaveStructState::Preload;
		NewStructInfo->RefConut = 1;
		NewStructInfo->LastRefConut = 0;
		NewStructInfo->LastSaveTime = FDateTime::Now();
	}
	else
	{
		// Check if the target is writable
		if (!FFileHelper::SaveStringToFile(TEXT(""), *Filename))
			return nullptr;

		NewStructInfo->Filename = Filename;
		NewStructInfo->Struct = ScriptStruct;
		NewStructInfo->Data.SetNumUninitialized(ScriptStruct->GetStructureSize());
		ScriptStruct->InitializeStruct(NewStructInfo->Data.GetData());
		NewStructInfo->State = ESaveStructState::Idle;
		NewStructInfo->RefConut = 1;
		NewStructInfo->LastRefConut = 0;
		NewStructInfo->LastSaveTime = FDateTime::Now();
	}

	ScriptStructHooker.Add(Filename, ScriptStruct);

	StructInfos.Add(Filename, nullptr);
	StructInfos[Filename].Reset(NewStructInfo.Release());

	return (FSaveStruct*)StructInfos[Filename]->Data.GetData();
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
		DataCopy.SetNumUninitialized(StructInfoPtr->Data.Num());
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

void UAutoSaveSubsystem::Initialize(FSubsystemCollectionBase & Collection)
{
	if (MaxThreadNum > 0)
		TaskThreads.SetNum(MaxThreadNum);
}

void UAutoSaveSubsystem::Deinitialize()
{
	for (TUniquePtr<FAsyncTask<FStructLoadOrSaveTask>>& Task : TaskThreads)
	{
		if (!Task) continue;
		Task->EnsureCompletion();
		Task = nullptr;
	}
}

void UAutoSaveSubsystem::Tick(float DeltaTime)
{
	HandleTaskDone();
	HandleTaskStart();
}
