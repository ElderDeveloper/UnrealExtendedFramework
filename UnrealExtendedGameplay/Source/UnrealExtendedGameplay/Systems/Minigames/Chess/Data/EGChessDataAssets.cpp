// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessAIProfile.h"
#include "EGChessAnimationSet.h"
#include "EGChessPieceSet.h"
#include "EGChessUIConfig.h"

#define LOCTEXT_NAMESPACE "EGChess"

UEGChessAIProfile::UEGChessAIProfile()
{
	DisplayName = LOCTEXT("AIDefaultName", "Computer");
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
	Add(MoveRightHand);
	Add(MoveLeftHand);
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
	SitPromptText = LOCTEXT("SitPrompt", "Sit");
	WaitingText = LOCTEXT("Waiting", "Waiting for an opponent");
	PlayVersusAIText = LOCTEXT("PlayVersusAI", "Play against the AI");
	UntimedText = LOCTEXT("Untimed", "Untimed");

	NoticeTexts.Add(EEGChessNotice::NotYourTurn, LOCTEXT("NoticeNotYourTurn", "It's not your turn"));
	NoticeTexts.Add(EEGChessNotice::IllegalMove, LOCTEXT("NoticeIllegal", "That move isn't allowed"));
	NoticeTexts.Add(EEGChessNotice::DrawOffered, LOCTEXT("NoticeDrawOffered", "Your opponent offers a draw"));
	NoticeTexts.Add(EEGChessNotice::DrawDeclined, LOCTEXT("NoticeDrawDeclined", "Draw offer declined"));
	NoticeTexts.Add(EEGChessNotice::OpponentJoined, LOCTEXT("NoticeOpponentJoined", "An opponent sat down"));
	NoticeTexts.Add(EEGChessNotice::OpponentLeft, LOCTEXT("NoticeOpponentLeft", "Your opponent left"));
	NoticeTexts.Add(EEGChessNotice::OpponentDisconnected, LOCTEXT("NoticeOpponentDisconnected", "Your opponent disconnected"));
	NoticeTexts.Add(EEGChessNotice::OpponentReconnected, LOCTEXT("NoticeOpponentReconnected", "Your opponent is back"));
	NoticeTexts.Add(EEGChessNotice::TakebackDone, LOCTEXT("NoticeTakeback", "Move taken back"));
	NoticeTexts.Add(EEGChessNotice::LowTime, LOCTEXT("NoticeLowTime", "Low time"));
	NoticeTexts.Add(EEGChessNotice::SeatTaken, LOCTEXT("NoticeSeatTaken", "That seat is taken"));
	NoticeTexts.Add(EEGChessNotice::RematchRequested, LOCTEXT("NoticeRematch", "Your opponent wants a rematch"));

	ReasonTexts.Add(EEGChessEndReason::Checkmate, LOCTEXT("ReasonCheckmate", "Checkmate"));
	ReasonTexts.Add(EEGChessEndReason::Resignation, LOCTEXT("ReasonResignation", "Resignation"));
	ReasonTexts.Add(EEGChessEndReason::Timeout, LOCTEXT("ReasonTimeout", "Time ran out"));
	ReasonTexts.Add(EEGChessEndReason::Stalemate, LOCTEXT("ReasonStalemate", "Stalemate"));
	ReasonTexts.Add(EEGChessEndReason::ThreefoldRepetition, LOCTEXT("ReasonRepetition", "Threefold repetition"));
	ReasonTexts.Add(EEGChessEndReason::FiftyMoveRule, LOCTEXT("ReasonFifty", "Fifty-move rule"));
	ReasonTexts.Add(EEGChessEndReason::InsufficientMaterial, LOCTEXT("ReasonMaterial", "Insufficient material"));
	ReasonTexts.Add(EEGChessEndReason::Agreement, LOCTEXT("ReasonAgreement", "Draw agreed"));
	ReasonTexts.Add(EEGChessEndReason::Forfeit, LOCTEXT("ReasonForfeit", "Forfeit"));

	PromptTitles.Add(EEGChessPromptKind::ConfirmResign, LOCTEXT("PromptResign", "Resign?"));
	PromptTitles.Add(EEGChessPromptKind::ConfirmLeave, LOCTEXT("PromptLeave", "Resign and leave?"));
	PromptTitles.Add(EEGChessPromptKind::DrawOffered, LOCTEXT("PromptDraw", "Your opponent offers a draw"));

	WinText = LOCTEXT("ResultWin", "You won");
	LoseText = LOCTEXT("ResultLose", "You lost");
	DrawText = LOCTEXT("ResultDraw", "Draw");
	WhiteWinsText = LOCTEXT("ResultWhiteWins", "White wins");
	BlackWinsText = LOCTEXT("ResultBlackWins", "Black wins");
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

#undef LOCTEXT_NAMESPACE
