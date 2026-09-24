// Copyright Moon Punch Games. All Rights Reserved.

#include "EFLogLibrary.h"

#include "EFLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EFLogLibrary)

void UEFLogLibrary::WriteEFLog(const UObject* WorldContextObject, FName Category, const FString& Message, EEFLogVerbosity Verbosity)
{
#if EF_LOG_ENABLED
	// An unset Category pin would otherwise become a category literally called "None".
	FEFLogCategory& LogCategory = EFLog::Private::FindOrAddCategory(Category.IsNone() ? FName(TEXT("Blueprint")) : Category);
	if (!LogCategory.IsEnabled(Verbosity))
	{
		return;
	}

	// A Blueprint has no __FILE__; its class path is what the Extended Log window can open instead.
	FString Source = TEXT("Blueprint");
	const UWorld* World = nullptr;
	if (WorldContextObject)
	{
		Source = WorldContextObject->GetClass()->GetPathName();
		World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	}

	EFLog::Private::SubmitFromWorld(LogCategory, Verbosity, World, MoveTemp(Source), FString(Message));
#endif
}
