// EFSubtitleLocalSubsystem.cpp

#include "EFSubtitleLocalSubsystem.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Data/EFSubtitleProjectSettings.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Widget/EFSubtitleDisplayWidget.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"


void UEFSubtitleLocalSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Configure queue from project settings
	const auto* Settings = GetDefault<UEFSubtitleProjectSettings>();
	if (Settings)
	{
		SubtitleQueue.SetQueueMode(Settings->DefaultQueueMode);
		SubtitleQueue.SetMaxStacked(Settings->MaxStackedSubtitles);
	}

	// Bind queue callbacks
	SubtitleQueue.OnActiveChanged.BindUObject(this, &UEFSubtitleLocalSubsystem::OnActiveSubtitleChanged);
	SubtitleQueue.OnExpired.BindUObject(this, &UEFSubtitleLocalSubsystem::OnSubtitleExpired);

	// Start a repeating timer for ticking the queue (~30fps is sufficient for subtitles)
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			QueueTickHandle,
			this,
			&UEFSubtitleLocalSubsystem::TickQueue,
			1.0f / 30.0f,	// ~33ms
			true			// Looping
		);
	}
}


void UEFSubtitleLocalSubsystem::Deinitialize()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(QueueTickHandle);
	}

	SubtitleQueue.ClearAll();

	if (DisplayWidget)
	{
		DisplayWidget->RemoveFromParent();
		DisplayWidget = nullptr;
	}

	Super::Deinitialize();
}


void UEFSubtitleLocalSubsystem::ReceiveSubtitle(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request)
{
	if (!AreSubtitlesEnabled())
	{
		return;
	}

	EnsureWidgetReady();
	SubtitleQueue.Enqueue(Entry, Request);
}


void UEFSubtitleLocalSubsystem::CancelSubtitle(int32 RequestId)
{
	SubtitleQueue.Cancel(RequestId);

	if (DisplayWidget && SubtitleQueue.IsEmpty())
	{
		DisplayWidget->HideSubtitle();
	}
}


void UEFSubtitleLocalSubsystem::ClearAllSubtitles()
{
	SubtitleQueue.ClearAll();

	if (DisplayWidget)
	{
		DisplayWidget->HideSubtitle();
	}
}


bool UEFSubtitleLocalSubsystem::AreSubtitlesEnabled() const
{
	// Default: always enabled. When ModularSettings integration is connected,
	// this will query UEFSubtitleUserSettings.
	return true;
}


float UEFSubtitleLocalSubsystem::GetSubtitleTextScale() const
{
	// Default: 1.0. When ModularSettings integration is connected,
	// this will query UEFSubtitleUserSettings.
	return 1.0f;
}


bool UEFSubtitleLocalSubsystem::HasActiveSubtitle() const
{
	return SubtitleQueue.GetActive().IsValid();
}


void UEFSubtitleLocalSubsystem::EnsureWidgetReady()
{
	if (DisplayWidget && DisplayWidget->IsInViewport())
	{
		return;
	}

	const auto* Settings = GetDefault<UEFSubtitleProjectSettings>();
	if (!Settings || !Settings->SubtitleWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[EFSubtitleLocalSubsystem] No SubtitleWidgetClass set in project settings!"));
		return;
	}

	// Get the player controller for this local player
	APlayerController* PC = GetLocalPlayer()->GetPlayerController(GetWorld());
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
		DisplayWidget->AddToViewport(100); // High Z-order to render on top
		DisplayWidget->ApplyVisualSettings();
	}
}


void UEFSubtitleLocalSubsystem::OnActiveSubtitleChanged(const FEFActiveSubtitle& Active)
{
	EnsureWidgetReady();

	if (DisplayWidget)
	{
		if (SubtitleQueue.GetQueueMode() == EEFSubtitleQueueMode::Stack)
		{
			DisplayWidget->AddStackedSubtitle(Active.Entry, Active.RequestId);
		}
		else
		{
			DisplayWidget->ShowSubtitle(Active.Entry, Active.Request);
		}
	}

	// Play audio
	PlaySubtitleAudio(Active.Entry, Active.Request);

	// Fire delegate
	OnSubtitleStarted.Broadcast(Active.Entry, Active.RequestId);
}


void UEFSubtitleLocalSubsystem::OnSubtitleExpired(int32 RequestId)
{
	if (DisplayWidget)
	{
		if (SubtitleQueue.GetQueueMode() == EEFSubtitleQueueMode::Stack)
		{
			DisplayWidget->RemoveStackedSubtitle(RequestId);
		}
		else if (SubtitleQueue.IsEmpty())
		{
			DisplayWidget->HideSubtitle();
		}
	}

	// Fire delegate
	OnSubtitleFinished.Broadcast(RequestId);
}


void UEFSubtitleLocalSubsystem::TickQueue()
{
	const float DeltaTime = 1.0f / 30.0f;
	SubtitleQueue.Tick(DeltaTime);

	// Update progress on display widget
	if (DisplayWidget && SubtitleQueue.GetActive().IsValid())
	{
		const FEFActiveSubtitle& Active = SubtitleQueue.GetActive();
		const float TotalTime = Active.ElapsedTime + Active.RemainingTime;
		const float Progress = (TotalTime > 0.0f) ? (Active.ElapsedTime / TotalTime) : 1.0f;
		DisplayWidget->UpdateSubtitle(DeltaTime, Progress);
	}
}


void UEFSubtitleLocalSubsystem::PlaySubtitleAudio(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request)
{
	USoundBase* Sound = ResolveCultureSound(Entry);
	if (!Sound)
	{
		return;
	}

	switch (Request.ExecutionType)
	{
	case EEFSubtitleExecutionType::Boundless:
	case EEFSubtitleExecutionType::PlayerOnly:
		UGameplayStatics::PlaySound2D(GetWorld(), Sound);
		break;

	case EEFSubtitleExecutionType::Location:
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), Sound, Request.WorldLocation);
		break;

	case EEFSubtitleExecutionType::AttachedToActor:
		if (Request.AttachActor.IsValid())
		{
			UGameplayStatics::SpawnSoundAttached(Sound,
				Request.AttachActor->GetRootComponent());
		}
		else
		{
			UGameplayStatics::PlaySound2D(GetWorld(), Sound);
		}
		break;
	}
}


USoundBase* UEFSubtitleLocalSubsystem::ResolveCultureSound(const FEFSubtitleEntry& Entry) const
{
	if (Entry.CultureSounds.Num() > 0)
	{
		// Get current culture
		const FString CurrentCulture = FInternationalization::Get().GetCurrentCulture()->GetName();
		const FString CurrentLanguage = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName();

		// Try exact match first (e.g., "en-US")
		for (const FEFCultureSound& CS : Entry.CultureSounds)
		{
			if (CS.CultureCode == CurrentCulture)
			{
				if (USoundBase* Sound = CS.Sound.LoadSynchronous())
				{
					return Sound;
				}
			}
		}

		// Try language-only match (e.g., "en")
		for (const FEFCultureSound& CS : Entry.CultureSounds)
		{
			if (CS.CultureCode == CurrentLanguage)
			{
				if (USoundBase* Sound = CS.Sound.LoadSynchronous())
				{
					return Sound;
				}
			}
		}
	}

	// Fallback to default voice sound
	if (!Entry.VoiceSound.IsNull())
	{
		return Entry.VoiceSound.LoadSynchronous();
	}

	return nullptr;
}
