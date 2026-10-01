// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessTypes.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGChess_Interaction_Sit, "ExtendedGameplay.Chess.Interaction.Sit", "Sit down at a chess table.");

namespace EGChess
{
	FString SquareName(int32 Square)
	{
		if (!IsValidSquare(Square))
		{
			return FString();
		}
		const TCHAR Chars[3] = { static_cast<TCHAR>(TEXT('a') + FileOf(Square)), static_cast<TCHAR>(TEXT('1') + RankOf(Square)), 0 };
		return FString(Chars);
	}

	int32 ParseSquare(const FString& Name)
	{
		if (Name.Len() < 2)
		{
			return INDEX_NONE;
		}
		const int32 File = FChar::ToLower(Name[0]) - TEXT('a');
		const int32 Rank = Name[1] - TEXT('1');
		return IsOnBoard(File, Rank) ? MakeSquare(File, Rank) : INDEX_NONE;
	}

	float PieceValue(EEGChessPieceType Type)
	{
		switch (Type)
		{
		case EEGChessPieceType::Pawn: return 1.0f;
		case EEGChessPieceType::Knight: return 3.0f;
		case EEGChessPieceType::Bishop: return 3.2f;
		case EEGChessPieceType::Rook: return 5.0f;
		case EEGChessPieceType::Queen: return 9.0f;
		default: return 0.0f;
		}
	}

	FString PieceLetter(EEGChessPieceType Type)
	{
		switch (Type)
		{
		case EEGChessPieceType::Knight: return TEXT("N");
		case EEGChessPieceType::Bishop: return TEXT("B");
		case EEGChessPieceType::Rook: return TEXT("R");
		case EEGChessPieceType::Queen: return TEXT("Q");
		case EEGChessPieceType::King: return TEXT("K");
		default: return FString();
		}
	}
}
