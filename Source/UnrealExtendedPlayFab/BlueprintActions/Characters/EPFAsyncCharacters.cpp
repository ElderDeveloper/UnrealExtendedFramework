// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncCharacters.h"
#include "Engine/GameInstance.h"

// ── Get Characters ───────────────────────────────────────────────────────────
UEPFAsyncGetCharacters* UEPFAsyncGetCharacters::GetAllCharacters(UObject* WorldContext)
{
	auto* A = NewObject<UEPFAsyncGetCharacters>(); A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetCharacters::Activate()
{
	auto* S = GetEPFSubsystemFromContext<UEPFCharacterSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Character subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnCharactersReceived.AddDynamic(this, &UEPFAsyncGetCharacters::HandleComplete);
	S->GetAllCharacters();
}
void UEPFAsyncGetCharacters::HandleComplete(const FEPFResult& Result, const TArray<FEPFCharacter>& Characters)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFCharacterSubsystem>(WorldContext.Get())) S->OnCharactersReceived.RemoveDynamic(this, &UEPFAsyncGetCharacters::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Characters) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get characters")));
	SetReadyToDestroy();
}

// ── Grant Character ──────────────────────────────────────────────────────────
UEPFAsyncGrantCharacter* UEPFAsyncGrantCharacter::GrantCharacter(UObject* WorldContext, const FString& CharacterName, const FString& ItemId)
{
	auto* A = NewObject<UEPFAsyncGrantCharacter>(); A->Name = CharacterName; A->Item = ItemId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGrantCharacter::Activate()
{
	if (Name.IsEmpty() || Item.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("CharacterName and ItemId required"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFCharacterSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Character subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnCharacterGranted.AddDynamic(this, &UEPFAsyncGrantCharacter::HandleComplete);
	S->GrantCharacter(Name, Item);
}
void UEPFAsyncGrantCharacter::HandleComplete(const FEPFResult& Result, const FString& CharId)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFCharacterSubsystem>(WorldContext.Get())) S->OnCharacterGranted.RemoveDynamic(this, &UEPFAsyncGrantCharacter::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(CharId) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to grant character")));
	SetReadyToDestroy();
}

// ── Delete Character ─────────────────────────────────────────────────────────
UEPFAsyncDeleteCharacter* UEPFAsyncDeleteCharacter::DeleteCharacter(UObject* WorldContext, const FString& CharacterId)
{
	auto* A = NewObject<UEPFAsyncDeleteCharacter>(); A->CharId = CharacterId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncDeleteCharacter::Activate()
{
	if (CharId.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("CharacterId cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFCharacterSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Character subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnCharacterDeleted.AddDynamic(this, &UEPFAsyncDeleteCharacter::HandleComplete);
	S->DeleteCharacter(CharId);
}
void UEPFAsyncDeleteCharacter::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFCharacterSubsystem>(WorldContext.Get())) S->OnCharacterDeleted.RemoveDynamic(this, &UEPFAsyncDeleteCharacter::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to delete character")));
	SetReadyToDestroy();
}

// ── Get Character Data ───────────────────────────────────────────────────────
UEPFAsyncGetCharData* UEPFAsyncGetCharData::GetCharacterData(UObject* WorldContext, const FString& CharacterId, const TArray<FString>& Keys)
{
	auto* A = NewObject<UEPFAsyncGetCharData>(); A->CharId = CharacterId; A->Keys = Keys; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetCharData::Activate()
{
	if (CharId.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("CharacterId cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFCharacterSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Character subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnCharacterDataReceived.AddDynamic(this, &UEPFAsyncGetCharData::HandleComplete);
	S->GetCharacterData(CharId, Keys);
}
void UEPFAsyncGetCharData::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFCharacterSubsystem>(WorldContext.Get())) S->OnCharacterDataReceived.RemoveDynamic(this, &UEPFAsyncGetCharData::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get character data")));
	SetReadyToDestroy();
}

// ── Get Character Stats ──────────────────────────────────────────────────────
UEPFAsyncGetCharStats* UEPFAsyncGetCharStats::GetCharacterStatistics(UObject* WorldContext, const FString& CharacterId)
{
	auto* A = NewObject<UEPFAsyncGetCharStats>(); A->CharId = CharacterId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetCharStats::Activate()
{
	if (CharId.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("CharacterId cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFCharacterSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Character subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnCharacterStatsReceived.AddDynamic(this, &UEPFAsyncGetCharStats::HandleComplete);
	S->GetCharacterStatistics(CharId);
}
void UEPFAsyncGetCharStats::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFCharacterSubsystem>(WorldContext.Get())) S->OnCharacterStatsReceived.RemoveDynamic(this, &UEPFAsyncGetCharStats::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get character stats")));
	SetReadyToDestroy();
}
