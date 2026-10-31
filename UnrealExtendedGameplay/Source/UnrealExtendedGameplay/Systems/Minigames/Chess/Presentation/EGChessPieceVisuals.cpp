// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessPieceVisuals.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EGChessEngineAssets.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessPosition.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessAnimationSet.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessPieceSet.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessTableStyle.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Player/EGChessCharacterInterface.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessTableActor.h"

namespace EGChessPieceVisualsPrivate
{
	// A clock button's press, as an offset in its parent's (the clock socket's) space. The travel is
	// in world units along the button's own down axis, so a socket imported with a scale (Blender
	// empties come in at 100) or a rotation cannot turn a 0.8 cm press into an 80 cm sideways jump.
	FVector PressOffset(const USceneComponent* Button, float Travel)
	{
		if (!Button || FMath::IsNearlyZero(Travel))
		{
			return FVector::ZeroVector;
		}
		const FVector WorldDelta = -Button->GetUpVector() * Travel;
		const USceneComponent* Parent = Button->GetAttachParent();
		return Parent ? Parent->GetSocketTransform(Button->GetAttachSocketName()).InverseTransformVector(WorldDelta) : WorldDelta;
	}

	/** Basic-shape stand-ins: mesh and size (as a fraction of a square) per type. */
	struct FFallbackShape
	{
		UStaticMesh* Mesh = nullptr;
		FVector Size = FVector(0.5f);
	};

	FFallbackShape FallbackFor(EEGChessPieceType Type)
	{
		FFallbackShape Shape;
		switch (Type)
		{
		case EEGChessPieceType::Pawn: Shape.Mesh = EGChessEngineAssets::Sphere(); Shape.Size = FVector(0.45f, 0.45f, 0.5f); break;
		case EEGChessPieceType::Knight: Shape.Mesh = EGChessEngineAssets::Cone(); Shape.Size = FVector(0.5f, 0.5f, 0.8f); break;
		case EEGChessPieceType::Bishop: Shape.Mesh = EGChessEngineAssets::Cone(); Shape.Size = FVector(0.45f, 0.45f, 1.0f); break;
		case EEGChessPieceType::Rook: Shape.Mesh = EGChessEngineAssets::Cube(); Shape.Size = FVector(0.5f, 0.5f, 0.7f); break;
		case EEGChessPieceType::Queen: Shape.Mesh = EGChessEngineAssets::Cylinder(); Shape.Size = FVector(0.5f, 0.5f, 1.15f); break;
		default: Shape.Mesh = EGChessEngineAssets::Cylinder(); Shape.Size = FVector(0.55f, 0.55f, 1.35f); break;
		}
		return Shape;
	}

	/** 0..1 to 0..1 with zero speed and acceleration at both ends: how a hand starts and stops. */
	float MinJerk(float Alpha)
	{
		const float A = FMath::Clamp(Alpha, 0.0f, 1.0f);
		return A * A * A * (10.0f + A * (-15.0f + 6.0f * A));
	}

	/**
	 * A carry: the piece rises first, crosses at height, and comes down last. Across the board it
	 * moves only between CarryLead and 1 - CarryLead; its height rises over the first CarryRise and
	 * falls over the last, so it is already well up when it starts across.
	 */
	constexpr float CarryLead = 0.18f;
	constexpr float CarryRise = 0.32f;

	/** A live hand counts as over a piece's grip point, and comes straight down, once this close to it sideways (cm)... */
	constexpr float LiveDescendRadius = 0.6f;
	/** ...and as on it once this close (cm). */
	constexpr float LiveArriveDistance = 0.35f;

	/**
	 * A critically damped spring after Target, never faster than MaxSpeed: the target is held within
	 * the distance the spring covers at that speed. Substepped, as the smoothing's approximation wants
	 * steps shorter than half its time.
	 */
	void FollowSpring(FVector& Value, FVector& Velocity, const FVector& Target, float DeltaTime, float SmoothTime, float MaxSpeed)
	{
		if (SmoothTime <= KINDA_SMALL_NUMBER)
		{
			Value = Target;
			Velocity = FVector::ZeroVector;
			return;
		}
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaTime / (0.4f * SmoothTime)), 1, 16);
		const float Step = DeltaTime / Steps;
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			const FVector Toward = (Target - Value).GetClampedToMaxSize(FMath::Max(MaxSpeed, 1.0f) * SmoothTime);
			FMath::CriticallyDampedSmoothing(Value, Velocity, Value + Toward, FVector::ZeroVector, Step, SmoothTime);
		}
	}

	/** A piece's capsule height in BoardSurface units: the set's pick height, or the stand-in's size. */
	float PieceHeight(const UEGChessPieceSet* Set, EEGChessPieceType Type, float PieceScale, float SquareSize)
	{
		if (Set && !Set->GetVisual(Type).Mesh.IsNull())
		{
			return Set->GetVisual(Type).PickHeight * PieceScale;
		}
		return FallbackFor(Type).Size.Z * SquareSize;
	}

	FQuat LocalFacing(EEGChessColor Color, EEGChessSeat WhiteSeat)
	{
		const bool bFacesPlusX = (Color == EEGChessColor::White) == (WhiteSeat == EEGChessSeat::A);
		return bFacesPlusX ? FQuat::Identity : FQuat(FVector::UpVector, PI);
	}

	/** Closest approach between a ray and a segment; returns distance and the ray parameter. */
	float RaySegmentDistance(const FVector& RayOrigin, const FVector& RayDir, const FVector& A, const FVector& B, float& OutRayT)
	{
		const FVector U = RayDir;
		const FVector V = B - A;
		const FVector W = RayOrigin - A;
		const float UU = FVector::DotProduct(U, U);
		const float UV = FVector::DotProduct(U, V);
		const float VV = FVector::DotProduct(V, V);
		const float UW = FVector::DotProduct(U, W);
		const float VW = FVector::DotProduct(V, W);
		const float Denominator = UU * VV - UV * UV;
		float S = 0.0f;
		float T = 0.0f;
		if (Denominator > KINDA_SMALL_NUMBER)
		{
			S = (UV * VW - VV * UW) / Denominator;
			T = (UU * VW - UV * UW) / Denominator;
		}
		S = FMath::Max(S, 0.0f);
		T = FMath::Clamp(T, 0.0f, 1.0f);
		// Re-project the ray parameter onto the clamped segment point.
		const FVector SegmentPoint = A + V * T;
		S = FMath::Max(0.0f, FVector::DotProduct(SegmentPoint - RayOrigin, U) / FMath::Max(UU, KINDA_SMALL_NUMBER));
		OutRayT = S;
		return FVector::Distance(RayOrigin + U * S, SegmentPoint);
	}
}

UEGChessPieceVisuals::UEGChessPieceVisuals()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

double UEGChessPieceVisuals::GetNow() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

// ---------------------------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------------------------

void UEGChessPieceVisuals::Initialize(AEGChessTableActor* InTable)
{
	Table = InTable;
	for (UStaticMeshComponent* Component : PieceComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	PieceComponents.Reset();
	States.Reset();
	PieceAt.Init(INDEX_NONE, 64);
	PresentedHistory.Reset();
	PresentedStartFEN.Reset();
	EndHand();
	Step = FStep();
	for (FLiveHand& Hand : LiveHands)
	{
		LowerLiveHand(Hand, /*bTellCharacter=*/ true);
	}
	SelectedPiece = INDEX_NONE;
	OutlinedPieces.Reset();
	bButtonsCaptured = false;

	if (!GhostComponent && InTable && InTable->GetBoardSurface())
	{
		GhostComponent = NewObject<UStaticMeshComponent>(InTable, NAME_None, RF_Transient);
		GhostComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GhostComponent->SetCastShadow(false);
		GhostComponent->SetupAttachment(InTable->GetBoardSurface());
		// After the board marks, which sort by small priorities of their own.
		GhostComponent->SetTranslucentSortPriority(100);
		GhostComponent->RegisterComponent();
		GhostComponent->SetVisibility(false);
	}
	if (!GhostDepthComponent && InTable && InTable->GetBoardSurface())
	{
		GhostDepthComponent = NewObject<UStaticMeshComponent>(InTable, NAME_None, RF_Transient);
		GhostDepthComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GhostDepthComponent->SetCastShadow(false);
		GhostDepthComponent->SetRenderInMainPass(false);
		GhostDepthComponent->SetRenderInDepthPass(false);
		GhostDepthComponent->SetRenderCustomDepth(true);
		GhostDepthComponent->SetupAttachment(InTable->GetBoardSurface());
		GhostDepthComponent->RegisterComponent();
		GhostDepthComponent->SetVisibility(false);
	}
}

float UEGChessPieceVisuals::GetPieceScale() const
{
	const AEGChessTableActor* T = Table.Get();
	const UEGChessPieceSet* Set = T ? T->GetPieceSet() : nullptr;
	if (!Set || !Set->bScaleToBoard)
	{
		return 1.0f;
	}
	return T->GetBoardGeometry().SquareSize / FMath::Max(Set->AuthoredSquareSize, KINDA_SMALL_NUMBER);
}

void UEGChessPieceVisuals::EnsureComponents(const TArray<FPieceState>& InStates)
{
	AEGChessTableActor* T = Table.Get();
	if (!T || !T->GetBoardSurface())
	{
		return;
	}
	while (PieceComponents.Num() < InStates.Num())
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(T, NAME_None, RF_Transient);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetupAttachment(T->GetBoardSurface());
		Component->RegisterComponent();
		PieceComponents.Add(Component);
	}
	for (int32 Index = 0; Index < PieceComponents.Num(); ++Index)
	{
		if (PieceComponents[Index])
		{
			PieceComponents[Index]->SetVisibility(Index < InStates.Num());
		}
	}
}

void UEGChessPieceVisuals::ApplyMesh(int32 Piece)
{
	using namespace EGChessPieceVisualsPrivate;

	if (!States.IsValidIndex(Piece) || !PieceComponents.IsValidIndex(Piece) || !PieceComponents[Piece])
	{
		return;
	}
	const AEGChessTableActor* T = Table.Get();
	ApplyPieceAppearance(PieceComponents[Piece], T ? T->GetPieceSet() : nullptr, States[Piece].Type, States[Piece].Color);
}

void UEGChessPieceVisuals::ApplyPieceAppearance(UStaticMeshComponent* Component, const UEGChessPieceSet* Set, EEGChessPieceType Type, EEGChessColor Color)
{
	using namespace EGChessPieceVisualsPrivate;

	if (!Component)
	{
		return;
	}
	if (Set)
	{
		const FEGChessPieceVisual& Visual = Set->GetVisual(Type);
		UStaticMesh* Mesh = Color == EEGChessColor::Black ? EGChessEngineAssets::Resolve(Visual.MeshBlack) : nullptr;
		if (!Mesh)
		{
			Mesh = EGChessEngineAssets::Resolve(Visual.Mesh);
		}
		if (Mesh)
		{
			Component->SetStaticMesh(Mesh);
			Component->EmptyOverrideMaterials();
			const TArray<TSoftObjectPtr<UMaterialInterface>>& Materials = Color == EEGChessColor::White ? Visual.WhiteMaterials : Visual.BlackMaterials;
			for (int32 Slot = 0; Slot < Materials.Num(); ++Slot)
			{
				if (UMaterialInterface* Material = EGChessEngineAssets::Resolve(Materials[Slot]))
				{
					Component->SetMaterial(Slot, Material);
				}
			}
			return;
		}
	}

	// No set, or no mesh for this type: a basic-shape stand-in, tinted per colour.
	const FFallbackShape Shape = FallbackFor(Type);
	Component->SetStaticMesh(Shape.Mesh);
	Component->EmptyOverrideMaterials();
	Component->SetMaterial(0, EGChessEngineAssets::TintedMaterial(Component, Color == EEGChessColor::White ? FLinearColor(0.9f, 0.88f, 0.8f) : FLinearColor(0.08f, 0.06f, 0.05f)));
}

// ---------------------------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------------------------

bool UEGChessPieceVisuals::BuildStates(const FString& StartFEN, const TArray<FEGChessMoveRecord>& History, int32 Count, TArray<FPieceState>& OutStates) const
{
	FEGChessPosition Start;
	if (!Start.FromFEN(StartFEN.IsEmpty() ? FString(FEGChessPosition::StartFEN()) : StartFEN))
	{
		return false;
	}

	OutStates.Reset();
	TArray<int32> LocalPieceAt;
	LocalPieceAt.Init(INDEX_NONE, 64);
	for (int32 Square = 0; Square < 64; ++Square)
	{
		const uint8 Piece = Start.Board[Square];
		if (Piece == 0)
		{
			continue;
		}
		FPieceState State;
		State.Type = EGChessPiece::TypeOf(Piece);
		State.Color = EGChessPiece::ColorOf(Piece);
		State.Square = Square;
		LocalPieceAt[Square] = OutStates.Add(State);
	}

	const int32 Plies = FMath::Min(Count, History.Num());
	for (int32 Ply = 0; Ply < Plies; ++Ply)
	{
		ApplyRecord(OutStates, LocalPieceAt, History[Ply]);
	}
	return true;
}

void UEGChessPieceVisuals::ApplyRecord(TArray<FPieceState>& InOutStates, TArray<int32>& InOutPieceAt, const FEGChessMoveRecord& Record) const
{
	const FEGChessMove& Move = Record.Move;
	if (!Move.IsValid())
	{
		return;
	}

	const AEGChessTableActor* T = Table.Get();
	const EEGChessSeat CapturingSeat = T ? T->GetSeatForColor(Record.MoverColor) : EEGChessSeat::A;

	auto Capture = [&](int32 Square)
	{
		const int32 Victim = InOutPieceAt[Square];
		if (Victim == INDEX_NONE)
		{
			return;
		}
		int32 TrayCount = 0;
		for (const FPieceState& Other : InOutStates)
		{
			TrayCount += (Other.Square == INDEX_NONE && Other.TraySeat == CapturingSeat) ? 1 : 0;
		}
		InOutStates[Victim].Square = INDEX_NONE;
		InOutStates[Victim].TraySeat = CapturingSeat;
		InOutStates[Victim].TrayIndex = TrayCount;
		InOutPieceAt[Square] = INDEX_NONE;
	};

	if (Move.HasFlag(EGChessMoveFlags::EnPassant))
	{
		Capture(Move.To + (Record.MoverColor == EEGChessColor::White ? -8 : 8));
	}
	else
	{
		Capture(Move.To);
	}

	const int32 Mover = InOutPieceAt[Move.From];
	InOutPieceAt[Move.From] = INDEX_NONE;
	if (Mover != INDEX_NONE)
	{
		InOutStates[Mover].Square = Move.To;
		InOutPieceAt[Move.To] = Mover;
		if (Move.Promotion != EEGChessPieceType::None)
		{
			InOutStates[Mover].Type = Move.Promotion;
		}
	}

	int32 RookFrom = INDEX_NONE;
	int32 RookTo = INDEX_NONE;
	if (Move.HasFlag(EGChessMoveFlags::CastleKingSide))
	{
		RookFrom = Move.To + 1;
		RookTo = Move.To - 1;
	}
	else if (Move.HasFlag(EGChessMoveFlags::CastleQueenSide))
	{
		RookFrom = Move.To - 2;
		RookTo = Move.To + 1;
	}
	if (RookFrom != INDEX_NONE)
	{
		const int32 Rook = InOutPieceAt[RookFrom];
		InOutPieceAt[RookFrom] = INDEX_NONE;
		if (Rook != INDEX_NONE)
		{
			InOutStates[Rook].Square = RookTo;
			InOutPieceAt[RookTo] = Rook;
		}
	}
}

bool UEGChessPieceVisuals::MatchesHistory(const FString& StartFEN, const TArray<FEGChessMoveRecord>& History) const
{
	if (PresentedStartFEN != StartFEN || PresentedHistory.Num() != History.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < History.Num(); ++Index)
	{
		if (History[Index].Move != PresentedHistory[Index].Move)
		{
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Placement
// ---------------------------------------------------------------------------------------------

FTransform UEGChessPieceVisuals::SquareTransform(int32 Square, EEGChessPieceType Type, EEGChessColor Color) const
{
	using namespace EGChessPieceVisualsPrivate;

	const AEGChessTableActor* T = Table.Get();
	if (!T)
	{
		return FTransform::Identity;
	}
	const FEGChessBoardGeometry Geometry = T->GetBoardGeometry();
	FVector Location = Geometry.SquareToLocal(Square);
	FQuat Rotation = LocalFacing(Color, Geometry.WhiteSeat);
	FVector Scale(GetPieceScale());

	const UEGChessPieceSet* Set = T->GetPieceSet();
	const bool bHasMesh = Set && !Set->GetVisual(Type).Mesh.IsNull();
	if (bHasMesh)
	{
		const FEGChessPieceVisual& Visual = Set->GetVisual(Type);
		const float Yaw = Color == EEGChessColor::White ? Visual.YawOffsetWhite : Visual.YawOffsetBlack;
		Rotation = Rotation * FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
		Location -= Rotation.RotateVector(Visual.PivotOffset * Scale);
	}
	else
	{
		const FFallbackShape Shape = FallbackFor(Type);
		const FVector Size = Shape.Size * Geometry.SquareSize;
		Scale = Size / 100.0f;
		Location.Z += Size.Z * 0.5f;
	}
	return FTransform(Rotation, Location, Scale);
}

FTransform UEGChessPieceVisuals::TrayTransform(EEGChessSeat Seat, int32 Index, EEGChessPieceType Type, EEGChessColor Color) const
{
	const AEGChessTableActor* T = Table.Get();
	const USceneComponent* Tray = T ? T->GetTray(Seat) : nullptr;
	if (!T || !Tray)
	{
		return FTransform::Identity;
	}
	const float Spacing = T->GetBoardGeometry().SquareSize * 0.9f;
	const FVector Slot((Index % 8) * Spacing, (Index / 8) * Spacing, 0.0f);

	// A square transform at the origin gives the piece's own scale and pivot; move it to the slot.
	FTransform Piece = SquareTransform(0, Type, Color);
	const FEGChessBoardGeometry Geometry = T->GetBoardGeometry();
	Piece.SetLocation(Piece.GetLocation() - Geometry.SquareToLocal(0));

	const FTransform TrayLocal = Tray->GetRelativeTransform();
	FTransform Out = Piece;
	Out.SetLocation(TrayLocal.TransformPosition(Slot + Piece.GetLocation()));
	Out.SetRotation(TrayLocal.GetRotation() * Piece.GetRotation());
	return Out;
}

FTransform UEGChessPieceVisuals::RestingTransform(int32 Piece) const
{
	if (!States.IsValidIndex(Piece))
	{
		return FTransform::Identity;
	}
	const FPieceState& State = States[Piece];
	FTransform Out = State.Square != INDEX_NONE
		? SquareTransform(State.Square, State.Type, State.Color)
		: TrayTransform(State.TraySeat, State.TrayIndex, State.Type, State.Color);

	if (State.bToppled)
	{
		Out.SetRotation(Out.GetRotation() * FQuat(FVector::RightVector, FMath::DegreesToRadians(85.0f)));
	}
	if (Piece == SelectedPiece && State.Square != INDEX_NONE)
	{
		const AEGChessTableActor* T = Table.Get();
		const UEGChessPieceSet* Set = T ? T->GetPieceSet() : nullptr;
		if (bCarrying && T)
		{
			// Same pivot and facing as on its square, moved across the surface to the carry point.
			FVector Offset = CarryLocal - T->GetBoardGeometry().SquareToLocal(State.Square);
			Offset.Z = (Set ? Set->CarryLiftHeight : 3.0f) * GetPieceScale();
			Out.AddToTranslation(Offset);
		}
		else
		{
			const float Lift = Set ? Set->SelectedLiftHeight * GetPieceScale() : 1.5f;
			Out.AddToTranslation(FVector(0.0f, 0.0f, Lift));
		}
	}
	return Out;
}

void UEGChessPieceVisuals::PlacePiece(int32 Piece, const FTransform& Local)
{
	if (PieceComponents.IsValidIndex(Piece) && PieceComponents[Piece])
	{
		PieceComponents[Piece]->SetRelativeTransform(Local);
	}
}

void UEGChessPieceVisuals::RefreshPlacement()
{
	FinishCurrentStep();
	for (int32 Piece = 0; Piece < States.Num(); ++Piece)
	{
		PlacePiece(Piece, RestingTransform(Piece));
	}
}

UStaticMeshComponent* UEGChessPieceVisuals::GetPieceComponentAt(int32 Square) const
{
	const int32 Piece = PieceAt.IsValidIndex(Square) ? PieceAt[Square] : INDEX_NONE;
	return PieceComponents.IsValidIndex(Piece) ? PieceComponents[Piece].Get() : nullptr;
}

// ---------------------------------------------------------------------------------------------
// Snap and steps
// ---------------------------------------------------------------------------------------------

void UEGChessPieceVisuals::Snap(const FString& StartFEN, const TArray<FEGChessMoveRecord>& History, const FEGChessResult& Result)
{
	DropLiveHandPieces();
	EndHand();
	Step = FStep();
	bGameEndPending = false;
	SetComponentTickEnabled(false);

	TArray<FPieceState> NewStates;
	if (!BuildStates(StartFEN, History, History.Num(), NewStates))
	{
		return;
	}

	States = NewStates;
	PieceAt.Init(INDEX_NONE, 64);
	for (int32 Piece = 0; Piece < States.Num(); ++Piece)
	{
		if (States[Piece].Square != INDEX_NONE)
		{
			PieceAt[States[Piece].Square] = Piece;
		}
	}

	// The loser's king lies down if the game already ended that way.
	const AEGChessTableActor* T = Table.Get();
	const UEGChessPieceSet* Set = T ? T->GetPieceSet() : nullptr;
	if (Result.IsOver() && !Result.IsDraw() && (!Set || Set->bToppleKingOnLoss))
	{
		for (FPieceState& State : States)
		{
			State.bToppled = State.Type == EEGChessPieceType::King && State.Color != Result.GetWinner();
		}
	}

	PresentedStartFEN = StartFEN;
	PresentedHistory = History;
	SelectedPiece = INDEX_NONE;

	EnsureComponents(States);
	for (int32 Piece = 0; Piece < States.Num(); ++Piece)
	{
		ApplyMesh(Piece);
		PlacePiece(Piece, RestingTransform(Piece));
	}

	if (History.Num() > 0)
	{
		SetClockPressed(T ? T->GetSeatForColor(History.Last().MoverColor) : EEGChessSeat::A, false);
	}
	else if (T)
	{
		// The side to move has its button up.
		SetClockPressed(EGChess::OtherSeat(T->GetSeatForColor(EEGChessColor::White)), false);
	}
}

void UEGChessPieceVisuals::BeginStep(EStepKind Kind)
{
	FinishCurrentStep();
	Step = FStep();
	Step.Kind = Kind;
	Step.StartTime = GetNow();
	SetComponentTickEnabled(true);
}

void UEGChessPieceVisuals::PresentMove(const FEGChessMoveRecord& Record, int32 PlyIndex, EEGChessSeat MoverSeat, const FEGChessMoveTiming& Timing)
{
	// The mover's hand may already be over the board, maybe holding the piece on its new square: the
	// plan carries on from it. Read before the previous step finishes, which puts every piece down.
	FLiveHand& Live = LiveHands[EGChess::SeatIndex(MoverSeat)];
	FHandStart LiveStart;
	int32 LiveHeldPiece = INDEX_NONE;
	FTransform LiveHeldTransform = FTransform::Identity;
	if (Live.bActive)
	{
		LiveStart.bValid = true;
		LiveStart.Location = Live.Location;
		LiveStart.Reach = Live.Reach;
		LiveStart.Grip = Live.Grip;
		LiveHeldPiece = Live.HeldPiece;
		if (PieceComponents.IsValidIndex(LiveHeldPiece) && PieceComponents[LiveHeldPiece])
		{
			LiveHeldTransform = PieceComponents[LiveHeldPiece]->GetRelativeTransform();
		}
	}
	if (!Timing.bHandDriven || PlyIndex != PresentedHistory.Num())
	{
		LowerLiveHand(Live, /*bTellCharacter=*/ true);
		LiveStart = FHandStart();
		LiveHeldPiece = INDEX_NONE;
	}

	FinishCurrentStep();
	if (PlyIndex != PresentedHistory.Num())
	{
		return; // out of step; the table will reconcile with a snap
	}

	BeginStep(EStepKind::Move);
	Step.Record = Record;
	Step.MoverSeat = MoverSeat;
	Step.Timing = Timing;
	Step.MoverPiece = PieceAt.IsValidIndex(Record.Move.From) ? PieceAt[Record.Move.From] : INDEX_NONE;

	const int32 CapturedSquare = Record.Move.HasFlag(EGChessMoveFlags::EnPassant)
		? Record.Move.To + (Record.MoverColor == EEGChessColor::White ? -8 : 8)
		: Record.Move.To;
	Step.CapturedPiece = PieceAt.IsValidIndex(CapturedSquare) ? PieceAt[CapturedSquare] : INDEX_NONE;
	if (Step.CapturedPiece == Step.MoverPiece)
	{
		Step.CapturedPiece = INDEX_NONE;
	}

	if (Record.Move.HasFlag(EGChessMoveFlags::CastleKingSide))
	{
		Step.RookPiece = PieceAt[Record.Move.To + 1];
		Step.RookTo = Record.Move.To - 1;
	}
	else if (Record.Move.HasFlag(EGChessMoveFlags::CastleQueenSide))
	{
		Step.RookPiece = PieceAt[Record.Move.To - 2];
		Step.RookTo = Record.Move.To + 1;
	}

	if (SelectedPiece != INDEX_NONE)
	{
		SelectedPiece = INDEX_NONE;
	}

	// A hand moves the pieces one at a time in the order a player would: the captured piece off the
	// board, then the mover, then (castling) the rook. Each trip runs from where its piece rests now
	// to where it rests once the record is applied. A live hand already holding the mover sets it down
	// first, and the captured piece is cleared off to the tray as it comes down.
	const bool bLiveHoldsMover = LiveStart.bValid && LiveHeldPiece == Step.MoverPiece && States.IsValidIndex(LiveHeldPiece) && LiveStart.Grip > 0.5f;
	LiveStart.bHoldingFirst = bLiveHoldsMover;
	const float CaptureSlide = [this]()
	{
		const AEGChessTableActor* T = Table.Get();
		const UEGChessPieceSet* Set = T ? T->GetPieceSet() : nullptr;
		return Set ? FMath::Max(0.15f, Set->MoveTime * 0.7f) : 0.3f;
	}();
	if (bLiveHoldsMover && States.IsValidIndex(Step.CapturedPiece))
	{
		// It comes down once the piece it takes is on its way off the square.
		LiveStart.Dwell = CaptureSlide * 0.5f;
	}
	if (LiveStart.bValid && !bLiveHoldsMover && States.IsValidIndex(LiveHeldPiece))
	{
		PlacePiece(LiveHeldPiece, RestingTransform(LiveHeldPiece)); // not the piece that moved: back on its square
	}
	TArray<FHandTrip> Trips;
	int32 CaptureTrips = 0;
	if (Timing.bHandDriven)
	{
		for (const int32 Piece : { Step.CapturedPiece, Step.MoverPiece, Step.RookPiece })
		{
			if (!States.IsValidIndex(Piece) || (bLiveHoldsMover && Piece == Step.CapturedPiece))
			{
				continue;
			}
			FHandTrip& Trip = Trips.AddDefaulted_GetRef();
			Trip.Piece = Piece;
			Trip.Type = States[Piece].Type; // the pawn's own grip, even when it promotes
			Trip.From = bLiveHoldsMover && Piece == Step.MoverPiece ? LiveHeldTransform : RestingTransform(Piece);
			CaptureTrips += Piece == Step.CapturedPiece ? 1 : 0;
		}
	}

	// The identity model advances now; the components catch up through the step.
	ApplyRecord(States, PieceAt, Record);
	PresentedHistory.Add(Record);

	if (Timing.bHandDriven)
	{
		for (FHandTrip& Trip : Trips)
		{
			Trip.To = RestingTransform(Trip.Piece);
		}
		BuildHandPlan(Step.Hand, Trips, CaptureTrips, Timing, /*bWithClearance=*/ true, LiveStart);
		if (Step.Hand.Keys.Num() == 0)
		{
			Step.Timing.bHandDriven = false; // nothing to plan with; the pieces move by themselves
			LowerLiveHand(Live, /*bTellCharacter=*/ true);
			return;
		}
		HandLiveHandToPlan(Live);
		if (bLiveHoldsMover)
		{
			// Picked up already, by the live hand (its sound too). What it takes slides off to the tray.
			Step.bPicked = true;
			Step.PickedAt = GetNow();
			if (States.IsValidIndex(Step.CapturedPiece))
			{
				const AEGChessTableActor* T = Table.Get();
				const UEGChessPieceSet* Set = T ? T->GetPieceSet() : nullptr;
				AddTween(Step.CapturedPiece, RestingTransform(Step.CapturedPiece), CaptureSlide, (Set ? Set->LiftHeight : 3.0f) * GetPieceScale(), true, GetNow());
			}
		}
		// The character reads this frame's hand state only if its mesh ticks after us.
		if (USkeletalMeshComponent* Mesh = GetHandMesh(Timing.Pawn.Get()))
		{
			Mesh->AddTickPrerequisiteComponent(this);
			HandTickMesh = Mesh;
		}
		Step.Hand.State.bRightHand = Timing.bRightHand;
		if (const UEGChessAnimationSet* AnimSet = Timing.AnimationSet.Get())
		{
			Step.Hand.State.BlendArcHeight = AnimSet->BlendArcHeight;
			Step.Hand.State.BlendOutTime = AnimSet->BlendOutTime;
		}
		TickHand(GetNow());
	}
}

float UEGChessPieceVisuals::EstimateHandMoveDuration(const FEGChessMoveRecord& Record, EEGChessSeat MoverSeat, const FEGChessMoveTiming& Timing) const
{
	const FEGChessMove& Move = Record.Move;
	const EEGChessColor Victim = Record.MoverColor == EEGChessColor::White ? EEGChessColor::Black : EEGChessColor::White;

	TArray<FHandTrip> Trips;
	int32 CaptureTrips = 0;
	if (Record.CapturedType != EEGChessPieceType::None)
	{
		const int32 CapturedSquare = Move.HasFlag(EGChessMoveFlags::EnPassant) ? Move.To + (Record.MoverColor == EEGChessColor::White ? -8 : 8) : Move.To;
		FHandTrip& Trip = Trips.AddDefaulted_GetRef();
		Trip.Type = Record.CapturedType;
		Trip.From = SquareTransform(CapturedSquare, Record.CapturedType, Victim);
		Trip.To = TrayTransform(MoverSeat, 0, Record.CapturedType, Victim);
		CaptureTrips = 1;
	}
	FHandTrip& MoverTrip = Trips.AddDefaulted_GetRef();
	MoverTrip.Type = Record.MovedType;
	MoverTrip.From = SquareTransform(Move.From, Record.MovedType, Record.MoverColor);
	MoverTrip.To = SquareTransform(Move.To, Record.MovedType, Record.MoverColor);
	if (Move.IsCastle())
	{
		const bool bKingSide = Move.HasFlag(EGChessMoveFlags::CastleKingSide);
		FHandTrip& RookTrip = Trips.AddDefaulted_GetRef();
		RookTrip.Type = EEGChessPieceType::Rook;
		RookTrip.From = SquareTransform(bKingSide ? Move.To + 1 : Move.To - 2, EEGChessPieceType::Rook, Record.MoverColor);
		RookTrip.To = SquareTransform(bKingSide ? Move.To - 1 : Move.To + 1, EEGChessPieceType::Rook, Record.MoverColor);
	}

	FHandPlan Plan;
	BuildHandPlan(Plan, Trips, CaptureTrips, Timing, /*bWithClearance=*/ false);
	return Plan.LetGoTime;
}

void UEGChessPieceVisuals::PresentTakeback(const TArray<FEGChessMoveRecord>& History, int32 NewLength)
{
	DropLiveHandPieces();
	FinishCurrentStep();

	TArray<FPieceState> Target;
	if (!BuildStates(PresentedStartFEN, PresentedHistory, NewLength, Target) || Target.Num() != States.Num())
	{
		Snap(PresentedStartFEN, History, FEGChessResult());
		return;
	}

	BeginStep(EStepKind::Glide);
	const AEGChessTableActor* T = Table.Get();
	const UEGChessPieceSet* Set = T ? T->GetPieceSet() : nullptr;
	const float Duration = (Set ? Set->MoveTime : 0.45f) / (Set ? Set->TakebackSpeed : 2.0f);
	const double Now = GetNow();

	States = Target;
	PieceAt.Init(INDEX_NONE, 64);
	for (int32 Piece = 0; Piece < States.Num(); ++Piece)
	{
		if (States[Piece].Square != INDEX_NONE)
		{
			PieceAt[States[Piece].Square] = Piece;
		}
		ApplyMesh(Piece);
		AddTween(Piece, RestingTransform(Piece), Duration, 0.0f, false, Now);
	}
	PresentedHistory.SetNum(FMath::Min(NewLength, PresentedHistory.Num()));
	EmitEvent(EEGChessEvent::Takeback, INDEX_NONE, EEGChessPieceType::None, EEGChessColor::White, false);

	if (T)
	{
		SetClockPressed(PresentedHistory.Num() > 0 ? T->GetSeatForColor(PresentedHistory.Last().MoverColor) : EGChess::OtherSeat(T->GetSeatForColor(EEGChessColor::White)), true);
	}
}

void UEGChessPieceVisuals::PresentReset(const FString& StartFEN)
{
	DropLiveHandPieces();
	FinishCurrentStep();
	bGameEndPending = false;

	TArray<FPieceState> Target;
	if (!BuildStates(StartFEN, TArray<FEGChessMoveRecord>(), 0, Target))
	{
		return;
	}
	if (Target.Num() != States.Num())
	{
		Snap(StartFEN, TArray<FEGChessMoveRecord>(), FEGChessResult());
		return;
	}

	BeginStep(EStepKind::Glide);
	const AEGChessTableActor* T = Table.Get();
	const UEGChessPieceSet* Set = T ? T->GetPieceSet() : nullptr;
	const float Duration = Set ? Set->ResetTime : 1.0f;
	const double Now = GetNow();

	// Identities are start squares, so every piece goes back to its own home.
	States = Target;
	PieceAt.Init(INDEX_NONE, 64);
	for (int32 Piece = 0; Piece < States.Num(); ++Piece)
	{
		PieceAt[States[Piece].Square] = Piece;
		ApplyMesh(Piece);
		AddTween(Piece, RestingTransform(Piece), Duration, (Set ? Set->LiftHeight : 3.0f) * GetPieceScale(), true, Now);
	}
	PresentedStartFEN = StartFEN;
	PresentedHistory.Reset();
	SelectedPiece = INDEX_NONE;

	EmitEvent(EEGChessEvent::PiecesReset, INDEX_NONE, EEGChessPieceType::None, EEGChessColor::White, false);
	if (T)
	{
		SetClockPressed(EGChess::OtherSeat(T->GetSeatForColor(EEGChessColor::White)), true);
	}
}

void UEGChessPieceVisuals::PresentGameEnd(const FEGChessResult& Result)
{
	const AEGChessTableActor* T = Table.Get();
	const UEGChessPieceSet* Set = T ? T->GetPieceSet() : nullptr;
	if (!Result.IsOver() || Result.IsDraw() || (Set && !Set->bToppleKingOnLoss))
	{
		return;
	}
	if (IsBusy())
	{
		PendingGameEnd = Result;
		bGameEndPending = true;
		return;
	}

	BeginStep(EStepKind::Topple);
	const double Now = GetNow();
	for (int32 Piece = 0; Piece < States.Num(); ++Piece)
	{
		FPieceState& State = States[Piece];
		if (State.Type == EEGChessPieceType::King && State.Color != Result.GetWinner() && State.Square != INDEX_NONE)
		{
			State.bToppled = true;
			AddTween(Piece, RestingTransform(Piece), Set ? Set->ToppleTime : 0.8f, 0.0f, false, Now);
			EmitEvent(EEGChessEvent::KingTopple, State.Square, State.Type, State.Color, false);
		}
	}
}

void UEGChessPieceVisuals::HandleCharacterNotify(EEGChessSeat Seat, FName Notify, FName ClockName)
{
	// The clock press clip only starts once the last piece is down, so a press before that is stale.
	if (Step.Kind != EStepKind::Move || Step.MoverSeat != Seat || Notify != ClockName || Step.bPressed || !Step.bLanded)
	{
		return;
	}
	DoPress();
}

void UEGChessPieceVisuals::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const double Now = GetNow();
	TickStep(Now);
	TickLiveHands(DeltaTime);

	// Clock button tween.
	const AEGChessTableActor* T = Table.Get();
	if (T && ButtonTweenStart >= 0.0 && bButtonsCaptured)
	{
		const float Alpha = FMath::Clamp(static_cast<float>((Now - ButtonTweenStart) / 0.08), 0.0f, 1.0f);
		const UEGChessTableStyle* Style = T->GetTableStyle();
		const float Travel = Style ? Style->ButtonPressTravel : 0.8f;
		for (const EEGChessSeat Seat : { EEGChessSeat::A, EEGChessSeat::B })
		{
			if (UStaticMeshComponent* Button = T->GetClockButton(Seat))
			{
				const FVector Raised = Seat == EEGChessSeat::A ? ButtonRaisedA : ButtonRaisedB;
				const FVector Pressed = Raised + EGChessPieceVisualsPrivate::PressOffset(Button, Travel);
				const FVector Current = Button->GetRelativeLocation();
				const FVector Target = Seat == PressedSeat ? Pressed : Raised;
				Button->SetRelativeLocation(FMath::Lerp(Current, Target, Alpha));
			}
		}
		if (Alpha >= 1.0f)
		{
			ButtonTweenStart = -1.0;
		}
	}

	if (Step.Kind == EStepKind::None && ButtonTweenStart < 0.0 && !WantsLiveHandTick())
	{
		SetComponentTickEnabled(false);
	}
}

void UEGChessPieceVisuals::TickStep(double Now)
{
	if (Step.Kind == EStepKind::None)
	{
		return;
	}

	for (const FTween& Tween : Step.Tweens)
	{
		EvaluateTween(Tween, Now);
	}

	if (Step.Kind == EStepKind::Move)
	{
		const float Elapsed = static_cast<float>(Now - Step.StartTime);
		if (Step.Timing.bHandDriven)
		{
			TickHand(Now);
		}
		else
		{
			if (!Step.bPicked && Elapsed >= Step.Timing.PickTime)
			{
				DoPick(Now);
			}
			if (Step.bPicked && !Step.bLanded && Elapsed >= Step.Timing.PlaceTime && AreTweensDone(Now))
			{
				DoLand(Now);
			}
		}
		// The clock press clip's notify presses the button; this only covers a notify that never comes.
		const float ClockGrace = Step.Timing.bCharacterDriven ? 0.3f : 0.0f;
		if (Step.bLanded && !Step.bPressed && Elapsed >= Step.Timing.ClockPressTime + ClockGrace)
		{
			DoPress();
		}
		if (Step.bLanded && Step.bPressed && AreTweensDone(Now))
		{
			FinishTweens();
			EndHand();
			Step = FStep();
		}
	}
	else if (AreTweensDone(Now))
	{
		FinishTweens();
		Step = FStep();
	}

	if (Step.Kind == EStepKind::None && bGameEndPending)
	{
		bGameEndPending = false;
		PresentGameEnd(PendingGameEnd);
	}
}

void UEGChessPieceVisuals::DoPick(double Now)
{
	Step.bPicked = true;
	Step.PickedAt = Now;
	if (!States.IsValidIndex(Step.MoverPiece))
	{
		return;
	}

	const AEGChessTableActor* T = Table.Get();
	const UEGChessPieceSet* Set = T ? T->GetPieceSet() : nullptr;
	const float Travel = FMath::Max(0.05f, Step.Timing.PlaceTime - Step.Timing.PickTime);
	const float Lift = (Set ? Set->LiftHeight : 3.0f) * GetPieceScale();
	AddTween(Step.MoverPiece, RestingTransform(Step.MoverPiece), Travel, Lift, true, Now);
	EmitEvent(EEGChessEvent::PiecePickUp, Step.Record.Move.From, Step.Record.MovedType, Step.Record.MoverColor, Step.Timing.bCharacterDriven);
}

void UEGChessPieceVisuals::DoLand(double Now)
{
	Step.bLanded = true;

	// Snap the mover onto its square, whatever the tween had reached.
	for (int32 Index = Step.Tweens.Num() - 1; Index >= 0; --Index)
	{
		if (Step.Tweens[Index].Piece == Step.MoverPiece)
		{
			Step.Tweens.RemoveAt(Index);
		}
	}
	if (States.IsValidIndex(Step.MoverPiece))
	{
		ApplyMesh(Step.MoverPiece); // promotion swaps as the pawn lands
		PlacePiece(Step.MoverPiece, RestingTransform(Step.MoverPiece));
	}

	const AEGChessTableActor* T = Table.Get();
	const UEGChessPieceSet* Set = T ? T->GetPieceSet() : nullptr;
	const float Slide = Set ? FMath::Max(0.15f, Set->MoveTime * 0.7f) : 0.3f;

	if (States.IsValidIndex(Step.CapturedPiece))
	{
		AddTween(Step.CapturedPiece, RestingTransform(Step.CapturedPiece), Slide, (Set ? Set->LiftHeight : 3.0f) * GetPieceScale(), true, Now);
	}
	if (States.IsValidIndex(Step.RookPiece))
	{
		AddTween(Step.RookPiece, RestingTransform(Step.RookPiece), Slide, (Set ? Set->LiftHeight : 3.0f) * GetPieceScale() * 0.6f, true, Now);
	}

	const FEGChessMove& Move = Step.Record.Move;
	EEGChessEvent Event = EEGChessEvent::PiecePlace;
	if (Move.Promotion != EEGChessPieceType::None)
	{
		Event = EEGChessEvent::Promote;
	}
	else if (Move.IsCastle())
	{
		Event = EEGChessEvent::Castle;
	}
	else if (Move.IsCapture())
	{
		Event = EEGChessEvent::Capture;
	}
	EmitEvent(Event, Move.To, Move.Promotion != EEGChessPieceType::None ? Move.Promotion : Step.Record.MovedType, Step.Record.MoverColor, Step.Timing.bCharacterDriven);
	if (Step.Record.bGivesCheck)
	{
		EmitEvent(EEGChessEvent::Check, Move.To, Step.Record.MovedType, Step.Record.MoverColor, Step.Timing.bCharacterDriven);
	}

	// A seated character that does not move pieces by hand still reaches over and presses the clock.
	float PressDelay = 0.0f;
	if (Step.Timing.bCharacterDriven && T && T->GetSeat(Step.MoverSeat)->PlayClockPressClip(Step.Timing.PlayRate, Step.Timing.bRightHand, PressDelay))
	{
		Step.Timing.ClockPressTime = static_cast<float>(Now - Step.StartTime) + PressDelay;
	}
}

void UEGChessPieceVisuals::DoPress()
{
	Step.bPressed = true;
	SetClockPressed(Step.MoverSeat, true);
	EmitEvent(EEGChessEvent::ClockPress, INDEX_NONE, EEGChessPieceType::None, Step.Record.MoverColor, Step.Timing.bCharacterDriven);
}

void UEGChessPieceVisuals::FinishCurrentStep()
{
	if (Step.Kind == EStepKind::None)
	{
		return;
	}
	// Land everything silently: the next step is already on its way.
	if (Step.Kind == EStepKind::Move)
	{
		if (!Step.bPressed)
		{
			SetClockPressed(Step.MoverSeat, false);
		}
		if (States.IsValidIndex(Step.MoverPiece))
		{
			ApplyMesh(Step.MoverPiece);
		}
	}
	EndHand();
	FinishTweens();
	for (int32 Piece = 0; Piece < States.Num(); ++Piece)
	{
		PlacePiece(Piece, RestingTransform(Piece));
	}
	Step = FStep();
}

// ---------------------------------------------------------------------------------------------
// Hand
// ---------------------------------------------------------------------------------------------

FVector UEGChessPieceVisuals::GetGripOffset(EEGChessPieceType Type, float HeightFraction) const
{
	using namespace EGChessPieceVisualsPrivate;

	const AEGChessTableActor* T = Table.Get();
	const UEGChessPieceSet* Set = T ? T->GetPieceSet() : nullptr;
	const float SquareSize = T ? T->GetBoardGeometry().SquareSize : 5.0f;
	const float Height = PieceHeight(Set, Type, GetPieceScale(), SquareSize);
	if (Set && !Set->GetVisual(Type).Mesh.IsNull())
	{
		// SquareTransform moves the pivot by -PivotOffset so the base sits on the square; undo that.
		return Set->GetVisual(Type).PivotOffset * GetPieceScale() + FVector(0.0f, 0.0f, Height * HeightFraction);
	}
	// Stand-ins are centred on their pivot.
	return FVector(0.0f, 0.0f, Height * (HeightFraction - 0.5f));
}

float UEGChessPieceVisuals::GetClearanceBetween(const FVector& From, const FVector& To, const TArray<int32>& Ignore) const
{
	using namespace EGChessPieceVisualsPrivate;

	const AEGChessTableActor* T = Table.Get();
	if (!T)
	{
		return 0.0f;
	}
	const FEGChessBoardGeometry Geometry = T->GetBoardGeometry();
	const UEGChessPieceSet* Set = T->GetPieceSet();
	const FVector2D A(From);
	const FVector2D B(To);
	float Highest = 0.0f;
	for (int32 Piece = 0; Piece < States.Num(); ++Piece)
	{
		if (States[Piece].Square == INDEX_NONE || Ignore.Contains(Piece))
		{
			continue;
		}
		const FVector2D Centre(Geometry.SquareToLocal(States[Piece].Square));
		const FVector2D Closest = FMath::ClosestPointOnSegment2D(Centre, A, B);
		if (FVector2D::Distance(Centre, Closest) < Geometry.SquareSize * 0.75f)
		{
			Highest = FMath::Max(Highest, PieceHeight(Set, States[Piece].Type, GetPieceScale(), Geometry.SquareSize));
		}
	}
	return Highest;
}

void UEGChessPieceVisuals::BuildHandPlan(FHandPlan& Out, const TArray<FHandTrip>& Trips, int32 CaptureTrips, const FEGChessMoveTiming& Timing, bool bWithClearance,
	const FHandStart& From) const
{
	Out = FHandPlan();
	const UEGChessAnimationSet* Set = Timing.AnimationSet.Get();
	if (!Set || Trips.Num() == 0)
	{
		return;
	}

	const float Rate = FMath::Max(Timing.PlayRate, KINDA_SMALL_NUMBER);
	const FVector Up(0.0f, 0.0f, 1.0f);
	auto TravelTime = [Set, Rate](const FVector& A, const FVector& B)
	{
		const float Seconds = FVector::Dist2D(A, B) / FMath::Max(Set->TravelSpeed, 1.0f);
		return FMath::Clamp(Seconds, Set->MinTravelTime, FMath::Max(Set->MinTravelTime, Set->MaxTravelTime)) / Rate;
	};
	// From a live hand: it is already out over the board, so a short way is short.
	auto LiveTravelTime = [Set, Rate](const FVector& A, const FVector& B)
	{
		const float Seconds = FVector::Dist(A, B) / FMath::Max(Set->TravelSpeed, 1.0f);
		return FMath::Clamp(Seconds, Set->LowerTime, FMath::Max(Set->LowerTime, Set->MaxTravelTime)) / Rate;
	};

	float Time = 0.0f;
	auto AddKey = [&Out, &Time](float Duration, const FVector& Location, float Grip, float CarryHeight)
	{
		Time += Duration;
		FHandKey& Key = Out.Keys.AddDefaulted_GetRef();
		Key.Time = Time;
		Key.Location = Location;
		Key.Reach = 1.0f;
		Key.Grip = Grip;
		Key.CarryHeight = CarryHeight;
	};
	auto GripPoint = [](const FTransform& Rest, const FVector& Offset)
	{
		return Rest.GetLocation() + Rest.GetRotation().RotateVector(Offset);
	};

	// The hand eases off its animation onto a point above the first piece, or carries on from where a
	// live hand already is.
	const FVector FirstOffset = GetGripOffset(Trips[0].Type, Set->GripHeightFraction);
	const FVector FirstApproach = GripPoint(Trips[0].From, FirstOffset) + Up * Set->ApproachHeight;
	FHandKey& Start = Out.Keys.AddDefaulted_GetRef();
	if (From.bValid)
	{
		Start.Location = From.Location;
		Start.Reach = From.Reach;
		Start.Grip = From.Grip;
		if (!From.bHoldingFirst)
		{
			AddKey(LiveTravelTime(From.Location, FirstApproach), FirstApproach, 0.0f, 0.0f);
		}
	}
	else
	{
		Start.Location = FirstApproach;
		Start.Reach = 0.0f;
		AddKey(Set->ReachTime / Rate, FirstApproach, 0.0f, 0.0f);
	}

	// The capture trip, from arriving over the captured piece to arriving over the mover, is fitted
	// to CaptureTripTime afterwards.
	const float CaptureFrom = Time;
	float CaptureTo = Time;
	FVector LastApproach = FirstApproach;

	for (int32 Index = 0; Index < Trips.Num(); ++Index)
	{
		const FHandTrip& Trip = Trips[Index];
		const FVector Offset = GetGripOffset(Trip.Type, Set->GripHeightFraction);
		const FVector GripFrom = GripPoint(Trip.From, Offset);
		const FVector GripTo = GripPoint(Trip.To, Offset);
		const FVector ApproachFrom = GripFrom + Up * Set->ApproachHeight;
		const FVector ApproachTo = GripTo + Up * Set->ApproachHeight;

		if (Index > 0)
		{
			AddKey(TravelTime(LastApproach, ApproachFrom), ApproachFrom, 0.0f, 0.0f);
		}
		if (Index == CaptureTrips && CaptureTrips > 0)
		{
			CaptureTo = Time;
		}

		if (Index == 0 && From.bValid && From.bHoldingFirst)
		{
			// Already in the fingers: across at the height it is carried at, then down onto its square
			// (no time at all when the hand is already setting it there).
			const float CarryStart = Time;
			const FVector Over(GripTo.X, GripTo.Y, FMath::Max(From.Location.Z, ApproachTo.Z));
			if (FVector::Dist2D(From.Location, GripTo) > EGChessPieceVisualsPrivate::LiveArriveDistance)
			{
				AddKey(LiveTravelTime(From.Location, Over), Over, 1.0f, 0.0f);
			}
			if (From.Dwell > 0.0f)
			{
				AddKey(From.Dwell, Out.Keys.Last().Location, 1.0f, 0.0f);
			}
			if (FVector::Dist(Out.Keys.Last().Location, GripTo) > EGChessPieceVisualsPrivate::LiveArriveDistance)
			{
				AddKey(Set->LowerTime / Rate, GripTo, 1.0f, 0.0f);
			}
			Out.Actions.Add({ Time, EHandAction::Release, Trip.Piece });

			FHandCarry& Held = Out.Carries.AddDefaulted_GetRef();
			Held.Piece = Trip.Piece;
			Held.Start = CarryStart;
			Held.End = Time;
			Held.From = Trip.From;
			Held.To = Trip.To;
			Held.GripOffset = Offset;

			AddKey(Set->ReleaseTime / Rate, GripTo + Up * 0.5f, 0.0f, 0.0f);
			AddKey(Set->LowerTime / Rate, ApproachTo, 0.0f, 0.0f);
			LastApproach = ApproachTo;
			continue;
		}

		// Down onto the piece with the fingers parting, then closing on it.
		AddKey(Set->LowerTime / Rate, GripFrom, 0.25f, 0.0f);
		AddKey(Set->GripTime / Rate, GripFrom, 1.0f, 0.0f);
		Out.Actions.Add({ Time, EHandAction::Grip, Trip.Piece });
		const float CarryStart = Time;

		// Up, across and down, high enough that the piece's base clears what it passes over.
		float CarryHeight = Set->ApproachHeight;
		if (bWithClearance)
		{
			const float BaseHeight = FMath::Min(GripFrom.Z, GripTo.Z) - Offset.Z;
			const float Highest = GetClearanceBetween(Trip.From.GetLocation(), Trip.To.GetLocation(), { Trip.Piece });
			CarryHeight = FMath::Max(CarryHeight, Highest + Set->CarryClearance - BaseHeight);
		}
		AddKey(TravelTime(GripFrom, GripTo), GripTo, 1.0f, FMath::Max(CarryHeight, KINDA_SMALL_NUMBER));
		Out.Actions.Add({ Time, EHandAction::Release, Trip.Piece });

		FHandCarry& Carry = Out.Carries.AddDefaulted_GetRef();
		Carry.Piece = Trip.Piece;
		Carry.Start = CarryStart;
		Carry.End = Time;
		Carry.From = Trip.From;
		Carry.To = Trip.To;
		Carry.GripOffset = Offset;

		// Let go, and lift off.
		AddKey(Set->ReleaseTime / Rate, GripTo + Up * 0.5f, 0.0f, 0.0f);
		AddKey(Set->LowerTime / Rate, ApproachTo, 0.0f, 0.0f);
		LastApproach = ApproachTo;
	}

	Out.LetGoTime = Time;
	Out.Actions.Add({ Time, EHandAction::LetGo, INDEX_NONE });

	// Fit the capture trip into CaptureTripTime: stretch or squeeze its keys, shift everything after.
	if (CaptureTrips > 0 && CaptureTo > CaptureFrom)
	{
		const float Planned = CaptureTo - CaptureFrom;
		const float Wanted = Set->CaptureTripTime / Rate;
		auto Remap = [CaptureFrom, CaptureTo, Planned, Wanted](float InTime)
		{
			if (InTime <= CaptureFrom)
			{
				return InTime;
			}
			if (InTime <= CaptureTo)
			{
				return CaptureFrom + (InTime - CaptureFrom) * (Wanted / Planned);
			}
			return InTime + (Wanted - Planned);
		};
		for (FHandKey& Key : Out.Keys)
		{
			Key.Time = Remap(Key.Time);
		}
		for (FHandAction& Action : Out.Actions)
		{
			Action.Time = Remap(Action.Time);
		}
		for (FHandCarry& Carry : Out.Carries)
		{
			Carry.Start = Remap(Carry.Start);
			Carry.End = Remap(Carry.End);
		}
		Out.LetGoTime = Remap(Out.LetGoTime);
	}
}

void UEGChessPieceVisuals::EvaluateHandPath(const FHandPlan& Plan, float Time, FVector& OutLocation, float& OutReach, float& OutGrip) const
{
	using namespace EGChessPieceVisualsPrivate;

	const TArray<FHandKey>& Keys = Plan.Keys;
	int32 Next = 0;
	while (Next < Keys.Num() && Keys[Next].Time <= Time)
	{
		++Next;
	}
	if (Next == 0 || Next == Keys.Num())
	{
		const FHandKey& Key = Keys[Next == 0 ? 0 : Keys.Num() - 1];
		OutLocation = Key.Location;
		OutReach = Key.Reach;
		OutGrip = Key.Grip;
		return;
	}

	const FHandKey& A = Keys[Next - 1];
	const FHandKey& B = Keys[Next];
	const float Alpha = (Time - A.Time) / FMath::Max(B.Time - A.Time, KINDA_SMALL_NUMBER);
	if (B.CarryHeight > 0.0f)
	{
		const float Across = MinJerk((Alpha - CarryLead) / (1.0f - 2.0f * CarryLead));
		const float Lift = MinJerk(Alpha / CarryRise) * MinJerk((1.0f - Alpha) / CarryRise);
		OutLocation = FMath::Lerp(A.Location, B.Location, Across) + FVector(0.0f, 0.0f, B.CarryHeight * Lift);
	}
	else
	{
		OutLocation = FMath::Lerp(A.Location, B.Location, MinJerk(Alpha));
	}
	OutReach = FMath::Lerp(A.Reach, B.Reach, MinJerk(Alpha));
	OutGrip = FMath::Lerp(A.Grip, B.Grip, FMath::SmoothStep(0.0f, 1.0f, Alpha));
}

void UEGChessPieceVisuals::TickHand(double Now)
{
	using namespace EGChessPieceVisualsPrivate;

	FHandPlan& Plan = Step.Hand;
	if (Plan.Keys.Num() == 0)
	{
		return;
	}
	const float Time = static_cast<float>(Now - Step.StartTime);
	while (!Plan.bLetGo && Plan.NextAction < Plan.Actions.Num() && Plan.Actions[Plan.NextAction].Time <= Time)
	{
		DoHandAction(Plan.Actions[Plan.NextAction++], Now);
	}
	if (Plan.bLetGo)
	{
		return;
	}

	FVector Location;
	float Reach = 0.0f;
	float Grip = 0.0f;
	EvaluateHandPath(Plan, Time, Location, Reach, Grip);

	// A carried piece sits on the planned grip point, where the hand is told to be.
	for (const FHandCarry& Carry : Plan.Carries)
	{
		if (Time >= Carry.Start && Time < Carry.End)
		{
			const float Alpha = MinJerk((Time - Carry.Start) / FMath::Max(Carry.End - Carry.Start, KINDA_SMALL_NUMBER));
			const FQuat Rotation = FQuat::Slerp(Carry.From.GetRotation(), Carry.To.GetRotation(), Alpha);
			const FVector Scale = FMath::Lerp(Carry.From.GetScale3D(), Carry.To.GetScale3D(), Alpha);
			PlacePiece(Carry.Piece, FTransform(Rotation, Location - Rotation.RotateVector(Carry.GripOffset), Scale));
		}
	}

	const AEGChessTableActor* T = Table.Get();
	const USceneComponent* Surface = T ? T->GetBoardSurface() : nullptr;
	FEGChessHandState& State = Plan.State;
	State.bActive = true;
	State.bFollowsPointer = false;
	State.ReachAlpha = Reach;
	State.GripAlpha = Grip;
	State.HoverAlpha = 1.0f; // out over the board the fingers open, as a live hand's do
	State.GripLocation = Surface ? Surface->GetComponentTransform().TransformPosition(Location) : Location;
	State.LookLocation = State.GripLocation;
	State.UpVector = Surface ? Surface->GetUpVector() : FVector::UpVector;
	PushHandState(State);
}

void UEGChessPieceVisuals::DoHandAction(const FHandAction& Action, double Now)
{
	const FEGChessMove& Move = Step.Record.Move;
	const EEGChessColor Mover = Step.Record.MoverColor;
	const EEGChessColor Victim = Mover == EEGChessColor::White ? EEGChessColor::Black : EEGChessColor::White;
	const int32 CapturedSquare = Move.HasFlag(EGChessMoveFlags::EnPassant) ? Move.To + (Mover == EEGChessColor::White ? -8 : 8) : Move.To;
	const int32 RookFrom = Move.HasFlag(EGChessMoveFlags::CastleKingSide) ? Move.To + 1 : Move.To - 2;

	switch (Action.Type)
	{
	case EHandAction::Grip:
		if (Action.Piece == Step.MoverPiece)
		{
			Step.bPicked = true;
			Step.PickedAt = Now;
			EmitEvent(EEGChessEvent::PiecePickUp, Move.From, Step.Record.MovedType, Mover, true);
		}
		else if (Action.Piece == Step.CapturedPiece)
		{
			EmitEvent(EEGChessEvent::PiecePickUp, CapturedSquare, Step.Record.CapturedType, Victim, true);
		}
		else if (Action.Piece == Step.RookPiece)
		{
			EmitEvent(EEGChessEvent::PiecePickUp, RookFrom, EEGChessPieceType::Rook, Mover, true);
		}
		break;

	case EHandAction::Release:
		if (!States.IsValidIndex(Action.Piece))
		{
			break;
		}
		if (Action.Piece == Step.MoverPiece)
		{
			ApplyMesh(Action.Piece); // promotion swaps as the pawn lands
		}
		PlacePiece(Action.Piece, RestingTransform(Action.Piece));
		if (Action.Piece == Step.MoverPiece)
		{
			EEGChessEvent Event = EEGChessEvent::PiecePlace;
			if (Move.Promotion != EEGChessPieceType::None)
			{
				Event = EEGChessEvent::Promote;
			}
			else if (Move.IsCapture())
			{
				Event = EEGChessEvent::Capture;
			}
			const EEGChessPieceType Landed = Move.Promotion != EEGChessPieceType::None ? Move.Promotion : Step.Record.MovedType;
			EmitEvent(Event, Move.To, Landed, Mover, true);
			// Castling announces itself, and any check, once the rook is down too.
			if (Step.Record.bGivesCheck && !Move.IsCastle())
			{
				EmitEvent(EEGChessEvent::Check, Move.To, Step.Record.MovedType, Mover, true);
			}
		}
		else if (Action.Piece == Step.RookPiece)
		{
			EmitEvent(EEGChessEvent::Castle, Step.RookTo, EEGChessPieceType::Rook, Mover, true);
			if (Step.Record.bGivesCheck)
			{
				EmitEvent(EEGChessEvent::Check, Move.To, Step.Record.MovedType, Mover, true);
			}
		}
		break;

	case EHandAction::LetGo:
		LetGoOfBoard(Now);
		break;
	}
}

void UEGChessPieceVisuals::LetGoOfBoard(double Now)
{
	FHandPlan& Plan = Step.Hand;
	Plan.bLetGo = true;
	FEGChessHandState State = Plan.State;
	State.bActive = false;
	PushHandState(State);
	if (USkeletalMeshComponent* Mesh = HandTickMesh.Get())
	{
		Mesh->RemoveTickPrerequisiteComponent(this);
	}
	HandTickMesh.Reset();

	// Whatever the plan moved rests exactly where the record put it.
	for (const int32 Piece : { Step.CapturedPiece, Step.MoverPiece, Step.RookPiece })
	{
		if (States.IsValidIndex(Piece))
		{
			PlacePiece(Piece, RestingTransform(Piece));
		}
	}
	Step.bLanded = true;

	// The same hand goes on to the clock.
	float PressDelay = 0.15f;
	const AEGChessTableActor* T = Table.Get();
	if (T)
	{
		T->GetSeat(Step.MoverSeat)->PlayClockPressClip(Step.Timing.PlayRate, Step.Timing.bRightHand, PressDelay);
	}
	Step.Timing.ClockPressTime = static_cast<float>(Now - Step.StartTime) + PressDelay;
}

void UEGChessPieceVisuals::PushHandState(const FEGChessHandState& State)
{
	APawn* Pawn = Step.Timing.Pawn.Get();
	if (Pawn && Pawn->Implements<UEGChessCharacterInterface>())
	{
		IEGChessCharacterInterface::Execute_SetChessHandState(Pawn, State);
	}
}

void UEGChessPieceVisuals::EndHand()
{
	// A move cut short (the next move, a takeback, a snap) still has to hand the arm back.
	if (Step.Kind == EStepKind::Move && Step.Timing.bHandDriven && Step.Hand.Keys.Num() > 0 && !Step.Hand.bLetGo)
	{
		Step.Hand.bLetGo = true;
		FEGChessHandState State = Step.Hand.State;
		State.bActive = false;
		PushHandState(State);
	}
	if (USkeletalMeshComponent* Mesh = HandTickMesh.Get())
	{
		Mesh->RemoveTickPrerequisiteComponent(this);
	}
	HandTickMesh.Reset();
}

// ---------------------------------------------------------------------------------------------
// Live hands
// ---------------------------------------------------------------------------------------------

EEGChessColor UEGChessPieceVisuals::GetPresentedSideToMove() const
{
	if (PresentedHistory.Num() > 0)
	{
		return EGChess::Opposite(PresentedHistory.Last().MoverColor);
	}
	if (!bStartSideCached || CachedStartSideFEN != PresentedStartFEN)
	{
		FEGChessPosition Start;
		CachedStartSide = Start.FromFEN(PresentedStartFEN.IsEmpty() ? FString(FEGChessPosition::StartFEN()) : PresentedStartFEN)
			? Start.SideToMove
			: EEGChessColor::White;
		CachedStartSideFEN = PresentedStartFEN;
		bStartSideCached = true;
	}
	return CachedStartSide;
}

bool UEGChessPieceVisuals::WantsLiveHandTick() const
{
	if (LiveHands[0].bActive || LiveHands[1].bActive)
	{
		return true;
	}
	const AEGChessTableActor* T = Table.Get();
	if (!T || T->GetPhase() != EEGChessPhase::Playing)
	{
		return false;
	}
	for (const EEGChessSeat Seat : { EEGChessSeat::A, EEGChessSeat::B })
	{
		if (T->GetSeatHandIntent(Seat).bRaised && T->UsesLiveHand(Seat))
		{
			return true;
		}
	}
	return false;
}

bool UEGChessPieceVisuals::IsHandInMoveStep(EEGChessSeat Seat) const
{
	return Step.Kind == EStepKind::Move && Step.MoverSeat == Seat && Step.Timing.bHandDriven
		&& Step.Hand.Keys.Num() > 0 && !Step.Hand.bLetGo;
}

void UEGChessPieceVisuals::TickLiveHands(float DeltaTime)
{
	for (const EEGChessSeat Seat : { EEGChessSeat::A, EEGChessSeat::B })
	{
		TickLiveHand(Seat, DeltaTime);
	}
}

float UEGChessPieceVisuals::GetNearbyPieceTop(const FVector& Location, int32 Ignore, const UEGChessAnimationSet* Set) const
{
	using namespace EGChessPieceVisualsPrivate;

	const AEGChessTableActor* T = Table.Get();
	if (!T || !Set)
	{
		return 0.0f;
	}
	const FEGChessBoardGeometry Geometry = T->GetBoardGeometry();
	const UEGChessPieceSet* PieceSet = T->GetPieceSet();
	const float Radius = Set->HoverClearanceRadius * Geometry.SquareSize;
	float Highest = 0.0f;
	for (int32 Piece = 0; Piece < States.Num(); ++Piece)
	{
		if (Piece == Ignore || States[Piece].Square == INDEX_NONE)
		{
			continue;
		}
		if (FVector::Dist2D(Geometry.SquareToLocal(States[Piece].Square), Location) <= Radius)
		{
			Highest = FMath::Max(Highest, PieceHeight(PieceSet, States[Piece].Type, GetPieceScale(), Geometry.SquareSize));
		}
	}
	return Highest;
}

FVector UEGChessPieceVisuals::GetGripPointOnSquare(int32 Piece, int32 Square, const UEGChessAnimationSet* Set) const
{
	const FPieceState& State = States[Piece];
	const FTransform Rest = SquareTransform(Square, State.Type, State.Color);
	return Rest.GetLocation() + Rest.GetRotation().RotateVector(GetGripOffset(State.Type, Set ? Set->GripHeightFraction : 0.75f));
}

void UEGChessPieceVisuals::TickLiveHand(EEGChessSeat Seat, float DeltaTime)
{
	using namespace EGChessPieceVisualsPrivate;

	FLiveHand& Hand = LiveHands[EGChess::SeatIndex(Seat)];
	AEGChessTableActor* T = Table.Get();
	if (!T || IsHandInMoveStep(Seat))
	{
		return; // a committed move of this seat's own has the hand until it lets go
	}

	// Up only on its own turn as the board shows it, and while nothing else moves every piece. A hand
	// setting its piece down stays for the move to land even if the game ends first (checkmate).
	const UEGChessSeatComponent* SeatComponent = T->GetSeat(Seat);
	APawn* Pawn = SeatComponent->GetSeatedPawn();
	const UEGChessAnimationSet* Set = T->UsesLiveHand(Seat) ? SeatComponent->GetMoveAnimationSet() : nullptr;
	const FEGChessHandIntent Intent = T->GetSeatHandIntent(Seat);
	const EEGChessColor Color = T->GetSeatColor(Seat);
	const bool bPlacing = EGChess::IsValidSquare(Intent.PlaceSquare);
	const bool bWanted = Set && Pawn && Intent.bRaised && States.Num() > 0
		&& GetPresentedSideToMove() == Color
		&& Step.Kind != EStepKind::Glide
		&& (T->GetPhase() == EEGChessPhase::Playing || bPlacing);

	if (Hand.bActive && (!bWanted || Hand.Pawn.Get() != Pawn))
	{
		LowerLiveHand(Hand, /*bTellCharacter=*/ true);
	}
	if (!bWanted)
	{
		return;
	}

	const FEGChessBoardGeometry Geometry = T->GetBoardGeometry();
	const float Rate = T->GetMatchRulesRef().MoveAnimation == EEGChessMoveAnimation::Fast ? FMath::Max(Set->FastPlayRate, 1.0f) : 1.0f;
	const FVector Up(0.0f, 0.0f, 1.0f);
	const float Min = -0.5f * Geometry.SquareSize;
	const float Max = 7.5f * Geometry.SquareSize;
	const FVector Pointer(FMath::Clamp(Intent.Pointer.X, Min, Max), FMath::Clamp(Intent.Pointer.Y, Min, Max), 0.0f);

	// What it should hold: one of its own player's pieces, standing on the board as shown.
	int32 Wanted = PieceAt.IsValidIndex(Intent.HeldSquare) ? PieceAt[Intent.HeldSquare] : INDEX_NONE;
	if (!States.IsValidIndex(Wanted) || States[Wanted].Color != Color || IsPieceInStep(Wanted))
	{
		Wanted = INDEX_NONE;
	}
	if (Hand.bActive && States.IsValidIndex(Hand.HeldPiece) && (IsPieceInStep(Hand.HeldPiece) || States[Hand.HeldPiece].Square == INDEX_NONE))
	{
		Hand.HeldPiece = INDEX_NONE; // something else moves it now
		Hand.Grip = 0.0f;
	}

	// The open hand stays over the pieces near it; a carried piece's base clears them.
	auto HoverHeight = [&](const FVector& At)
	{
		return FMath::Max(GetNearbyPieceTop(At, Hand.HeldPiece, Set) + Set->HoverClearance, Set->HoverMinHeight);
	};
	auto CarryHeight = [&](const FVector& At, int32 Piece)
	{
		const float GripRise = GetGripPointOnSquare(Piece, States[Piece].Square, Set).Z;
		return FMath::Max(GetNearbyPieceTop(At, Piece, Set) + Set->CarryClearance, Set->ApproachHeight) + GripRise;
	};

	if (!Hand.bActive)
	{
		// Rising: it eases off the animation (over ReachTime) straight onto the point over the pointer.
		Hand = FLiveHand();
		Hand.bActive = true;
		Hand.Pawn = Pawn;
		Hand.Location = FVector(Pointer.X, Pointer.Y, HoverHeight(Pointer));
		if (USkeletalMeshComponent* Mesh = GetHandMesh(Pawn))
		{
			Mesh->AddTickPrerequisiteComponent(this);
			Hand.TickMesh = Mesh;
		}
	}
	Hand.RaisedTime += DeltaTime;
	Hand.BobTime += DeltaTime;
	Hand.Reach = MinJerk(Hand.RaisedTime / FMath::Max(Set->ReachTime / Rate, 0.01f));

	// The goal: put back what it should no longer hold, pick up what it should, then carry, or set down.
	enum class EGoal : uint8 { Hover, Pick, Return, Carry, Place };
	EGoal Goal = EGoal::Hover;
	const bool bHolding = States.IsValidIndex(Hand.HeldPiece);
	if (bHolding && Hand.HeldPiece != Wanted)
	{
		Goal = EGoal::Return;
	}
	else if (!bHolding && Wanted != INDEX_NONE)
	{
		Goal = EGoal::Pick;
	}
	else if (bHolding)
	{
		Goal = bPlacing ? EGoal::Place : EGoal::Carry;
	}

	FVector Target(Pointer.X, Pointer.Y, HoverHeight(Pointer));
	float GripTarget = 0.0f;
	bool bHoming = false; // over a point and coming straight down onto it
	FVector Point = FVector::ZeroVector;
	switch (Goal)
	{
	case EGoal::Hover:
		break;
	case EGoal::Pick:
		Point = GetGripPointOnSquare(Wanted, States[Wanted].Square, Set);
		bHoming = true;
		break;
	case EGoal::Return:
		Point = GetGripPointOnSquare(Hand.HeldPiece, States[Hand.HeldPiece].Square, Set);
		bHoming = true;
		GripTarget = 1.0f;
		break;
	case EGoal::Carry:
		Target = FVector(Pointer.X, Pointer.Y, CarryHeight(Pointer, Hand.HeldPiece));
		GripTarget = 1.0f;
		break;
	case EGoal::Place:
	{
		Point = GetGripPointOnSquare(Hand.HeldPiece, Intent.PlaceSquare, Set);
		GripTarget = 1.0f;
		const int32 Taken = PieceAt[Intent.PlaceSquare];
		if (States.IsValidIndex(Taken) && Taken != Hand.HeldPiece)
		{
			// A capture: it waits over the piece it takes, which the move's plan clears off as it comes down.
			const float GripRise = GetGripPointOnSquare(Hand.HeldPiece, States[Hand.HeldPiece].Square, Set).Z;
			const float OverTaken = PieceHeight(T->GetPieceSet(), States[Taken].Type, GetPieceScale(), Geometry.SquareSize) + Set->CarryClearance + GripRise;
			Target = FVector(Point.X, Point.Y, FMath::Max(OverTaken, CarryHeight(Point, Hand.HeldPiece)));
		}
		else
		{
			bHoming = true;
		}
		break;
	}
	}

	const float Sideways = bHoming ? FVector::Dist2D(Hand.Location, Point) : 0.0f;
	const bool bOver = bHoming && Sideways <= LiveDescendRadius;
	const bool bArrived = bHoming && FVector::Dist(Hand.Location, Point) <= LiveArriveDistance;
	if (bHoming)
	{
		// Across at the height it travels at, then straight down.
		const float Cruise = FMath::Max(Point.Z + Set->ApproachHeight, bHolding ? CarryHeight(Hand.Location, Hand.HeldPiece) : HoverHeight(Hand.Location));
		Target = bOver ? Point : FVector(Point.X, Point.Y, Cruise);
		if (Goal == EGoal::Pick)
		{
			GripTarget = bArrived ? 1.0f : (bOver ? 0.25f : 0.0f); // the fingers part on the way down, then close
		}
		else if (Goal == EGoal::Return && bArrived)
		{
			GripTarget = 0.0f;
		}
	}
	else if (Set->HoverBobHeight > 0.0f)
	{
		Target.Z += Set->HoverBobHeight * FMath::Sin(UE_TWO_PI * Hand.BobTime / FMath::Max(Set->HoverBobPeriod, 0.1f));
	}

	// Going for a piece (or a square) it moves briskly, so a click picks up at once; following the pointer it trails.
	const float SmoothTime = bHoming ? FMath::Min(Set->HoverSmoothTime, 0.5f * Set->LowerTime / Rate) : Set->HoverSmoothTime;
	FollowSpring(Hand.Location, Hand.Velocity, Target, DeltaTime, SmoothTime, Set->HoverMaxSpeed);
	const float GripRate = GripTarget > Hand.Grip ? Rate / FMath::Max(Set->GripTime, 0.01f) : Rate / FMath::Max(Set->ReleaseTime, 0.01f);
	Hand.Grip = FMath::FInterpConstantTo(Hand.Grip, GripTarget, DeltaTime, GripRate);

	// Closed on the piece: it leaves its square. Opened over its square: it stays there.
	if (Goal == EGoal::Pick && bArrived && Hand.Grip >= 1.0f - KINDA_SMALL_NUMBER)
	{
		const FPieceState& State = States[Wanted];
		const FTransform Rest = SquareTransform(State.Square, State.Type, State.Color);
		Hand.HeldPiece = Wanted;
		Hand.HeldGripOffset = GetGripOffset(State.Type, Set->GripHeightFraction);
		Hand.HeldRotation = Rest.GetRotation();
		Hand.HeldScale = Rest.GetScale3D();
		Hand.Location = Point;
		Hand.Velocity = FVector::ZeroVector;
		EmitHandEvent(Seat, EEGChessEvent::PiecePickUp, State.Square, State.Type, State.Color);
	}
	else if (Goal == EGoal::Return && bArrived && Hand.Grip <= KINDA_SMALL_NUMBER)
	{
		const FPieceState& State = States[Hand.HeldPiece];
		PlacePiece(Hand.HeldPiece, RestingTransform(Hand.HeldPiece));
		EmitHandEvent(Seat, EEGChessEvent::PiecePlace, State.Square, State.Type, State.Color);
		Hand.HeldPiece = INDEX_NONE;
	}

	// The held piece hangs from the grip point, where the hand is told to be.
	if (States.IsValidIndex(Hand.HeldPiece))
	{
		PlacePiece(Hand.HeldPiece, FTransform(Hand.HeldRotation, Hand.Location - Hand.HeldRotation.RotateVector(Hand.HeldGripOffset), Hand.HeldScale));
	}

	const USceneComponent* Surface = T->GetBoardSurface();
	FEGChessHandState& State = Hand.State;
	State.bActive = true;
	State.bFollowsPointer = true;
	State.bRightHand = T->IsClockOnSeatRight(Seat);
	State.ReachAlpha = Hand.Reach;
	State.GripAlpha = Hand.Grip;
	State.HoverAlpha = 1.0f;
	State.GripLocation = Surface ? Surface->GetComponentTransform().TransformPosition(Hand.Location) : Hand.Location;
	State.LookLocation = State.GripLocation;
	State.UpVector = Surface ? Surface->GetUpVector() : FVector::UpVector;
	State.BlendArcHeight = Set->BlendArcHeight;
	State.BlendOutTime = Set->BlendOutTime;
	if (Pawn->Implements<UEGChessCharacterInterface>())
	{
		IEGChessCharacterInterface::Execute_SetChessHandState(Pawn, State);
	}
}

void UEGChessPieceVisuals::LowerLiveHand(FLiveHand& Hand, bool bTellCharacter)
{
	if (!Hand.bActive)
	{
		return;
	}
	if (States.IsValidIndex(Hand.HeldPiece) && !IsPieceInStep(Hand.HeldPiece))
	{
		PlacePiece(Hand.HeldPiece, RestingTransform(Hand.HeldPiece));
	}
	APawn* Pawn = Hand.Pawn.Get();
	if (bTellCharacter && Pawn && Pawn->Implements<UEGChessCharacterInterface>())
	{
		FEGChessHandState State = Hand.State;
		State.bActive = false;
		IEGChessCharacterInterface::Execute_SetChessHandState(Pawn, State);
	}
	HandLiveHandToPlan(Hand);
}

void UEGChessPieceVisuals::HandLiveHandToPlan(FLiveHand& Hand)
{
	// A step on the same mesh keeps the tick order it set up itself.
	USkeletalMeshComponent* Mesh = Hand.TickMesh.Get();
	if (Mesh && Mesh != HandTickMesh.Get())
	{
		Mesh->RemoveTickPrerequisiteComponent(this);
	}
	Hand = FLiveHand();
}

void UEGChessPieceVisuals::DropLiveHandPieces()
{
	for (FLiveHand& Hand : LiveHands)
	{
		Hand.HeldPiece = INDEX_NONE;
		Hand.Grip = 0.0f;
	}
}

void UEGChessPieceVisuals::EmitHandEvent(EEGChessSeat Seat, EEGChessEvent Event, int32 Square, EEGChessPieceType Type, EEGChessColor Color)
{
	AEGChessTableActor* T = Table.Get();
	if (!T)
	{
		return;
	}
	FEGChessEventContext Context;
	Context.Event = Event;
	Context.PieceType = Type;
	Context.PieceColor = Color;
	Context.FromSquare = Square;
	Context.ToSquare = Square;
	Context.Seat = Seat;
	Context.bMovedByCharacter = true;
	Context.bLocalPlayerActed = T->IsSeatLocallyControlled(Seat);
	Context.bSpatial = true;
	Context.Location = T->GetBoardGeometry().SquareToWorld(Square);
	T->TriggerEvent(Context);
}

USkeletalMeshComponent* UEGChessPieceVisuals::GetHandMesh(const APawn* Pawn) const
{
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
	return Character ? Character->GetMesh() : nullptr;
}

// ---------------------------------------------------------------------------------------------
// Tweens
// ---------------------------------------------------------------------------------------------

void UEGChessPieceVisuals::AddTween(int32 Piece, const FTransform& To, float Duration, float Lift, bool bArc, double Now)
{
	if (!PieceComponents.IsValidIndex(Piece) || !PieceComponents[Piece])
	{
		return;
	}
	FTween Tween;
	Tween.Piece = Piece;
	Tween.From = PieceComponents[Piece]->GetRelativeTransform();
	Tween.To = To;
	Tween.StartTime = Now;
	Tween.Duration = FMath::Max(Duration, 0.01f);
	Tween.Lift = Lift;
	Tween.bArc = bArc;
	Step.Tweens.Add(Tween);
}

void UEGChessPieceVisuals::EvaluateTween(const FTween& Tween, double Now) const
{
	UStaticMeshComponent* Component = PieceComponents.IsValidIndex(Tween.Piece) ? PieceComponents[Tween.Piece].Get() : nullptr;
	if (!Component)
	{
		return;
	}
	const float Alpha = FMath::Clamp(static_cast<float>((Now - Tween.StartTime) / Tween.Duration), 0.0f, 1.0f);
	const float Eased = FMath::SmoothStep(0.0f, 1.0f, Alpha);

	float Height = 0.0f;
	if (Tween.bArc && Tween.Lift > 0.0f)
	{
		const AEGChessTableActor* T = Table.Get();
		const UEGChessPieceSet* Set = T ? T->GetPieceSet() : nullptr;
		const UCurveFloat* Curve = Set ? Set->MoveArc.Get() : nullptr;
		Height = Tween.Lift * (Curve ? Curve->GetFloatValue(Alpha) : FMath::Sin(Alpha * PI));
	}

	FTransform Result;
	Result.SetLocation(FMath::Lerp(Tween.From.GetLocation(), Tween.To.GetLocation(), Eased) + FVector(0.0f, 0.0f, Height));
	Result.SetRotation(FQuat::Slerp(Tween.From.GetRotation(), Tween.To.GetRotation(), Eased));
	Result.SetScale3D(FMath::Lerp(Tween.From.GetScale3D(), Tween.To.GetScale3D(), Eased));
	Component->SetRelativeTransform(Result);
}

bool UEGChessPieceVisuals::AreTweensDone(double Now) const
{
	for (const FTween& Tween : Step.Tweens)
	{
		if (Now < Tween.StartTime + Tween.Duration)
		{
			return false;
		}
	}
	return true;
}

void UEGChessPieceVisuals::FinishTweens()
{
	for (const FTween& Tween : Step.Tweens)
	{
		PlacePiece(Tween.Piece, Tween.To);
	}
	Step.Tweens.Reset();
}

// ---------------------------------------------------------------------------------------------
// Local presentation
// ---------------------------------------------------------------------------------------------

void UEGChessPieceVisuals::SetSelection(int32 Square)
{
	const int32 NewPiece = PieceAt.IsValidIndex(Square) ? PieceAt[Square] : INDEX_NONE;
	if (NewPiece == SelectedPiece)
	{
		return;
	}
	const int32 Previous = SelectedPiece;
	SelectedPiece = NewPiece;
	// A new selection starts on its own square; the driver carries it from there.
	bCarrying = false;
	if (States.IsValidIndex(Previous) && !IsPieceInStep(Previous))
	{
		PlacePiece(Previous, RestingTransform(Previous));
	}
	if (States.IsValidIndex(SelectedPiece) && !IsPieceInStep(SelectedPiece))
	{
		PlacePiece(SelectedPiece, RestingTransform(SelectedPiece));
	}
}

void UEGChessPieceVisuals::SetCarry(bool bActive, const FVector& SurfaceLocal)
{
	const bool bWasCarrying = bCarrying;
	bCarrying = bActive;
	CarryLocal = SurfaceLocal;
	if ((bActive || bWasCarrying) && States.IsValidIndex(SelectedPiece) && !IsPieceInStep(SelectedPiece))
	{
		PlacePiece(SelectedPiece, RestingTransform(SelectedPiece));
	}
}

bool UEGChessPieceVisuals::IsPieceInStep(int32 Piece) const
{
	return Step.Kind != EStepKind::None
		&& (Step.Kind != EStepKind::Move || Piece == Step.MoverPiece || Piece == Step.CapturedPiece || Piece == Step.RookPiece);
}

void UEGChessPieceVisuals::SetOutlines(const TArray<int32>& Squares, int32 Stencil, int32 EmphasisSquare, int32 EmphasisStencil)
{
	TArray<int32> NewPieces;
	for (const int32 Square : Squares)
	{
		const int32 Piece = PieceAt.IsValidIndex(Square) ? PieceAt[Square] : INDEX_NONE;
		if (Piece != INDEX_NONE)
		{
			NewPieces.AddUnique(Piece);
		}
	}
	const int32 EmphasisPiece = PieceAt.IsValidIndex(EmphasisSquare) ? PieceAt[EmphasisSquare] : INDEX_NONE;
	if (EmphasisPiece != INDEX_NONE)
	{
		NewPieces.AddUnique(EmphasisPiece);
	}
	for (const int32 Piece : OutlinedPieces)
	{
		if (!NewPieces.Contains(Piece) && PieceComponents.IsValidIndex(Piece) && PieceComponents[Piece])
		{
			PieceComponents[Piece]->SetRenderCustomDepth(false);
		}
	}
	for (const int32 Piece : NewPieces)
	{
		if (PieceComponents.IsValidIndex(Piece) && PieceComponents[Piece])
		{
			PieceComponents[Piece]->SetRenderCustomDepth(true);
			PieceComponents[Piece]->SetCustomDepthStencilValue(Piece == EmphasisPiece && EmphasisStencil > 0 ? EmphasisStencil : Stencil);
		}
	}
	OutlinedPieces = NewPieces;
}

void UEGChessPieceVisuals::ShowGhost(int32 FromSquare, int32 TargetSquare)
{
	const AEGChessTableActor* T = Table.Get();
	const UEGChessPieceSet* Set = T ? T->GetPieceSet() : nullptr;
	UMaterialInterface* GhostMaterial = Set ? EGChessEngineAssets::Resolve(Set->GhostMaterial) : nullptr;
	if (!GhostMaterial)
	{
		// No authored ghost: the engine's unlit see-through material in the set's tint.
		const FLinearColor Tint = Set ? Set->GhostColor : FLinearColor(1.0f, 0.78f, 0.35f, 0.4f);
		UMaterialInterface* Base = EGChessEngineAssets::UnlitTranslucentMaterial();
		if (!FallbackGhostMaterial || FallbackGhostMaterial->Parent != Base)
		{
			FallbackGhostMaterial = Base ? UMaterialInstanceDynamic::Create(Base, this) : nullptr;
		}
		if (FallbackGhostMaterial)
		{
			FallbackGhostMaterial->SetVectorParameterValue(TEXT("Color"), Tint);
		}
		GhostMaterial = FallbackGhostMaterial;
	}
	const int32 Piece = PieceAt.IsValidIndex(FromSquare) ? PieceAt[FromSquare] : INDEX_NONE;
	if (!GhostComponent || !GhostMaterial || !States.IsValidIndex(Piece) || !PieceComponents[Piece])
	{
		HideGhost();
		return;
	}
	UStaticMeshComponent* Source = PieceComponents[Piece];
	const FTransform Transform = SquareTransform(TargetSquare, States[Piece].Type, States[Piece].Color);

	GhostComponent->SetStaticMesh(Source->GetStaticMesh());
	const int32 NumMaterials = FMath::Max(1, GhostComponent->GetNumMaterials());
	for (int32 Slot = 0; Slot < NumMaterials; ++Slot)
	{
		GhostComponent->SetMaterial(Slot, GhostMaterial);
	}
	GhostComponent->SetRelativeTransform(Transform);
	GhostComponent->SetVisibility(true);

	const int32 OutlineStencil = !Set ? 0 : (Set->GhostOutlineStencil == 0 ? Set->OutlineStencil : Set->GhostOutlineStencil);
	if (GhostDepthComponent && OutlineStencil > 0)
	{
		GhostDepthComponent->SetStaticMesh(Source->GetStaticMesh());
		for (int32 Slot = 0; Slot < Source->GetNumMaterials(); ++Slot)
		{
			GhostDepthComponent->SetMaterial(Slot, Source->GetMaterial(Slot));
		}
		GhostDepthComponent->SetCustomDepthStencilValue(OutlineStencil);
		GhostDepthComponent->SetRelativeTransform(Transform);
		GhostDepthComponent->SetVisibility(true);
	}
	else if (GhostDepthComponent)
	{
		GhostDepthComponent->SetVisibility(false);
	}
}

void UEGChessPieceVisuals::HideGhost()
{
	if (GhostComponent)
	{
		GhostComponent->SetVisibility(false);
	}
	if (GhostDepthComponent)
	{
		GhostDepthComponent->SetVisibility(false);
	}
}

bool UEGChessPieceVisuals::PickPiece(const FVector& Origin, const FVector& Direction, int32& OutSquare) const
{
	using namespace EGChessPieceVisualsPrivate;

	const AEGChessTableActor* T = Table.Get();
	if (!T)
	{
		return false;
	}
	const FEGChessBoardGeometry Geometry = T->GetBoardGeometry();
	const FVector LocalOrigin = Geometry.SurfaceToWorld.InverseTransformPosition(Origin);
	const FVector LocalDir = Geometry.SurfaceToWorld.InverseTransformVector(Direction).GetSafeNormal();
	const UEGChessPieceSet* Set = T->GetPieceSet();
	const float Scale = GetPieceScale();

	float BestT = MAX_flt;
	int32 BestSquare = INDEX_NONE;
	for (int32 Square = 0; Square < 64; ++Square)
	{
		const int32 Piece = PieceAt[Square];
		if (!States.IsValidIndex(Piece))
		{
			continue;
		}
		float Radius = Geometry.SquareSize * 0.4f;
		float Height = Geometry.SquareSize * 1.2f;
		if (Set && !Set->GetVisual(States[Piece].Type).Mesh.IsNull())
		{
			Radius = Set->GetVisual(States[Piece].Type).PickRadius * Scale;
			Height = Set->GetVisual(States[Piece].Type).PickHeight * Scale;
		}
		const FVector Base = Geometry.SquareToLocal(Square);
		float RayT = 0.0f;
		const float Distance = RaySegmentDistance(LocalOrigin, LocalDir, Base, Base + FVector(0.0f, 0.0f, Height), RayT);
		if (Distance <= Radius && RayT < BestT)
		{
			BestT = RayT;
			BestSquare = Square;
		}
	}
	OutSquare = BestSquare;
	return BestSquare != INDEX_NONE;
}

void UEGChessPieceVisuals::SetClockPressed(EEGChessSeat InPressedSeat, bool bAnimate)
{
	const AEGChessTableActor* T = Table.Get();
	if (!T)
	{
		return;
	}
	UStaticMeshComponent* ButtonA = T->GetClockButton(EEGChessSeat::A);
	UStaticMeshComponent* ButtonB = T->GetClockButton(EEGChessSeat::B);
	if (!ButtonA || !ButtonB)
	{
		return;
	}
	if (!bButtonsCaptured)
	{
		ButtonRaisedA = ButtonA->GetRelativeLocation();
		ButtonRaisedB = ButtonB->GetRelativeLocation();
		bButtonsCaptured = true;
	}
	PressedSeat = InPressedSeat;
	if (bAnimate)
	{
		ButtonTweenStart = GetNow();
		SetComponentTickEnabled(true);
	}
	else
	{
		const UEGChessTableStyle* Style = T->GetTableStyle();
		const float Travel = Style ? Style->ButtonPressTravel : 0.8f;
		ButtonA->SetRelativeLocation(ButtonRaisedA + EGChessPieceVisualsPrivate::PressOffset(ButtonA, PressedSeat == EEGChessSeat::A ? Travel : 0.0f));
		ButtonB->SetRelativeLocation(ButtonRaisedB + EGChessPieceVisualsPrivate::PressOffset(ButtonB, PressedSeat == EEGChessSeat::B ? Travel : 0.0f));
		ButtonTweenStart = -1.0;
	}
}

void UEGChessPieceVisuals::EmitEvent(EEGChessEvent Event, int32 Square, EEGChessPieceType Type, EEGChessColor Color, bool bMovedByCharacter)
{
	AEGChessTableActor* T = Table.Get();
	if (!T)
	{
		return;
	}
	FEGChessEventContext Context;
	Context.Event = Event;
	Context.PieceType = Type;
	Context.PieceColor = Color;
	Context.ToSquare = Square;
	Context.FromSquare = Step.Kind == EStepKind::Move ? Step.Record.Move.From : INDEX_NONE;
	Context.Seat = T->GetSeatForColor(Color);
	Context.bMovedByCharacter = bMovedByCharacter;
	Context.bLocalPlayerActed = T->IsSeatLocallyControlled(Context.Seat);
	Context.bSpatial = true;
	if (Event == EEGChessEvent::ClockPress)
	{
		const UStaticMeshComponent* Button = T->GetClockButton(Step.Kind == EStepKind::Move ? Step.MoverSeat : Context.Seat);
		Context.Location = Button ? Button->GetComponentLocation() : T->GetActorLocation();
	}
	else if (EGChess::IsValidSquare(Square))
	{
		Context.Location = T->GetBoardGeometry().SquareToWorld(Square);
	}
	else
	{
		Context.Location = T->GetBoardGeometry().GetCenterWorld();
	}
	T->TriggerEvent(Context);
}
