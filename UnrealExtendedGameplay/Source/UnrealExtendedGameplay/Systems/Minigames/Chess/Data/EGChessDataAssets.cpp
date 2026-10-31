// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessAIProfile.h"
#include "Internationalization/StringTableRegistry.h"
#include "EGChessAnimationSet.h"
#include "EGChessPieceSet.h"
#include "EGChessUIConfig.h"

UEGChessAIProfile::UEGChessAIProfile()
{
	DisplayName = LOCTABLE("ExtendedFramework", "Common.Computer");
}

const FEGChessPieceVisual& UEGChessPieceSet::GetVisual(EEGChessPieceType Type) const
{
	switch (Type)
	{
	case EEGChessPieceType::Pawn: return Pawn;
	case EEGChessPieceType::Knight: return Knight;
	case EEGChessPieceType::Bishop: return Bishop;
	case EEGChessPieceType::Rook: return Rook;
	case EEGChessPieceType::Queen: return Queen;
	default: return King;
	}
}

void UEGChessPieceSet::GatherSoftPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	for (const EEGChessPieceType Type : { EEGChessPieceType::Pawn, EEGChessPieceType::Knight, EEGChessPieceType::Bishop, EEGChessPieceType::Rook, EEGChessPieceType::Queen, EEGChessPieceType::King })
	{
		const FEGChessPieceVisual& Visual = GetVisual(Type);
		auto Add = [&OutPaths](const FSoftObjectPath& Path)
		{
			if (Path.IsValid())
			{
				OutPaths.AddUnique(Path);
			}
		};
		Add(Visual.Mesh.ToSoftObjectPath());
		Add(Visual.MeshBlack.ToSoftObjectPath());
		for (const TSoftObjectPtr<UMaterialInterface>& Material : Visual.WhiteMaterials)
		{
			Add(Material.ToSoftObjectPath());
		}
		for (const TSoftObjectPtr<UMaterialInterface>& Material : Visual.BlackMaterials)
		{
			Add(Material.ToSoftObjectPath());
		}
		Add(Visual.IconWhite.ToSoftObjectPath());
		Add(Visual.IconBlack.ToSoftObjectPath());
	}
	if (GhostMaterial.ToSoftObjectPath().IsValid())
	{
		OutPaths.AddUnique(GhostMaterial.ToSoftObjectPath());
	}
	if (MoveArc.ToSoftObjectPath().IsValid())
	{
		OutPaths.AddUnique(MoveArc.ToSoftObjectPath());
	}
}

TSoftObjectPtr<UAnimMontage> UEGChessAnimationSet::GetEnterMontage(EEGChessEntrySide Side) const
{
	switch (Side)
	{
	case EEGChessEntrySide::Right: return EnterRight;
	case EEGChessEntrySide::Back: return EnterBack;
	default: return EnterLeft;
	}
}

TSoftObjectPtr<UAnimMontage> UEGChessAnimationSet::GetExitMontage(EEGChessEntrySide Side) const
{
	switch (Side)
	{
	case EEGChessEntrySide::Right: return ExitRight;
	case EEGChessEntrySide::Back: return ExitBack;
	default: return ExitLeft;
	}
}

TSoftObjectPtr<UAnimMontage> UEGChessAnimationSet::GetReactionMontage(EEGChessReaction Reaction) const
{
	switch (Reaction)
	{
	case EEGChessReaction::OfferDraw: return OfferDraw;
	case EEGChessReaction::Resign: return Resign;
	case EEGChessReaction::Check: return Check;
	case EEGChessReaction::Win: return Win;
	case EEGChessReaction::Lose: return Lose;
	case EEGChessReaction::Draw: return Draw;
	default: return nullptr;
	}
}

void UEGChessAnimationSet::GatherSoftPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	auto Add = [&OutPaths](const TSoftObjectPtr<UAnimMontage>& Montage)
	{
		if (Montage.ToSoftObjectPath().IsValid())
		{
			OutPaths.AddUnique(Montage.ToSoftObjectPath());
		}
	};
	Add(EnterLeft);
	Add(EnterRight);
	Add(EnterBack);
	Add(SeatedIdle);
	Add(ExitLeft);
	Add(ExitRight);
	Add(ExitBack);
	Add(ClockPressRightHand);
	Add(ClockPressLeftHand);
	for (const TSoftObjectPtr<UAnimMontage>& Montage : ThinkingIdles)
	{
		Add(Montage);
	}
	Add(OfferDraw);
	Add(Resign);
	Add(Check);
	Add(Win);
	Add(Lose);
	Add(Draw);
}

UEGChessUIConfig::UEGChessUIConfig()
{
	SitPromptText = LOCTABLE("ExtendedFramework", "EGChess.SitPrompt");
	WaitingText = LOCTABLE("ExtendedFramework", "Common.WaitingForAnOpponent");
	PlayVersusAIText = LOCTABLE("ExtendedFramework", "EGChess.PlayVersusAI");
	UntimedText = LOCTABLE("ExtendedFramework", "Common.Untimed");

	NoticeTexts.Add(EEGChessNotice::NotYourTurn, LOCTABLE("ExtendedFramework", "EGChess.NoticeNotYourTurn"));
	NoticeTexts.Add(EEGChessNotice::IllegalMove, LOCTABLE("ExtendedFramework", "EGChess.NoticeIllegal"));
	NoticeTexts.Add(EEGChessNotice::DrawOffered, LOCTABLE("ExtendedFramework", "Common.YourOpponentOffersADraw"));
	NoticeTexts.Add(EEGChessNotice::DrawDeclined, LOCTABLE("ExtendedFramework", "EGChess.NoticeDrawDeclined"));
	NoticeTexts.Add(EEGChessNotice::OpponentJoined, LOCTABLE("ExtendedFramework", "EGChess.NoticeOpponentJoined"));
	NoticeTexts.Add(EEGChessNotice::OpponentLeft, LOCTABLE("ExtendedFramework", "EGChess.NoticeOpponentLeft"));
	NoticeTexts.Add(EEGChessNotice::OpponentDisconnected, LOCTABLE("ExtendedFramework", "EGChess.NoticeOpponentDisconnected"));
	NoticeTexts.Add(EEGChessNotice::OpponentReconnected, LOCTABLE("ExtendedFramework", "EGChess.NoticeOpponentReconnected"));
	NoticeTexts.Add(EEGChessNotice::TakebackDone, LOCTABLE("ExtendedFramework", "EGChess.NoticeTakeback"));
	NoticeTexts.Add(EEGChessNotice::LowTime, LOCTABLE("ExtendedFramework", "EGChess.NoticeLowTime"));
	NoticeTexts.Add(EEGChessNotice::SeatTaken, LOCTABLE("ExtendedFramework", "EGChess.NoticeSeatTaken"));
	NoticeTexts.Add(EEGChessNotice::RematchRequested, LOCTABLE("ExtendedFramework", "EGChess.NoticeRematch"));

	ReasonTexts.Add(EEGChessEndReason::Checkmate, LOCTABLE("ExtendedFramework", "EGChess.ReasonCheckmate"));
	ReasonTexts.Add(EEGChessEndReason::Resignation, LOCTABLE("ExtendedFramework", "EGChess.ReasonResignation"));
	ReasonTexts.Add(EEGChessEndReason::Timeout, LOCTABLE("ExtendedFramework", "EGChess.ReasonTimeout"));
	ReasonTexts.Add(EEGChessEndReason::Stalemate, LOCTABLE("ExtendedFramework", "EGChess.ReasonStalemate"));
	ReasonTexts.Add(EEGChessEndReason::ThreefoldRepetition, LOCTABLE("ExtendedFramework", "EGChess.ReasonRepetition"));
	ReasonTexts.Add(EEGChessEndReason::FiftyMoveRule, LOCTABLE("ExtendedFramework", "EGChess.ReasonFifty"));
	ReasonTexts.Add(EEGChessEndReason::InsufficientMaterial, LOCTABLE("ExtendedFramework", "EGChess.ReasonMaterial"));
	ReasonTexts.Add(EEGChessEndReason::Agreement, LOCTABLE("ExtendedFramework", "EGChess.ReasonAgreement"));
	ReasonTexts.Add(EEGChessEndReason::Forfeit, LOCTABLE("ExtendedFramework", "EGChess.ReasonForfeit"));

	PromptTitles.Add(EEGChessPromptKind::ConfirmResign, LOCTABLE("ExtendedFramework", "EGChess.PromptResign"));
	PromptTitles.Add(EEGChessPromptKind::ConfirmLeave, LOCTABLE("ExtendedFramework", "EGChess.PromptLeave"));
	PromptTitles.Add(EEGChessPromptKind::DrawOffered, LOCTABLE("ExtendedFramework", "Common.YourOpponentOffersADraw"));

	WinText = LOCTABLE("ExtendedFramework", "EGChess.ResultWin");
	LoseText = LOCTABLE("ExtendedFramework", "EGChess.ResultLose");
	DrawText = LOCTABLE("ExtendedFramework", "EGChess.ResultDraw");
	WhiteWinsText = LOCTABLE("ExtendedFramework", "EGChess.ResultWhiteWins");
	BlackWinsText = LOCTABLE("ExtendedFramework", "EGChess.ResultBlackWins");
}

FText UEGChessUIConfig::GetNoticeText(EEGChessNotice Notice) const
{
	const FText* Found = NoticeTexts.Find(Notice);
	return Found ? *Found : FText::GetEmpty();
}

FText UEGChessUIConfig::GetReasonText(EEGChessEndReason Reason) const
{
	const FText* Found = ReasonTexts.Find(Reason);
	return Found ? *Found : FText::GetEmpty();
}

FText UEGChessUIConfig::GetPromptTitle(EEGChessPromptKind Kind) const
{
	const FText* Found = PromptTitles.Find(Kind);
	return Found ? *Found : FText::GetEmpty();
}

