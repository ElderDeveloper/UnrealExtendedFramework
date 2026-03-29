// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSVoiceChatComponent.h"
#include "EEOSVoiceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UnrealExtendedEOS.h"
#include "EngineUtils.h"

UEEOSVoiceChatComponent::UEEOSVoiceChatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
	bVoiceAutoActivate = true;
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void UEEOSVoiceChatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bVoiceAutoActivate)
	{
		ActivateVoice();
	}
}

void UEEOSVoiceChatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeactivateVoice();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UEEOSVoiceChatComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// Could trigger viewport visualization refresh here
}
#endif

// ── Actions ──────────────────────────────────────────────────────────────────

void UEEOSVoiceChatComponent::ActivateVoice()
{
	if (bIsActive) return;
	if (RoomName.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("VoiceChatComponent: Cannot activate — RoomName is empty"));
		return;
	}

	UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem();
	if (!VoiceSub) return;

	// Join the room
	VoiceSub->JoinVoiceRoom(RoomName);
	bIsActive = true;

	// Configure transmit based on role
	if (CanSend() && !OwnerUserId.IsEmpty())
	{
		// Source or Transceiver: this voice point transmits
		VoiceSub->TransmitToSelectedRoom(RoomName);
	}

	// Start proximity timer if attenuation has distance falloff
	if (CanReceive() && HasDistanceFalloff())
	{
		StartProximityTimer();
	}

	const TCHAR* RoleName = Role == EEOSVoiceRole::Transceiver ? TEXT("Transceiver") :
							Role == EEOSVoiceRole::Source ? TEXT("Source") : TEXT("Listener");
	UE_LOG(LogExtendedEOS, Log, TEXT("VoiceChatComponent: Activated [%s] on room '%s'"), RoleName, *RoomName);
}

void UEEOSVoiceChatComponent::DeactivateVoice()
{
	if (!bIsActive) return;

	StopProximityTimer();

	UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem();
	if (VoiceSub && !RoomName.IsEmpty())
	{
		VoiceSub->LeaveVoiceRoom(RoomName);
	}

	bIsActive = false;
	UE_LOG(LogExtendedEOS, Log, TEXT("VoiceChatComponent: Deactivated room '%s'"), *RoomName);
}

void UEEOSVoiceChatComponent::SetRoom(const FString& NewRoomName)
{
	if (NewRoomName == RoomName) return;

	bool bWasActive = bIsActive;

	if (bIsActive)
	{
		DeactivateVoice();
	}

	RoomName = NewRoomName;

	if (bWasActive)
	{
		ActivateVoice();
	}
}

void UEEOSVoiceChatComponent::SetRole(EEOSVoiceRole NewRole)
{
	if (NewRole == Role) return;

	bool bWasActive = bIsActive;

	if (bIsActive)
	{
		DeactivateVoice();
	}

	Role = NewRole;

	if (bWasActive)
	{
		ActivateVoice();
	}
}

void UEEOSVoiceChatComponent::SetAttenuationSettings(USoundAttenuation* NewSettings)
{
	AttenuationSettings = NewSettings;

	if (bIsActive && CanReceive())
	{
		StopProximityTimer();

		if (HasDistanceFalloff())
		{
			StartProximityTimer();
		}
		else
		{
			// No falloff → reset all volumes to full
			UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem();
			if (VoiceSub)
			{
				for (UEEOSVoiceChatComponent* Other : GetComponentsInSameRoom())
				{
					if (Other->CanSend() && !Other->OwnerUserId.IsEmpty())
					{
						VoiceSub->SetPlayerVolume(Other->OwnerUserId, 1.0f);
					}
				}
			}
		}
	}
}

// ── Queries ──────────────────────────────────────────────────────────────────

bool UEEOSVoiceChatComponent::HasDistanceFalloff() const
{
	if (!AttenuationSettings) return false;

	const FSoundAttenuationSettings& Attenuation = AttenuationSettings->Attenuation;
	return Attenuation.bAttenuate && Attenuation.FalloffDistance > 0.f;
}

TArray<UEEOSVoiceChatComponent*> UEEOSVoiceChatComponent::GetComponentsInSameRoom() const
{
	TArray<UEEOSVoiceChatComponent*> Results;

	UWorld* World = GetWorld();
	if (!World) return Results;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UEEOSVoiceChatComponent*> Components;
		(*It)->GetComponents<UEEOSVoiceChatComponent>(Components);

		for (UEEOSVoiceChatComponent* Comp : Components)
		{
			if (Comp != this && Comp->RoomName == RoomName && Comp->IsVoiceActive())
			{
				Results.Add(Comp);
			}
		}
	}

	return Results;
}

float UEEOSVoiceChatComponent::GetVolumeAtDistance(float Distance) const
{
	return CalculateVolumeAtDistance(Distance);
}

// ── Internal ─────────────────────────────────────────────────────────────────

void UEEOSVoiceChatComponent::StartProximityTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			ProximityTimerHandle,
			this,
			&UEEOSVoiceChatComponent::UpdateProximityVolumes,
			UpdateInterval,
			true
		);
	}
}

void UEEOSVoiceChatComponent::StopProximityTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ProximityTimerHandle);
	}
}

void UEEOSVoiceChatComponent::UpdateProximityVolumes()
{
	if (!CanReceive()) return;

	UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem();
	if (!VoiceSub) return;

	const FVector MyLocation = GetComponentLocation();

	UWorld* World = GetWorld();
	if (!World) return;

	// Find all Source/Transceiver components in the same room
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UEEOSVoiceChatComponent*> Components;
		(*It)->GetComponents<UEEOSVoiceChatComponent>(Components);

		for (UEEOSVoiceChatComponent* OtherComp : Components)
		{
			if (OtherComp == this) continue;
			if (!OtherComp->IsVoiceActive()) continue;
			if (OtherComp->RoomName != RoomName) continue;
			if (!OtherComp->CanSend()) continue;
			if (OtherComp->OwnerUserId.IsEmpty()) continue;

			float Distance = FVector::Dist(MyLocation, OtherComp->GetComponentLocation());
			float Volume = CalculateVolumeAtDistance(Distance);

			VoiceSub->SetPlayerVolume(OtherComp->OwnerUserId, Volume);
		}
	}
}

float UEEOSVoiceChatComponent::CalculateVolumeAtDistance(float Distance) const
{
	if (!AttenuationSettings)
	{
		return 1.0f; // No settings = full volume (2D)
	}

	const FSoundAttenuationSettings& Attenuation = AttenuationSettings->Attenuation;

	if (!Attenuation.bAttenuate)
	{
		return 1.0f; // Attenuation disabled = 2D
	}

	const float FalloffDistance = Attenuation.FalloffDistance;
	const float InnerRadius = Attenuation.AttenuationShapeExtents.X;

	if (Distance <= InnerRadius)
	{
		return 1.0f;
	}

	if (FalloffDistance <= 0.f)
	{
		return 0.0f;
	}

	float OuterRadius = InnerRadius + FalloffDistance;
	if (Distance >= OuterRadius)
	{
		return 0.0f;
	}

	float NormalizedDistance = (Distance - InnerRadius) / FalloffDistance;
	NormalizedDistance = FMath::Clamp(NormalizedDistance, 0.f, 1.f);

	switch (Attenuation.DistanceAlgorithm)
	{
		case EAttenuationDistanceModel::Linear:
			return 1.0f - NormalizedDistance;

		case EAttenuationDistanceModel::Logarithmic:
			return FMath::Clamp(0.5f * -FMath::Loge(NormalizedDistance), 0.f, 1.f);

		case EAttenuationDistanceModel::Inverse:
			return FMath::Clamp(0.02f / (NormalizedDistance + 0.02f), 0.f, 1.f);

		case EAttenuationDistanceModel::LogReverse:
			return FMath::Clamp(1.0f - 0.5f * FMath::Loge(1.0f - NormalizedDistance * (1.0f - KINDA_SMALL_NUMBER)), 0.f, 1.f);

		case EAttenuationDistanceModel::NaturalSound:
		{
			float dBAttenuation = Attenuation.dBAttenuationAtMax * NormalizedDistance;
			return FMath::Pow(10.f, dBAttenuation / 20.f);
		}

		case EAttenuationDistanceModel::Custom:
			if (Attenuation.CustomAttenuationCurve.GetRichCurveConst())
			{
				return FMath::Clamp(Attenuation.CustomAttenuationCurve.GetRichCurveConst()->Eval(NormalizedDistance), 0.f, 1.f);
			}
			return 1.0f - NormalizedDistance;

		default:
			return 1.0f - NormalizedDistance;
	}
}

UEEOSVoiceSubsystem* UEEOSVoiceChatComponent::GetVoiceSubsystem() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return nullptr;

	return GI->GetSubsystem<UEEOSVoiceSubsystem>();
}
