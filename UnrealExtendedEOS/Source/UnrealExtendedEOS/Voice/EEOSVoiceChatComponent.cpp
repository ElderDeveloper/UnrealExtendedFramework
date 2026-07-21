// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSVoiceChatComponent.h"
#include "EEOSVoiceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UnrealExtendedEOS.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

/** Resolve the local player's listen position: the possessed pawn if any, else the camera.
 *  Player 0 — the same local user the voice subsystem resolves everywhere. */
static bool UEEOSVoiceChatComponent_GetLocalListenerLocation(UWorld* World, FVector& OutLocation)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return false;
	}
	if (const APawn* LocalPawn = PC->GetPawn())
	{
		OutLocation = LocalPawn->GetActorLocation();
		return true;
	}
	if (PC->PlayerCameraManager)
	{
		OutLocation = PC->PlayerCameraManager->GetCameraLocation();
		return true;
	}
	return false;
}

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
	if (bRegistered) return;
	if (RoomName.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("VoiceChatComponent: Cannot activate — RoomName is empty"));
		return;
	}

	UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem();
	if (!VoiceSub) return;

	// Register interest in the room (refcounted; transmit rooms join the composed transmit
	// set). The subsystem owns transmit composition — this component must never call
	// TransmitToSelectedRoom directly, which would clobber other components' transmission.
	RegisteredRoomName = RoomName;
	bRegisteredTransmit = CanSend();
	bRegistered = true;

	// Track the room lifecycle: rooms are lobby-managed, so the channel may come up (or go
	// away) at any time relative to this component's activation.
	VoiceSub->OnVoiceRoomJoined.AddUniqueDynamic(this, &UEEOSVoiceChatComponent::HandleVoiceRoomJoined);
	VoiceSub->OnVoiceRoomLeft.AddUniqueDynamic(this, &UEEOSVoiceChatComponent::HandleVoiceRoomLeft);

	const bool bRoomLive = VoiceSub->RegisterVoiceRoomUser(RegisteredRoomName, bRegisteredTransmit);
	if (bRoomLive)
	{
		// Synchronous confirmation — the lobby already joined this channel.
		ConfirmActive();
	}
	else
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("VoiceChatComponent: Registered on room '%s' — waiting for the lobby RTC channel to come up"), *RegisteredRoomName);
	}
}

void UEEOSVoiceChatComponent::DeactivateVoice()
{
	if (!bRegistered) return;

	StopProximityTimer();

	UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem();
	if (VoiceSub)
	{
		VoiceSub->OnVoiceRoomJoined.RemoveDynamic(this, &UEEOSVoiceChatComponent::HandleVoiceRoomJoined);
		VoiceSub->OnVoiceRoomLeft.RemoveDynamic(this, &UEEOSVoiceChatComponent::HandleVoiceRoomLeft);
		VoiceSub->ClearPlayerVolumeContributions(this);
		// Refcounted: the last component out only removes the room from transmit composition —
		// channel membership belongs to the lobby.
		VoiceSub->UnregisterVoiceRoomUser(RegisteredRoomName, bRegisteredTransmit);
	}

	ContributedPlayers.Empty();
	bRegistered = false;
	bIsActive = false;
	UE_LOG(LogExtendedEOS, Log, TEXT("VoiceChatComponent: Deactivated room '%s'"), *RegisteredRoomName);
	RegisteredRoomName.Reset();
	bRegisteredTransmit = false;
}

void UEEOSVoiceChatComponent::SetRoom(const FString& NewRoomName)
{
	if (NewRoomName == RoomName) return;

	const bool bWasRegistered = bRegistered;

	if (bRegistered)
	{
		DeactivateVoice();
	}

	RoomName = NewRoomName;

	if (bWasRegistered)
	{
		ActivateVoice();
	}
}

void UEEOSVoiceChatComponent::SetRole(EEOSVoiceRole NewRole)
{
	if (NewRole == Role) return;

	const bool bWasRegistered = bRegistered;

	if (bRegistered)
	{
		DeactivateVoice();
	}

	Role = NewRole;

	if (bWasRegistered)
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
			// No falloff → withdraw this component's volume contributions; the subsystem
			// re-aggregates (and restores 1.0 where no other component contributes).
			if (UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem())
			{
				VoiceSub->ClearPlayerVolumeContributions(this);
			}
			ContributedPlayers.Empty();
		}
	}
}

// ── Room confirmation ────────────────────────────────────────────────────────

void UEEOSVoiceChatComponent::HandleVoiceRoomJoined(const FString& InRoomName)
{
	if (bRegistered && !bIsActive && InRoomName == RegisteredRoomName)
	{
		ConfirmActive();
	}
}

void UEEOSVoiceChatComponent::HandleVoiceRoomLeft(const FString& InRoomName)
{
	if (bRegistered && bIsActive && InRoomName == RegisteredRoomName)
	{
		// Room went away (lobby left / connection lost). Stay registered so a rejoin of the
		// same room re-confirms automatically, but stop acting on it.
		bIsActive = false;
		StopProximityTimer();
		if (UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem())
		{
			VoiceSub->ClearPlayerVolumeContributions(this);
		}
		ContributedPlayers.Empty();
		UE_LOG(LogExtendedEOS, Log, TEXT("VoiceChatComponent: Room '%s' ended — component inactive until the room returns"), *RegisteredRoomName);
	}
}

void UEEOSVoiceChatComponent::ConfirmActive()
{
	bIsActive = true;

	// Start proximity updates if attenuation has distance falloff
	if (CanReceive() && HasDistanceFalloff())
	{
		StartProximityTimer();
	}

	const TCHAR* RoleName = Role == EEOSVoiceRole::Transceiver ? TEXT("Transceiver") :
							Role == EEOSVoiceRole::Source ? TEXT("Source") : TEXT("Listener");
	UE_LOG(LogExtendedEOS, Log, TEXT("VoiceChatComponent: Active [%s] on room '%s'"), RoleName, *RegisteredRoomName);
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
	if (!CanReceive() || !bIsActive) return;

	UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem();
	if (!VoiceSub) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// ── LOCALITY GATE (see the class comment) ────────────────────────────────
	// Contributions set the LOCAL machine's per-player receive volumes, so every contribution
	// must be computed against the LOCAL player's perspective. A pawn-mounted receiver only
	// qualifies when the pawn is locally controlled: its component location IS the local
	// player's ears. A replicated proxy of a REMOTE player's pawn describes what THAT player
	// hears at THEIR position — it must contribute nothing here.
	//
	// The failure this prevents: remote players B and C stand next to each other, both far
	// from me. Without the gate, B's replicated transceiver runs this listener logic on MY
	// machine, computes volume(B↔C) ≈ 1.0 for C, and the subsystem's max-wins aggregation
	// makes me hear C at full volume. With the gate, only MY pawn's component contributes
	// for C — ≈ 0.0 at that distance — and C is correctly inaudible.
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && !OwnerPawn->IsLocallyControlled())
	{
		// Withdraw anything contributed before a possession change demoted this component.
		if (ContributedPlayers.Num() > 0)
		{
			VoiceSub->ClearPlayerVolumeContributions(this);
			ContributedPlayers.Empty();
		}
		return;
	}

	// World-fixture receivers (speakers/radios — non-pawn owners) are placed identically on
	// every machine, so the fixture itself carries no locality. The local perspective is
	// injected into the math instead: attenuate by the LOCAL listener's distance to this
	// fixture ("audio comes from here" — walk away from the speaker and it fades), the same
	// value for every source in the room. The source's own distance to the fixture is
	// deliberately not part of the math (a PA replays the room feed at the speaker; gating
	// who feeds the room is the Source components' / gameplay's job via OwnerUserId).
	const bool bIsWorldFixture = (OwnerPawn == nullptr);
	float FixtureVolume = 1.0f;
	if (bIsWorldFixture)
	{
		FVector ListenerLocation;
		if (!UEEOSVoiceChatComponent_GetLocalListenerLocation(World, ListenerLocation))
		{
			return; // no local player/camera yet — nobody to hear anything
		}
		FixtureVolume = CalculateVolumeAtDistance(FVector::Dist(GetComponentLocation(), ListenerLocation));
	}

	const FVector MyLocation = GetComponentLocation();

	// Find all Source/Transceiver components in the same room and report a per-player volume
	// CONTRIBUTION. The subsystem applies the max across all components, because the VoiceChat
	// API only has a global per-player volume (see the class comment for the limitation).
	TSet<FString> FoundPlayers;
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

			// Locally-controlled pawn: source position ↔ my ears (this component). World
			// fixture: local listener ↔ this fixture, computed once above. Both distances
			// terminate at the LOCAL player's viewpoint — never at a remote player's.
			const float Volume = bIsWorldFixture
				? FixtureVolume
				: CalculateVolumeAtDistance(FVector::Dist(MyLocation, OtherComp->GetComponentLocation()));

			VoiceSub->SetPlayerVolumeContribution(this, OtherComp->OwnerUserId, Volume);
			FoundPlayers.Add(OtherComp->OwnerUserId);
		}
	}

	// Withdraw contributions for players no longer present (component destroyed / left room)
	// so a stale value cannot pin their aggregate volume.
	for (const FString& Player : ContributedPlayers)
	{
		if (!FoundPlayers.Contains(Player))
		{
			VoiceSub->ClearPlayerVolumeContribution(this, Player);
		}
	}
	ContributedPlayers = MoveTemp(FoundPlayers);
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
