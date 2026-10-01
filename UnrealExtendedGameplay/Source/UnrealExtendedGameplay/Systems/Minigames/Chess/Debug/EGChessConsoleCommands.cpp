// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessRules.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessTableActor.h"

#if !UE_BUILD_SHIPPING

namespace EGChessConsole
{
	/** The table nearest the first local player's pawn (the "focused" table), else any. */
	AEGChessTableActor* FindTable(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		FVector Origin = FVector::ZeroVector;
		if (const APlayerController* Controller = World->GetFirstPlayerController())
		{
			if (const APawn* Pawn = Controller->GetPawn())
			{
				Origin = Pawn->GetActorLocation();
			}
		}
		AEGChessTableActor* Best = nullptr;
		float BestDistance = MAX_flt;
		for (TActorIterator<AEGChessTableActor> It(World); It; ++It)
		{
			const float Distance = FVector::DistSquared(It->GetActorLocation(), Origin);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = *It;
			}
		}
		return Best;
	}

	const TCHAR* PhaseName(EEGChessPhase Phase)
	{
		switch (Phase)
		{
		case EEGChessPhase::WaitingForOpponent: return TEXT("WaitingForOpponent");
		case EEGChessPhase::Starting: return TEXT("Starting");
		case EEGChessPhase::Playing: return TEXT("Playing");
		case EEGChessPhase::GameOver: return TEXT("GameOver");
		default: return TEXT("Idle");
		}
	}
}

static TAutoConsoleVariable<int32> CVarEGChessDebug(
	TEXT("EG.Chess.Debug"),
	0,
	TEXT("1 draws chess table squares, anchors and entry spots."),
	ECVF_Cheat);

static FAutoConsoleCommandWithWorld CmdEGChessDump(
	TEXT("EG.Chess.Dump"),
	TEXT("Log the focused chess table's phase, FEN, clocks and seats."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		const AEGChessTableActor* Table = EGChessConsole::FindTable(World);
		if (!Table)
		{
			UE_LOG(LogTemp, Display, TEXT("EG.Chess: no chess table in this world."));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("EG.Chess %s: phase %s, white seat %s"), *Table->GetName(), EGChessConsole::PhaseName(Table->GetPhase()), Table->GetWhiteSeat() == EEGChessSeat::A ? TEXT("A") : TEXT("B"));
		UE_LOG(LogTemp, Display, TEXT("  FEN %s"), *Table->GetFEN());
		UE_LOG(LogTemp, Display, TEXT("  clocks white %.1f black %.1f (%s)"), Table->GetRemainingTime(EEGChessColor::White), Table->GetRemainingTime(EEGChessColor::Black), Table->IsTimed() ? TEXT("timed") : TEXT("untimed"));
		for (const EEGChessSeat Seat : { EEGChessSeat::A, EEGChessSeat::B })
		{
			const FEGChessSeatOccupant& Occupant = Table->GetSeat(Seat)->GetOccupantRef();
			UE_LOG(LogTemp, Display, TEXT("  seat %s: %s%s%s"), Seat == EEGChessSeat::A ? TEXT("A") : TEXT("B"),
				*Table->GetSeatDisplayName(Seat).ToString(), Occupant.bAI ? TEXT(" [AI]") : TEXT(""), Occupant.bDisconnected ? TEXT(" [disconnected]") : TEXT(""));
		}
		UE_LOG(LogTemp, Display, TEXT("  moves: %s"), *FString::Join(Table->GetMoveListSAN(), TEXT(" ")));
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdEGChessSetFEN(
	TEXT("EG.Chess.SetFEN"),
	TEXT("EG.Chess.SetFEN <fen>: set the focused table's position (server)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		AEGChessTableActor* Table = EGChessConsole::FindTable(World);
		if (Table && Args.Num() > 0)
		{
			Table->DebugSetPosition(FString::Join(Args, TEXT(" ")));
		}
	}));

static FAutoConsoleCommandWithWorld CmdEGChessAIvsAI(
	TEXT("EG.Chess.AIvsAI"),
	TEXT("Make the focused chess table play itself (server)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (AEGChessTableActor* Table = EGChessConsole::FindTable(World))
		{
			Table->DebugStartAIvsAI();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdEGChessPerft(
	TEXT("EG.Chess.Perft"),
	TEXT("EG.Chess.Perft <depth>: count legal move sequences from the focused table's position."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const AEGChessTableActor* Table = EGChessConsole::FindTable(World);
		const int32 Depth = Args.Num() > 0 ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 6) : 3;
		const FEGChessPosition Position = Table ? Table->GetPosition() : FEGChessPosition::Start();
		const double Start = FPlatformTime::Seconds();
		const uint64 Nodes = EGChessRules::Perft(Position, Depth);
		UE_LOG(LogTemp, Display, TEXT("EG.Chess perft(%d) = %llu in %.2f s"), Depth, Nodes, FPlatformTime::Seconds() - Start);
	}));

#endif
