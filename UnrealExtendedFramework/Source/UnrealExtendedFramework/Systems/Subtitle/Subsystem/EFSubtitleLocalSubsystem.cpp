// EFSubtitleLocalSubsystem.cpp
#include "EFSubtitleLocalSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleProjectSettings.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleStyleProfile.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Policy/EFSubtitleAudioPlayer.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Policy/EFSubtitleQueuePolicy.h"
#include "UnrealExtendedFramework/Systems/Subtitle/UserSettings/EFSubtitleSettingsHelpers.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Widget/EFSubtitleDisplayWidget.h"

void UEFSubtitleLocalSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SeedPresentationStateFromProjectSettings();
	EnsurePoliciesReady();

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			QueueTickHandle,
			this,
			&UEFSubtitleLocalSubsystem::TickQueue,
			1.0f / 30.0f,
			true);
	}
}

void UEFSubtitleLocalSubsystem::Deinitialize()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(QueueTickHandle);
	}

	if (QueuePolicy)
	{
		QueuePolicy->ClearAll();
	}

	if (DisplayWidget)
	{
		DisplayWidget->RemoveFromParent();
		DisplayWidget = nullptr;
	}

	QueuePolicy = nullptr;
	AudioPlayer = nullptr;

	Super::Deinitialize();
}

void UEFSubtitleLocalSubsystem::SeedPresentationStateFromProjectSettings()
{
	PresentationState = FEFSubtitlePresentationState();

	const UEFSubtitleProjectSettings* Settings = GetDefault<UEFSubtitleProjectSettings>();
	if (Settings)
	{
		PresentationState.BackgroundOpacity = Settings->BackgroundSettings.BackgroundOpacity;
	}
}

void UEFSubtitleLocalSubsystem::EnsurePoliciesReady()
{
	const UEFSubtitleProjectSettings* Settings = GetDefault<UEFSubtitleProjectSettings>();

	if (!QueuePolicy)
	{
		UClass* QueueClass = (Settings && Settings->QueuePolicyClass)
			? Settings->QueuePolicyClass.Get()
			: UEFSubtitleQueuePolicy_Default::StaticClass();

		QueuePolicy = NewObject<UEFSubtitleQueuePolicy>(this, QueueClass);
		if (QueuePolicy)
		{
			const EEFSubtitleQueueMode Mode = Settings ? Settings->DefaultQueueMode : EEFSubtitleQueueMode::Replace;
			const int32 MaxStacked = Settings ? Settings->MaxStackedSubtitles : 3;
			QueuePolicy->Configure(Mode, MaxStacked);
			QueuePolicy->OnActiveChanged.BindUObject(this, &UEFSubtitleLocalSubsystem::OnActiveSubtitleChanged);
			QueuePolicy->OnExpired.BindUObject(this, &UEFSubtitleLocalSubsystem::OnSubtitleExpired);
		}
	}

	if (!AudioPlayer)
	{
		UClass* AudioClass = (Settings && Settings->AudioPlayerClass)
			? Settings->AudioPlayerClass.Get()
			: UEFSubtitleAudioPlayer_Default::StaticClass();

		AudioPlayer = NewObject<UEFSubtitleAudioPlayer>(this, AudioClass);
	}
}

void UEFSubtitleLocalSubsystem::ReceiveSubtitle(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request)
{
	if (!AreSubtitlesEnabled())
	{
		return;
	}

	EnsurePoliciesReady();
	EnsureWidgetReady();

	if (QueuePolicy)
	{
		QueuePolicy->Enqueue(Entry, Request);
	}
}

void UEFSubtitleLocalSubsystem::CancelSubtitle(int32 RequestId)
{
	EnsurePoliciesReady();
	if (QueuePolicy)
	{
		QueuePolicy->Cancel(RequestId);
		if (DisplayWidget && QueuePolicy->IsEmpty())
		{
			DisplayWidget->HideSubtitle();
		}
	}
}

void UEFSubtitleLocalSubsystem::ClearAllSubtitles()
{
	EnsurePoliciesReady();
	if (QueuePolicy)
	{
		QueuePolicy->ClearAll();
	}

	if (DisplayWidget)
	{
		DisplayWidget->HideSubtitle();
	}
}

void UEFSubtitleLocalSubsystem::ApplyPresentationState(const FEFSubtitlePresentationState& NewState)
{
	const bool bWasEnabled = PresentationState.bEnabled;
	PresentationState = NewState;

	if (!PresentationState.bEnabled && bWasEnabled)
	{
		ClearAllSubtitles();
	}

	RefreshWidgetPresentation();
}

void UEFSubtitleLocalSubsystem::SetSubtitlesEnabled(bool bEnabled)
{
	if (PresentationState.bEnabled == bEnabled)
	{
		return;
	}

	FEFSubtitlePresentationState NewState = PresentationState;
	NewState.bEnabled = bEnabled;
	ApplyPresentationState(NewState);
}

void UEFSubtitleLocalSubsystem::SetSubtitleTextScale(float Scale)
{
	FEFSubtitlePresentationState NewState = PresentationState;
	NewState.TextScale = FMath::Clamp(Scale, 0.5f, 3.0f);
	ApplyPresentationState(NewState);
}

void UEFSubtitleLocalSubsystem::SetSubtitleBackgroundOpacity(float Opacity)
{
	FEFSubtitlePresentationState NewState = PresentationState;
	NewState.BackgroundOpacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
	ApplyPresentationState(NewState);
}

void UEFSubtitleLocalSubsystem::SetShowSpeakerLabels(bool bShow)
{
	FEFSubtitlePresentationState NewState = PresentationState;
	NewState.bShowSpeakerLabels = bShow;
	ApplyPresentationState(NewState);
}

void UEFSubtitleLocalSubsystem::SetClosedCaptionsEnabled(bool bEnabled)
{
	FEFSubtitlePresentationState NewState = PresentationState;
	NewState.bClosedCaptions = bEnabled;
	ApplyPresentationState(NewState);
}

void UEFSubtitleLocalSubsystem::SetStyleProfileId(FName ProfileId)
{
	FEFSubtitlePresentationState NewState = PresentationState;
	NewState.StyleProfileId = ProfileId;
	ApplyPresentationState(NewState);
}

bool UEFSubtitleLocalSubsystem::HasActiveSubtitle() const
{
	return QueuePolicy && QueuePolicy->GetActive().IsValid();
}

UEFSubtitleStyleProfile* UEFSubtitleLocalSubsystem::ResolveActiveStyleProfile() const
{
	return EFSubtitleSettingsHelpers::ResolveStyleProfile(PresentationState.StyleProfileId);
}

void UEFSubtitleLocalSubsystem::RefreshWidgetPresentation()
{
	if (!DisplayWidget)
	{
		return;
	}

	DisplayWidget->ApplyPresentationState(PresentationState, ResolveActiveStyleProfile());
}

void UEFSubtitleLocalSubsystem::EnsureWidgetReady()
{
	if (DisplayWidget && DisplayWidget->IsInViewport())
	{
		RefreshWidgetPresentation();
		return;
	}

	const UEFSubtitleProjectSettings* Settings = GetDefault<UEFSubtitleProjectSettings>();
	if (!Settings || !Settings->SubtitleWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[EFSubtitleLocalSubsystem] No SubtitleWidgetClass set in project settings!"));
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	APlayerController* PC = LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
	if (!PC)
	{
		return;
	}

	if (!DisplayWidget)
	{
		DisplayWidget = CreateWidget<UEFSubtitleDisplayWidget>(PC, Settings->SubtitleWidgetClass);
	}

	if (DisplayWidget && !DisplayWidget->IsInViewport())
	{
		DisplayWidget->AddToViewport(100);
	}

	RefreshWidgetPresentation();
}

void UEFSubtitleLocalSubsystem::OnActiveSubtitleChanged(const FEFActiveSubtitle& Active)
{
	if (!AreSubtitlesEnabled())
	{
		return;
	}

	EnsureWidgetReady();

	if (DisplayWidget && QueuePolicy)
	{
		if (QueuePolicy->GetQueueMode() == EEFSubtitleQueueMode::Stack)
		{
			DisplayWidget->AddStackedSubtitle(Active.Entry, Active.RequestId);
		}
		else
		{
			DisplayWidget->ShowSubtitle(Active.Entry, Active.Request);
		}
	}

	if (AudioPlayer)
	{
		AudioPlayer->PlaySubtitleAudio(GetWorld(), Active.Entry, Active.Request);
	}

	OnSubtitleStarted.Broadcast(Active.Entry, Active.RequestId);
}

void UEFSubtitleLocalSubsystem::OnSubtitleExpired(int32 RequestId)
{
	if (DisplayWidget && QueuePolicy)
	{
		if (QueuePolicy->GetQueueMode() == EEFSubtitleQueueMode::Stack)
		{
			DisplayWidget->RemoveStackedSubtitle(RequestId);
		}
		else if (QueuePolicy->IsEmpty())
		{
			DisplayWidget->HideSubtitle();
		}
	}

	OnSubtitleFinished.Broadcast(RequestId);
}

void UEFSubtitleLocalSubsystem::TickQueue()
{
	if (!QueuePolicy)
	{
		return;
	}

	const float DeltaTime = 1.0f / 30.0f;
	QueuePolicy->Tick(DeltaTime);

	if (DisplayWidget && QueuePolicy->GetActive().IsValid())
	{
		const FEFActiveSubtitle& Active = QueuePolicy->GetActive();
		const float TotalTime = Active.ElapsedTime + Active.RemainingTime;
		const float Progress = (TotalTime > 0.0f) ? (Active.ElapsedTime / TotalTime) : 1.0f;
		DisplayWidget->UpdateSubtitle(DeltaTime, Progress);
	}
}
