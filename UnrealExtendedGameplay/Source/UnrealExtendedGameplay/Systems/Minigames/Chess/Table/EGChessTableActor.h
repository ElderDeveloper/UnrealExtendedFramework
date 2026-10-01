// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGChessBoardGeometry.h"
#include "EGChessMatchTypes.h"
#include "EGChessSeatComponent.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Math/RandomStream.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessPosition.h"

#include "EGChessTableActor.generated.h"

class AController;
class AGameModeBase;
class APlayerController;
class UAudioComponent;
class UCameraComponent;
class UEGChessAI;
class UEGChessAIProfile;
class UEGChessAnimationSet;
class UEGChessBoardFeedbackComponent;
class UEGChessEffectsSet;
class UEGChessIconCapture;
class UEGChessInputConfig;
class UEGChessPieceSet;
class UEGChessPieceVisuals;
class UEGChessPlayerComponent;
class UEGChessSoundSet;
class UEGChessTableConfig;
class UEGChessTableStyle;
class UEGChessUIConfig;
class UEGInteractionProviderComponent;
class UInstancedStaticMeshComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UTexture;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGChessEventTriggered, const FEGChessEventContext&, Context);

/**
 * AEGChessTableActor
 *
 * The one actor a game places. The board, pieces, clock, chairs, seats, cameras, move markers,
 * sounds and animation playback all belong to it, and every piece of content comes from its
 * UEGChessTableConfig. With no config it still plays, using engine basic shapes.
 *
 * ---------------------------------------------------------------------------
 * Authority
 * ---------------------------------------------------------------------------
 *
 * The table is the only writer of match state, on the server. Clients send requests through
 * UEGChessPlayerComponent on their PlayerController (a client can only call Server RPCs on an
 * actor its connection owns; the table is shared). The table adds that component to a player's
 * controller itself when they first sit.
 *
 * Durable state replicates through RepNotify so late joiners get it; one-shot presentation goes
 * through multicasts so it does not replay for someone who just joined. Every machine rebuilds
 * the position by replaying MoveHistory from StartFEN.
 *
 * ---------------------------------------------------------------------------
 * Override points
 * ---------------------------------------------------------------------------
 *
 * PlayChessSound and SpawnChessEffect are the only way the plugin makes a sound or an effect;
 * override them to route through another audio system. OnChessEvent broadcasts the same events
 * for games that only want to listen.
 */
UCLASS(Blueprintable, ClassGroup = (Chess))
class UNREALEXTENDEDGAMEPLAY_API AEGChessTableActor : public AActor
{
	GENERATED_BODY()

public:
	AEGChessTableActor();

	// -----------------------------------------------------------------
	// Configuration
	// -----------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UEGChessTableConfig> Config = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	bool bOverrideMatchRules = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (EditCondition = "bOverrideMatchRules"))
	FEGChessMatchRules MatchRulesOverride;

	/** Input slot of the Sit interaction on the interactor's interaction system. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	FGameplayTag SitInputTag;

	/**
	 * Added to a player's controller when they first sit, unless it already has one. A game
	 * subclasses UEGChessPlayerComponent to route input and widgets through its own systems.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TSubclassOf<UEGChessPlayerComponent> PlayerComponentClass;

	/** Draw squares, anchors, pick shapes and entry sweeps (also EG.Chess.Debug 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Debug")
	bool bDebugDraw = false;

	// -----------------------------------------------------------------
	// Components
	// -----------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UStaticMeshComponent> TableMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UStaticMeshComponent> BoardMesh;

	/** Centre of the corner square on Seat A's left; +X towards Seat B, +Y to Seat A's right. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<USceneComponent> BoardSurface;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<USceneComponent> TrayA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<USceneComponent> TrayB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UStaticMeshComponent> ClockMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UStaticMeshComponent> ClockButtonA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UStaticMeshComponent> ClockButtonB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UStaticMeshComponent> ClockHandA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UStaticMeshComponent> ClockHandB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UEGChessSeatComponent> SeatA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UStaticMeshComponent> ChairA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UCameraComponent> CameraA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UEGChessSeatComponent> SeatB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UStaticMeshComponent> ChairB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UCameraComponent> CameraB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UEGChessBoardFeedbackComponent> Feedback;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UEGChessPieceVisuals> PieceVisuals;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UEGInteractionProviderComponent> SitInteraction;

	// -----------------------------------------------------------------
	// Delegates (every machine)
	// -----------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Chess")
	FEGChessPhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Chess")
	FEGChessSeatChanged OnSeatChanged;

	UPROPERTY(BlueprintAssignable, Category = "Chess")
	FEGChessColoursAssigned OnColoursAssigned;

	UPROPERTY(BlueprintAssignable, Category = "Chess")
	FEGChessMoveCommitted OnMoveCommitted;

	UPROPERTY(BlueprintAssignable, Category = "Chess")
	FEGChessTakeback OnTakeback;

	UPROPERTY(BlueprintAssignable, Category = "Chess")
	FEGChessDrawOffered OnDrawOffered;

	UPROPERTY(BlueprintAssignable, Category = "Chess")
	FEGChessRematchRequested OnRematchRequested;

	UPROPERTY(BlueprintAssignable, Category = "Chess")
	FEGChessClockLow OnClockLow;

	UPROPERTY(BlueprintAssignable, Category = "Chess")
	FEGChessGameEnded OnGameEnded;

	/** Any replicated change a UI might want to redraw for. */
	UPROPERTY(BlueprintAssignable, Category = "Chess")
	FEGChessStateChanged OnStateChanged;

	/** Every presentation event, after PlayChessSound and SpawnChessEffect. */
	UPROPERTY(BlueprintAssignable, Category = "Chess")
	FEGChessEventTriggered OnChessEvent;

	// -----------------------------------------------------------------
	// Queries
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Chess")
	EEGChessPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	double GetPhaseEndServerTime() const { return PhaseEndServerTime; }

	const FEGChessPosition& GetPosition() const { return Position; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	FString GetFEN() const { return Position.ToFEN(); }

	UFUNCTION(BlueprintPure, Category = "Chess")
	EEGChessColor GetSideToMove() const { return Position.SideToMove; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	EEGChessSeat GetWhiteSeat() const { return WhiteSeat; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	EEGChessColor GetSeatColor(EEGChessSeat Seat) const { return Seat == WhiteSeat ? EEGChessColor::White : EEGChessColor::Black; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	EEGChessSeat GetSeatForColor(EEGChessColor Color) const { return Color == EEGChessColor::White ? WhiteSeat : EGChess::OtherSeat(WhiteSeat); }

	UFUNCTION(BlueprintPure, Category = "Chess")
	UEGChessSeatComponent* GetSeat(EEGChessSeat Seat) const { return Seat == EEGChessSeat::A ? SeatA : SeatB; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	FEGChessClockState GetClock() const { return Clock; }

	/** Remaining time for Color right now, on this machine's view of server time. */
	UFUNCTION(BlueprintPure, Category = "Chess")
	float GetRemainingTime(EEGChessColor Color) const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool IsTimed() const { return Clock.bTimed; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	FEGChessResult GetResult() const { return Result; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	TArray<FEGChessMoveRecord> GetMoveHistory() const { return MoveHistory; }

	const TArray<FEGChessMoveRecord>& GetMoveHistoryRef() const { return MoveHistory; }
	const FEGChessMatchRules& GetMatchRulesRef() const { return ActiveRules; }
	const FEGChessClockState& GetClockRef() const { return Clock; }
	const FString& GetStartFEN() const { return StartFEN; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	TArray<FString> GetMoveListSAN() const;

	/** Squares the piece on FromSquare can legally move to, for the side to move. */
	UFUNCTION(BlueprintPure, Category = "Chess")
	TArray<int32> GetLegalTargets(int32 FromSquare) const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool FindSeatOfPlayer(const APlayerState* PlayerState, EEGChessSeat& OutSeat) const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool FindSeatOfPawn(const APawn* Pawn, EEGChessSeat& OutSeat) const;

	UFUNCTION(BlueprintPure, Category = "Chess")
	FEGChessMatchRules GetMatchRules() const { return ActiveRules; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	FEGChessDrawOfferState GetDrawOffer() const { return DrawOffer; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool HasRematchRequest(EEGChessSeat Seat) const { return (RematchRequests & (1 << EGChess::SeatIndex(Seat))) != 0; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	int32 GetTakebacksUsed() const { return TakebacksUsed; }

	UFUNCTION(BlueprintPure, Category = "Chess")
	bool IsVisualContentReady() const { return bVisualsReady; }

	/** Captured pieces of Color (the pieces Color has lost). */
	UFUNCTION(BlueprintPure, Category = "Chess")
	TArray<EEGChessPieceType> GetCapturedPieces(EEGChessColor Color) const;

	/** White's material minus black's. */
	UFUNCTION(BlueprintPure, Category = "Chess")
	float GetMaterialBalance() const { return Position.Material(EEGChessColor::White) - Position.Material(EEGChessColor::Black); }

	/** Player name for a seat, or the AI profile's display name. */
	UFUNCTION(BlueprintPure, Category = "Chess")
	FText GetSeatDisplayName(EEGChessSeat Seat) const;

	/** Icon for a piece: authored, rendered at runtime, or null (show a letter). */
	UFUNCTION(BlueprintCallable, Category = "Chess")
	UTexture* GetPieceIcon(EEGChessPieceType Type, EEGChessColor Color);

	UFUNCTION(BlueprintPure, Category = "Chess")
	UEGChessTableConfig* GetConfig() const { return Config; }

	UEGChessTableStyle* GetTableStyle() const;
	UEGChessPieceSet* GetPieceSet() const;
	UEGChessSoundSet* GetSoundSet() const;
	UEGChessEffectsSet* GetEffectsSet() const;
	UEGChessUIConfig* GetUIConfig() const;
	UEGChessInputConfig* GetInputConfig() const;
	UEGChessAnimationSet* GetAnimationSetFor(const APawn* Pawn) const;

	FEGChessBoardGeometry GetBoardGeometry() const;
	USceneComponent* GetBoardSurface() const { return BoardSurface; }
	USceneComponent* GetTray(EEGChessSeat Seat) const { return Seat == EEGChessSeat::A ? TrayA : TrayB; }
	UStaticMeshComponent* GetClockButton(EEGChessSeat Seat) const { return Seat == EEGChessSeat::A ? ClockButtonA : ClockButtonB; }

	/** Whether the clock sits on this seat's right (decides which hand the move clip uses). */
	bool IsClockOnSeatRight(EEGChessSeat Seat) const;

	/** True when the seat's player is controlled on this machine. */
	bool IsSeatLocallyControlled(EEGChessSeat Seat) const;

	double GetServerTime() const;

	// -----------------------------------------------------------------
	// Server
	// -----------------------------------------------------------------

	/** Change the rules between games. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chess")
	void SetMatchRules(const FEGChessMatchRules& NewRules);

	/** Give the AI's seat a body. The table snaps it into the seat and animates it like a player. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chess")
	void SetSeatPawn(EEGChessSeat Seat, APawn* Pawn);

	/** Eject a seated player, e.g. for a threat in the game world. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chess")
	void ForceStandUp(APawn* Pawn, bool bForfeit, bool bImmediate);

	/** Set a position (debug / puzzles). Ends any game in progress without a result. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chess")
	bool DebugSetPosition(const FString& Fen);

	/** Both seats become the AI and games repeat. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Chess")
	void DebugStartAIvsAI();

	bool RequestSeat(APawn* Pawn, UPrimitiveComponent* HitComponent);
	bool CanPawnSit(const APawn* Pawn) const;

	void HandleLeaveSeat(APlayerController* Controller);
	void HandleSubmitMove(APlayerController* Controller, const FEGChessMove& Move);
	void HandleResign(APlayerController* Controller);
	void HandleOfferDraw(APlayerController* Controller);
	void HandleRespondDraw(APlayerController* Controller, bool bAccept);
	void HandleRequestTakeback(APlayerController* Controller);
	void HandleRequestRematch(APlayerController* Controller);
	void HandleStartVersusAI(APlayerController* Controller);

	// -----------------------------------------------------------------
	// Presentation routing (every machine)
	// -----------------------------------------------------------------

	/** The only way the plugin plays a sound. Default: the sound set through UGameplayStatics. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Chess")
	void PlayChessSound(EEGChessEvent Event, const FEGChessEventContext& Context);
	virtual void PlayChessSound_Implementation(EEGChessEvent Event, const FEGChessEventContext& Context);

	/** The only way the plugin spawns an effect. Default: the effects set through Niagara. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Chess")
	void SpawnChessEffect(EEGChessEvent Event, const FEGChessEventContext& Context);
	virtual void SpawnChessEffect_Implementation(EEGChessEvent Event, const FEGChessEventContext& Context);

	void TriggerEvent(const FEGChessEventContext& Context);

	/** A 2D event for this machine's seated player (UI feedback, results). */
	void TriggerLocalEvent(EEGChessEvent Event);

	// -----------------------------------------------------------------
	// Called by seats and the local player component
	// -----------------------------------------------------------------

	void HandleSeatOccupantChanged(EEGChessSeat Seat);
	void HandleSeatActionStarted(EEGChessSeat Seat, EEGChessSeatAction Action);
	void HandleSeatExitFinished(EEGChessSeat Seat);

	/** A seated character's move clip reached a notify. Games that play montages themselves forward notifies here. */
	UFUNCTION(BlueprintCallable, Category = "Chess")
	void HandleCharacterNotify(EEGChessSeat Seat, FName NotifyName);

	void SetLocalDriver(UEGChessPlayerComponent* Driver);
	UEGChessPlayerComponent* GetLocalDriver() const { return LocalDriver.Get(); }

	/** Rebuild and draw the marks for this machine's viewer. */
	void RefreshMarks();

	/** Show or hide the coordinate labels on this machine. */
	void SetCoordinatesVisible(bool bVisible);

	// -----------------------------------------------------------------
	// AActor
	// -----------------------------------------------------------------

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	// -----------------------------------------------------------------
	// Replicated state
	// -----------------------------------------------------------------

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	EEGChessPhase Phase = EEGChessPhase::Idle;

	UPROPERTY(Replicated)
	double PhaseEndServerTime = 0.0;

	UPROPERTY(ReplicatedUsing = OnRep_WhiteSeat)
	EEGChessSeat WhiteSeat = EEGChessSeat::A;

	UPROPERTY(ReplicatedUsing = OnRep_GameIndex)
	int32 GameIndex = 0;

	UPROPERTY(ReplicatedUsing = OnRep_MoveHistory)
	TArray<FEGChessMoveRecord> MoveHistory;

	UPROPERTY(Replicated)
	FString StartFEN;

	UPROPERTY(ReplicatedUsing = OnRep_Clock)
	FEGChessClockState Clock;

	UPROPERTY(ReplicatedUsing = OnRep_Result)
	FEGChessResult Result;

	UPROPERTY(ReplicatedUsing = OnRep_DrawOffer)
	FEGChessDrawOfferState DrawOffer;

	UPROPERTY(ReplicatedUsing = OnRep_Rematch)
	uint8 RematchRequests = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Rules)
	FEGChessMatchRules ActiveRules;

	UPROPERTY(Replicated)
	int32 TakebacksUsed = 0;

	UFUNCTION()
	void OnRep_Phase();

	UFUNCTION()
	void OnRep_WhiteSeat();

	UFUNCTION()
	void OnRep_GameIndex();

	UFUNCTION()
	void OnRep_MoveHistory();

	UFUNCTION()
	void OnRep_Clock();

	UFUNCTION()
	void OnRep_Result();

	UFUNCTION()
	void OnRep_DrawOffer();

	UFUNCTION()
	void OnRep_Rematch();

	UFUNCTION()
	void OnRep_Rules();

	// -----------------------------------------------------------------
	// Multicasts
	// -----------------------------------------------------------------

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMoveCommitted(const FEGChessMoveRecord& Record, int32 PlyIndex, EEGChessSeat MoverSeat);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastTakeback(int32 NewLength);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastReaction(EEGChessSeat Seat, EEGChessReaction Reaction, int32 Variant);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastGameEnded(const FEGChessResult& EndResult);

private:
	// Content
	void ResolveConfigLoads();
	void OnVisualContentLoaded();
	void EnsureInteractionContentLoaded();
	void ApplyStyle(bool bEditorPreview);
	void BuildFallbackBoard();
	void RefreshCoordinates();
	void UpdateClockHands();

	// Position
	FString GetEffectiveStartFEN() const;
	void RebuildPosition();
	TConstArrayView<uint64> GetRepetitionView() const;
	void ScheduleVisualReconcile();
	void ReconcileVisuals();

	// Server flow
	void SetPhase(EEGChessPhase NewPhase, double EndServerTime = 0.0);
	void UpdatePhaseAfterSeatChange();
	void OccupyWithAI(EEGChessSeat Seat);
	void BeginStarting(bool bRematch);
	void BeginPlaying();
	void CommitMove(const FEGChessMove& Move);
	void EndGame(const FEGChessResult& NewResult);
	void ForfeitSeat(EEGChessSeat Seat);
	void FreeSeat(EEGChessSeat Seat, bool bAnimatedExit, bool bImmediate);
	void ArmFlagTimer();
	void HandleFlagFall();
	void ScheduleAITurn();
	void PerformAIMove();
	void ScheduleThinkingIdle();
	void PlayThinkingIdle();
	float EstimateLastMovePresentation() const;
	UEGChessAIProfile* GetEffectiveAIProfile() const;
	UEGChessAI* GetOrCreateAI();
	bool ResolveRequester(APlayerController* Controller, EEGChessSeat& OutSeat) const;
	bool IsSeatAI(EEGChessSeat Seat) const;
	bool IsAIGame() const;
	void NotifyPlayer(EEGChessSeat Seat, EEGChessNotice Notice);
	UEGChessPlayerComponent* GetPlayerComponent(EEGChessSeat Seat) const;
	void BindOccupantDelegates(EEGChessSeat Seat);
	void UnbindOccupantDelegates(EEGChessSeat Seat);
	FEGChessMatchRules ResolveRules() const;

	UFUNCTION()
	void HandleOccupantPawnDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleOccupantPossessionChanged(APawn* OldPawn, APawn* NewPawn);

	void HandleLogout(AGameModeBase* GameMode, AController* Exiting);
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void TryReclaimSeat(APlayerController* Controller);

	UFUNCTION()
	void HandleSitFocusChanged(bool bFocused, UPrimitiveComponent* HitComponent);

	void NotifyStateChanged();
	void DebugDraw() const;

	// Local presentation state
	FEGChessPosition Position;
	TArray<uint64> PositionHashes;
	int32 IrreversibleIndex = 0;

	TWeakObjectPtr<UEGChessPlayerComponent> LocalDriver;
	bool bVisualsReady = false;
	bool bCoordinatesVisible = false;
	int32 LastCountdownSecond = -1;
	bool bLowTimeLoopPlaying = false;
	bool bLowTimeBroadcast[2] = { false, false };
	int32 PresentedGameIndex = -1;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> FallbackBoard;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> CoordinateLabels;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> LowTimeLoop;

	UPROPERTY(Transient)
	TObjectPtr<UEGChessIconCapture> IconCapture;

	TSharedPtr<FStreamableHandle> VisualContentHandle;
	TSharedPtr<FStreamableHandle> InteractionContentHandle;

	// Server-only state
	UPROPERTY(Transient)
	TObjectPtr<UEGChessAI> AIInstance;

	UPROPERTY(Transient)
	TObjectPtr<UEGChessAIProfile> FallbackAIProfile;

	int32 ThinkingPly = INDEX_NONE;
	uint8 LastRematchBits = 0;
	FRandomStream AIRandom;
	bool bNextGameIsRematch = false;
	bool bHasPlayedGame = false;
	int32 LastCheckReactionPly = -2;
	FTimerHandle StartTimer;
	FTimerHandle FlagTimer;
	FTimerHandle AITimer;
	FTimerHandle AIDrawTimer;
	FTimerHandle ThinkingTimer;
	FTimerHandle RestartTimer;
	FTimerHandle ReconcileTimer;
	FTimerHandle ForfeitTimers[2];
	FDelegateHandle LogoutHandle;
	FDelegateHandle PostLoginHandle;
	TWeakObjectPtr<AController> OccupantControllers[2];
	TWeakObjectPtr<APawn> BoundPawns[2];
	TWeakObjectPtr<UPrimitiveComponent> FocusedChair;
};
