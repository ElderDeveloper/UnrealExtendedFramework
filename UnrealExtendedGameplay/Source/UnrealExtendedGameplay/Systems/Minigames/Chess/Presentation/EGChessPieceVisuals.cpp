// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessPieceVisuals.h"

#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EGChessEngineAssets.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessPosition.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessPieceSet.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessTableStyle.h"
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
	Step = FStep();
	SelectedPiece = INDEX_NONE;
	OutlinedPieces.Reset();
	bButtonsCaptured = false;

	if (!GhostComponent && InTable && InTable->GetBoardSurface())
	{
		GhostComponent = NewObject<UStaticMeshComponent>(InTable, NAME_None, RF_Transient);
		GhostComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GhostComponent->SetCastShadow(false);
		GhostComponent->SetupAttachment(InTable->GetBoardSurface());
		GhostComponent->RegisterComponent();
		GhostComponent->SetVisibility(false);
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
		const float Lift = Set ? Set->SelectedLiftHeight * GetPieceScale() : 1.5f;
		Out.AddToTranslation(FVector(0.0f, 0.0f, Lift));
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

	// The identity model advances now; the components catch up through the step.
	ApplyRecord(States, PieceAt, Record);
	PresentedHistory.Add(Record);
}

void UEGChessPieceVisuals::PresentTakeback(const TArray<FEGChessMoveRecord>& History, int32 NewLength)
{
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

void UEGChessPieceVisuals::HandleCharacterNotify(EEGChessSeat Seat, FName Notify, FName PickName, FName PlaceName, FName ClockName)
{
	if (Step.Kind != EStepKind::Move || Step.MoverSeat != Seat)
	{
		return;
	}
	const double Now = GetNow();
	if (Notify == PickName && !Step.bPicked)
	{
		DoPick(Now);
	}
	else if (Notify == PlaceName && !Step.bLanded)
	{
		if (!Step.bPicked)
		{
			DoPick(Now);
		}
		DoLand(Now);
	}
	else if (Notify == ClockName && !Step.bPressed)
	{
		if (!Step.bLanded)
		{
			if (!Step.bPicked)
			{
				DoPick(Now);
			}
			DoLand(Now);
		}
		DoPress();
	}
}

void UEGChessPieceVisuals::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const double Now = GetNow();
	TickStep(Now);

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

	if (Step.Kind == EStepKind::None && ButtonTweenStart < 0.0)
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
		// A character-driven step waits a little past the expected time for its notify. A piece that
		// keeps its own timing waits for nothing; only the clock press still belongs to the hand.
		const float ClockGrace = Step.Timing.bCharacterDriven ? 0.3f : 0.0f;
		const float PieceGrace = Step.Timing.bPieceFollowsHand ? ClockGrace : 0.0f;
		if (!Step.bPicked && Elapsed >= Step.Timing.PickTime + PieceGrace)
		{
			DoPick(Now);
		}
		if (Step.bPicked && !Step.bLanded && Elapsed >= Step.Timing.PlaceTime + PieceGrace && AreTweensDone(Now))
		{
			DoLand(Now);
		}
		if (Step.bLanded && !Step.bPressed && Elapsed >= Step.Timing.ClockPressTime + ClockGrace)
		{
			DoPress();
		}
		if (Step.bLanded && Step.bPressed && AreTweensDone(Now))
		{
			FinishTweens();
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
	FinishTweens();
	for (int32 Piece = 0; Piece < States.Num(); ++Piece)
	{
		PlacePiece(Piece, RestingTransform(Piece));
	}
	Step = FStep();
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
	if (Step.Kind == EStepKind::None)
	{
		if (States.IsValidIndex(Previous))
		{
			PlacePiece(Previous, RestingTransform(Previous));
		}
		if (States.IsValidIndex(SelectedPiece))
		{
			PlacePiece(SelectedPiece, RestingTransform(SelectedPiece));
		}
	}
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
	const int32 Piece = PieceAt.IsValidIndex(FromSquare) ? PieceAt[FromSquare] : INDEX_NONE;
	if (!GhostComponent || !GhostMaterial || !States.IsValidIndex(Piece) || !PieceComponents[Piece])
	{
		HideGhost();
		return;
	}
	GhostComponent->SetStaticMesh(PieceComponents[Piece]->GetStaticMesh());
	const int32 NumMaterials = FMath::Max(1, GhostComponent->GetNumMaterials());
	for (int32 Slot = 0; Slot < NumMaterials; ++Slot)
	{
		GhostComponent->SetMaterial(Slot, GhostMaterial);
	}
	GhostComponent->SetRelativeTransform(SquareTransform(TargetSquare, States[Piece].Type, States[Piece].Color));
	GhostComponent->SetVisibility(true);
}

void UEGChessPieceVisuals::HideGhost()
{
	if (GhostComponent)
	{
		GhostComponent->SetVisibility(false);
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
