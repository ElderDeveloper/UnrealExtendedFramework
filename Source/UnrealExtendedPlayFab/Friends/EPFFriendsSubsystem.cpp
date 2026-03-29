// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFFriendsSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFFriendsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFFriendsSubsystem::Deinitialize()
{
	CachedFriends.Empty();
	Super::Deinitialize();
}


// ── Get Friends List ─────────────────────────────────────────────────────────

void UEPFFriendsSubsystem::GetFriendsList(bool bIncludeSteamFriends, bool bIncludeFacebookFriends)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetBoolField(TEXT("IncludeSteamFriends"), bIncludeSteamFriends);
	Body->SetBoolField(TEXT("IncludeFacebookFriends"), bIncludeFacebookFriends);

	// Request profile constraints to get display names and Steam IDs
	TSharedPtr<FJsonObject> ProfileConstraints = MakeShared<FJsonObject>();
	ProfileConstraints->SetBoolField(TEXT("ShowDisplayName"), true);
	Body->SetObjectField(TEXT("ProfileConstraints"), ProfileConstraints);

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetFriendsList"),
		Body,
		EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				CachedFriends.Empty();
				const TArray<TSharedPtr<FJsonValue>>* FriendsArr = nullptr;
				if (Response->TryGetArrayField(TEXT("Friends"), FriendsArr) && FriendsArr)
				{
					for (const auto& FriendVal : *FriendsArr)
					{
						const TSharedPtr<FJsonObject>* FriendObj = nullptr;
						if (FriendVal->TryGetObject(FriendObj) && FriendObj)
						{
							FEPFFriend Friend;
							Friend.PlayFabId = (*FriendObj)->GetStringField(TEXT("FriendPlayFabId"));

							// Profile info
							const TSharedPtr<FJsonObject>* ProfileObj = nullptr;
							if ((*FriendObj)->TryGetObjectField(TEXT("Profile"), ProfileObj) && ProfileObj)
							{
								Friend.DisplayName = (*ProfileObj)->GetStringField(TEXT("DisplayName"));
							}

							// Try TitleDisplayName fallback
							if (Friend.DisplayName.IsEmpty())
							{
								Friend.DisplayName = (*FriendObj)->GetStringField(TEXT("TitleDisplayName"));
							}

							// Steam info
							const TSharedPtr<FJsonObject>* SteamObj = nullptr;
							if ((*FriendObj)->TryGetObjectField(TEXT("SteamInfo"), SteamObj) && SteamObj)
							{
								Friend.SteamId = (*SteamObj)->GetStringField(TEXT("SteamId"));
							}

							// Tags
							const TArray<TSharedPtr<FJsonValue>>* TagsArr = nullptr;
							if ((*FriendObj)->TryGetArrayField(TEXT("Tags"), TagsArr) && TagsArr)
							{
								for (const auto& TagVal : *TagsArr)
								{
									FString Tag;
									if (TagVal->TryGetString(Tag)) Friend.Tags.Add(Tag);
								}
							}

							CachedFriends.Add(Friend);
						}
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFFriends — Found %d friends"), CachedFriends.Num());
			}
			OnFriendsReceived.Broadcast(Result, CachedFriends);
		})
	);
}


// ── Add Friend ───────────────────────────────────────────────────────────────

void UEPFFriendsSubsystem::AddFriend(const FString& FriendPlayFabId)
{
	if (FriendPlayFabId.IsEmpty()) { OnFriendAdded.Broadcast(FEPFResult::Failure(TEXT("FriendPlayFabId cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("FriendPlayFabId"), FriendPlayFabId);

	SendPlayFabRequestDetailed(TEXT("/Client/AddFriend"), Body, EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFFriends — Friend added"));
			OnFriendAdded.Broadcast(Result);
		}));
}

void UEPFFriendsSubsystem::AddFriendByDisplayName(const FString& DisplayName)
{
	if (DisplayName.IsEmpty()) { OnFriendAdded.Broadcast(FEPFResult::Failure(TEXT("DisplayName cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("FriendTitleDisplayName"), DisplayName);

	SendPlayFabRequestDetailed(TEXT("/Client/AddFriend"), Body, EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			OnFriendAdded.Broadcast(Result);
		}));
}


// ── Remove Friend ────────────────────────────────────────────────────────────

void UEPFFriendsSubsystem::RemoveFriend(const FString& FriendPlayFabId)
{
	if (FriendPlayFabId.IsEmpty()) { OnFriendRemoved.Broadcast(FEPFResult::Failure(TEXT("FriendPlayFabId cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("FriendPlayFabId"), FriendPlayFabId);

	SendPlayFabRequestDetailed(TEXT("/Client/RemoveFriend"), Body, EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this, FriendPlayFabId](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) 			{
				CachedFriends.RemoveAll([&](const FEPFFriend& F) { return F.PlayFabId == FriendPlayFabId; });
			}
			OnFriendRemoved.Broadcast(Result);
		}));
}


// ── Set Friend Tags ──────────────────────────────────────────────────────────

void UEPFFriendsSubsystem::SetFriendTags(const FString& FriendPlayFabId, const TArray<FString>& Tags)
{
	if (FriendPlayFabId.IsEmpty()) { OnFriendTagsUpdated.Broadcast(FEPFResult::Failure(TEXT("FriendPlayFabId cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("FriendPlayFabId"), FriendPlayFabId);

	TArray<TSharedPtr<FJsonValue>> TagsArr;
	for (const auto& Tag : Tags) TagsArr.Add(MakeShared<FJsonValueString>(Tag));
	Body->SetArrayField(TEXT("Tags"), TagsArr);

	SendPlayFabRequestDetailed(TEXT("/Client/SetFriendTags"), Body, EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this, FriendPlayFabId, Tags](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) 			{
				FEPFFriend* Found = CachedFriends.FindByPredicate([&](const FEPFFriend& F) { return F.PlayFabId == FriendPlayFabId; });
				if (Found) Found->Tags = Tags;
			}
			OnFriendTagsUpdated.Broadcast(Result);
		}));
}


// ── Queries ──────────────────────────────────────────────────────────────────

TArray<FEPFFriend> UEPFFriendsSubsystem::GetCachedFriends() const { return CachedFriends; }

bool UEPFFriendsSubsystem::IsFriend(const FString& PlayFabId) const
{
	for (const auto& F : CachedFriends) { if (F.PlayFabId == PlayFabId) return true; }
	return false;
}

int32 UEPFFriendsSubsystem::GetFriendCount() const { return CachedFriends.Num(); }

bool UEPFFriendsSubsystem::FindFriend(const FString& PlayFabId, FEPFFriend& OutFriend) const
{
	for (const auto& F : CachedFriends) { if (F.PlayFabId == PlayFabId) { OutFriend = F; return true; } }
	return false;
}
