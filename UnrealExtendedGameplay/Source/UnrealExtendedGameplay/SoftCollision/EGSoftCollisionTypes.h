// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"

#include "EGSoftCollisionTypes.generated.h"

/**
 * Soft Collision
 *
 * Pawns stop blocking each other. They overlap slightly and push themselves apart, smoothly and
 * without network corrections. Docs/SoftCollisionPlan.md is the design; the notes in these headers
 * explain the parts a reader would get wrong without it.
 *
 *  - A pawn only ever pushes itself. Every push is decided by the machine that already owns that
 *    pawn's movement, which is what makes a player's push predictable on their own client.
 *  - Shoving someone out of the way comes from the momentum share: a runner hitting an idle pawn
 *    makes the idle pawn push itself away hard, while the runner is barely deflected.
 *  - One UEGSoftCollisionComponent per pawn, one UEGSoftCollisionSubsystem per world. Components
 *    never tick; the subsystem snapshots every pawn once per frame, finds neighbours on a grid,
 *    computes pushes on worker threads, and hands each pawn its result before movement ticks.
 *  - How a push moves a pawn is a driver's job (UEGSoftCollisionDriver): a predicted root motion
 *    source for player Characters, velocity changes for everything the server alone simulates.
 */

DECLARE_LOG_CATEGORY_EXTERN(LogEGSoftCollision, Log, All);
DECLARE_STATS_GROUP(TEXT("Extended Soft Collision"), STATGROUP_EGSoftCollision, STATCAT_Advanced);

/** The shape a pawn pushes with. Heights are always vertical; pushes are always horizontal. */
UENUM(BlueprintType)
enum class EEGSoftCollisionShapeType : uint8
{
	/** Vertical cylinder. Capsules and spheres use this. */
	Cylinder,
	/** Box rotated by the primitive's yaw. */
	Box
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGSoftCollisionShape
{
	GENERATED_BODY()

	/** Read the shape from the push primitive (capsule, sphere or box) every frame, so crouching and scaling follow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soft Collision")
	bool bFromPrimitive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soft Collision", meta = (EditCondition = "!bFromPrimitive"))
	EEGSoftCollisionShapeType Type = EEGSoftCollisionShapeType::Cylinder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soft Collision", meta = (EditCondition = "!bFromPrimitive", ClampMin = "1.0"))
	float Radius = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soft Collision", meta = (EditCondition = "!bFromPrimitive", ClampMin = "1.0"))
	float HalfHeight = 88.0f;

	/** Box half extents along the primitive's forward (X) and right (Y). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soft Collision", meta = (EditCondition = "!bFromPrimitive"))
	FVector2D BoxHalfExtent = FVector2D(40.0f, 40.0f);
};

/** How the push grows with penetration (0 when touching, 1 when the centres meet). */
UENUM(BlueprintType)
enum class EEGSoftPushResponse : uint8
{
	Linear,
	Quadratic,
	SmoothStep
};

/** How a single push fades over its duration. */
UENUM(BlueprintType)
enum class EEGSoftPushDecay : uint8
{
	Constant,
	Linear,
	EaseOut
};

UENUM(BlueprintType)
enum class EEGSoftPushBand : uint8
{
	None,
	/** Gentle and continuous while pawns overlap. */
	Separation,
	/** A fast approach: stronger, and fires the bump events. */
	Bump
};

/**
 * How hard the server tries to keep a predicting client free of corrections while it is pushed.
 * The flags it touches belong to the CharacterMovementComponent and are restored afterwards.
 */
UENUM(BlueprintType)
enum class EEGSoftCollisionSafetyLevel : uint8
{
	/** Level 0. Move-anchored prediction keys only. */
	KeysOnly,
	/**
	 * Level 1. While a push is active the server adopts the client's position whenever it is within
	 * the normal error tolerance (bServerAcceptClientAuthoritativePosition). No new way to cheat.
	 */
	AcceptClientPosition,
	/**
	 * Level 2. Also skips the error check during the push window
	 * (bIgnoreClientMovementErrorChecksAndCorrection). The server checks each move for impossible
	 * displacement itself and closes the window when it finds one. Reasonable for co-op.
	 */
	ClientAuthoritativeWindow
};

/** What the server does with a request whose first copy was lost and that arrives after its move. */
UENUM(BlueprintType)
enum class EEGSoftCollisionLatePolicy : uint8
{
	/** Apply the rest of the push so it ends at the same client time as on the client. */
	TrimElapsed,
	/** Reject it. The client removes its push and the movement correction does the rest. */
	Reject
};

/** Where a driver runs. */
UENUM(BlueprintType)
enum class EEGSoftCollisionDriverRole : uint8
{
	/** The controlling machine applies it at once; the server applies the same push from a request. */
	Predicted,
	/** Only the server scans and applies. Right for pawns the server alone simulates. */
	ServerAuthority,
	/** The machine that controls the pawn scans and applies, with no request. Client-driven pawns. */
	OwnerAuthority
};

/**
 * Who may push whom. Resolved once on the game thread (registration, RefreshFilter) and copied
 * into the frame snapshot, so worker threads never call into game code.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGSoftCollisionFilter
{
	GENERATED_BODY()

	/** This pawn's layer, 0 to 31. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soft Collision", meta = (ClampMin = "0", ClampMax = "31"))
	int32 Layer = 0;

	/** Layers allowed to push this pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soft Collision", meta = (Bitmask))
	int32 PushedByLayers = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soft Collision")
	bool bCanPush = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soft Collision")
	bool bCanBePushed = true;
};

/**
 * A temporary change to how a pawn takes part, pushed by gameplay or an anim notify and removed by
 * handle (UEGSoftCollisionComponent::PushModifier). Several combine: scales multiply, switches add up,
 * so an attack's immovable window and a heavy-armour weight both hold, and removing one leaves the
 * other. Push it on every machine that simulates the pawn; montage notifies already run on the
 * server and the owning client.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGSoftCollisionModifier
{
	GENERATED_BODY()

	/** Multiplies the profile's weight. Heavier pawns take a smaller share of every push. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soft Collision", meta = (ClampMin = "0.01"))
	float WeightScale = 1.0f;

	/** Multiplies the profile's push power. 0 pushes nobody. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soft Collision", meta = (ClampMin = "0.0"))
	float PushPowerScale = 1.0f;

	/** Nobody can push this pawn while the modifier is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soft Collision")
	bool bImmovable = false;

	/** This pawn pushes nobody while the modifier is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soft Collision")
	bool bPushesNobody = false;
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGSoftCollisionModifierHandle
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	bool IsValid() const { return Id != 0; }
	void Invalidate() { Id = 0; }
};

/** Active modifiers of one pawn. Free of actors so it can be tested on its own. */
struct UNREALEXTENDEDGAMEPLAY_API FEGSoftCollisionModifierStack
{
	/** Returns the new modifier's id, never 0. */
	int32 Push(const FEGSoftCollisionModifier& Modifier);
	bool Remove(int32 Id);
	bool IsEmpty() const { return Modifiers.IsEmpty(); }
	int32 Num() const { return Modifiers.Num(); }
	FEGSoftCollisionModifier GetCombined() const;
	void Reset() { Modifiers.Reset(); }

private:
	TArray<TPair<int32, FEGSoftCollisionModifier>> Modifiers;
	int32 NextId = 0;
};

/**
 * Strength, bands and the reissue rule. Plain numbers, copied per scanning pawn into the frame
 * snapshot, so the math runs on worker threads and in tests without any UObject.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGSoftCollisionTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separation")
	EEGSoftPushResponse Response = EEGSoftPushResponse::SmoothStep;

	/** Push speed in cm/s at full penetration with an even share. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separation", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float SeparationStrength = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separation", meta = (ClampMin = "0.02", ForceUnits = "s"))
	float SeparationDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separation")
	EEGSoftPushDecay SeparationDecay = EEGSoftPushDecay::Constant;

	/** A pawn slower than this still counts as moving this fast in the momentum share, so idle pawns can push too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separation", meta = (ClampMin = "1.0", ForceUnits = "cm/s"))
	float IdleFloorSpeed = 40.0f;

	/** Penetration (0..1) past which the push ramps steeply, so pawns never fully merge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separation", meta = (ClampMin = "0.0", ClampMax = "0.99"))
	float DeepPenetration = 0.6f;

	/** Push multiplier reached when the centres meet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Separation", meta = (ClampMin = "1.0"))
	float DeepPenetrationMultiplier = 3.0f;

	/** Relative approach speed needed for a bump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bump", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float BumpMinRelativeSpeed = 250.0f;

	/** Momentum share needed for a bump: only the pawn being run into is bumped, not the runner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bump", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BumpMinShare = 0.6f;

	/** Relative speed at which the bump reaches BumpStrength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bump", meta = (ClampMin = "1.0", ForceUnits = "cm/s"))
	float BumpReferenceSpeed = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bump", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float BumpStrength = 380.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bump", meta = (ClampMin = "0.02", ForceUnits = "s"))
	float BumpDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bump")
	EEGSoftPushDecay BumpDecay = EEGSoftPushDecay::EaseOut;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bump", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float BumpCooldown = 0.6f;

	/** Cap on the summed push. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MaxTotalStrength = 650.0f;

	/** Pushes weaker than this are not issued. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MinPushStrength = 8.0f;

	/** A new push replaces the active one only when its direction turned more than this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reissue", meta = (ClampMin = "0.0", ClampMax = "180.0", ForceUnits = "deg"))
	float ReissueAngle = 12.0f;

	/** ...or its strength changed by more than this fraction... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reissue", meta = (ClampMin = "0.0"))
	float ReissueStrengthRatio = 0.2f;

	/** ...or the active push ends within this time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reissue", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ReissueLeadTime = 0.035f;
};

/** How often pawns scan. Only the machine that decides a pawn's pushes scans it. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGSoftCollisionScanSettings
{
	GENERATED_BODY()

	/** Seconds between scans for player-controlled pawns. 0 scans every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float PlayerScanInterval = 0.0f;

	/** Seconds between scans for server-controlled pawns near a player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float NearScanInterval = 0.0f;

	/** Seconds between scans for server-controlled pawns far from every player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float FarScanInterval = 0.2f;

	/** Distance to the nearest player inside which a server-controlled pawn counts as near. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float NearDistance = 2500.0f;

	/** Distance to the nearest player beyond which a server-controlled pawn does not scan. 0 never culls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float CullDistance = 10000.0f;

	/** Below this horizontal speed, with nothing in contact, a pawn sleeps until a moving pawn comes near. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float SleepSpeed = 5.0f;
};

/** Requests, validation and the safety net. Used only for predicted pushes. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGSoftCollisionNetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	EEGSoftCollisionSafetyLevel SafetyLevel = EEGSoftCollisionSafetyLevel::AcceptClientPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	EEGSoftCollisionLatePolicy LatePolicy = EEGSoftCollisionLatePolicy::TrimElapsed;

	/** Token bucket refill rate for push requests. Also enforced by the server. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network", meta = (ClampMin = "1.0"))
	float MaxRequestsPerSecond = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network", meta = (ClampMin = "1.0"))
	float RequestBurst = 4.0f;

	/** Unacknowledged requests are repeated inside batches for this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RedundancyWindow = 0.15f;

	/** Minimum time between repeats of the same request. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ResendInterval = 0.05f;

	/** A push neither acknowledged nor rejected within the round-trip time plus this is removed by the client. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float UnansweredTimeout = 0.25f;

	/** Requests whose client timestamp is further than this from the server's are stale (or from another timestamp epoch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float MaxRequestAge = 1.0f;

	/** Extra reach the server allows when checking that a pushing pawn really had a neighbour. Covers latency. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ValidationSlack = 150.0f;

	/** Extra client time the safety flags stay on after a push ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float SafetyGrace = 0.1f;

	/** Level 2: displacement beyond the server's own simulation that the server tolerates per move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float AuthoritativeWindowTolerance = 10.0f;

	/** Minimum time between cosmetic bumps sent to other machines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float CosmeticBumpInterval = 0.25f;
};

/** Quantized push. Both machines build their push from these exact integers. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGSoftPushQuantized
{
	GENERATED_BODY()

	/** Direction, 65536 steps per turn. */
	UPROPERTY()
	uint16 Yaw = 0;

	/** Speed in steps of 0.1 cm/s. */
	UPROPERTY()
	uint16 Strength = 0;

	bool operator==(const FEGSoftPushQuantized& Other) const { return Yaw == Other.Yaw && Strength == Other.Strength; }
	bool operator!=(const FEGSoftPushQuantized& Other) const { return !(*this == Other); }
};

/**
 * One push request, client to server. About 11 bytes.
 * Sequence identifies the push. ClientTimeStamp is the CharacterMovementComponent client move
 * timestamp the push starts after: the same clock the engine uses for root motion start times,
 * so it names the move the push starts on. Duration and decay come from the profile and are
 * never sent.
 */
USTRUCT()
struct UNREALEXTENDEDGAMEPLAY_API FEGSoftPushRequest
{
	GENERATED_BODY()

	UPROPERTY()
	uint16 Sequence = 0;

	UPROPERTY()
	float ClientTimeStamp = 0.0f;

	UPROPERTY()
	FEGSoftPushQuantized Push;

	UPROPERTY()
	EEGSoftPushBand Band = EEGSoftPushBand::None;
};

/** Cosmetic bump for machines that did not compute it. Replicated to everyone but the owner. */
USTRUCT()
struct UNREALEXTENDEDGAMEPLAY_API FEGSoftBumpCosmetic
{
	GENERATED_BODY()

	/** Changes on every bump, so the rep notify fires. */
	UPROPERTY()
	uint8 Counter = 0;

	UPROPERTY()
	FEGSoftPushQuantized Push;
};

/** Everything a driver needs to apply one push. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGSoftPushCommand
{
	GENERATED_BODY()

	/** De-quantized push velocity, horizontal, cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Soft Collision")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Soft Collision")
	float Duration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Soft Collision")
	EEGSoftPushDecay Decay = EEGSoftPushDecay::Constant;

	UPROPERTY(BlueprintReadOnly, Category = "Soft Collision")
	EEGSoftPushBand Band = EEGSoftPushBand::None;

	UPROPERTY()
	FEGSoftPushQuantized Quantized;

	UPROPERTY()
	uint16 Sequence = 0;

	/** Set for predicted pushes: client move time the push starts after. */
	UPROPERTY()
	bool bHasStartTime = false;

	UPROPERTY()
	float StartTime = 0.0f;

	/** Time already missed by a late server copy. */
	UPROPERTY()
	float ElapsedTrim = 0.0f;
};

/** Blueprint view of a push. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGSoftPushEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Soft Collision")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Soft Collision")
	float Duration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Soft Collision")
	EEGSoftPushBand Band = EEGSoftPushBand::None;

	UPROPERTY(BlueprintReadOnly, Category = "Soft Collision")
	int32 NeighbourCount = 0;

	/** Fired on the owning client ahead of the server. */
	UPROPERTY(BlueprintReadOnly, Category = "Soft Collision")
	bool bPredicted = false;

	/** Received as a cosmetic replay on a machine that did not compute it. */
	UPROPERTY(BlueprintReadOnly, Category = "Soft Collision")
	bool bCosmetic = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGSoftPushSignature, const FEGSoftPushEvent&, Push);

/**
 * One pawn in the frame snapshot. Plain data: worker threads and tests read only this.
 * Location is the shape's centre.
 */
struct FEGSoftBody
{
	FVector Location = FVector::ZeroVector;
	FVector2D Velocity = FVector2D::ZeroVector;
	/** Box axes: forward (cos, sin) of the yaw. Right is (-sin, cos). */
	FVector2D Axis = FVector2D(1.0f, 0.0f);
	FVector2D BoxHalfExtent = FVector2D::ZeroVector;
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	float Weight = 1.0f;
	float PushPower = 1.0f;
	uint32 PushedByMask = 0xFFFFFFFFu;
	int32 Id = INDEX_NONE;
	EEGSoftCollisionShapeType Shape = EEGSoftCollisionShapeType::Cylinder;
	uint8 Layer = 0;
	bool bEnabled = true;
	bool bCanPush = true;
	bool bCanBePushed = true;

	/** Largest horizontal distance from the centre to the shape's edge. */
	float GetMaxExtent() const
	{
		return Shape == EEGSoftCollisionShapeType::Box ? BoxHalfExtent.Size() : Radius;
	}
};

/** What a scan found for one pawn. */
struct FEGSoftPushResult
{
	/** Summed push velocity, horizontal, cm/s. */
	FVector2D Velocity = FVector2D::ZeroVector;
	/** The separation part alone, capped. Used when a bump is still cooling down. */
	FVector2D SeparationVelocity = FVector2D::ZeroVector;
	EEGSoftPushBand Band = EEGSoftPushBand::None;
	int32 NeighbourCount = 0;
	int32 NeighbourTests = 0;
	float MaxPenetration = 0.0f;
};
