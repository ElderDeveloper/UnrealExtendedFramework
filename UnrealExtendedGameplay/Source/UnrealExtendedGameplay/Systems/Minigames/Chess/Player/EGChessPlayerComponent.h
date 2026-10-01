// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Feedback/EGChessBoardFeedback.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Feedback/EGChessFeedbackTypes.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessMatchTypes.h"

#include "EGChessPlayerComponent.generated.h"

class ACameraActor;
class AController;
class AEGChessTableActor;
class APlayerController;
class UEGChessGameOverWidgetBase;
class UEGChessHUDWidgetBase;
class UEGChessMenuWidgetBase;
class UEGChessPromotionWidgetBase;
class UEGChessPromptWidgetBase;
class UEGChessWidgetBase;
class UInputAction;
class UInputMappingContext;
class UUserWidget;

UENUM(BlueprintType)
enum class EEGChessWidgetKind : uint8
{
	HUD,
	Menu,
	Prompt,
	Promotion,
	GameOver
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGChessNoticePosted, EEGChessNotice, Notice, const FText&, Text);

/**
 * UEGChessPlayerComponent
 *
 * Everything that belongs to one player at a chess table: the Server RPCs (a client can only
 * call them on an actor its connection owns, and the table is shared), the board input, the
 * square cursor and selection, the local board camera, and the widgets.
 *
 * The table adds it to a player's controller on the server when they first sit; a game may add
 * it to its PlayerController class instead. ActiveTable replicates to the owning client only, and
 * its arrival is what switches the client into board mode, so the order in which the dynamic
 * component and the table's own state arrive does not matter.
 *
 * Two hooks keep it generic: ApplyBoardInputMode (mapping context, cursor, input mode) and
 * AddChessWidget/RemoveChessWidget (where widgets go). A game with its own input stack or UI
 * layers overrides those and nothing else.
 */
UCLASS(ClassGroup = (Chess), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGChessPlayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEGChessPlayerComponent();

	static UEGChessPlayerComponent* Find(const AController* Controller);

	/** Server: the controller's component, created (as Class, else this class) and replicated if it has none. */
	static UEGChessPlayerComponent* FindOrAdd(APlayerController* Controller, TSubclassOf<UEGChessPlayerComponent> Class = nullptr);

	// -----------------------------------------------------------------
	// State
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool IsSeated() const { return ActiveTable != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	AEGChessTableActor* GetTable() const { return ActiveTable; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	EEGChessSeat GetSeat() const { return ActiveSeat; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	EEGChessColor GetLocalColor() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool IsMyTurn() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	EEGChessView GetView() const { return CurrentView; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool CanResign() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool CanOfferDraw() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool CanTakeback() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool CanToggleView() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool CanPlayVersusAI() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool HasIncomingDrawOffer() const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool IsAwaitingServer() const { return bAwaitingServer; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	int32 GetCursorSquare() const { return CursorSquare; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	int32 GetSelectedSquare() const { return SelectedSquare; }

	/** Board input is suspended while a modal chess widget is open. */
	UFUNCTION(BlueprintPure, Category = "Chess")
	bool IsBoardInputBlocked() const { return ModalWidgets.Num() > 0; }

	// -----------------------------------------------------------------
	// Actions (local; widgets call these)
	// -----------------------------------------------------------------

	/** Stand up. Asks "Resign and leave?" first during a game. */
	UFUNCTION(BlueprintCallable, Category = "Chess")
	void RequestLeave();

	/** Asks "Resign?" first. */
	UFUNCTION(BlueprintCallable, Category = "Chess")
	void RequestResign();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void OfferDraw();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void RespondToDraw(bool bAccept);

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void RequestTakeback();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void RequestRematch();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void PlayVersusAI();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void ToggleView();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void SetView(EEGChessView View);

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void OpenMenu();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void CloseMenu();

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void ChoosePromotion(EEGChessPieceType Piece);

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void CancelPromotion();

	/** A prompt widget reports its answer here. */
	UFUNCTION(BlueprintCallable, Category = "Chess")
	void AnswerPrompt(EEGChessPromptKind Kind, bool bAccepted);

	UFUNCTION(BlueprintCallable, Category = "Chess")
	void SetFeedbackOptions(const FEGChessFeedbackOptions& Options);

	/** Per-player marker options, for a game to bind to its own settings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	FEGChessFeedbackOptions FeedbackOptions;

	UPROPERTY(BlueprintAssignable, Category = "Chess")
	FEGChessNoticePosted OnNotice;

	// -----------------------------------------------------------------
	// Hooks
	// -----------------------------------------------------------------

	/**
	 * Everything that changes when sitting down, restored on standing up: the mapping context,
	 * the mouse cursor and the input mode. Default: Enhanced Input subsystem plus GameAndUI.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Chess")
	void ApplyBoardInputMode(bool bSeated);
	virtual void ApplyBoardInputMode_Implementation(bool bSeated);

	/** Where widgets go. Default: the viewport at the UI config's Z-order. */
	UFUNCTION(BlueprintNativeEvent, Category = "Chess")
	void AddChessWidget(UUserWidget* Widget, EEGChessWidgetKind Kind);
	virtual void AddChessWidget_Implementation(UUserWidget* Widget, EEGChessWidgetKind Kind);

	UFUNCTION(BlueprintNativeEvent, Category = "Chess")
	void RemoveChessWidget(UUserWidget* Widget, EEGChessWidgetKind Kind);
	virtual void RemoveChessWidget_Implementation(UUserWidget* Widget, EEGChessWidgetKind Kind);

	// -----------------------------------------------------------------
	// Server
	// -----------------------------------------------------------------

	void SetActiveTable(AEGChessTableActor* Table, EEGChessSeat Seat);

	UFUNCTION(Server, Reliable)
	void ServerLeaveSeat();

	UFUNCTION(Server, Reliable)
	void ServerSubmitMove(const FEGChessMove& Move);

	UFUNCTION(Server, Reliable)
	void ServerResign();

	UFUNCTION(Server, Reliable)
	void ServerOfferDraw();

	UFUNCTION(Server, Reliable)
	void ServerRespondDraw(bool bAccept);

	UFUNCTION(Server, Reliable)
	void ServerRequestTakeback();

	UFUNCTION(Server, Reliable)
	void ServerRequestRematch();

	UFUNCTION(Server, Reliable)
	void ServerStartVersusAI();

	UFUNCTION(Client, Reliable)
	void ClientNotice(EEGChessNotice Notice);

	UFUNCTION(Client, Reliable)
	void ClientMoveRejected();

	// -----------------------------------------------------------------
	// Called by the table on this machine
	// -----------------------------------------------------------------

	void HandleTableStateChanged();
	void HandleMoveCommitted(const FEGChessMoveRecord& Record);
	void HandleGameEnded(const FEGChessResult& Result);
	void FillFeedbackInput(FEGChessFeedbackInput& Input) const;
	void PostNotice(EEGChessNotice Notice);

	/** The widget told us it closed itself. */
	void NotifyWidgetClosed(UEGChessWidgetBase* Widget);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_ActiveTable)
	TObjectPtr<AEGChessTableActor> ActiveTable = nullptr;

	UPROPERTY(Replicated)
	EEGChessSeat ActiveSeat = EEGChessSeat::A;

	UFUNCTION()
	void OnRep_ActiveTable();

	// For subclasses that route input or widgets through their own game systems.
	APlayerController* GetPlayerController() const;
	bool IsLocalPlayer() const;
	AEGChessTableActor* GetBoardTable() const { return BoardTable.Get(); }

	/** The config's mapping context, else the default one built in code. */
	UInputMappingContext* GetActiveMappingContext() const;
	void BuildDefaultInput();

	/** The table's input config, or the class defaults when it has none: the look tuning always has values. */
	const class UEGChessInputConfig* GetLookSettings() const;

	/** True when squares are picked at the centre of the view and no cursor is shown. */
	bool UsesViewCentreAim() const;

	/** Last chance to scale the look input, in degrees, e.g. by the player's own sensitivity setting. */
	virtual FVector2D AdjustLookInput(const FVector2D& Degrees) const { return Degrees; }

private:

	// Board mode
	void EnterBoardMode(AEGChessTableActor* Table);
	void ExitBoardMode();
	void BindInput();
	void UnbindInput();
	UInputAction* ResolveAction(UInputAction* Configured, UInputAction* Default) const;

	void HandleCursorAction(const FInputActionValue& Value);
	void HandleCursorReleased(const FInputActionValue& Value);
	void HandleSelectAction(const FInputActionValue& Value);
	void HandleCancelAction(const FInputActionValue& Value);
	void HandleToggleViewAction(const FInputActionValue& Value);
	void HandleMenuAction(const FInputActionValue& Value);
	void HandleLeaveAction(const FInputActionValue& Value);
	void HandleLookAction(const FInputActionValue& Value);

	/** Adds the look offset to a view rotation, clamped by the look settings. */
	FQuat ApplyLookOffset(const FQuat& ViewRotation) const;

	// Cursor and selection
	void UpdateMouseCursor();
	void StepCursor(const FVector2D& Direction);
	void SetCursorSquare(int32 Square, bool bFromMouse);
	void ConfirmAt(int32 Square);
	void Select(int32 Square);
	void ClearSelection(bool bPlaySound);
	void SendMove(const FEGChessMove& Move);
	void FlashIllegal(int32 Square);
	void RefreshSelectionMoves();
	void RefreshLocalPresentation();

	// Camera
	void SpawnBoardCamera(float BlendTime);
	void DestroyBoardCamera();
	void UpdateBoardCamera(float DeltaTime);
	FTransform ComputeViewTransform(EEGChessView View, float& OutFOV) const;

	// Widgets
	template <typename T>
	T* CreateChessWidget(const TSoftClassPtr<T>& Class, EEGChessWidgetKind Kind, bool bModal);
	void CloseWidget(UEGChessWidgetBase* Widget);
	void OpenPrompt(EEGChessPromptKind Kind);
	void ClosePrompt(EEGChessPromptKind Kind);
	void RefreshWidgets();
	void CloseAllWidgets();

	void TriggerLocalEvent(EEGChessEvent Event);

	// Local board-mode state
	TWeakObjectPtr<AEGChessTableActor> BoardTable;
	bool bInBoardMode = false;
	bool bPrevShowMouseCursor = false;
	bool bPrevAutoManageCamera = true;
	EEGChessView CurrentView = EEGChessView::Elevated;
	bool bViewChosenThisSession = false;

	/** Seated free look, in degrees, relative to the current view. Reset on entering and on view changes. */
	float LookYaw = 0.0f;
	float LookPitch = 0.0f;

	int32 CursorSquare = INDEX_NONE;
	int32 SelectedSquare = INDEX_NONE;
	int32 IllegalSquare = INDEX_NONE;
	double IllegalUntil = 0.0;
	TArray<FEGChessMove> SelectedMoves;
	bool bAwaitingServer = false;
	double AwaitingSince = 0.0;
	int32 PendingPromotionFrom = INDEX_NONE;
	int32 PendingPromotionTo = INDEX_NONE;

	FVector2D CursorAxis = FVector2D::ZeroVector;
	FVector2D LastCursorStep = FVector2D::ZeroVector;
	double NextCursorRepeat = 0.0;
	FVector2D LastMousePosition = FVector2D(-1.0, -1.0);
	bool bMouseDriving = false;

	/** View-centre aim: the square it last resolved to, so a pad-moved cursor survives a still view. -2 = none yet. */
	int32 LastAimSquare = -2;

	/** The square a pick ray means: a legal destination of the selection beats a piece standing in front of it. */
	int32 ResolvePickedSquare(const FVector& Origin, const FVector& Direction) const;

	// World interaction is blocked while seated, so nothing is focused or outlined behind the board.
	// Only undone if this component set it.
	void SetWorldInteractionBlocked(bool bBlocked);
	bool bBlockedInteractionSystem = false;
	bool bBlockedLegacyInteraction = false;

	// The ViewCentre centre dot.
	void SetCentreDotVisible(bool bVisible);
	TSharedPtr<class SWidget> CentreDotWidget;
	TSharedPtr<struct FSlateBrush> CentreDotBrush;

	/** The aim mode the default mapping context was built for; a change rebuilds it. */
	bool bDefaultInputForViewCentre = true;

	bool bLowTimeWarned = false;
	int32 LastSeenPly = -1;
	EEGChessPhase LastSeenPhase = EEGChessPhase::Idle;
	bool bDrawOfferAnnounced = false;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> BoardCamera;

	FTransform ViewBlendFrom;
	double ViewBlendStart = -1.0;
	FVector EyeLagLocation = FVector::ZeroVector;
	bool bEyeLagInitialized = false;
	FTimerHandle CameraDestroyTimer;

	UPROPERTY(Transient)
	TObjectPtr<UEGChessHUDWidgetBase> HUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UEGChessMenuWidgetBase> MenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UEGChessPromptWidgetBase> PromptWidget;

	UPROPERTY(Transient)
	TObjectPtr<UEGChessPromotionWidgetBase> PromotionWidget;

	UPROPERTY(Transient)
	TObjectPtr<UEGChessGameOverWidgetBase> GameOverWidget;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UEGChessWidgetBase>> ModalWidgets;

	EEGChessPromptKind OpenPromptKind = EEGChessPromptKind::ConfirmResign;

	// Input
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DefaultCursorAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DefaultSelectAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DefaultCancelAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DefaultToggleViewAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DefaultMenuAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DefaultLeaveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DefaultLookAction;

	/** Right mouse held: the chord that turns mouse movement into look, so plain moves still hover squares. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DefaultLookHoldAction;

	/** Right mouse tapped: cancel. A tap, so a right-drag look never cancels the selection. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DefaultCancelMouseAction;

	TArray<uint32> BindingHandles;
};
