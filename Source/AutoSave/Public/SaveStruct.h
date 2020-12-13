#pragma once

#include "CoreMinimal.h"
#include "AutoSaveSubsystem.h"
#include "Templates/UnrealTemplate.h"

struct FSaveStructInfo;

class UAutoSaveSubsystem;

template<typename SaveStructType>
class FSaveStructPtr : public FNoncopyable
{
public:

	FORCEINLINE FSaveStructPtr()
		: AutoSaveSubsystem(nullptr)
		, Info(nullptr)
	{
	}

	FORCEINLINE FSaveStructPtr(UAutoSaveSubsystem* InAutoSaveSubsystem, const FString& Filename, FSaveStructLoadDelegate OnLoaded = FSaveStructLoadDelegate())
		: AutoSaveSubsystem(InAutoSaveSubsystem)
		, Info(nullptr)
	{
		if (AutoSaveSubsystem->AddSaveStructRef(Filename, SaveStructType::StaticStruct(), OnLoaded))
		{
			Info = AutoSaveSubsystem->StructInfos[Filename].Get();
		}
	}

	FORCEINLINE ~FSaveStructPtr()
	{
		if (Info)
		{
			AutoSaveSubsystem->RemoveSaveStructRef(Info->Filename);
		}
	}

	FORCEINLINE SaveStructType* Get() const
	{
		return Info ? (SaveStructType*)Info->Data.GetData() : nullptr;
	}

	FORCEINLINE explicit operator bool() const
	{
		return Info != nullptr;
	}

	FORCEINLINE const bool IsValid() const
	{
		return Info != nullptr;
	}

	FORCEINLINE const bool IsLoaded() const
	{
		check(IsValid());
		return Info->State == ESaveStructState::Idle || Info->State == ESaveStructState::Saving;
	}

	FORCEINLINE SaveStructType& operator*() const
	{
		check(IsValid());
		return *Get();
	}

	FORCEINLINE SaveStructType* operator->() const
	{
		check(IsValid());
		return Get();
	}

private:

	UAutoSaveSubsystem* AutoSaveSubsystem;

	FSaveStructInfo* Info;

};
