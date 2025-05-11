// Fill out your copyright notice in the Description page of Project Settings.


#include "EFLoadingProcessTask.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UnrealExtendedFramework/Settings/Loading/EFLoadingScreenManager.h"
#include "UObject/ScriptInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EFLoadingProcessTask)

/*UEFLoadingProcessTask* UEFLoadingProcessTask::CreateLoadingScreenProcessTask(UObject* WorldContextObject, const FString& ShowLoadingScreenReason)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UEFLoadingScreenManager* LoadingScreenManager = GameInstance ? GameInstance->GetSubsystem<UEFLoadingScreenManager>() : nullptr;

	if (LoadingScreenManager)
	{
		UEFLoadingProcessTask* NewLoadingTask = NewObject<UEFLoadingProcessTask>(LoadingScreenManager);
		NewLoadingTask->SetShowLoadingScreenReason(ShowLoadingScreenReason);

		LoadingScreenManager->RegisterLoadingProcessor(NewLoadingTask);
		
		return NewLoadingTask;
	}

	return nullptr;
}

void UEFLoadingProcessTask::Unregister()
{
	UEFLoadingScreenManager* LoadingScreenManager = Cast<UEFLoadingScreenManager>(GetOuter());
	LoadingScreenManager->UnregisterLoadingProcessor(this);
}

void UEFLoadingProcessTask::SetShowLoadingScreenReason(const FString& InReason)
{
	Reason = InReason;
}

bool UEFLoadingProcessTask::ShouldShowLoadingScreen(FString& OutReason) const
{
	OutReason = Reason;
	return true;
}*/