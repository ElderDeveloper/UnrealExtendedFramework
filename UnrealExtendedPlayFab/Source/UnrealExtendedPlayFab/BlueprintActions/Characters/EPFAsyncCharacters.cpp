// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncCharacters.h"

// ── Get Characters ───────────────────────────────────────────────────────────
UEPFAsyncGetCharacters* UEPFAsyncGetCharacters::GetAllCharacters(UObject* WorldContext)
{
	auto* A = NewObject<UEPFAsyncGetCharacters>(); A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetCharacters::ExecuteAction()
{
	auto* S = RequireGameInstanceSubsystem<UEPFCharacterSubsystem>();
	if (!S) return;
	S->OnCharactersReceived.AddDynamic(this, &UEPFAsyncGetCharacters::HandleComplete);
	CallWithContext([&] { S->GetAllCharacters(); });
}
void UEPFAsyncGetCharacters::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFCharacterSubsystem>()) S->OnCharactersReceived.RemoveDynamic(this, &UEPFAsyncGetCharacters::HandleComplete);
}
void UEPFAsyncGetCharacters::HandleComplete(const FEPFResult& Result, const TArray<FEPFCharacter>& Characters)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Characters) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get characters")));
}

// ── Grant Character ──────────────────────────────────────────────────────────
UEPFAsyncGrantCharacter* UEPFAsyncGrantCharacter::GrantCharacter(UObject* WorldContext, const FString& CharacterName, const FString& ItemId)
{
	auto* A = NewObject<UEPFAsyncGrantCharacter>(); A->Name = CharacterName; A->Item = ItemId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGrantCharacter::ExecuteAction()
{
	if (Name.IsEmpty() || Item.IsEmpty()) { FailInvalidArgument(TEXT("CharacterName and ItemId required")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFCharacterSubsystem>();
	if (!S) return;
	S->OnCharacterGranted.AddDynamic(this, &UEPFAsyncGrantCharacter::HandleComplete);
	CallWithContext([&] { S->GrantCharacter(Name, Item); });
}
void UEPFAsyncGrantCharacter::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFCharacterSubsystem>()) S->OnCharacterGranted.RemoveDynamic(this, &UEPFAsyncGrantCharacter::HandleComplete);
}
void UEPFAsyncGrantCharacter::HandleComplete(const FEPFResult& Result, const FString& CharId)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(CharId) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to grant character")));
}

// ── Delete Character ─────────────────────────────────────────────────────────
UEPFAsyncDeleteCharacter* UEPFAsyncDeleteCharacter::DeleteCharacter(UObject* WorldContext, const FString& CharacterId)
{
	auto* A = NewObject<UEPFAsyncDeleteCharacter>(); A->CharId = CharacterId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncDeleteCharacter::ExecuteAction()
{
	if (CharId.IsEmpty()) { FailInvalidArgument(TEXT("CharacterId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFCharacterSubsystem>();
	if (!S) return;
	S->OnCharacterDeleted.AddDynamic(this, &UEPFAsyncDeleteCharacter::HandleComplete);
	CallWithContext([&] { S->DeleteCharacter(CharId); });
}
void UEPFAsyncDeleteCharacter::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFCharacterSubsystem>()) S->OnCharacterDeleted.RemoveDynamic(this, &UEPFAsyncDeleteCharacter::HandleComplete);
}
void UEPFAsyncDeleteCharacter::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to delete character")));
}

// ── Get Character Data ───────────────────────────────────────────────────────
UEPFAsyncGetCharData* UEPFAsyncGetCharData::GetCharacterData(UObject* WorldContext, const FString& CharacterId, const TArray<FString>& Keys)
{
	auto* A = NewObject<UEPFAsyncGetCharData>(); A->CharId = CharacterId; A->RequestedKeys = Keys; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetCharData::ExecuteAction()
{
	if (CharId.IsEmpty()) { FailInvalidArgument(TEXT("CharacterId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFCharacterSubsystem>();
	if (!S) return;
	S->OnCharacterDataReceived.AddDynamic(this, &UEPFAsyncGetCharData::HandleComplete);
	CallWithContext([&] { S->GetCharacterData(CharId, RequestedKeys); });
}
void UEPFAsyncGetCharData::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFCharacterSubsystem>()) S->OnCharacterDataReceived.RemoveDynamic(this, &UEPFAsyncGetCharData::HandleComplete);
}
void UEPFAsyncGetCharData::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get character data")));
}

// ── Get Character Stats ──────────────────────────────────────────────────────
UEPFAsyncGetCharStats* UEPFAsyncGetCharStats::GetCharacterStatistics(UObject* WorldContext, const FString& CharacterId)
{
	auto* A = NewObject<UEPFAsyncGetCharStats>(); A->CharId = CharacterId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetCharStats::ExecuteAction()
{
	if (CharId.IsEmpty()) { FailInvalidArgument(TEXT("CharacterId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFCharacterSubsystem>();
	if (!S) return;
	S->OnCharacterStatsReceived.AddDynamic(this, &UEPFAsyncGetCharStats::HandleComplete);
	CallWithContext([&] { S->GetCharacterStatistics(CharId); });
}
void UEPFAsyncGetCharStats::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFCharacterSubsystem>()) S->OnCharacterStatsReceived.RemoveDynamic(this, &UEPFAsyncGetCharStats::HandleComplete);
}
void UEPFAsyncGetCharStats::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get character stats")));
}
