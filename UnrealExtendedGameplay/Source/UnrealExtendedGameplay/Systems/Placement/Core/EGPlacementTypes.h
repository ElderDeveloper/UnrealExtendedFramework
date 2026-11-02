// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/SoftObjectPtr.h"

#include "EGPlacementTypes.generated.h"

class AActor;
class UEGPlacementAnchorComponent;
class UEGPlacementRegionComponent;
class UEGPlacementSpatialSubsystem;
class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
class USkeletalMesh;
class UStaticMesh;
class UWorld;

// ---------------------------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------------------------

/** Which way a surface faces, from its normal and the profile's surface bands. Finer kinds of surface are tags. */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EEGPlacementOrientation : uint8
{
	None = 0 UMETA(Hidden),
	Floor = 1 << 0,
	Wall = 1 << 1,
	Ceiling = 1 << 2,
};
ENUM_CLASS_FLAGS(EEGPlacementOrientation)

/** Valid: allowed here. Invalid: refused, with reasons. Pending: data needed for an answer is not available yet. */
UENUM(BlueprintType)
enum class EEGPlacementStatus : uint8
{
	Valid,
	Invalid,
	Pending,
};

/** Who is asking. Preview runs on the requesting machine every frame; Authority decides a commit. */
UENUM(BlueprintType)
enum class EEGPlacementScope : uint8
{
	Preview,
	Authority,
};

/** The scopes a rule runs in. */
UENUM(BlueprintType)
enum class EEGPlacementRuleScope : uint8
{
	Both,
	PreviewOnly,
	AuthorityOnly,
};

UENUM(BlueprintType)
enum class EEGPlacementRuleResult : uint8
{
	Pass,
	Fail,
	Pending,
};

/** Which up the object takes: the surface normal, or world up (gravity) for floors and ceilings, a level normal for walls. */
UENUM(BlueprintType)
enum class EEGPlacementAlignment : uint8
{
	Surface,
	WorldUp,
};

/** The stable frame a grid and a spin are measured in. Never the aim. */
UENUM(BlueprintType)
enum class EEGPlacementGridFrame : uint8
{
	World,
	/** The support component's own frame, so a grid on a moving platform moves with it. */
	Support,
};

UENUM(BlueprintType)
enum class EEGPlacementTargetKind : uint8
{
	None,
	Component,
	Hit,
	Anchor,
};

/** Which claims disagreed with what the authority measured. */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EEGPlacementClaimMismatch : uint8
{
	None = 0 UMETA(Hidden),
	Orientation = 1 << 0,
	Support = 1 << 1,
	Pose = 1 << 2,
	Anchor = 1 << 3,
};
ENUM_CLASS_FLAGS(EEGPlacementClaimMismatch)

// ---------------------------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------------------------

/** An authored box in pivot space. Rotation is relative to the pivot. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementBox
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FRotator Rotation = FRotator::ZeroRotator;

	/** Half size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0.0"))
	FVector Extent = FVector(10.0);

	FEGPlacementBox() = default;
	FEGPlacementBox(const FVector& InCenter, const FVector& InExtent, const FRotator& InRotation = FRotator::ZeroRotator)
		: Center(InCenter), Rotation(InRotation), Extent(InExtent) {}
};

/** A box in world space, the form every test works on. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementOrientedBox
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FQuat Rotation = FQuat::Identity;

	/** Half size along the box's own axes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FVector Extent = FVector::ZeroVector;

	FEGPlacementOrientedBox() = default;
	FEGPlacementOrientedBox(const FVector& InCenter, const FQuat& InRotation, const FVector& InExtent)
		: Center(InCenter), Rotation(InRotation), Extent(InExtent) {}

	FVector GetAxis(int32 Index) const;
	void GetCorners(FVector OutCorners[8]) const;
	FBox GetBounds() const;
	FTransform GetTransform() const { return FTransform(Rotation, Center); }
};

/**
 * What the object occupies, in pivot space. Footprint boxes rest on the support; clearance boxes
 * must stay free. An empty clearance list uses the footprint. Shapes come from data, never from a
 * preview mesh's current bounds.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementShape
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TArray<FEGPlacementBox> Footprint;

	/** Empty uses Footprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TArray<FEGPlacementBox> Clearance;

	const TArray<FEGPlacementBox>& GetClearance() const { return Clearance.Num() > 0 ? Clearance : Footprint; }
	bool IsEmpty() const { return Footprint.Num() == 0; }
	/** Bounds of every footprint and clearance box in pivot space. */
	FBox GetLocalBounds() const;
};

// ---------------------------------------------------------------------------------------------
// Identity and spec
// ---------------------------------------------------------------------------------------------

/**
 * The project's identity for something, held unchanged: a SQL id, a GUID, a row name. The plugin
 * never generates, parses or converts it. Namespace keeps ids from different tables apart.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementHandle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FName Namespace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FString Value;

	FEGPlacementHandle() = default;
	FEGPlacementHandle(FName InNamespace, FString InValue) : Namespace(InNamespace), Value(MoveTemp(InValue)) {}

	bool IsValid() const { return !Value.IsEmpty(); }
	FString ToString() const { return Namespace.IsNone() ? Value : FString::Printf(TEXT("%s:%s"), *Namespace.ToString(), *Value); }

	bool operator==(const FEGPlacementHandle& Other) const { return Namespace == Other.Namespace && Value == Other.Value; }
	bool operator!=(const FEGPlacementHandle& Other) const { return !(*this == Other); }
	friend uint32 GetTypeHash(const FEGPlacementHandle& Handle) { return HashCombine(GetTypeHash(Handle.Namespace), GetTypeHash(Handle.Value)); }
};

/** How the object turns. Spin is about the object's up, measured from the stable grid frame. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementRotationPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	EEGPlacementAlignment Alignment = EEGPlacementAlignment::Surface;

	/** Spin snapping step in degrees. 0 turns snapping off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float SpinStepDegrees = 15.0f;

	/** Whether free rotation (the caller's bFreeRotation) may skip the spin step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	bool bAllowFreeSpin = true;

	/** Always use FixedSpinDegrees, ignoring the aim and any requested spin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	bool bUseFixedSpin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (EditCondition = "bUseFixedSpin"))
	float FixedSpinDegrees = 0.0f;
};

/** Position snapping in a stable frame. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementGrid
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	bool bEnabled = false;

	/** Cell size along the grid frame's axes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (EditCondition = "bEnabled", ClampMin = "0.1"))
	FVector Step = FVector(50.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (EditCondition = "bEnabled"))
	EEGPlacementGridFrame Frame = EEGPlacementGridFrame::World;

	/** Grid origin in the grid frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (EditCondition = "bEnabled"))
	FVector Origin = FVector::ZeroVector;
};

/**
 * What the object is, geometrically. A project builds it from whatever it stores: a DataTable
 * row, a data asset, a database row or native tuning. Clients may send handles but never specs:
 * the host resolves the spec from its own data.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FEGPlacementShape Shape;

	/** Distance from the pivot to the contact point along the object's up. A crate pivoted at its centre uses half its height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	float ContactOffset = 0.0f;

	/** Surfaces the object may rest on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (Bitmask, BitmaskEnum = "/Script/UnrealExtendedGameplay.EEGPlacementOrientation"))
	int32 AllowedOrientations = static_cast<int32>(EEGPlacementOrientation::Floor);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (InlineEditConditionToggle))
	bool bOverrideMaxSlope = false;

	/** Used instead of the profile's DefaultMaxSlopeDegrees when set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (EditCondition = "bOverrideMaxSlope", ClampMin = "0.0", ClampMax = "90.0"))
	float MaxSlopeDegrees = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FEGPlacementRotationPolicy Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (InlineEditConditionToggle))
	bool bOverrideGrid = false;

	/** Used instead of the profile's grid when set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (EditCondition = "bOverrideGrid"))
	FEGPlacementGrid Grid;

	/** For rules and anchors to match: "needs a timber support", "hangs on hooks". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FGameplayTagContainer Tags;

	/** Per-item data for the project's own rules. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FInstancedStruct ProjectData;

	/** The project's identity for the item. Opaque to the plugin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FEGPlacementHandle Source;

	bool AllowsOrientation(EEGPlacementOrientation Orientation) const
	{
		return Orientation != EEGPlacementOrientation::None && (AllowedOrientations & static_cast<int32>(Orientation)) != 0;
	}
};

// ---------------------------------------------------------------------------------------------
// Profile settings
// ---------------------------------------------------------------------------------------------

/** What the solvers and rules trace against. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementQuery
{
	GENERATED_BODY()

	/** Used when CollisionProfile is None. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TEnumAsByte<ECollisionChannel> Channel = ECC_Visibility;

	/** Traces by profile instead of by channel when set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FName CollisionProfile = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	bool bTraceComplex = false;

	/** How far solvers probe along the normal on either side of the proposed pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "1.0"))
	float ProbeDistance = 50.0f;

	/** How far GroundConform probes above the contact point before looking down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0.0"))
	float ProbeLift = 40.0f;
};

/** Normal thresholds that separate floors, walls and ceilings (Z of the unit normal). */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementSurfaceBands
{
	GENERATED_BODY()

	/** At or above: floor. 0.7 is about 45 degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float FloorMinNormalZ = 0.7f;

	/** At or below: ceiling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float CeilingMaxNormalZ = -0.7f;
};

// ---------------------------------------------------------------------------------------------
// Per-call inputs
// ---------------------------------------------------------------------------------------------

/** A revision something reported, so a later answer can tell whether it changed. 0 means untracked. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementRevision
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TWeakObjectPtr<UObject> Source;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	int32 Revision = 0;

	bool operator==(const FEGPlacementRevision& Other) const { return Source == Other.Source && Revision == Other.Revision; }
};

/** A specific thing the caller aims at. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementTarget
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	EEGPlacementTargetKind Kind = EEGPlacementTargetKind::None;

	/** Kind Component: solvers trace only this component. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TWeakObjectPtr<USceneComponent> Component;

	/** Kind Hit: the aimed hit. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FHitResult Hit;

	/** Kind Anchor: the chosen anchor. The authority still validates it. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TWeakObjectPtr<UEGPlacementAnchorComponent> Anchor;
};

/** What the caller believes is true. The authority measures again and compares. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementClaims
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	bool bHasClaims = false;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	EEGPlacementOrientation Orientation = EEGPlacementOrientation::None;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FEGPlacementHandle Support;

	/** The anchor the caller's answer used, if any. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TWeakObjectPtr<UEGPlacementAnchorComponent> Anchor;

	/** The revisions the caller's answer depended on. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TArray<FEGPlacementRevision> Revisions;
};

/**
 * What the caller asks for. Pose is always a pivot pose: aim providers put the pivot ContactOffset
 * above the aimed point with the object's up along the surface normal; a solved pose fed back in
 * solves to itself.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementIntent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FTransform Pose = FTransform::Identity;

	/** The surface normal the caller saw. Without it solvers use the pose's up. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	bool bHasSurfaceHint = false;

	UPROPERTY(BlueprintReadWrite, Category = "Placement", meta = (EditCondition = "bHasSurfaceHint"))
	FVector SurfaceNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FEGPlacementTarget Target;

	/** Skip position snapping. Independent from bFreeRotation. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	bool bFreePosition = false;

	/** Skip spin snapping, when the spec allows it. Independent from bFreePosition. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	bool bFreeRotation = false;

	/** Extra spin about the object's up, added before snapping. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	float SpinDegrees = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FEGPlacementClaims Claims;
};

/** The world and who is asking. Built by the caller; the evaluator never looks up a local player. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TObjectPtr<UWorld> World = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TObjectPtr<AActor> Requester = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TArray<TObjectPtr<AActor>> IgnoredActors;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TArray<TObjectPtr<UPrimitiveComponent>> IgnoredComponents;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	EEGPlacementScope Scope = EEGPlacementScope::Preview;

	/** Null uses the world's subsystem. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TObjectPtr<UEGPlacementSpatialSubsystem> Spatial = nullptr;

	/** Where the requester looks from, for reach and line of sight. Unset uses the requester's eyes. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	bool bHasViewLocation = false;

	UPROPERTY(BlueprintReadWrite, Category = "Placement", meta = (EditCondition = "bHasViewLocation"))
	FVector ViewLocation = FVector::ZeroVector;

	/** Data for the project's own rules, for example a snapshot of the requester's inventory. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FInstancedStruct ContextData;

	/** The requester's view location, or its eyes. */
	FVector GetViewLocation() const;
};

// ---------------------------------------------------------------------------------------------
// Working state and results
// ---------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementReason
{
	GENERATED_BODY()

	/** Under Extended.Gameplay.Placement.Reason, or a project's own root. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FGameplayTag Tag;

	/** A developer-facing explanation. Projects map tags to their own player-facing text. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FText Text;

	/** The rule or solver that raised it. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TWeakObjectPtr<UObject> Raiser;

	FEGPlacementReason() = default;
	FEGPlacementReason(FGameplayTag InTag, FText InText, const UObject* InRaiser = nullptr)
		: Tag(InTag), Text(MoveTemp(InText)), Raiser(const_cast<UObject*>(InRaiser)) {}
};

/** What the object rests on. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementSupport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TWeakObjectPtr<UPrimitiveComponent> Component;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TWeakObjectPtr<AActor> Actor;

	/** From IEGPlacementSupportInterface, or the component's path. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FEGPlacementHandle Handle;

	bool IsSet() const { return Component.IsValid() || Actor.IsValid(); }
};

/** What solvers fill in and rules read. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FTransform Pose = FTransform::Identity;

	/** Whether the solver found a surface; SurfaceHit is meaningful only then. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	bool bHasSurface = false;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FHitResult SurfaceHit;

	/** The measured surface normal. Solvers without a surface leave the pose's up. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FVector Normal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	EEGPlacementOrientation Orientation = EEGPlacementOrientation::None;

	/** Degrees between the normal and the ideal for its orientation (straight up, down or level). */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	float SlopeDegrees = 0.0f;

	/** Solvers may set Support.Component; the evaluator fills the rest. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FEGPlacementSupport Support;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TWeakObjectPtr<UEGPlacementAnchorComponent> Anchor;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TArray<FEGPlacementOrientedBox> FootprintBoxes;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TArray<FEGPlacementOrientedBox> ClearanceBoxes;

	/** Regions overlapping the footprint or clearance bounds. Native only: Blueprint has no weak-pointer arrays. */
	UPROPERTY()
	TArray<TWeakObjectPtr<UEGPlacementRegionComponent>> Regions;

	/** Whether the area's regions are known to be loaded. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	bool bAreaAvailable = true;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TArray<FEGPlacementRevision> Revisions;

	/** The solver's verdict. Rules run only on Valid. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	EEGPlacementStatus SolveStatus = EEGPlacementStatus::Valid;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TArray<FEGPlacementReason> SolveReasons;

	/** Uniform scale taken from the intent. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	float Scale = 1.0f;

	FVector GetUp() const { return Pose.GetRotation().GetUpVector(); }
	FVector GetContactPoint(float ContactOffset) const { return Pose.GetLocation() - GetUp() * ContactOffset * Scale; }
};

/** What callers get back. Pose is always set, also when invalid, so a preview shows a red ghost where the player aims. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	EEGPlacementStatus Status = EEGPlacementStatus::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FTransform Pose = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	EEGPlacementOrientation Orientation = EEGPlacementOrientation::None;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FEGPlacementSupport Support;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TWeakObjectPtr<UEGPlacementAnchorComponent> Anchor;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TArray<FEGPlacementReason> Reasons;

	/** EEGPlacementClaimMismatch bits. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement", meta = (Bitmask, BitmaskEnum = "/Script/UnrealExtendedGameplay.EEGPlacementClaimMismatch"))
	int32 ClaimMismatches = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TArray<FEGPlacementRevision> Revisions;

	/** A rule answered by sampling rather than exactly (regions that do not share a plane). */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	bool bApproximate = false;

	/** The full working state, for debug drawing and project code that needs more. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FEGPlacementState State;

	bool IsValid() const { return Status == EEGPlacementStatus::Valid; }
	bool HasReason(const FGameplayTag& Tag) const;
	FString DescribeReasons() const;
};

// ---------------------------------------------------------------------------------------------
// Visuals
// ---------------------------------------------------------------------------------------------

/** One mesh of a preview: static or skeletal. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementVisualMesh
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	/** Used when StaticMesh is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/** Relative to the pivot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FTransform RelativeTransform = FTransform::Identity;

	/** Per-slot overrides; empty slots keep the mesh's own material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;
};

/** What a preview draws. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementVisual
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TArray<FEGPlacementVisualMesh> Meshes;

	bool IsEmpty() const { return Meshes.Num() == 0; }
	void GetSoftPaths(TArray<FSoftObjectPath>& OutPaths) const;
};
