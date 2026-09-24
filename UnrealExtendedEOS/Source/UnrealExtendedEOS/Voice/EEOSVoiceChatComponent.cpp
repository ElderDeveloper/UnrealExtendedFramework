// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSVoiceChatComponent.h"
#include "EEOSVoiceSubsystem.h"
#include "EEOSVoiceCaptureState.h"
#include "AudioCapture.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UnrealExtendedEOS.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "Sound/SoundGroups.h"
#include "Sound/SoundWaveProcedural.h"

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
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
	bWantsInitializeComponent = true;
	bVoiceAutoActivate = true;
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void UEEOSVoiceChatComponent::BeginPlay()
{
	Super::BeginPlay();
	CaptureState = MakeShared<FEEOSVoiceCaptureState, ESPMode::ThreadSafe>();
	if (FSlateApplication::IsInitialized())
	{
		AppActiveHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddWeakLambda(this,
			[this](bool bActive) { bAppFocused = bActive; if (!bActive) CancelPushToTalk(); RefreshCaptureState(); });
	}

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &UEEOSVoiceChatComponent::HandleOwnerControllerChanged);
	}

	// Room events are needed before activation: a Current Lobby component learns its room from them.
	BindRoomEvents();
	EnsureInputMappingRegistered();

	if (bVoiceAutoActivate)
	{
		ActivateVoice();
	}
	else if (bFollowLobbyRoom)
	{
		FollowLobbyRoom();
	}
	UpdateLocalCapture();
}

void UEEOSVoiceChatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bWantsVoice = false;
	CancelPushToTalk();
	StopReplay();
	StopLocalCapture();
	bPreviousToggleMuteActionHeld = false;
	bPreviousPushToTalkActionHeld = false;
	if (AppActiveHandle.IsValid() && FSlateApplication::IsInitialized())
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(AppActiveHandle);
	Unregister();
	UnbindRoomEvents();

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UEEOSVoiceChatComponent::HandleOwnerControllerChanged);
	}

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UEEOSVoiceChatComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// Editing the volume on a running component (the PIE details panel) applies it straight away.
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UEEOSVoiceChatComponent, MicrophoneVolume))
	{
		ApplyMicrophoneVolume();
	}
	else if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UEEOSVoiceChatComponent, bLocalCaptureTest))
	{
		bLocalCaptureFailed = false;
		UpdateLocalCapture();
	}
}
#endif

// ── Actions ──────────────────────────────────────────────────────────────────

void UEEOSVoiceChatComponent::ActivateVoice()
{
	bWantsVoice = true;
	BindRoomEvents();
	if (bFollowLobbyRoom) FollowLobbyRoom();
	Register();
	EnsureInputMappingRegistered();
	UpdateLocalCapture();
}

void UEEOSVoiceChatComponent::DeactivateVoice()
{
	bWantsVoice = false;
	CancelPushToTalk();
	StopReplay();
	StopLocalCapture();
	Unregister();
}

void UEEOSVoiceChatComponent::SetRoom(const FString& NewRoomName)
{
	if (NewRoomName == RoomName) return;

	MoveToRoom(NewRoomName);
}

void UEEOSVoiceChatComponent::SetRoomSource(const EEOSVoiceRoomSource NewSource)
{
	RoomSource = NewSource;
	SetFollowLobbyRoom(NewSource == EEOSVoiceRoomSource::Lobby);
}

void UEEOSVoiceChatComponent::SetRole(EEOSVoiceRole NewRole)
{
	if (NewRole == Role) return;

	const bool bWasRegistered = bRegistered;

	if (bRegistered)
	{
		Unregister();
	}

	Role = NewRole;

	if (bWasRegistered)
	{
		Register();
	}
	UpdateLocalCapture();
}

void UEEOSVoiceChatComponent::Register()
{
	UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem();
	if (!bWantsVoice || !VoiceSub) return;
	BindRoomEvents();
	for (const FString& Room : GetRoomBindings())
	{
		if (Room.IsEmpty() || RegisteredRooms.Contains(Room)) continue;
		const bool bTransmit = WantsTransmit();
		RegisteredRooms.Add(Room, bTransmit);
		VoiceSub->RegisterVoiceRoomUser(Room, bTransmit);
	}
	bRegistered = !RegisteredRooms.IsEmpty();
	RefreshRoomActivity();
	RefreshCaptureState();
}

void UEEOSVoiceChatComponent::Unregister()
{
	StopProximityTimer();
	if (UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem())
	{
		VoiceSub->ClearPlayerVolumeContributions(this);
		VoiceSub->UnregisterCaptureState(this);
		for (const auto& Room : RegisteredRooms) VoiceSub->UnregisterVoiceRoomUser(Room.Key, Room.Value);
	}
	bCaptureRegistered = false;
	RegisteredRooms.Empty();
	ContributedPlayers.Empty();
	bRegistered = false;
	bIsActive = false;
}

void UEEOSVoiceChatComponent::MoveToRoom(const FString& NewRoomName)
{
	const FString Previous = RoomName;
	RoomName = NewRoomName;
	RemoveRoomBinding(Previous);
	AddRoomBinding(NewRoomName);
}

void UEEOSVoiceChatComponent::FollowLobbyRoom()
{
	const UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem();
	const FString NewRoom = bFollowLobbyRoom && VoiceSub ? VoiceSub->GetLobbyVoiceRoomName() : FString();
	if (FollowedLobbyRoom == NewRoom) return;
	const FString Previous = FollowedLobbyRoom;
	FollowedLobbyRoom = NewRoom;
	if (!RoomBindings.Contains(Previous))
	{
		if (const bool* Transmit = RegisteredRooms.Find(Previous))
		{
			if (UEEOSVoiceSubsystem* Voice = GetVoiceSubsystem()) Voice->UnregisterVoiceRoomUser(Previous, *Transmit);
			RegisteredRooms.Remove(Previous);
		}
	}
	Register();
}

void UEEOSVoiceChatComponent::BindRoomEvents()
{
	if (UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem())
	{
		VoiceSub->OnVoiceRoomJoined.AddUniqueDynamic(this, &UEEOSVoiceChatComponent::HandleVoiceRoomJoined);
		VoiceSub->OnVoiceRoomLeft.AddUniqueDynamic(this, &UEEOSVoiceChatComponent::HandleVoiceRoomLeft);
		VoiceSub->OnVoiceRoomJoinFailed.AddUniqueDynamic(this, &UEEOSVoiceChatComponent::HandleRoomJoinFailed);
		VoiceSub->OnVoiceRoomLeaveFailed.AddUniqueDynamic(this, &UEEOSVoiceChatComponent::HandleRoomLeaveFailed);
	}
}

void UEEOSVoiceChatComponent::UnbindRoomEvents()
{
	if (UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem())
	{
		VoiceSub->OnVoiceRoomJoined.RemoveDynamic(this, &UEEOSVoiceChatComponent::HandleVoiceRoomJoined);
		VoiceSub->OnVoiceRoomLeft.RemoveDynamic(this, &UEEOSVoiceChatComponent::HandleVoiceRoomLeft);
		VoiceSub->OnVoiceRoomJoinFailed.RemoveDynamic(this, &UEEOSVoiceChatComponent::HandleRoomJoinFailed);
		VoiceSub->OnVoiceRoomLeaveFailed.RemoveDynamic(this, &UEEOSVoiceChatComponent::HandleRoomLeaveFailed);
	}
}

bool UEEOSVoiceChatComponent::WantsTransmit() const
{
	return CanSend() && bWantsVoice && GetLocalVoiceController() && bAppFocused && !bInputSuppressed
		&& !bMicrophoneMuted && (bTransmitAllowed || bMicrophoneTest)
		&& (bMicrophoneTest || InputMode == EEOSVoiceInputMode::VoiceActivation || bPushToTalkHeld);
}

void UEEOSVoiceChatComponent::SetTransmitAllowed(const bool bAllowed)
{
	if (bTransmitAllowed == bAllowed)
	{
		return;
	}

	bTransmitAllowed = bAllowed;
	// Losing permission closes the key gate, so a toggle cannot reopen by itself when it returns.
	if (!bAllowed) CancelPushToTalk();
	RefreshTransmitRegistration();
}

void UEEOSVoiceChatComponent::RefreshTransmitRegistration()
{
	if (UEEOSVoiceSubsystem* VoiceSub = GetVoiceSubsystem())
	{
		const bool bTransmit = WantsTransmit();
		for (auto& Room : RegisteredRooms)
		{
			if (Room.Value == bTransmit) continue;
			// New state in before the old one out: the other order drops the room's count to zero
			// in between, which takes the room out of the transmit set (an extra UpdateSending to EOS)
			// and logs it as abandoned on every push-to-talk edge.
			VoiceSub->RegisterVoiceRoomUser(Room.Key, bTransmit);
			VoiceSub->UnregisterVoiceRoomUser(Room.Key, Room.Value);
			Room.Value = bTransmit;
		}
	}
	RefreshCaptureState();
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
	if (bFollowLobbyRoom) FollowLobbyRoom();
	const bool bRequested = PendingRoomJoins.Remove(InRoomName) > 0;
	RefreshRoomActivity();
	if ((bRequested || RegisteredRooms.Contains(InRoomName)) && !AnnouncedRooms.Contains(InRoomName))
	{
		AnnouncedRooms.Add(InRoomName);
		OnRoomJoined.Broadcast(InRoomName);
	}
}

void UEEOSVoiceChatComponent::HandleVoiceRoomLeft(const FString& InRoomName)
{
	const bool bRequested = PendingRoomLeaves.Remove(InRoomName) > 0;
	const bool bAnnounced = AnnouncedRooms.Remove(InRoomName) > 0;
	if (bRequested) RemoveRoomBinding(InRoomName);
	RefreshRoomActivity();
	if (bRequested || bAnnounced) OnRoomLeft.Broadcast(InRoomName);
}

void UEEOSVoiceChatComponent::HandleOwnerControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	CancelPushToTalk();
	bPreviousToggleMuteActionHeld = false;
	// A key still down from the previous controller must come up before it counts as a press.
	bPreviousPushToTalkActionHeld = true;
	EnsureInputMappingRegistered();
	RefreshTransmitRegistration();
	UpdateProximityVolumes();
	ApplyMicrophoneVolume();
	UpdateLocalCapture();
}

void UEEOSVoiceChatComponent::ConfirmActive()
{
	bIsActive = true;
	if (CanReceive() && HasDistanceFalloff()) StartProximityTimer();
	ApplyMicrophoneVolume();
}

// ── Queries ──────────────────────────────────────────────────────────────────

bool UEEOSVoiceChatComponent::HasDistanceFalloff() const
{
	if (!AttenuationSettings) return false;

	const FSoundAttenuationSettings& Attenuation = AttenuationSettings->Attenuation;
	return Attenuation.bAttenuate && Attenuation.FalloffDistance > 0.f;
}

APlayerState* UEEOSVoiceChatComponent::GetOwningPlayerState() const
{
	for (AActor* Actor = GetOwner(); Actor; Actor = Actor->GetOwner())
	{
		if (APlayerState* PlayerState = Cast<APlayerState>(Actor))
		{
			return PlayerState;
		}
		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			return Pawn->GetPlayerState();
		}
		if (const AController* Controller = Cast<AController>(Actor))
		{
			return Controller->PlayerState;
		}
	}

	return nullptr;
}

FString UEEOSVoiceChatComponent::GetOwnerUserId() const
{
	const APlayerState* PlayerState = GetOwningPlayerState();
	if (!PlayerState)
	{
		return FString();
	}

	const FUniqueNetIdPtr NetId = PlayerState->GetUniqueId().GetUniqueNetId();
	if (!NetId.IsValid())
	{
		return FString();
	}

	// Non-EOS ids (the null subsystem in PIE, a Steam net driver) are not voice participants.
	const FString ProductUserId = UEEOSBlueprintLibrary::ExtractProductUserId(NetId->ToString());
	return UEEOSBlueprintLibrary::IsValidProductUserId(ProductUserId) ? ProductUserId : FString();
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
			if (Comp != this && SharesActiveRoom(Comp) && Comp->IsVoiceActive())
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
	// who feeds the room is the Source components' / gameplay's job via their owning player).
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
			if (!SharesActiveRoom(OtherComp)) continue;
			if (!OtherComp->CanSend()) continue;

			const FString OtherUserId = OtherComp->GetOwnerUserId();
			if (OtherUserId.IsEmpty()) continue;

			// Locally-controlled pawn: source position ↔ my ears (this component). World
			// fixture: local listener ↔ this fixture, computed once above. Both distances
			// terminate at the LOCAL player's viewpoint — never at a remote player's.
			const float Volume = bIsWorldFixture
				? FixtureVolume
				: CalculateVolumeAtDistance(FVector::Dist(MyLocation, OtherComp->GetComponentLocation()));

			VoiceSub->SetPlayerVolumeContribution(this, OtherUserId, Volume);
			FoundPlayers.Add(OtherUserId);
		}
	}

	// Room members with no voice point on this machine (not net-relevant, not spawned yet) have no
	// distance. Without a contribution they would fall back to full volume, so a listener with
	// falloff reports them explicitly: a pawn listener cannot place them and hears nothing; a world
	// fixture replays the whole room feed at its own location, so they play at the fixture's volume.
	// Without falloff this component is 2D and leaves them alone.
	if (HasDistanceFalloff())
	{
		const float UnplacedVolume = bIsWorldFixture ? FixtureVolume : 0.0f;
		const FString LocalPlayerName = VoiceSub->GetLocalVoicePlayerName();
		for (const FString& Room : GetJoinedRooms())
		{
			for (const FString& Member : VoiceSub->GetPlayersInRoom(Room))
			{
				if (Member.IsEmpty() || Member == LocalPlayerName || FoundPlayers.Contains(Member))
				{
					continue;
				}

				VoiceSub->SetPlayerVolumeContribution(this, Member, UnplacedVolume);
				FoundPlayers.Add(Member);
			}
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

void UEEOSVoiceChatComponent::PostLoad()
{
	Super::PostLoad();
	if (!bLegacyRoomSettingsMigrated)
	{
		if (RoomSource == EEOSVoiceRoomSource::Named)
		{
			bFollowLobbyRoom = false;
			if (!RoomName.IsEmpty()) RoomBindings.Add(RoomName);
		}
		bLegacyRoomSettingsMigrated = true;
	}
}

TArray<FString> UEEOSVoiceChatComponent::GetRoomBindings() const
{
	TSet<FString> Rooms = RoomBindings;
	if (bFollowLobbyRoom && !FollowedLobbyRoom.IsEmpty()) Rooms.Add(FollowedLobbyRoom);
	Rooms.Remove(FString());
	return Rooms.Array();
}

TArray<FString> UEEOSVoiceChatComponent::GetJoinedRooms() const
{
	TArray<FString> Rooms;
	const UEEOSVoiceSubsystem* Voice = GetVoiceSubsystem();
	if (Voice)
		for (const FString& Room : GetRoomBindings())
			if (Voice->IsInRoom(Room)) Rooms.Add(Room);
	return Rooms;
}

void UEEOSVoiceChatComponent::AddRoomBinding(const FString& InRoomName)
{
	if (InRoomName.IsEmpty()) return;
	RoomBindings.Add(InRoomName);
	Register();
}

void UEEOSVoiceChatComponent::RemoveRoomBinding(const FString& InRoomName)
{
	RoomBindings.Remove(InRoomName);
	if (bFollowLobbyRoom && FollowedLobbyRoom == InRoomName) return;
	if (const bool* Transmit = RegisteredRooms.Find(InRoomName))
	{
		if (UEEOSVoiceSubsystem* Voice = GetVoiceSubsystem())
			Voice->UnregisterVoiceRoomUser(InRoomName, *Transmit);
		RegisteredRooms.Remove(InRoomName);
	}
	bRegistered = !RegisteredRooms.IsEmpty();
	RefreshRoomActivity();
	RefreshCaptureState();
}

void UEEOSVoiceChatComponent::SetFollowLobbyRoom(bool bFollow)
{
	bFollowLobbyRoom = bFollow;
	FollowLobbyRoom();
}

bool UEEOSVoiceChatComponent::JoinRoom(const FString& InRoomName, const FString& ChannelCredentials)
{
	UEEOSVoiceSubsystem* Voice = GetVoiceSubsystem();
	if (!Voice || InRoomName.IsEmpty() || (CanSend() && !GetLocalVoiceController()))
	{
		OnRoomJoinFailed.Broadcast(InRoomName, TEXT("A local voice user and non-empty room name are required."));
		return false;
	}
	BindRoomEvents();
	if (PendingRoomJoins.Contains(InRoomName)) return true;
	PendingRoomJoins.Add(InRoomName);
	const bool bHadBinding = RoomBindings.Contains(InRoomName);
	AddRoomBinding(InRoomName);
	if (!Voice->JoinVoiceRoom(InRoomName, ChannelCredentials))
	{
		PendingRoomJoins.Remove(InRoomName);
		if (!bHadBinding) RemoveRoomBinding(InRoomName);
		return false;
	}
	return true;
}

bool UEEOSVoiceChatComponent::LeaveRoom(const FString& InRoomName)
{
	UEEOSVoiceSubsystem* Voice = GetVoiceSubsystem();
	if (!Voice || (CanSend() && !GetLocalVoiceController()))
	{
		OnRoomLeaveFailed.Broadcast(InRoomName, TEXT("A local voice user is required."));
		return false;
	}
	BindRoomEvents();
	PendingRoomLeaves.Add(InRoomName);
	if (!Voice->LeaveVoiceRoom(InRoomName))
	{
		PendingRoomLeaves.Remove(InRoomName);
		return false;
	}
	return true;
}

void UEEOSVoiceChatComponent::HandleRoomJoinFailed(const FString& InRoomName, const FString& Error)
{
	if (PendingRoomJoins.Remove(InRoomName)) OnRoomJoinFailed.Broadcast(InRoomName, Error);
}

void UEEOSVoiceChatComponent::HandleRoomLeaveFailed(const FString& InRoomName, const FString& Error)
{
	if (PendingRoomLeaves.Remove(InRoomName)) OnRoomLeaveFailed.Broadcast(InRoomName, Error);
}

void UEEOSVoiceChatComponent::RefreshRoomActivity()
{
	const bool bWasActive = bIsActive;
	bIsActive = bWantsVoice && !GetJoinedRooms().IsEmpty();
	if (bIsActive && !bWasActive) ConfirmActive();
	if (!bIsActive)
	{
		StopProximityTimer();
		if (UEEOSVoiceSubsystem* Voice = GetVoiceSubsystem()) Voice->ClearPlayerVolumeContributions(this);
		ContributedPlayers.Empty();
	}
	else if (CanReceive()) UpdateProximityVolumes();
}

bool UEEOSVoiceChatComponent::SharesActiveRoom(const UEEOSVoiceChatComponent* Other) const
{
	if (!Other) return false;
	const TArray<FString> OtherRooms = Other->GetJoinedRooms();
	for (const FString& Room : GetJoinedRooms())
		if (OtherRooms.Contains(Room)) return true;
	return false;
}

APlayerController* UEEOSVoiceChatComponent::GetLocalVoiceController() const
{
	for (AActor* Actor = GetOwner(); Actor; Actor = Actor->GetOwner())
	{
		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
			return PC && PC->IsLocalController() ? PC : nullptr;
		}
		if (APlayerController* PC = Cast<APlayerController>(Actor))
			return PC->IsLocalController() ? PC : nullptr;
		if (const APlayerState* PS = Cast<APlayerState>(Actor))
		{
			APlayerController* PC = PS->GetPlayerController();
			return PC && PC->IsLocalController() ? PC : nullptr;
		}
	}
	return nullptr;
}

void UEEOSVoiceChatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bFollowLobbyRoom) FollowLobbyRoom();
	EnsureInputMappingRegistered();
	APlayerController* PC = GetLocalVoiceController();
	const UEnhancedPlayerInput* PlayerInput = PC ? Cast<UEnhancedPlayerInput>(PC->PlayerInput) : nullptr;
	const bool bReadEnhancedInput = bAutoBindInput && bWantsVoice && CanSend() && PlayerInput;
	const bool bActionHeld = bReadEnhancedInput && InputMode != EEOSVoiceInputMode::VoiceActivation
		&& PushToTalkAction && PlayerInput->GetActionValue(PushToTalkAction).Get<bool>();
	if (bAutoBindInput)
	{
		if (InputMode == EEOSVoiceInputMode::PushToToggle)
		{
			// Presses only: holding the key keeps nothing open and letting go closes nothing.
			if (bActionHeld && !bPreviousPushToTalkActionHeld) PushToTalkPressed();
		}
		else if (bActionHeld && !bPushToTalkHeld)
		{
			PushToTalkPressed();
		}
		else if (!bActionHeld && bPushToTalkHeld)
		{
			PushToTalkReleased();
		}
	}
	else if (bPushToTalkHeld && (!PC || !bAppFocused || bInputSuppressed))
	{
		CancelPushToTalk();
	}
	bPreviousPushToTalkActionHeld = bActionHeld;
	const bool bToggleMuteActionHeld = bReadEnhancedInput && ToggleMuteAction
		&& PlayerInput->GetActionValue(ToggleMuteAction).Get<bool>();
	if (bToggleMuteActionHeld && !bPreviousToggleMuteActionHeld && bAppFocused && !bInputSuppressed)
		ToggleMicrophoneMute();
	bPreviousToggleMuteActionHeld = bToggleMuteActionHeld;
	RefreshTransmitRegistration();
}

void UEEOSVoiceChatComponent::SetInputMode(EEOSVoiceInputMode NewMode)
{
	if (InputMode == NewMode) return;
	InputMode = NewMode;
	CancelPushToTalk();
	if (CaptureState)
	{
		FScopeLock Lock(&CaptureState->Mutex);
		CaptureState->HoldUntil = 0.0;
		CaptureState->bSpeech = false;
	}
	RefreshTransmitRegistration();
}

void UEEOSVoiceChatComponent::SetDetectionSettings(float Threshold, float ReleaseSeconds)
{
	DetectionThreshold = FMath::Clamp(Threshold, 0.001f, 0.5f);
	DetectionReleaseSeconds = FMath::Clamp(ReleaseSeconds, 0.05f, 2.0f);
	RefreshCaptureState();
}

void UEEOSVoiceChatComponent::SetMicrophoneVolume(const float Volume)
{
	MicrophoneVolume = FMath::Clamp(Volume, 0.0f, 2.0f);
	ApplyMicrophoneVolume();
}

void UEEOSVoiceChatComponent::ApplyMicrophoneVolume()
{
	// The volume belongs to the voice user, not to a room, so only the local player's own live voice
	// point sets it. A live room also means the voice user is logged in and its login defaults are done.
	if (!bIsActive || !CanSend() || !GetLocalVoiceController()) return;
	if (UEEOSVoiceSubsystem* Voice = GetVoiceSubsystem()) Voice->SetInputVolume(MicrophoneVolume);
}

void UEEOSVoiceChatComponent::SetMicrophoneMuted(bool bMuted)
{
	if (bMicrophoneMuted == bMuted) return;
	bMicrophoneMuted = bMuted;
	if (bMuted) CancelPushToTalk();
	RefreshTransmitRegistration();
	OnMicrophoneMuteChanged.Broadcast(bMuted);
}

void UEEOSVoiceChatComponent::ToggleMicrophoneMute()
{
	SetMicrophoneMuted(!bMicrophoneMuted);
}

void UEEOSVoiceChatComponent::SetMicrophoneTest(bool bEnabled)
{
	if (bMicrophoneTest == bEnabled) return;
	bMicrophoneTest = bEnabled;
	CancelPushToTalk();
	if (CaptureState)
	{
		FScopeLock Lock(&CaptureState->Mutex);
		CaptureState->HoldUntil = 0.0;
		CaptureState->bSpeech = false;
	}
	RefreshTransmitRegistration();
}

void UEEOSVoiceChatComponent::PushToTalkPressed()
{
	// The second press of a toggle closes it, whatever else has changed since it opened.
	if (InputMode == EEOSVoiceInputMode::PushToToggle && bPushToTalkHeld)
	{
		CancelPushToTalk();
		return;
	}
	// The local capture test opens the key without transmit permission: it sends nothing, and
	// WantsTransmit still asks for permission before anything reaches a room.
	if (InputMode == EEOSVoiceInputMode::VoiceActivation || !bWantsVoice || (!bTransmitAllowed && !LocalCapture)
		|| bMicrophoneMuted || bInputSuppressed || !bAppFocused || !GetLocalVoiceController()) return;
	// A new take cuts the previous replay, or speakers would feed it back into the microphone.
	StopReplay();
	bPushToTalkHeld = true;
	RefreshTransmitRegistration();
}

void UEEOSVoiceChatComponent::PushToTalkReleased()
{
	// A toggle is closed by its next press, never by letting go of the key.
	if (InputMode == EEOSVoiceInputMode::PushToToggle) return;
	CancelPushToTalk();
}

void UEEOSVoiceChatComponent::CancelPushToTalk()
{
	const bool bWasHeld = bPushToTalkHeld;
	bPushToTalkHeld = false;
	// Closes the key in the capture state first, so the audio thread has stopped adding to the take.
	RefreshTransmitRegistration();
	if (bWasHeld)
	{
		ReplayLastTake();
	}
}

void UEEOSVoiceChatComponent::SetLocalCaptureTest(const bool bEnabled)
{
	if (bLocalCaptureTest == bEnabled) return;
	bLocalCaptureTest = bEnabled;
	bLocalCaptureFailed = false;
	UpdateLocalCapture();
}

bool UEEOSVoiceChatComponent::IsLocalCaptureRunning() const
{
	return LocalCapture != nullptr;
}

void UEEOSVoiceChatComponent::UpdateLocalCapture()
{
	// Same rule as for EOS: only the local player's own voice point captures.
	if (!bLocalCaptureTest || !bWantsVoice || !CanSend() || !GetLocalVoiceController())
	{
		StopLocalCapture();
		return;
	}
	if (LocalCapture || bLocalCaptureFailed) return;

	if (!CaptureState) CaptureState = MakeShared<FEEOSVoiceCaptureState, ESPMode::ThreadSafe>();
	UAudioCapture* Capture = NewObject<UAudioCapture>(this);
	if (!Capture->OpenDefaultAudioStream())
	{
		bLocalCaptureFailed = true;
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceChatComponent: Local capture test could not open the default microphone (no capture device, or no capture backend loaded)."));
		return;
	}

	const int32 SampleRate = Capture->GetSampleRate();
	const int32 Channels = Capture->GetNumChannels();
	// Runs on the capture thread, so it holds the shared state and never this component.
	const TSharedPtr<FEEOSVoiceCaptureState, ESPMode::ThreadSafe> State = CaptureState;
	const FAudioGeneratorHandle Handle = Capture->AddGeneratorDelegate(
		[State, SampleRate, Channels](const float* Audio, int32 NumSamples)
		{
			if (!Audio || NumSamples <= 0) return;
			TArray<int16> Pcm;
			Pcm.SetNumUninitialized(NumSamples);
			double Sum = 0.0;
			for (int32 Index = 0; Index < NumSamples; ++Index)
			{
				const float Sample = FMath::Clamp(Audio[Index], -1.0f, 1.0f);
				Sum += static_cast<double>(Sample) * Sample;
				Pcm[Index] = static_cast<int16>(Sample * 32767.0f);
			}
			// The empty room is the local source: measured and recorded like a room, never sent.
			State->Process(FString(), static_cast<float>(FMath::Sqrt(Sum / NumSamples)), FPlatformTime::Seconds());
			State->Record(FString(), Pcm, SampleRate, Channels);
		});
	LocalCaptureHandleId = Handle.Id;
	LocalCapture = Capture;
	Capture->StartCapturingAudio();
	RefreshCaptureState();
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceChatComponent: Local capture test started on the default microphone (%d Hz, %d channel(s)). Nothing from it is sent."),
		SampleRate, Channels);
}

void UEEOSVoiceChatComponent::StopLocalCapture()
{
	if (!LocalCapture) return;
	LocalCapture->StopCapturingAudio();
	FAudioGeneratorHandle Handle;
	Handle.Id = LocalCaptureHandleId;
	// Takes the generator's lock, so a buffer already being delivered finishes before this returns.
	LocalCapture->RemoveGeneratorDelegate(Handle);
	LocalCapture = nullptr;
	LocalCaptureHandleId = INDEX_NONE;
	RefreshCaptureState();
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceChatComponent: Local capture test stopped."));
}

void UEEOSVoiceChatComponent::SetReplayOnRelease(const bool bEnabled)
{
	if (bReplayOnRelease == bEnabled) return;
	bReplayOnRelease = bEnabled;
	if (!bEnabled) StopReplay();
	RefreshCaptureState();
}

bool UEEOSVoiceChatComponent::IsReplaying() const
{
	const UAudioComponent* Audio = ReplayAudio.Get();
	return Audio && Audio->IsPlaying();
}

void UEEOSVoiceChatComponent::ReplayLastTake()
{
	if (!CaptureState) return;

	// Always take it, so a take that is not played cannot leak into the next one.
	TArray<int16> Samples;
	int32 SampleRate = 0;
	int32 Channels = 0;
	CaptureState->TakeRecording(Samples, SampleRate, Channels);

	UWorld* World = GetWorld();
	if (!bReplayOnRelease || !bWantsVoice || !World) return;
	if (Samples.IsEmpty() || SampleRate <= 0 || Channels <= 0)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceChatComponent: Nothing to replay - no audio arrived while push-to-talk was held. EOS only delivers captured audio for a joined room it is sending to."));
		return;
	}

	StopReplay(); // A new take replaces one still playing.

	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(static_cast<uint32>(SampleRate));
	Wave->NumChannels = Channels;
	Wave->SoundGroup = SOUNDGROUP_Voice;
	Wave->bLooping = false;
	// Procedural waves have no length of their own (INDEFINITELY_LOOPING_DURATION, AudioMixerCore/AudioDefines.h),
	// and an underrun plays silence rather than finishing, so the timer below is what ends the replay.
	Wave->Duration = 10000.0f;
	Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * static_cast<int32>(sizeof(int16)));

	UAudioComponent* Audio = UGameplayStatics::CreateSound2D(this, Wave, 1.0f, 1.0f, 0.0f, nullptr, false, /*bAutoDestroy*/ true);
	if (!Audio)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceChatComponent: Push-to-talk replay skipped - no audio device"));
		return;
	}

	ReplayAudio = Audio;
	Audio->Play();

	const float Seconds = static_cast<float>(Samples.Num()) / static_cast<float>(SampleRate * Channels);
	World->GetTimerManager().SetTimer(ReplayStopHandle, this, &UEEOSVoiceChatComponent::StopReplay, Seconds + 0.2f, false);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceChatComponent: Replaying %.2f s of push-to-talk audio (%d Hz, %d channel(s))"), Seconds, SampleRate, Channels);
}

void UEEOSVoiceChatComponent::StopReplay()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReplayStopHandle);
	}
	if (UAudioComponent* Audio = ReplayAudio.Get())
	{
		Audio->Stop(); // Auto-destroys: created with bAutoDestroy.
	}
	ReplayAudio.Reset();
}

void UEEOSVoiceChatComponent::SetInputSuppressed(bool bSuppressed)
{
	if (bInputSuppressed == bSuppressed) return;
	bInputSuppressed = bSuppressed;
	if (bSuppressed) CancelPushToTalk();
	RefreshTransmitRegistration();
}

void UEEOSVoiceChatComponent::EnsureInputMappingRegistered()
{
	if (!bAutoBindInput || !PushToTalkContext) return;
	APlayerController* PC = GetLocalVoiceController();
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Sub = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Sub) return;
	int32 ExistingPriority = 0;
	if (!Sub->HasMappingContext(PushToTalkContext, ExistingPriority))
	{
		FModifyContextOptions Options;
		Options.bNotifyUserSettings = true;
		Sub->AddMappingContext(PushToTalkContext, 0, Options);
	}
	else if (UEnhancedInputUserSettings* Settings = Sub->GetUserSettings())
	{
		if (!Settings->IsMappingContextRegistered(PushToTalkContext))
			Settings->RegisterInputMappingContext(PushToTalkContext);
	}
}

void UEEOSVoiceChatComponent::RefreshCaptureState()
{
	UEEOSVoiceSubsystem* Voice = GetVoiceSubsystem();
	const bool bLocalSource = bWantsVoice && CanSend() && GetLocalVoiceController();
	if (!bLocalSource)
	{
		if (Voice && bCaptureRegistered) Voice->UnregisterCaptureState(this);
		bCaptureRegistered = false;
		return;
	}
	if (!CaptureState) CaptureState = MakeShared<FEEOSVoiceCaptureState, ESPMode::ThreadSafe>();
	{
		FScopeLock Lock(&CaptureState->Mutex);
		CaptureState->Rooms.Empty();
		for (const auto& Room : RegisteredRooms) CaptureState->Rooms.Add(Room.Key);
		CaptureState->bEnabled = bAppFocused && !bInputSuppressed && !bMicrophoneMuted && (bTransmitAllowed || bMicrophoneTest);
		CaptureState->bLocalCapture = LocalCapture != nullptr;
		CaptureState->bLocalEnabled = bAppFocused && !bInputSuppressed && !bMicrophoneMuted;
		CaptureState->bVoiceActivation = InputMode == EEOSVoiceInputMode::VoiceActivation;
		CaptureState->bPushToTalkHeld = bPushToTalkHeld;
		CaptureState->bTest = bMicrophoneTest;
		CaptureState->Threshold = DetectionThreshold;
		CaptureState->Release = DetectionReleaseSeconds;
		CaptureState->bRecordOnPushToTalk = bReplayOnRelease;
		CaptureState->MaxRecordSeconds = ReplayMaxSeconds;
		if (!CaptureState->bEnabled && !(CaptureState->bLocalCapture && CaptureState->bLocalEnabled))
		{
			CaptureState->bSpeech = false;
			CaptureState->HoldUntil = 0.0;
		}
	}
	if (Voice && !bCaptureRegistered)
	{
		Voice->RegisterCaptureState(this, CaptureState);
		bCaptureRegistered = true;
	}
}

bool UEEOSVoiceChatComponent::IsTalking() const
{
	if (!CaptureState) return false;
	// The local capture test reports speech on its own stream: no room or transmit permission needed.
	if (LocalCapture) return CaptureState->IsTalking();
	return bIsActive && WantsTransmit() && CaptureState->IsTalking();
}

float UEEOSVoiceChatComponent::GetCaptureLevel() const
{
	if (!CaptureState) return 0.0f;
	FScopeLock Lock(&CaptureState->Mutex);
	return CaptureState->Level;
}
