// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessPlayerComponent.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UnrealExtendedGameplay/Interaction/EGInteractionComponent.h"
#include "UnrealExtendedGameplay/InteractionSystem/EGInteractionSystemComponent.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EGChessCharacterInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "Engine/GameViewportClient.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessMoveGen.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessAIProfile.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessAnimationSet.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessInputConfig.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessPieceSet.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessTableStyle.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessUIConfig.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Presentation/EGChessEngineAssets.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Presentation/EGChessPieceVisuals.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessTableActor.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/UI/EGChessWidgetBase.h"

namespace EGChessPlayerComponentPrivate
{
	/** The pointer goes to the server at most this often while the hand is up (20 Hz)... */
	constexpr double HandPointerSendInterval = 0.05;
	/** ...and only once it has moved this far (cm), so a still mouse sends nothing. */
	constexpr float HandPointerSendDistance = 0.3f;
}

UEGChessPlayerComponent::UEGChessPlayerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UEGChessPlayerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UEGChessPlayerComponent, ActiveTable, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UEGChessPlayerComponent, ActiveSeat, COND_OwnerOnly);
}

UEGChessPlayerComponent* UEGChessPlayerComponent::Find(const AController* Controller)
{
	return Controller ? Controller->FindComponentByClass<UEGChessPlayerComponent>() : nullptr;
}

UEGChessPlayerComponent* UEGChessPlayerComponent::FindOrAdd(APlayerController* Controller, TSubclassOf<UEGChessPlayerComponent> Class)
{
	if (!Controller)
	{
		return nullptr;
	}
	if (UEGChessPlayerComponent* Existing = Find(Controller))
	{
		return Existing;
	}
	if (!Controller->HasAuthority())
	{
		return nullptr;
	}
	UClass* ComponentClass = Class ? Class.Get() : UEGChessPlayerComponent::StaticClass();
	UEGChessPlayerComponent* Component = NewObject<UEGChessPlayerComponent>(Controller, ComponentClass, TEXT("EGChessPlayer"));
	Component->SetIsReplicated(true);
	Component->RegisterComponent();
	Controller->AddInstanceComponent(Component);
	return Component;
}

APlayerController* UEGChessPlayerComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

bool UEGChessPlayerComponent::IsLocalPlayer() const
{
	const APlayerController* Controller = GetPlayerController();
	return Controller && Controller->IsLocalController();
}

void UEGChessPlayerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bInBoardMode)
	{
		ExitBoardMode();
	}
	Super::EndPlay(EndPlayReason);
}

// =============================================================================================
// Queries
// =============================================================================================

EEGChessColor UEGChessPlayerComponent::GetLocalColor() const
{
	return ActiveTable ? ActiveTable->GetSeatColor(ActiveSeat) : EEGChessColor::White;
}

bool UEGChessPlayerComponent::IsMyTurn() const
{
	return ActiveTable && ActiveTable->GetPhase() == EEGChessPhase::Playing && ActiveTable->GetSideToMove() == GetLocalColor();
}

bool UEGChessPlayerComponent::CanResign() const
{
	return ActiveTable && ActiveTable->GetPhase() == EEGChessPhase::Playing;
}

bool UEGChessPlayerComponent::CanOfferDraw() const
{
	if (!ActiveTable || ActiveTable->GetPhase() != EEGChessPhase::Playing)
	{
		return false;
	}
	const FEGChessDrawOfferState Offer = ActiveTable->GetDrawOffer();
	return !Offer.bPending && Offer.GetCooldown(GetLocalColor()) == 0;
}

bool UEGChessPlayerComponent::CanTakeback() const
{
	if (!ActiveTable || ActiveTable->GetPhase() != EEGChessPhase::Playing)
	{
		return false;
	}
	const UEGChessSeatComponent* Other = ActiveTable->GetSeat(EGChess::OtherSeat(ActiveSeat));
	const int32 Plies = ActiveTable->GetMoveHistoryRef().Num();
	if (!Other->IsAI() || Plies == 0 || (IsMyTurn() && Plies < 2))
	{
		return false;
	}
	const UEGChessAIProfile* Profile = ActiveTable->GetMatchRulesRef().AIProfile;
	return !Profile || Profile->MaxTakebacks < 0 || ActiveTable->GetTakebacksUsed() < Profile->MaxTakebacks;
}

bool UEGChessPlayerComponent::CanToggleView() const
{
	return ActiveTable && ActiveTable->GetMatchRulesRef().bAllowViewToggle && !UsesFixedCamera();
}

bool UEGChessPlayerComponent::CanPlayVersusAI() const
{
	return ActiveTable
		&& ActiveTable->GetPhase() == EEGChessPhase::WaitingForOpponent
		&& ActiveTable->GetMatchRulesRef().OpponentPolicy == EEGChessOpponentPolicy::PlayersOrAI
		&& !ActiveTable->GetSeat(EGChess::OtherSeat(ActiveSeat))->IsOccupied();
}

bool UEGChessPlayerComponent::HasIncomingDrawOffer() const
{
	if (!ActiveTable || ActiveTable->GetPhase() != EEGChessPhase::Playing)
	{
		return false;
	}
	const FEGChessDrawOfferState Offer = ActiveTable->GetDrawOffer();
	return Offer.bPending && Offer.OfferedBy != GetLocalColor();
}

// =============================================================================================
// Server
// =============================================================================================

void UEGChessPlayerComponent::SetActiveTable(AEGChessTableActor* Table, EEGChessSeat Seat)
{
	ActiveSeat = Seat;
	ActiveTable = Table;
	if (IsLocalPlayer())
	{
		OnRep_ActiveTable();
	}
}

void UEGChessPlayerComponent::ServerLeaveSeat_Implementation() { if (ActiveTable) { ActiveTable->HandleLeaveSeat(GetPlayerController()); } }
void UEGChessPlayerComponent::ServerSubmitMove_Implementation(const FEGChessMove& Move) { if (ActiveTable) { ActiveTable->HandleSubmitMove(GetPlayerController(), Move); } }
void UEGChessPlayerComponent::ServerSetHandIntent_Implementation(const FEGChessHandIntent& Intent) { if (ActiveTable) { ActiveTable->HandleSetHandIntent(GetPlayerController(), Intent); } }
void UEGChessPlayerComponent::ServerSetHandPointer_Implementation(FVector_NetQuantize10 Pointer) { if (ActiveTable) { ActiveTable->HandleSetHandPointer(GetPlayerController(), Pointer); } }
void UEGChessPlayerComponent::ServerResign_Implementation() { if (ActiveTable) { ActiveTable->HandleResign(GetPlayerController()); } }
void UEGChessPlayerComponent::ServerOfferDraw_Implementation() { if (ActiveTable) { ActiveTable->HandleOfferDraw(GetPlayerController()); } }
void UEGChessPlayerComponent::ServerRespondDraw_Implementation(bool bAccept) { if (ActiveTable) { ActiveTable->HandleRespondDraw(GetPlayerController(), bAccept); } }
void UEGChessPlayerComponent::ServerRequestTakeback_Implementation() { if (ActiveTable) { ActiveTable->HandleRequestTakeback(GetPlayerController()); } }
void UEGChessPlayerComponent::ServerRequestRematch_Implementation() { if (ActiveTable) { ActiveTable->HandleRequestRematch(GetPlayerController()); } }
void UEGChessPlayerComponent::ServerStartVersusAI_Implementation() { if (ActiveTable) { ActiveTable->HandleStartVersusAI(GetPlayerController()); } }

void UEGChessPlayerComponent::ClientNotice_Implementation(EEGChessNotice Notice)
{
	PostNotice(Notice);
}

void UEGChessPlayerComponent::ClientMoveRejected_Implementation()
{
	bAwaitingServer = false;
	FlashIllegal(SelectedSquare);
	ClearSelection(false);
}

void UEGChessPlayerComponent::OnRep_ActiveTable()
{
	if (bInBoardMode && BoardTable.Get() != ActiveTable)
	{
		ExitBoardMode();
	}
	if (ActiveTable && !bInBoardMode)
	{
		EnterBoardMode(ActiveTable);
	}
}

// =============================================================================================
// Board mode
// =============================================================================================

void UEGChessPlayerComponent::EnterBoardMode(AEGChessTableActor* Table)
{
	APlayerController* Controller = GetPlayerController();
	if (!Table || !Controller || !Controller->IsLocalController())
	{
		return;
	}
	BoardTable = Table;
	bInBoardMode = true;

	bPrevAutoManageCamera = Controller->bAutoManageActiveCameraTarget;
	Controller->bAutoManageActiveCameraTarget = false;
	Controller->SetIgnoreMoveInput(true);
	Controller->SetIgnoreLookInput(true);

	if (UsesFixedCamera())
	{
		// The fixed camera is the seat's board camera; the head-following view would move it.
		CurrentView = EEGChessView::Elevated;
	}
	else if (!bViewChosenThisSession)
	{
		CurrentView = Table->GetMatchRulesRef().DefaultView;
	}
	LookYaw = 0.0f;
	LookPitch = 0.0f;

	ApplyBoardInputMode(true);
	BindInput();
	SetWorldInteractionBlocked(true);
	SetCentreDotVisible(UsesViewCentreAim() && GetLookSettings()->bShowCentreDot);

	const UEGChessSeatComponent* Seat = Table->GetSeat(ActiveSeat);
	const float BlendTime = FMath::Clamp(Seat->GetEnterDuration(Seat->GetSeatedPawn(), Seat->GetAnimState().Side), 0.3f, 3.0f);
	SpawnBoardCamera(BlendTime);

	CursorSquare = Table->GetPosition().FindKing(GetLocalColor());
	SelectedSquare = INDEX_NONE;
	SelectedMoves.Reset();
	bAwaitingServer = false;
	SentFromSquare = INDEX_NONE;
	LocalHandIntent = FEGChessHandIntent();
	SentHandIntent = FEGChessHandIntent();
	LastHandPointerSendTime = -1.0;
	bLowTimeWarned = false;
	LastSeenPly = Table->GetMoveHistoryRef().Num();
	LastSeenPhase = Table->GetPhase();
	LastMousePosition = FVector2D(-1.0, -1.0);
	LastAimSquare = -2;

	Table->SetLocalDriver(this);
	const UEGChessTableStyle* Style = Table->GetTableStyle();
	Table->SetCoordinatesVisible(FeedbackOptions.bShowCoordinates || (Style && Style->bShowCoordinatesByDefault));

	if (const UEGChessUIConfig* UI = Table->GetUIConfig())
	{
		HUDWidget = CreateChessWidget(UI->HUDClass, EEGChessWidgetKind::HUD, false);
	}

	if (APawn* Pawn = Controller->GetPawn())
	{
		if (Pawn->Implements<UEGChessCharacterInterface>())
		{
			IEGChessCharacterInterface::Execute_OnChessViewChanged(Pawn, CurrentView == EEGChessView::Eye);
		}
	}

	SetComponentTickEnabled(true);
	RefreshWidgets();
	RefreshLocalPresentation();
}

void UEGChessPlayerComponent::ExitBoardMode()
{
	APlayerController* Controller = GetPlayerController();
	AEGChessTableActor* Table = BoardTable.Get();

	if (Table)
	{
		Table->PieceVisuals->SetSelection(INDEX_NONE);
		Table->PieceVisuals->SetOutlines(TArray<int32>(), 0);
		Table->PieceVisuals->HideGhost();
		if (Table->GetLocalDriver() == this)
		{
			Table->SetLocalDriver(nullptr);
		}
	}

	CloseAllWidgets();
	UnbindInput();
	ApplyBoardInputMode(false);
	SetWorldInteractionBlocked(false);
	SetCentreDotVisible(false);
	SetCameraFollowsPawnMesh(false);

	if (Controller)
	{
		if (APawn* Pawn = Controller->GetPawn())
		{
			Controller->SetViewTargetWithBlend(Pawn, 0.6f, VTBlend_EaseInOut, 2.0f);
			if (Pawn->Implements<UEGChessCharacterInterface>())
			{
				IEGChessCharacterInterface::Execute_OnChessViewChanged(Pawn, false);
			}
		}
		Controller->bAutoManageActiveCameraTarget = bPrevAutoManageCamera;
		Controller->SetIgnoreMoveInput(false);
		Controller->SetIgnoreLookInput(false);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(CameraDestroyTimer, FTimerDelegate::CreateUObject(this, &UEGChessPlayerComponent::DestroyBoardCamera), 0.7f, false);
		}
	}
	else
	{
		DestroyBoardCamera();
	}

	bInBoardMode = false;
	BoardTable.Reset();
	SelectedSquare = INDEX_NONE;
	SelectedMoves.Reset();
	CursorSquare = INDEX_NONE;
	PendingPromotionFrom = PendingPromotionTo = INDEX_NONE;
	CursorAxis = FVector2D::ZeroVector;
	bCarryActive = false;
	bSelectedByHeldPress = false;
	SentToSquare = INDEX_NONE;
	SentFromSquare = INDEX_NONE;
	LocalHandIntent = FEGChessHandIntent();
	if (!SentHandIntent.HasSameState(LocalHandIntent))
	{
		ServerSetHandIntent(LocalHandIntent);
	}
	SentHandIntent = LocalHandIntent;
	SetComponentTickEnabled(false);
}

// =============================================================================================
// Input
// =============================================================================================

void UEGChessPlayerComponent::BuildDefaultInput()
{
	const EEGChessAimMode AimMode = GetLookSettings()->AimMode;
	if (DefaultMappingContext && DefaultInputAimMode == AimMode)
	{
		return;
	}
	DefaultInputAimMode = AimMode;
	const bool bViewCentre = AimMode == EEGChessAimMode::ViewCentre;
	const bool bFixed = AimMode == EEGChessAimMode::FixedCursor;

	auto MakeAction = [this](EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this);
		Action->ValueType = Type;
		return Action;
	};
	DefaultCursorAction = MakeAction(EInputActionValueType::Axis2D);
	DefaultSelectAction = MakeAction(EInputActionValueType::Boolean);
	DefaultCancelAction = MakeAction(EInputActionValueType::Boolean);
	DefaultToggleViewAction = MakeAction(EInputActionValueType::Boolean);
	DefaultMenuAction = MakeAction(EInputActionValueType::Boolean);
	DefaultLeaveAction = MakeAction(EInputActionValueType::Boolean);
	DefaultLookAction = MakeAction(EInputActionValueType::Axis2D);
	DefaultLookHoldAction = MakeAction(EInputActionValueType::Boolean);
	DefaultCancelMouseAction = MakeAction(EInputActionValueType::Boolean);

	UInputMappingContext* Context = NewObject<UInputMappingContext>(this);
	DefaultMappingContext = Context;

	// One-dimensional keys become 2D: X is right, Y is away from the player.
	auto MapDirection = [this, Context](const FKey& Key, bool bVertical, bool bNegative)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(DefaultCursorAction, Key);
		if (bNegative)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		}
		if (bVertical)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Context);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
	};
	MapDirection(EKeys::W, true, false);
	MapDirection(EKeys::S, true, true);
	MapDirection(EKeys::D, false, false);
	MapDirection(EKeys::A, false, true);
	MapDirection(EKeys::Up, true, false);
	MapDirection(EKeys::Down, true, true);
	MapDirection(EKeys::Right, false, false);
	MapDirection(EKeys::Left, false, true);
	MapDirection(EKeys::Gamepad_DPad_Up, true, false);
	MapDirection(EKeys::Gamepad_DPad_Down, true, true);
	MapDirection(EKeys::Gamepad_DPad_Right, false, false);
	MapDirection(EKeys::Gamepad_DPad_Left, false, true);
	Context->MapKey(DefaultCursorAction, EKeys::Gamepad_Left2D);

	for (const FKey& Key : { EKeys::LeftMouseButton, EKeys::Enter, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom })
	{
		Context->MapKey(DefaultSelectAction, Key);
	}
	for (const FKey& Key : { EKeys::BackSpace, EKeys::Gamepad_FaceButton_Right })
	{
		Context->MapKey(DefaultCancelAction, Key);
	}

	const UEGChessInputConfig* Look = GetLookSettings();
	if (bViewCentre || bFixed)
	{
		// No look drag to share it with: right mouse is a plain cancel.
		Context->MapKey(DefaultCancelAction, EKeys::RightMouseButton);
	}
	else
	{
		// Right mouse does two jobs: a tap cancels, a hold turns mouse movement into look.
		FEnhancedActionKeyMapping& Tap = Context->MapKey(DefaultCancelMouseAction, EKeys::RightMouseButton);
		Tap.Triggers.Add(NewObject<UInputTriggerTap>(Context));
		Context->MapKey(DefaultLookHoldAction, EKeys::RightMouseButton);
	}
	// The fixed camera has no look at all, from the mouse or the stick.
	if (!bFixed)
	{
		FEnhancedActionKeyMapping& Mouse = Context->MapKey(DefaultLookAction, EKeys::Mouse2D);
		if (!bViewCentre)
		{
			UInputTriggerChordAction* Chord = NewObject<UInputTriggerChordAction>(Context);
			Chord->ChordAction = DefaultLookHoldAction;
			Mouse.Triggers.Add(Chord);
		}
		UInputModifierScalar* Scale = NewObject<UInputModifierScalar>(Context);
		Scale->Scalar = FVector(Look->MouseLookDegreesPerUnit, Look->MouseLookDegreesPerUnit, 1.0);
		Mouse.Modifiers.Add(Scale);
	}
	if (!bFixed)
	{
		FEnhancedActionKeyMapping& Stick = Context->MapKey(DefaultLookAction, EKeys::Gamepad_Right2D);
		Stick.Modifiers.Add(NewObject<UInputModifierDeadZone>(Context));
		Stick.Modifiers.Add(NewObject<UInputModifierScaleByDeltaTime>(Context));
		UInputModifierScalar* Scale = NewObject<UInputModifierScalar>(Context);
		Scale->Scalar = FVector(Look->GamepadLookDegreesPerSecond, Look->GamepadLookDegreesPerSecond, 1.0);
		Stick.Modifiers.Add(Scale);
	}
	for (const FKey& Key : { EKeys::V, EKeys::Gamepad_FaceButton_Top })
	{
		Context->MapKey(DefaultToggleViewAction, Key);
	}
	for (const FKey& Key : { EKeys::Escape, EKeys::Gamepad_Special_Right })
	{
		Context->MapKey(DefaultMenuAction, Key);
	}
	for (const FKey& Key : { EKeys::Q, EKeys::Gamepad_FaceButton_Left })
	{
		Context->MapKey(DefaultLeaveAction, Key);
	}
}

UInputAction* UEGChessPlayerComponent::ResolveAction(UInputAction* Configured, UInputAction* Default) const
{
	return Configured ? Configured : Default;
}

UInputMappingContext* UEGChessPlayerComponent::GetActiveMappingContext() const
{
	const AEGChessTableActor* Table = BoardTable.Get();
	const UEGChessInputConfig* Config = Table ? Table->GetInputConfig() : nullptr;
	return (Config && Config->MappingContext) ? Config->MappingContext.Get() : DefaultMappingContext.Get();
}

UInputAction* UEGChessPlayerComponent::GetControlAction(const EControl Control) const
{
	// The same choice BindInput makes, so a hint never names an action nothing is bound to.
	const AEGChessTableActor* Table = BoardTable.Get();
	const UEGChessInputConfig* Config = Table ? Table->GetInputConfig() : nullptr;
	const bool bUseConfig = Config && Config->MappingContext;
	auto Pick = [&](UInputAction* Configured, UInputAction* Default)
	{
		return bUseConfig ? Configured : ResolveAction(Configured, Default);
	};

	switch (Control)
	{
	case EControl::Select:      return Pick(Config ? Config->Select.Get() : nullptr, DefaultSelectAction);
	case EControl::Cancel:      return Pick(Config ? Config->Cancel.Get() : nullptr, DefaultCancelAction);
	case EControl::CancelMouse: return bUseConfig ? nullptr : DefaultCancelMouseAction.Get();
	case EControl::ToggleView:  return Pick(Config ? Config->ToggleView.Get() : nullptr, DefaultToggleViewAction);
	case EControl::Menu:        return Pick(Config ? Config->Menu.Get() : nullptr, DefaultMenuAction);
	case EControl::Leave:       return Pick(Config ? Config->Leave.Get() : nullptr, DefaultLeaveAction);
	}
	return nullptr;
}

void UEGChessPlayerComponent::BindInput()
{
	UnbindInput();
	BuildDefaultInput();

	APlayerController* Controller = GetPlayerController();
	UEnhancedInputComponent* Input = Controller ? Cast<UEnhancedInputComponent>(Controller->InputComponent) : nullptr;
	if (!Input)
	{
		return;
	}
	const AEGChessTableActor* Table = BoardTable.Get();
	const UEGChessInputConfig* Config = Table ? Table->GetInputConfig() : nullptr;
	const bool bUseConfig = Config && Config->MappingContext;

	auto Pick = [&](UInputAction* Configured, UInputAction* Default)
	{
		return bUseConfig ? Configured : ResolveAction(Configured, Default);
	};

	if (UInputAction* Cursor = Pick(Config ? Config->Cursor.Get() : nullptr, DefaultCursorAction))
	{
		BindingHandles.Add(Input->BindAction(Cursor, ETriggerEvent::Triggered, this, &UEGChessPlayerComponent::HandleCursorAction).GetHandle());
		BindingHandles.Add(Input->BindAction(Cursor, ETriggerEvent::Completed, this, &UEGChessPlayerComponent::HandleCursorReleased).GetHandle());
	}
	if (UInputAction* Action = Pick(Config ? Config->Select.Get() : nullptr, DefaultSelectAction))
	{
		BindingHandles.Add(Input->BindAction(Action, ETriggerEvent::Started, this, &UEGChessPlayerComponent::HandleSelectAction).GetHandle());
		BindingHandles.Add(Input->BindAction(Action, ETriggerEvent::Completed, this, &UEGChessPlayerComponent::HandleSelectReleased).GetHandle());
	}
	if (UInputAction* Action = Pick(Config ? Config->Cancel.Get() : nullptr, DefaultCancelAction))
	{
		BindingHandles.Add(Input->BindAction(Action, ETriggerEvent::Started, this, &UEGChessPlayerComponent::HandleCancelAction).GetHandle());
	}
	if (UInputAction* Action = Pick(Config ? Config->ToggleView.Get() : nullptr, DefaultToggleViewAction))
	{
		BindingHandles.Add(Input->BindAction(Action, ETriggerEvent::Started, this, &UEGChessPlayerComponent::HandleToggleViewAction).GetHandle());
	}
	if (UInputAction* Action = Pick(Config ? Config->Menu.Get() : nullptr, DefaultMenuAction))
	{
		BindingHandles.Add(Input->BindAction(Action, ETriggerEvent::Started, this, &UEGChessPlayerComponent::HandleMenuAction).GetHandle());
	}
	if (UInputAction* Action = Pick(Config ? Config->Leave.Get() : nullptr, DefaultLeaveAction))
	{
		BindingHandles.Add(Input->BindAction(Action, ETriggerEvent::Started, this, &UEGChessPlayerComponent::HandleLeaveAction).GetHandle());
	}
	if (UInputAction* Action = Pick(Config ? Config->Look.Get() : nullptr, DefaultLookAction))
	{
		BindingHandles.Add(Input->BindAction(Action, ETriggerEvent::Triggered, this, &UEGChessPlayerComponent::HandleLookAction).GetHandle());
	}
	if (!bUseConfig && DefaultCancelMouseAction)
	{
		// The tap trigger fires Triggered on release; Started would fire on every press, drags included.
		BindingHandles.Add(Input->BindAction(DefaultCancelMouseAction, ETriggerEvent::Triggered, this, &UEGChessPlayerComponent::HandleCancelAction).GetHandle());
	}
}

void UEGChessPlayerComponent::UnbindInput()
{
	APlayerController* Controller = GetPlayerController();
	UEnhancedInputComponent* Input = Controller ? Cast<UEnhancedInputComponent>(Controller->InputComponent) : nullptr;
	if (Input)
	{
		for (const uint32 Handle : BindingHandles)
		{
			Input->RemoveBindingByHandle(Handle);
		}
	}
	BindingHandles.Reset();
}

void UEGChessPlayerComponent::ApplyBoardInputMode_Implementation(bool bSeated)
{
	APlayerController* Controller = GetPlayerController();
	if (!Controller)
	{
		return;
	}
	BuildDefaultInput();

	const AEGChessTableActor* Table = BoardTable.Get();
	const UEGChessInputConfig* Config = Table ? Table->GetInputConfig() : nullptr;
	UInputMappingContext* Context = GetActiveMappingContext();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());

	if (bSeated)
	{
		if (Subsystem && Context)
		{
			Subsystem->AddMappingContext(Context, Config ? Config->MappingPriority : 10);
		}
		bPrevShowMouseCursor = Controller->bShowMouseCursor;
		if (UsesViewCentreAim())
		{
			// First-person play: no cursor, the mouse is captured and turns the camera.
			Controller->SetShowMouseCursor(false);
			Controller->SetInputMode(FInputModeGameOnly());
			return;
		}
		Controller->SetShowMouseCursor(true);
		FInputModeGameAndUI Mode;
		// A click never blinks the cursor, and a piece dragged with the left button keeps it in sight.
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(Mode);
		// Capture during any press: a game viewport hands a mouse press to input only when it captures
		// (FSceneViewport::OnMouseButtonDown), so capturing for the right button alone dropped every
		// plain left press and only a double click (IE_DoubleClick counts as a press) selected. Mouse
		// movement reaches input only under capture too; the look still needs the right button held,
		// which the default mapping chords.
		if (UGameViewportClient* Viewport = Controller->GetLocalPlayer() ? Controller->GetLocalPlayer()->ViewportClient.Get() : nullptr)
		{
			Viewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);
		}
	}
	else
	{
		if (Subsystem && Context)
		{
			Subsystem->RemoveMappingContext(Context);
		}
		Controller->SetShowMouseCursor(bPrevShowMouseCursor);
		Controller->SetInputMode(FInputModeGameOnly());
	}
}

void UEGChessPlayerComponent::HandleCursorAction(const FInputActionValue& Value)
{
	CursorAxis = Value.Get<FVector2D>();
	bMouseDriving = false;
}

void UEGChessPlayerComponent::HandleCursorReleased(const FInputActionValue& Value)
{
	CursorAxis = FVector2D::ZeroVector;
}

void UEGChessPlayerComponent::HandleSelectAction(const FInputActionValue& Value)
{
	if (IsBoardInputBlocked())
	{
		return;
	}
	if (bMouseDriving)
	{
		LastMousePosition = FVector2D(-1.0, -1.0);
		LastAimSquare = -2;
		UpdateMouseCursor();
	}
	const int32 Before = SelectedSquare;
	ConfirmAt(CursorSquare);
	bSelectedByHeldPress = EGChess::IsValidSquare(SelectedSquare) && SelectedSquare != Before;
}

void UEGChessPlayerComponent::HandleSelectReleased(const FInputActionValue& Value)
{
	const bool bPickedByThisPress = bSelectedByHeldPress;
	bSelectedByHeldPress = false;
	const UEGChessInputConfig* Settings = GetLookSettings();
	// A hand picks a piece up on one click and sets it down on the next; it is never dragged.
	if (!bPickedByThisPress || IsBoardInputBlocked() || bAwaitingServer || !Settings->bCarrySelectedPiece || !Settings->bDropOnRelease
		|| !EGChess::IsValidSquare(SelectedSquare) || UsesHandCarry())
	{
		return;
	}
	if (bMouseDriving)
	{
		LastMousePosition = FVector2D(-1.0, -1.0);
		LastAimSquare = -2;
		UpdateMouseCursor();
	}
	// Released where it was picked up: it stays carried and the next click places it. Released over
	// a legal square: that is a drag, so place it. Anywhere else it stays carried too.
	const int32 Square = CursorSquare;
	if (Square != SelectedSquare && SelectedMoves.ContainsByPredicate([Square](const FEGChessMove& Move) { return Move.To == Square; }))
	{
		ConfirmAt(Square);
	}
}

void UEGChessPlayerComponent::HandleCancelAction(const FInputActionValue& Value)
{
	if (PromotionWidget)
	{
		CancelPromotion();
		return;
	}
	if (PromptWidget)
	{
		AnswerPrompt(OpenPromptKind, false);
		return;
	}
	if (MenuWidget)
	{
		CloseMenu();
		return;
	}
	ClearSelection(SelectedSquare != INDEX_NONE);
}

void UEGChessPlayerComponent::HandleLookAction(const FInputActionValue& Value)
{
	if (UsesFixedCamera())
	{
		return; // an authored mapping may still carry a look action
	}
	const UEGChessInputConfig* Look = GetLookSettings();
	const FVector2D Delta = AdjustLookInput(Value.Get<FVector2D>());
	LookYaw = FMath::Clamp(LookYaw + static_cast<float>(Delta.X), -Look->MaxLookYaw, Look->MaxLookYaw);
	LookPitch = FMath::Clamp(LookPitch + static_cast<float>(Delta.Y), -Look->MaxLookPitchDown, Look->MaxLookPitchUp);
}

const UEGChessInputConfig* UEGChessPlayerComponent::GetLookSettings() const
{
	const AEGChessTableActor* Table = BoardTable.Get();
	const UEGChessInputConfig* Config = Table ? Table->GetInputConfig() : nullptr;
	return Config ? Config : GetDefault<UEGChessInputConfig>();
}

bool UEGChessPlayerComponent::UsesViewCentreAim() const
{
	return GetLookSettings()->AimMode == EEGChessAimMode::ViewCentre;
}

bool UEGChessPlayerComponent::UsesFixedCamera() const
{
	return GetLookSettings()->AimMode == EEGChessAimMode::FixedCursor;
}

FQuat UEGChessPlayerComponent::ApplyLookOffset(const FQuat& ViewRotation) const
{
	if (FMath::IsNearlyZero(LookYaw) && FMath::IsNearlyZero(LookPitch))
	{
		return ViewRotation;
	}
	// Yaw about world up and pitch clamped short of vertical: turning the head, never rolling it.
	FRotator Rotation = ViewRotation.Rotator();
	Rotation.Yaw += LookYaw;
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch + LookPitch, -85.0f, 85.0f);
	return Rotation.Quaternion();
}

void UEGChessPlayerComponent::HandleToggleViewAction(const FInputActionValue& Value)
{
	if (!IsBoardInputBlocked())
	{
		ToggleView();
	}
}

void UEGChessPlayerComponent::HandleMenuAction(const FInputActionValue& Value)
{
	if (MenuWidget)
	{
		CloseMenu();
	}
	else if (!IsBoardInputBlocked())
	{
		OpenMenu();
	}
}

void UEGChessPlayerComponent::HandleLeaveAction(const FInputActionValue& Value)
{
	if (!IsBoardInputBlocked())
	{
		RequestLeave();
	}
}

// =============================================================================================
// Tick
// =============================================================================================

void UEGChessPlayerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AEGChessTableActor* Table = BoardTable.Get();
	const UWorld* World = GetWorld();
	if (!bInBoardMode || !Table || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();

	if (!IsBoardInputBlocked())
	{
		UpdateMouseCursor();

		// Held direction: one step, then repeat after a delay.
		if (CursorAxis.Size() > 0.5f)
		{
			const FVector2D Step = FMath::Abs(CursorAxis.X) >= FMath::Abs(CursorAxis.Y)
				? FVector2D(FMath::Sign(CursorAxis.X), 0.0)
				: FVector2D(0.0, FMath::Sign(CursorAxis.Y));
			const UEGChessInputConfig* Config = Table->GetInputConfig();
			if (Step != LastCursorStep)
			{
				StepCursor(Step);
				LastCursorStep = Step;
				NextCursorRepeat = Now + (Config ? Config->CursorRepeatDelay : 0.35f);
			}
			else if (Now >= NextCursorRepeat)
			{
				StepCursor(Step);
				NextCursorRepeat = Now + (Config ? Config->CursorRepeatInterval : 0.12f);
			}
		}
		else
		{
			LastCursorStep = FVector2D::ZeroVector;
		}
	}

	if (bAwaitingServer && Now - AwaitingSince > 3.0)
	{
		bAwaitingServer = false;
		ClearSelection(false);
	}

	if (IllegalSquare != INDEX_NONE && Now >= IllegalUntil)
	{
		IllegalSquare = INDEX_NONE;
		RefreshLocalPresentation();
	}

	if (Table->IsTimed() && Table->GetPhase() == EEGChessPhase::Playing && !bLowTimeWarned)
	{
		if (Table->GetRemainingTime(GetLocalColor()) <= Table->GetClockRef().LowTimeThreshold)
		{
			bLowTimeWarned = true;
			PostNotice(EEGChessNotice::LowTime);
		}
	}

	UpdateHandIntent();
	UpdateCarry(DeltaTime);
	UpdateBoardCamera(DeltaTime);
}

// =============================================================================================
// Cursor and selection
// =============================================================================================

void UEGChessPlayerComponent::UpdateMouseCursor()
{
	APlayerController* Controller = GetPlayerController();
	AEGChessTableActor* Table = BoardTable.Get();
	if (!Controller || !Table)
	{
		return;
	}

	FVector Origin;
	FVector Direction;
	const bool bViewCentre = UsesViewCentreAim();
	if (bViewCentre)
	{
		// The ray the player is looking down: the rendered view, so it is right mid-blend too.
		if (!Controller->PlayerCameraManager)
		{
			return;
		}
		Origin = Controller->PlayerCameraManager->GetCameraLocation();
		Direction = Controller->PlayerCameraManager->GetCameraRotation().Vector();
	}
	else
	{
		float X = 0.0f;
		float Y = 0.0f;
		if (!Controller->GetMousePosition(X, Y))
		{
			return;
		}
		const FVector2D Mouse(X, Y);
		if (FVector2D::Distance(Mouse, LastMousePosition) < 1.0f)
		{
			return;
		}
		LastMousePosition = Mouse;
		bMouseDriving = true;
		if (!Controller->DeprojectScreenPositionToWorld(X, Y, Origin, Direction))
		{
			return;
		}
	}

	const int32 Square = ResolvePickedSquare(Origin, Direction);
	if (bViewCentre)
	{
		// Only a change of aimed square moves the cursor, so a still view leaves a pad-stepped cursor alone.
		if (Square == LastAimSquare)
		{
			return;
		}
		LastAimSquare = Square;
		bMouseDriving = true;
	}
	SetCursorSquare(Square, true);
}

int32 UEGChessPlayerComponent::ResolvePickedSquare(const FVector& Origin, const FVector& Direction) const
{
	const AEGChessTableActor* Table = BoardTable.Get();
	if (!Table)
	{
		return INDEX_NONE;
	}
	int32 PieceSquare = INDEX_NONE;
	Table->PieceVisuals->PickPiece(Origin, Direction, PieceSquare);
	int32 PlaneSquare = INDEX_NONE;
	Table->GetBoardGeometry().RayToSquare(Origin, Direction, PlaneSquare);

	// With a piece picked up, the next click is a move: a ray aimed at an empty square behind other
	// pieces passes through their pick volumes first, and taking the piece in front there made the
	// player reselect instead of move. So the square the ray lands on wins when it is a destination,
	// then a piece the move would capture, then whatever the ray met first.
	if (EGChess::IsValidSquare(SelectedSquare))
	{
		auto IsDestination = [this](int32 Square)
		{
			return EGChess::IsValidSquare(Square) && SelectedMoves.ContainsByPredicate([Square](const FEGChessMove& Move) { return Move.To == Square; });
		};
		if (IsDestination(PlaneSquare))
		{
			return PlaneSquare;
		}
		if (IsDestination(PieceSquare))
		{
			return PieceSquare;
		}
	}
	return PieceSquare != INDEX_NONE ? PieceSquare : PlaneSquare;
}

bool UEGChessPlayerComponent::GetAimRay(FVector& OutOrigin, FVector& OutDirection) const
{
	const APlayerController* Controller = GetPlayerController();
	if (!Controller)
	{
		return false;
	}
	if (UsesViewCentreAim())
	{
		if (!Controller->PlayerCameraManager)
		{
			return false;
		}
		OutOrigin = Controller->PlayerCameraManager->GetCameraLocation();
		OutDirection = Controller->PlayerCameraManager->GetCameraRotation().Vector();
		return true;
	}
	float X = 0.0f;
	float Y = 0.0f;
	return Controller->GetMousePosition(X, Y) && Controller->DeprojectScreenPositionToWorld(X, Y, OutOrigin, OutDirection);
}

void UEGChessPlayerComponent::UpdateCarry(float DeltaTime)
{
	AEGChessTableActor* Table = BoardTable.Get();
	const UEGChessInputConfig* Settings = GetLookSettings();
	// Same conditions under which the selection is shown at all (RefreshLocalPresentation).
	// With a live hand the character's own hand carries the piece (UEGChessPieceVisuals).
	const bool bWanted = Table && Table->IsVisualContentReady() && Settings->bCarrySelectedPiece
		&& EGChess::IsValidSquare(SelectedSquare)
		&& Table->GetPhase() == EEGChessPhase::Playing && !FeedbackOptions.bHardcore
		&& !UsesHandCarry();
	if (!bWanted)
	{
		if (bCarryActive && Table)
		{
			Table->PieceVisuals->SetCarry(false);
		}
		bCarryActive = false;
		return;
	}

	const FEGChessBoardGeometry Geometry = Table->GetBoardGeometry();
	if (!bCarryActive || CarrySquare != SelectedSquare)
	{
		// Picked up from its own square, then it travels.
		CarryLocal = Geometry.SquareToLocal(SelectedSquare);
		CarrySquare = SelectedSquare;
		bCarryActive = true;
	}

	FVector Target = CarryLocal;
	if (bAwaitingServer && EGChess::IsValidSquare(SentToSquare))
	{
		// Placed: it waits over its destination until the move lands.
		Target = Geometry.SquareToLocal(SentToSquare);
	}
	else if (EGChess::IsValidSquare(PendingPromotionTo))
	{
		Target = Geometry.SquareToLocal(PendingPromotionTo);
	}
	else
	{
		// The pointer, or on keyboard or pad the square cursor.
		GetPointerOnBoard(Target);
	}

	CarryLocal = Settings->CarryFollowSpeed > 0.0f ? FMath::VInterpTo(CarryLocal, Target, DeltaTime, Settings->CarryFollowSpeed) : Target;
	Table->PieceVisuals->SetCarry(true, CarryLocal);
}

bool UEGChessPlayerComponent::GetPointerOnBoard(FVector& OutLocal) const
{
	const AEGChessTableActor* Table = BoardTable.Get();
	if (!Table)
	{
		return false;
	}
	const FEGChessBoardGeometry Geometry = Table->GetBoardGeometry();
	if (!bMouseDriving)
	{
		// Keyboard or pad: the square cursor.
		if (!EGChess::IsValidSquare(CursorSquare))
		{
			return false;
		}
		OutLocal = Geometry.SquareToLocal(CursorSquare);
		OutLocal.Z = 0.0f;
		return true;
	}
	FVector Origin;
	FVector Direction;
	if (!GetAimRay(Origin, Direction))
	{
		return false;
	}
	const FVector LocalOrigin = Geometry.SurfaceToWorld.InverseTransformPosition(Origin);
	const FVector LocalDir = Geometry.SurfaceToWorld.InverseTransformVector(Direction);
	if (LocalDir.Z >= -UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const FVector Hit = LocalOrigin + LocalDir * (-LocalOrigin.Z / LocalDir.Z);
	// Square centres run 0..7 squares; keep the pointer over the playing area.
	const float Min = -0.5f * Geometry.SquareSize;
	const float Max = 7.5f * Geometry.SquareSize;
	OutLocal = FVector(FMath::Clamp(Hit.X, Min, Max), FMath::Clamp(Hit.Y, Min, Max), 0.0f);
	return true;
}

bool UEGChessPlayerComponent::UsesHandCarry() const
{
	const AEGChessTableActor* Table = BoardTable.Get();
	return Table && Table->UsesLiveHand(ActiveSeat);
}

void UEGChessPlayerComponent::UpdateHandIntent()
{
	AEGChessTableActor* Table = BoardTable.Get();
	FEGChessHandIntent Intent;
	Intent.Pointer = LocalHandIntent.Pointer;
	if (Table && Table->IsVisualContentReady() && UsesHandCarry())
	{
		// The turn as the board shows it, not as it replicated: a hand setting a piece down keeps it
		// until that move has been shown, even when the new position arrives first.
		const bool bPresentedTurn = Table->PieceVisuals->GetPresentedSideToMove() == GetLocalColor();
		Intent.bRaised = bPresentedTurn && (Table->GetPhase() == EEGChessPhase::Playing || bAwaitingServer);
		if (Intent.bRaised)
		{
			FVector Pointer;
			if (GetPointerOnBoard(Pointer))
			{
				Intent.Pointer = Pointer;
			}
			else if (!LocalHandIntent.bRaised && EGChess::IsValidSquare(CursorSquare))
			{
				// Rising while the pointer is off the board: over the square cursor.
				Intent.Pointer = Table->GetBoardGeometry().SquareToLocal(CursorSquare);
				Intent.Pointer.Z = 0.0f;
			}

			if (bAwaitingServer && EGChess::IsValidSquare(SentFromSquare) && EGChess::IsValidSquare(SentToSquare))
			{
				// Sent: the piece goes down on its square and the hand waits there for the move to land.
				Intent.HeldSquare = static_cast<int8>(SentFromSquare);
				Intent.PlaceSquare = static_cast<int8>(SentToSquare);
			}
			else if (EGChess::IsValidSquare(SelectedSquare) && Table->GetPosition().HasColorAt(SelectedSquare, GetLocalColor()))
			{
				Intent.HeldSquare = static_cast<int8>(SelectedSquare);
				if (EGChess::IsValidSquare(PendingPromotionTo))
				{
					// Choosing a promotion: the pawn waits on its square.
					Intent.PlaceSquare = static_cast<int8>(PendingPromotionTo);
				}
			}
		}
	}
	LocalHandIntent = Intent;

	// The server hears changes only: the state reliably, the pointer streamed at a capped rate.
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (!LocalHandIntent.HasSameState(SentHandIntent))
	{
		ServerSetHandIntent(LocalHandIntent);
		SentHandIntent = LocalHandIntent;
		LastHandPointerSendTime = Now;
	}
	else if (LocalHandIntent.bRaised && Now - LastHandPointerSendTime >= EGChessPlayerComponentPrivate::HandPointerSendInterval
		&& FVector::DistSquared2D(LocalHandIntent.Pointer, SentHandIntent.Pointer) > FMath::Square(EGChessPlayerComponentPrivate::HandPointerSendDistance))
	{
		ServerSetHandPointer(LocalHandIntent.Pointer);
		SentHandIntent.Pointer = LocalHandIntent.Pointer;
		LastHandPointerSendTime = Now;
	}
}

void UEGChessPlayerComponent::SetWorldInteractionBlocked(bool bBlocked)
{
	const APlayerController* Controller = GetPlayerController();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	UEGInteractionSystemComponent* System = Pawn ? Pawn->FindComponentByClass<UEGInteractionSystemComponent>() : nullptr;
	UEGInteractionComponent* Legacy = Pawn ? Pawn->FindComponentByClass<UEGInteractionComponent>() : nullptr;

	if (bBlocked)
	{
		if (System && !System->IsInteractionBlocked())
		{
			System->SetInteractionBlocked(true);
			bBlockedInteractionSystem = true;
		}
		if (Legacy && !Legacy->IsInteractionBlocked())
		{
			Legacy->SetInteractionBlocked(true);
			bBlockedLegacyInteraction = true;
		}
		return;
	}
	if (System && bBlockedInteractionSystem)
	{
		System->SetInteractionBlocked(false);
	}
	if (Legacy && bBlockedLegacyInteraction)
	{
		Legacy->SetInteractionBlocked(false);
	}
	bBlockedInteractionSystem = false;
	bBlockedLegacyInteraction = false;
}

void UEGChessPlayerComponent::SetCentreDotVisible(bool bVisible)
{
	const APlayerController* Controller = GetPlayerController();
	ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	UGameViewportClient* Viewport = LocalPlayer ? LocalPlayer->ViewportClient.Get() : nullptr;

	if (CentreDotWidget.IsValid())
	{
		if (Viewport)
		{
			Viewport->RemoveViewportWidgetForPlayer(LocalPlayer, CentreDotWidget.ToSharedRef());
		}
		CentreDotWidget.Reset();
	}
	if (!bVisible || !Viewport)
	{
		return;
	}

	const UEGChessInputConfig* Look = GetLookSettings();
	const float Size = FMath::Max(Look->CentreDotSize, 1.0f);
	CentreDotBrush = MakeShared<FSlateRoundedBoxBrush>(Look->CentreDotColor, Size * 0.5f, FVector2f(Size, Size));
	CentreDotWidget = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Visibility(EVisibility::HitTestInvisible)
		[
			SNew(SBox)
			.WidthOverride(Size)
			.HeightOverride(Size)
			[
				SNew(SImage).Image(CentreDotBrush.Get())
			]
		];
	Viewport->AddViewportWidgetForPlayer(LocalPlayer, CentreDotWidget.ToSharedRef(), 5);
}

void UEGChessPlayerComponent::StepCursor(const FVector2D& Direction)
{
	const AEGChessTableActor* Table = BoardTable.Get();
	if (!Table)
	{
		return;
	}
	if (!EGChess::IsValidSquare(CursorSquare))
	{
		SetCursorSquare(Table->GetPosition().FindKing(GetLocalColor()), false);
		return;
	}
	SetCursorSquare(Table->GetBoardGeometry().StepForSeat(CursorSquare, ActiveSeat, FMath::RoundToInt(Direction.X), FMath::RoundToInt(Direction.Y)), false);
}

void UEGChessPlayerComponent::SetCursorSquare(int32 Square, bool bFromMouse)
{
	if (Square == CursorSquare)
	{
		return;
	}
	CursorSquare = Square;
	if (!bFromMouse && EGChess::IsValidSquare(Square))
	{
		TriggerLocalEvent(EEGChessEvent::CursorMove);
	}
	RefreshLocalPresentation();
}

void UEGChessPlayerComponent::ConfirmAt(int32 Square)
{
	const AEGChessTableActor* Table = BoardTable.Get();
	if (!Table || Table->GetPhase() != EEGChessPhase::Playing || bAwaitingServer || !EGChess::IsValidSquare(Square))
	{
		return;
	}
	const FEGChessPosition& Position = Table->GetPosition();
	const EEGChessColor Local = GetLocalColor();

	const FEGChessMove* Target = EGChess::IsValidSquare(SelectedSquare)
		? SelectedMoves.FindByPredicate([Square](const FEGChessMove& Move) { return Move.To == Square; })
		: nullptr;

	if (Target)
	{
		if (!IsMyTurn())
		{
			PostNotice(EEGChessNotice::NotYourTurn);
			return;
		}
		if (Target->Promotion != EEGChessPieceType::None)
		{
			PendingPromotionFrom = SelectedSquare;
			PendingPromotionTo = Square;
			const UEGChessUIConfig* UI = Table->GetUIConfig();
			PromotionWidget = UI ? CreateChessWidget(UI->PromotionClass, EEGChessWidgetKind::Promotion, true) : nullptr;
			if (PromotionWidget)
			{
				PromotionWidget->SetupPromotion(Local);
			}
			else
			{
				ChoosePromotion(EEGChessPieceType::Queen);
			}
			return;
		}
		SendMove(*Target);
		return;
	}

	if (Square == SelectedSquare)
	{
		ClearSelection(true);
		return;
	}
	if (Position.HasColorAt(Square, Local))
	{
		Select(Square);
		return;
	}
	if (EGChess::IsValidSquare(SelectedSquare))
	{
		if (!Position.IsEmpty(Square))
		{
			FlashIllegal(Square);
			ClearSelection(false);
		}
		else
		{
			ClearSelection(true);
		}
	}
}

void UEGChessPlayerComponent::Select(int32 Square)
{
	LastAimSquare = -2;
	SelectedSquare = Square;
	RefreshSelectionMoves();
	if (SelectedMoves.Num() == 0)
	{
		SelectedSquare = INDEX_NONE;
		FlashIllegal(Square);
		return;
	}
	TriggerLocalEvent(EEGChessEvent::Select);
	RefreshLocalPresentation();
}

void UEGChessPlayerComponent::RefreshSelectionMoves()
{
	SelectedMoves.Reset();
	const AEGChessTableActor* Table = BoardTable.Get();
	if (!Table || !EGChess::IsValidSquare(SelectedSquare))
	{
		return;
	}
	// While waiting for the opponent, show what the piece could do if it were our turn.
	FEGChessPosition Preview = Table->GetPosition();
	const EEGChessColor Local = GetLocalColor();
	if (Preview.SideToMove != Local)
	{
		Preview.SideToMove = Local;
		Preview.EnPassantSquare = -1;
	}
	EGChessMoveGen::GenerateLegalFrom(Preview, SelectedSquare, SelectedMoves);
}

void UEGChessPlayerComponent::ClearSelection(bool bPlaySound)
{
	const bool bHadSelection = SelectedSquare != INDEX_NONE;
	LastAimSquare = -2;
	SelectedSquare = INDEX_NONE;
	SelectedMoves.Reset();
	if (bPlaySound && bHadSelection)
	{
		TriggerLocalEvent(EEGChessEvent::Deselect);
	}
	RefreshLocalPresentation();
}

void UEGChessPlayerComponent::SendMove(const FEGChessMove& Move)
{
	bAwaitingServer = true;
	SentToSquare = Move.To;
	SentFromSquare = Move.From;
	AwaitingSince = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	ServerSubmitMove(Move);
}

void UEGChessPlayerComponent::FlashIllegal(int32 Square)
{
	IllegalSquare = Square;
	IllegalUntil = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + 0.35;
	TriggerLocalEvent(EEGChessEvent::Illegal);
	RefreshLocalPresentation();
}

void UEGChessPlayerComponent::FillFeedbackInput(FEGChessFeedbackInput& Input) const
{
	Input.bLocalSeated = true;
	Input.LocalColor = GetLocalColor();
	Input.CursorSquare = CursorSquare;
	Input.SelectedSquare = SelectedSquare;
	Input.SelectedMoves = SelectedMoves;
	Input.IllegalSquare = IllegalSquare;
	Input.Options = FeedbackOptions;
}

void UEGChessPlayerComponent::RefreshLocalPresentation()
{
	AEGChessTableActor* Table = BoardTable.Get();
	if (!Table || !Table->IsVisualContentReady())
	{
		return;
	}
	UEGChessPieceVisuals* Visuals = Table->PieceVisuals;
	const FEGChessPosition& Position = Table->GetPosition();
	const bool bPlaying = Table->GetPhase() == EEGChessPhase::Playing && !FeedbackOptions.bHardcore;

	// A live hand holds the selected piece itself; without one it lifts on its square.
	Visuals->SetSelection(bPlaying && !UsesHandCarry() ? SelectedSquare : INDEX_NONE);

	// Outline the hovered piece of ours and every piece a capture would take.
	TArray<int32> Outlined;
	if (bPlaying)
	{
		if (EGChess::IsValidSquare(CursorSquare) && Position.HasColorAt(CursorSquare, GetLocalColor()))
		{
			Outlined.Add(CursorSquare);
		}
		if (FeedbackOptions.bShowLegalMoves)
		{
			for (const FEGChessMove& Move : SelectedMoves)
			{
				if (Move.IsCapture())
				{
					Outlined.AddUnique(Move.HasFlag(EGChessMoveFlags::EnPassant) ? Move.To + (GetLocalColor() == EEGChessColor::White ? -8 : 8) : Move.To);
				}
			}
		}
	}
	const UEGChessPieceSet* Set = Table->GetPieceSet();
	const int32 OutlineStencil = Set ? Set->OutlineStencil : 120;
	const int32 SelectedStencil = Set && Set->SelectedOutlineStencil > 0 ? Set->SelectedOutlineStencil : OutlineStencil;
	Visuals->SetOutlines(Outlined, OutlineStencil, bPlaying ? SelectedSquare : INDEX_NONE, SelectedStencil);

	const bool bCursorOnTarget = SelectedMoves.ContainsByPredicate([this](const FEGChessMove& Move) { return Move.To == CursorSquare; });
	if (bPlaying && FeedbackOptions.bShowGhostPiece && FeedbackOptions.bShowLegalMoves && bCursorOnTarget)
	{
		Visuals->ShowGhost(SelectedSquare, CursorSquare);
	}
	else
	{
		Visuals->HideGhost();
	}

	Table->RefreshMarks();
}

void UEGChessPlayerComponent::SetFeedbackOptions(const FEGChessFeedbackOptions& Options)
{
	FeedbackOptions = Options;
	if (AEGChessTableActor* Table = BoardTable.Get())
	{
		Table->SetCoordinatesVisible(Options.bShowCoordinates);
	}
	RefreshLocalPresentation();
}

// =============================================================================================
// Table notifications
// =============================================================================================

void UEGChessPlayerComponent::HandleTableStateChanged()
{
	AEGChessTableActor* Table = BoardTable.Get();
	if (!Table)
	{
		return;
	}
	const int32 Plies = Table->GetMoveHistoryRef().Num();
	const EEGChessPhase Phase = Table->GetPhase();

	if (Phase != LastSeenPhase)
	{
		if (Phase == EEGChessPhase::Starting)
		{
			bLowTimeWarned = false;
			CursorSquare = Table->GetPosition().FindKing(GetLocalColor());
		}
		if (Phase != EEGChessPhase::Playing)
		{
			ClearSelection(false);
			if (PromotionWidget)
			{
				CancelPromotion();
			}
		}
	}

	if (Plies != LastSeenPly || Phase != LastSeenPhase)
	{
		// The position changed: keep the selection only if the piece is still ours and can move.
		if (EGChess::IsValidSquare(SelectedSquare))
		{
			if (!Table->GetPosition().HasColorAt(SelectedSquare, GetLocalColor()))
			{
				ClearSelection(false);
			}
			else
			{
				RefreshSelectionMoves();
				if (SelectedMoves.Num() == 0)
				{
					ClearSelection(false);
				}
			}
		}
		if (IsMyTurn() && (Plies != LastSeenPly || LastSeenPhase != EEGChessPhase::Playing))
		{
			TriggerLocalEvent(EEGChessEvent::YourTurn);
		}
	}

	LastSeenPly = Plies;
	LastSeenPhase = Phase;
	RefreshWidgets();
	RefreshLocalPresentation();
}

void UEGChessPlayerComponent::HandleMoveCommitted(const FEGChessMoveRecord& Record)
{
	bAwaitingServer = false;
	if (Record.MoverColor == GetLocalColor())
	{
		ClearSelection(false);
	}
}

void UEGChessPlayerComponent::HandleGameEnded(const FEGChessResult& Result)
{
	ClearSelection(false);
	if (PromotionWidget)
	{
		CancelPromotion();
	}
	CloseMenu();
	RefreshWidgets();
}

void UEGChessPlayerComponent::PostNotice(EEGChessNotice Notice)
{
	const AEGChessTableActor* Table = BoardTable.IsValid() ? BoardTable.Get() : ActiveTable.Get();
	const UEGChessUIConfig* UI = Table ? Table->GetUIConfig() : nullptr;
	const FText Text = UI ? UI->GetNoticeText(Notice) : FText::GetEmpty();

	if (HUDWidget)
	{
		HUDWidget->ShowNotice(Notice, Text);
	}
	OnNotice.Broadcast(Notice, Text);

	EEGChessEvent Event = EEGChessEvent::Notice;
	switch (Notice)
	{
	case EEGChessNotice::NotYourTurn:
	case EEGChessNotice::IllegalMove: Event = EEGChessEvent::Illegal; break;
	case EEGChessNotice::DrawOffered: Event = EEGChessEvent::DrawOffered; break;
	case EEGChessNotice::DrawDeclined: Event = EEGChessEvent::DrawDeclined; break;
	case EEGChessNotice::OpponentJoined: Event = EEGChessEvent::OpponentJoined; break;
	case EEGChessNotice::OpponentLeft: Event = EEGChessEvent::OpponentLeft; break;
	case EEGChessNotice::RematchRequested: Event = EEGChessEvent::RematchRequested; break;
	case EEGChessNotice::LowTime: Event = EEGChessEvent::LowTimeWarning; break;
	default: break;
	}
	TriggerLocalEvent(Event);
}

void UEGChessPlayerComponent::TriggerLocalEvent(EEGChessEvent Event)
{
	if (AEGChessTableActor* Table = BoardTable.Get())
	{
		Table->TriggerLocalEvent(Event);
	}
}

// =============================================================================================
// Actions
// =============================================================================================

void UEGChessPlayerComponent::RequestLeave()
{
	if (!ActiveTable)
	{
		return;
	}
	if (ActiveTable->GetPhase() == EEGChessPhase::Playing)
	{
		OpenPrompt(EEGChessPromptKind::ConfirmLeave);
		return;
	}
	ServerLeaveSeat();
}

void UEGChessPlayerComponent::RequestResign()
{
	if (CanResign())
	{
		OpenPrompt(EEGChessPromptKind::ConfirmResign);
	}
}

void UEGChessPlayerComponent::OfferDraw()
{
	if (CanOfferDraw())
	{
		ServerOfferDraw();
	}
}

void UEGChessPlayerComponent::RespondToDraw(bool bAccept)
{
	ClosePrompt(EEGChessPromptKind::DrawOffered);
	if (HasIncomingDrawOffer())
	{
		ServerRespondDraw(bAccept);
	}
}

void UEGChessPlayerComponent::RequestTakeback()
{
	if (CanTakeback())
	{
		ClearSelection(false);
		ServerRequestTakeback();
	}
}

void UEGChessPlayerComponent::RequestRematch()
{
	if (ActiveTable && ActiveTable->GetPhase() == EEGChessPhase::GameOver)
	{
		ServerRequestRematch();
	}
}

void UEGChessPlayerComponent::PlayVersusAI()
{
	if (CanPlayVersusAI())
	{
		ServerStartVersusAI();
	}
}

void UEGChessPlayerComponent::AnswerPrompt(EEGChessPromptKind Kind, bool bAccepted)
{
	ClosePrompt(Kind);
	switch (Kind)
	{
	case EEGChessPromptKind::ConfirmResign:
		if (bAccepted)
		{
			CloseMenu();
			ServerResign();
		}
		break;
	case EEGChessPromptKind::ConfirmLeave:
		if (bAccepted)
		{
			CloseMenu();
			ServerLeaveSeat();
		}
		break;
	case EEGChessPromptKind::DrawOffered:
		if (HasIncomingDrawOffer())
		{
			ServerRespondDraw(bAccepted);
		}
		break;
	}
}

void UEGChessPlayerComponent::ChoosePromotion(EEGChessPieceType Piece)
{
	const int32 From = PendingPromotionFrom;
	const int32 To = PendingPromotionTo;
	PendingPromotionFrom = PendingPromotionTo = INDEX_NONE;
	if (PromotionWidget)
	{
		CloseWidget(PromotionWidget);
	}
	const FEGChessMove* Move = SelectedMoves.FindByPredicate([From, To, Piece](const FEGChessMove& Candidate)
	{
		return Candidate.From == From && Candidate.To == To && Candidate.Promotion == Piece;
	});
	if (Move && IsMyTurn())
	{
		SendMove(*Move);
	}
}

void UEGChessPlayerComponent::CancelPromotion()
{
	PendingPromotionFrom = PendingPromotionTo = INDEX_NONE;
	if (PromotionWidget)
	{
		CloseWidget(PromotionWidget);
	}
}

void UEGChessPlayerComponent::ToggleView()
{
	SetView(CurrentView == EEGChessView::Elevated ? EEGChessView::Eye : EEGChessView::Elevated);
}

void UEGChessPlayerComponent::SetView(EEGChessView View)
{
	if (View == CurrentView || !CanToggleView())
	{
		return;
	}
	if (BoardCamera)
	{
		ViewBlendFrom = BoardCamera->GetActorTransform();
		ViewBlendStart = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	}
	CurrentView = View;
	bViewChosenThisSession = true;
	LookYaw = 0.0f;
	LookPitch = 0.0f;
	bViewHasHistory = false;

	const APlayerController* Controller = GetPlayerController();
	if (APawn* Pawn = Controller ? Controller->GetPawn() : nullptr)
	{
		if (Pawn->Implements<UEGChessCharacterInterface>())
		{
			IEGChessCharacterInterface::Execute_OnChessViewChanged(Pawn, View == EEGChessView::Eye);
		}
	}
	RefreshWidgets();
}

// =============================================================================================
// Camera
// =============================================================================================

void UEGChessPlayerComponent::SpawnBoardCamera(float BlendTime)
{
	UWorld* World = GetWorld();
	APlayerController* Controller = GetPlayerController();
	if (!World || !Controller)
	{
		return;
	}
	if (World->GetTimerManager().IsTimerActive(CameraDestroyTimer))
	{
		World->GetTimerManager().ClearTimer(CameraDestroyTimer);
	}
	if (!BoardCamera)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		BoardCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
		if (!BoardCamera)
		{
			return;
		}
		BoardCamera->SetReplicates(false);
		BoardCamera->GetCameraComponent()->bConstrainAspectRatio = false;
	}

	ViewBlendStart = -1.0;
	float FOV = 60.0f;
	BoardCamera->SetActorTransform(ComputeViewTransform(CurrentView, 0.0f, FOV));
	bViewHasHistory = true;
	BoardCamera->GetCameraComponent()->SetFieldOfView(FOV);
	Controller->SetViewTargetWithBlend(BoardCamera, BlendTime, VTBlend_EaseInOut, 2.0f);
	SetCameraFollowsPawnMesh(true);
}

USkeletalMeshComponent* UEGChessPlayerComponent::GetLocalPawnMesh() const
{
	const APlayerController* Controller = GetPlayerController();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		return nullptr;
	}
	if (Pawn->Implements<UEGChessCharacterInterface>())
	{
		if (USkeletalMeshComponent* Mesh = IEGChessCharacterInterface::Execute_GetChessMesh(Pawn))
		{
			return Mesh;
		}
	}
	const ACharacter* Character = Cast<ACharacter>(Pawn);
	return Character ? Character->GetMesh() : Pawn->FindComponentByClass<USkeletalMeshComponent>();
}

void UEGChessPlayerComponent::SetCameraFollowsPawnMesh(bool bFollow)
{
	// The component ticks with its controller, before the pawn animates. A view that follows a bone
	// would then read last frame's pose: one frame behind a body that leans in, enough to put the lens
	// inside it. Waiting on the mesh's tick, which completes only once its pose is evaluated, fixes that.
	if (USkeletalMeshComponent* Previous = CameraPrerequisiteMesh.Get())
	{
		RemoveTickPrerequisiteComponent(Previous);
	}
	CameraPrerequisiteMesh.Reset();

	if (bFollow)
	{
		if (USkeletalMeshComponent* Mesh = GetLocalPawnMesh())
		{
			AddTickPrerequisiteComponent(Mesh);
			CameraPrerequisiteMesh = Mesh;
		}
	}
}

void UEGChessPlayerComponent::DestroyBoardCamera()
{
	if (BoardCamera)
	{
		BoardCamera->Destroy();
		BoardCamera = nullptr;
	}
}

FTransform UEGChessPlayerComponent::ComputeViewTransform(EEGChessView View, float DeltaTime, float& OutFOV)
{
	const AEGChessTableActor* Table = BoardTable.Get();
	OutFOV = 60.0f;
	if (!Table)
	{
		return FTransform::Identity;
	}
	const UEGChessSeatComponent* Seat = Table->GetSeat(ActiveSeat);

	if (View == EEGChessView::Eye)
	{
		const APawn* Pawn = Seat->GetSeatedPawn();
		const UEGChessAnimationSet* Set = Table->GetAnimationSetFor(Pawn);
		FVector Eye = Seat->GetComponentLocation() + FVector(0.0f, 0.0f, 120.0f);
		if (Pawn)
		{
			const ACharacter* Character = Cast<ACharacter>(Pawn);
			const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : Pawn->FindComponentByClass<USkeletalMeshComponent>();
			const FName Socket = Set ? Set->EyeSocket : FName(TEXT("head"));
			if (Mesh && Mesh->DoesSocketExist(Socket))
			{
				Eye = Mesh->GetSocketLocation(Socket);
			}
			Eye += Pawn->GetActorQuat().RotateVector(Set ? Set->EyeOffset : FVector(10.0f, 0.0f, 5.0f));
		}
		EyeLagLocation = DeltaTime > 0.0f ? FMath::VInterpTo(EyeLagLocation, Eye, DeltaTime, 10.0f) : Eye;

		// Aim at the board, a little towards the player's own half, so a moving head never shakes it.
		const FEGChessBoardGeometry Geometry = Table->GetBoardGeometry();
		const FVector Toward = (Seat->GetComponentLocation() - Geometry.GetCenterWorld()).GetSafeNormal2D();
		const FVector Target = Geometry.GetCenterWorld() + Toward * Geometry.SquareSize * 1.5f;
		OutFOV = Seat->BoardCamera ? FMath::Max(Seat->BoardCamera->FieldOfView, 70.0f) : 70.0f;
		return FTransform(ApplyLookOffset((Target - EyeLagLocation).Rotation().Quaternion()), EyeLagLocation);
	}

	if (Seat->BoardCamera)
	{
		OutFOV = Seat->BoardCamera->FieldOfView;
		return FTransform(ApplyLookOffset(Seat->BoardCamera->GetComponentQuat()), Seat->BoardCamera->GetComponentLocation());
	}
	return FTransform(ApplyLookOffset(Seat->GetComponentQuat()), Seat->GetComponentLocation() + FVector(0.0f, 0.0f, 150.0f));
}

void UEGChessPlayerComponent::UpdateBoardCamera(float DeltaTime)
{
	const AEGChessTableActor* Table = BoardTable.Get();
	const UWorld* World = GetWorld();
	if (!BoardCamera || !Table || !World)
	{
		return;
	}

	float FOV = 60.0f;
	FTransform Desired = ComputeViewTransform(CurrentView, bViewHasHistory ? DeltaTime : 0.0f, FOV);
	bViewHasHistory = true;
	if (ViewBlendStart >= 0.0)
	{
		const float Alpha = FMath::Clamp(static_cast<float>((World->GetTimeSeconds() - ViewBlendStart) / 0.3), 0.0f, 1.0f);
		const float Eased = FMath::SmoothStep(0.0f, 1.0f, Alpha);
		Desired = FTransform(FQuat::Slerp(ViewBlendFrom.GetRotation(), Desired.GetRotation(), Eased), FMath::Lerp(ViewBlendFrom.GetLocation(), Desired.GetLocation(), Eased));
		if (Alpha >= 1.0f)
		{
			ViewBlendStart = -1.0;
		}
	}
	BoardCamera->SetActorTransform(Desired);
	BoardCamera->GetCameraComponent()->SetFieldOfView(FOV);
}

// =============================================================================================
// Widgets
// =============================================================================================

template <typename T>
T* UEGChessPlayerComponent::CreateChessWidget(const TSoftClassPtr<T>& Class, EEGChessWidgetKind Kind, bool bModal)
{
	APlayerController* Controller = GetPlayerController();
	UClass* Resolved = EGChessEngineAssets::ResolveClass(Class);
	if (!Controller || !Resolved)
	{
		return nullptr;
	}
	T* Widget = CreateWidget<T>(Controller, Resolved);
	if (!Widget)
	{
		return nullptr;
	}
	Widget->InitChessWidget(this);
	AddChessWidget(Widget, Kind);
	if (bModal || Widget->IsModal())
	{
		ModalWidgets.AddUnique(Widget);
		CursorAxis = FVector2D::ZeroVector;
	}
	return Widget;
}

void UEGChessPlayerComponent::AddChessWidget_Implementation(UUserWidget* Widget, EEGChessWidgetKind Kind)
{
	if (!Widget)
	{
		return;
	}
	const AEGChessTableActor* Table = BoardTable.Get();
	const UEGChessUIConfig* UI = Table ? Table->GetUIConfig() : nullptr;
	Widget->AddToViewport((UI ? UI->BaseZOrder : 50) + static_cast<int32>(Kind));
}

void UEGChessPlayerComponent::RemoveChessWidget_Implementation(UUserWidget* Widget, EEGChessWidgetKind Kind)
{
	if (Widget)
	{
		Widget->RemoveFromParent();
	}
}

void UEGChessPlayerComponent::CloseWidget(UEGChessWidgetBase* Widget)
{
	if (!Widget)
	{
		return;
	}
	EEGChessWidgetKind Kind = EEGChessWidgetKind::HUD;
	if (Widget == MenuWidget) { Kind = EEGChessWidgetKind::Menu; MenuWidget = nullptr; }
	else if (Widget == PromptWidget) { Kind = EEGChessWidgetKind::Prompt; PromptWidget = nullptr; }
	else if (Widget == PromotionWidget) { Kind = EEGChessWidgetKind::Promotion; PromotionWidget = nullptr; }
	else if (Widget == GameOverWidget) { Kind = EEGChessWidgetKind::GameOver; GameOverWidget = nullptr; }
	else if (Widget == HUDWidget) { Kind = EEGChessWidgetKind::HUD; HUDWidget = nullptr; }
	ModalWidgets.Remove(Widget);
	RemoveChessWidget(Widget, Kind);
}

void UEGChessPlayerComponent::NotifyWidgetClosed(UEGChessWidgetBase* Widget)
{
	if (Widget && Widget == PromotionWidget)
	{
		CancelPromotion();
		return;
	}
	if (Widget && Widget == PromptWidget)
	{
		AnswerPrompt(OpenPromptKind, false);
		return;
	}
	CloseWidget(Widget);
}

void UEGChessPlayerComponent::OpenMenu()
{
	if (MenuWidget || !BoardTable.IsValid())
	{
		return;
	}
	if (const UEGChessUIConfig* UI = BoardTable->GetUIConfig())
	{
		MenuWidget = CreateChessWidget(UI->MenuClass, EEGChessWidgetKind::Menu, true);
	}
}

void UEGChessPlayerComponent::CloseMenu()
{
	if (MenuWidget)
	{
		CloseWidget(MenuWidget);
	}
}

void UEGChessPlayerComponent::OpenPrompt(EEGChessPromptKind Kind)
{
	const AEGChessTableActor* Table = BoardTable.Get();
	const UEGChessUIConfig* UI = Table ? Table->GetUIConfig() : nullptr;
	if (PromptWidget)
	{
		CloseWidget(PromptWidget);
	}
	PromptWidget = UI ? CreateChessWidget(UI->PromptClass, EEGChessWidgetKind::Prompt, true) : nullptr;
	OpenPromptKind = Kind;
	if (PromptWidget)
	{
		PromptWidget->SetupPrompt(Kind, UI->GetPromptTitle(Kind));
		return;
	}

	// No prompt widget: confirmations go straight through; an offer waits for the menu.
	if (Kind == EEGChessPromptKind::ConfirmLeave)
	{
		ServerLeaveSeat();
	}
	else if (Kind == EEGChessPromptKind::ConfirmResign)
	{
		ServerResign();
	}
}

void UEGChessPlayerComponent::ClosePrompt(EEGChessPromptKind Kind)
{
	if (PromptWidget && OpenPromptKind == Kind)
	{
		CloseWidget(PromptWidget);
	}
}

void UEGChessPlayerComponent::RefreshWidgets()
{
	AEGChessTableActor* Table = BoardTable.Get();
	if (!Table)
	{
		return;
	}
	const UEGChessUIConfig* UI = Table->GetUIConfig();
	const EEGChessPhase Phase = Table->GetPhase();

	if (Phase == EEGChessPhase::GameOver && !GameOverWidget && UI)
	{
		GameOverWidget = CreateChessWidget(UI->GameOverClass, EEGChessWidgetKind::GameOver, false);
		if (GameOverWidget)
		{
			const FEGChessResult Result = Table->GetResult();
			const FText Headline = Result.IsDraw() ? UI->DrawText : (Result.GetWinner() == GetLocalColor() ? UI->WinText : UI->LoseText);
			GameOverWidget->SetupGameOver(Result, Headline, UI->GetReasonText(Result.Reason));
		}
	}
	else if (Phase != EEGChessPhase::GameOver && GameOverWidget)
	{
		CloseWidget(GameOverWidget);
	}

	if (HasIncomingDrawOffer())
	{
		if (!bDrawOfferAnnounced)
		{
			bDrawOfferAnnounced = true;
			if (UI && !UI->PromptClass.IsNull())
			{
				OpenPrompt(EEGChessPromptKind::DrawOffered);
			}
			PostNotice(EEGChessNotice::DrawOffered);
		}
	}
	else
	{
		bDrawOfferAnnounced = false;
		ClosePrompt(EEGChessPromptKind::DrawOffered);
	}

	for (UEGChessWidgetBase* Widget : { static_cast<UEGChessWidgetBase*>(HUDWidget.Get()), static_cast<UEGChessWidgetBase*>(MenuWidget.Get()), static_cast<UEGChessWidgetBase*>(PromptWidget.Get()), static_cast<UEGChessWidgetBase*>(PromotionWidget.Get()), static_cast<UEGChessWidgetBase*>(GameOverWidget.Get()) })
	{
		if (Widget)
		{
			Widget->HandleStateChanged();
		}
	}
}

void UEGChessPlayerComponent::CloseAllWidgets()
{
	for (UEGChessWidgetBase* Widget : { static_cast<UEGChessWidgetBase*>(PromotionWidget.Get()), static_cast<UEGChessWidgetBase*>(PromptWidget.Get()), static_cast<UEGChessWidgetBase*>(MenuWidget.Get()), static_cast<UEGChessWidgetBase*>(GameOverWidget.Get()), static_cast<UEGChessWidgetBase*>(HUDWidget.Get()) })
	{
		CloseWidget(Widget);
	}
	ModalWidgets.Reset();
}
