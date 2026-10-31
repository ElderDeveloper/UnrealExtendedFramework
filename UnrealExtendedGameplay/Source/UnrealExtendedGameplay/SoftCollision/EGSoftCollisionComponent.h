// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "EGSoftCollisionMath.h"
#include "EGSoftCollisionTypes.h"
#include "Prediction/EGSoftCollisionPrediction.h"

#include "EGSoftCollisionComponent.generated.h"

class AController;
class APawn;
class UCharacterMovementComponent;
class UEGSoftCollisionDriver;
class UEGSoftCollisionProfile;
class UEGSoftCollisionSubsystem;
class UPrimitiveComponent;

/** What the snapshot needs to know about a pawn besides its body. */
struct FEGSoftCollisionSnapshotInfo
{
	/** This machine decides the pawn's pushes. */
	bool bScanner = false;
	bool bPlayerControlled = false;
	float SleepSpeed = 0.0f;
};

/**
 * UEGSoftCollisionComponent
 * Soft pawn-vs-pawn collision on one pawn. Add it to any pawn; nothing else is required.
 *
 * At BeginPlay it stops the push primitive (the root capsule, box or sphere) from blocking the
 * soft channel (Pawn by default), registers with the world's UEGSoftCollisionSubsystem and picks
 * its drivers. It never ticks: the subsystem snapshots it once per frame and hands it a result
 * before movement ticks.
 *
 * ---------------------------------------------------------------------------
 * Who decides a push
 * ---------------------------------------------------------------------------
 * The machine that controls the pawn's movement. With the default drivers that is:
 *  - the owning client for a player's Character, which applies the push at once as a predicted
 *    root motion source and sends a request to the server;
 *  - the server for AI and unpossessed pawns, which applies velocity changes directly;
 *  - standalone and the listen-server host for their own pawn, which apply directly.
 * The server never scans a remote player's pawn. It validates the request and applies the same
 * push, starting at the client move time the request names, inside the RPC handler, which runs
 * before the move it belongs to.
 *
 * ---------------------------------------------------------------------------
 * One push at a time
 * ---------------------------------------------------------------------------
 * A new push replaces the active one. On the owning client the old push is removed at the moment
 * the new one starts, between two moves. The server removes it after processing the client move
 * that ends at the new push's start time, so both machines switch on the same move.
 *
 * ---------------------------------------------------------------------------
 * Events
 * ---------------------------------------------------------------------------
 * OnSoftPush fires for every push issued, OnBump for bumps. On the owning client they fire at once
 * (bPredicted). On the server they fire when a push is applied. Other clients get bumps only, as a
 * throttled cosmetic replay (bCosmetic), which is the hook for shove animations and voice lines.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGSoftCollisionComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UEGSoftCollisionSubsystem;

public:
	UEGSoftCollisionComponent();

	UFUNCTION(BlueprintPure, Category = "Extended|Soft Collision")
	static UEGSoftCollisionComponent* FindSoftCollisionComponent(const AActor* Actor);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -----------------------------------------------------------------
	// Settings
	// -----------------------------------------------------------------

	/** Empty uses the profile class defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soft Collision")
	TObjectPtr<UEGSoftCollisionProfile> Profile;

	/** Who may push whom. The pawn's IEGSoftCollisionInterface can override it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soft Collision")
	FEGSoftCollisionFilter Filter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soft Collision")
	bool bStartEnabled = true;

	/** Stop the push primitive from blocking SoftChannel at BeginPlay, and restore it at EndPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soft Collision|Collision")
	bool bConfigureCollision = true;

	/** The object channel soft pawns stop blocking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soft Collision|Collision")
	TEnumAsByte<ECollisionChannel> SoftChannel = ECC_Pawn;

	/**
	 * Response to SoftChannel while soft. Ignore is cheapest: Overlap makes the physics scene report
	 * every pawn pair, which costs a lot with hundreds of pawns.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soft Collision|Collision")
	TEnumAsByte<ECollisionResponse> SoftResponse = ECR_Ignore;

	// -----------------------------------------------------------------
	// Events
	// -----------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Extended|Soft Collision")
	FEGSoftPushSignature OnSoftPush;

	UPROPERTY(BlueprintAssignable, Category = "Extended|Soft Collision")
	FEGSoftPushSignature OnBump;

	// -----------------------------------------------------------------
	// Control
	// -----------------------------------------------------------------

	/** A disabled pawn neither pushes nor is pushed (dead, ragdolled, mounted). Call it on every machine. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Soft Collision")
	void SetSoftCollisionEnabled(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "Extended|Soft Collision")
	bool IsSoftCollisionEnabled() const { return bSoftCollisionEnabled; }

	/** Cancels the active push and re-picks the drivers. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Soft Collision")
	void SetProfile(UEGSoftCollisionProfile* NewProfile);

	/** Re-read the filter and profile override from the pawn's IEGSoftCollisionInterface. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Soft Collision")
	void RefreshFilter();

	UFUNCTION(BlueprintPure, Category = "Extended|Soft Collision")
	UEGSoftCollisionProfile* GetEffectiveProfile() const;

	UFUNCTION(BlueprintPure, Category = "Extended|Soft Collision")
	bool IsPushActive() const;

	/** The active push velocity, before decay. Zero when no push is active. */
	UFUNCTION(BlueprintPure, Category = "Extended|Soft Collision")
	FVector GetActivePushVelocity() const { return IsPushActive() ? Active.Command.Velocity : FVector::ZeroVector; }

	UFUNCTION(BlueprintPure, Category = "Extended|Soft Collision")
	UPrimitiveComponent* GetPushPrimitive() const { return PushPrimitive.Get(); }

	// -----------------------------------------------------------------
	// For drivers and the subsystem
	// -----------------------------------------------------------------

	APawn* GetPawn() const { return CachedPawn.Get(); }
	UCharacterMovementComponent* GetCharacterMovement() const { return CachedCharacterMovement.Get(); }
	UEGSoftCollisionDriver* GetActiveDriver() const;

	/** Game thread, once per snapshot. */
	void WriteSnapshot(FEGSoftBody& OutBody, FEGSoftCollisionSnapshotInfo& OutInfo);

	/** The profile tuning with this pawn's adjustments (avoidance). */
	void GetEffectiveTuning(FEGSoftCollisionTuning& OutTuning) const;

	/** Game thread, before movement ticks. */
	void ApplyScanResult(const FEGSoftPushResult& Result, const FEGSoftCollisionTuning& Tuning);

	/** Game thread, before movement ticks, while registered for frame updates. Returns whether it still needs them. */
	bool TickFrame(float DeltaSeconds);

protected:
	UFUNCTION(Server, Unreliable)
	void ServerSoftPush(const TArray<FEGSoftPushRequest>& Requests);

	UFUNCTION(Client, Reliable)
	void ClientRejectSoftPush(uint16 Sequence);

	UFUNCTION()
	void OnRep_LastAcceptedSequence();

	UFUNCTION()
	void OnRep_CosmeticBump();

	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	UFUNCTION()
	void HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	/** Highest sequence the server accepted. Owner only. */
	UPROPERTY(ReplicatedUsing = OnRep_LastAcceptedSequence)
	uint16 LastAcceptedSequence = 0;

	/** Bumps for everyone but the owner. */
	UPROPERTY(ReplicatedUsing = OnRep_CosmeticBump)
	FEGSoftBumpCosmetic CosmeticBump;

private:
	/** The push this component is running. */
	struct FActivePush
	{
		FEGSoftPushCommand Command;
		/** The scan's velocity before the pawn's interface reshaped it; the reissue rule compares against this. */
		FVector2D RawVelocity = FVector2D::ZeroVector;
		const UEGSoftCollisionDriver* Driver = nullptr;
		int32 Handle = INDEX_NONE;
		double StartTime = 0.0;
		double EndTime = 0.0;
		bool bActive = false;
	};

	enum class ERejectReason : uint8
	{
		None,
		NotPredicted,
		Disabled,
		BadBand,
		TooStrong,
		RateLimited,
		Stale,
		NoNeighbour,
		BumpCooldown,
		Late,
		DriverFailed,
		DebugReject
	};

	void ResolveDrivers();
	UEGSoftCollisionDriver* PickDefaultDriver(bool bForPlayer) const;
	void CachePushPrimitive();
	void ConfigureCollision();
	void RestoreCollision();
	bool ComputePlayerControlled() const;
	void UpdateControlState(bool bForce);
	EEGSoftCollisionDriverRole GetEffectiveRole() const;
	bool IsScanner() const;
	bool IsPredictingClient() const;

	FEGSoftPushCommand MakeCommand(const FEGSoftPushQuantized& Quantized, EEGSoftPushBand Band, const FEGSoftCollisionTuning& Tuning) const;
	void IssuePush(const FVector2D& Velocity, EEGSoftPushBand Band, const FEGSoftCollisionTuning& Tuning, int32 NeighbourCount);
	bool StartPush(const FEGSoftPushCommand& Command, bool bCancelPrevious);
	void CancelActivePush();
	void BroadcastPush(const FEGSoftPushCommand& Command, int32 NeighbourCount, bool bPredicted, bool bCosmetic);
	void SendCosmeticBump(const FEGSoftPushQuantized& Quantized);
	void UpdateFrameUpdateRegistration();

	ERejectReason ValidateRequest(const FEGSoftPushRequest& Request, bool& bOutEpochMismatch);
	void ProcessServerRequest(const FEGSoftPushRequest& Request);
	void RejectRequest(uint16 Sequence, ERejectReason Reason);
	void UpdateMovementBinding();
	void ResetPredictionState();

	double GetGameTime() const;
	double GetRealTime() const;
	float GetRoundTripTime() const;
	UEGSoftCollisionSubsystem* GetSubsystem() const;
	void DebugDrawPush(const FEGSoftPushCommand& Command, bool bPredicted) const;

	UPROPERTY(Transient)
	TObjectPtr<UEGSoftCollisionProfile> ResolvedProfile;

	UPROPERTY(Transient)
	TObjectPtr<UEGSoftCollisionDriver> PlayerDriver;

	UPROPERTY(Transient)
	TObjectPtr<UEGSoftCollisionDriver> AIDriver;

	TWeakObjectPtr<APawn> CachedPawn;
	TWeakObjectPtr<UCharacterMovementComponent> CachedCharacterMovement;
	TWeakObjectPtr<UPrimitiveComponent> PushPrimitive;

	FEGSoftCollisionFilter EffectiveFilter;
	TEnumAsByte<ECollisionResponse> SavedResponse = ECR_Block;
	bool bCollisionConfigured = false;
	bool bSoftCollisionEnabled = true;
	bool bPlayerControlled = false;
	bool bControlStateValid = false;
	bool bFrameUpdatesRegistered = false;
	bool bMovementBound = false;

	int32 RegistryIndex = INDEX_NONE;

	FActivePush Active;
	uint16 NextSequence = 0;
	double LastBumpTime = -1.0e9;
	double LastCosmeticBumpTime = -1.0e9;
	double BeginPlayTime = 0.0;
	EGSoftCollision::FTokenBucket IssueBucket;

	FEGSoftPushClientQueue ClientQueue;
	TArray<FEGSoftPushRequest> SendScratch;

	FEGSoftPushServerState ServerState;
	FEGSoftCollisionSafetyLease SafetyLease;
};
