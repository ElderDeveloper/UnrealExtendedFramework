// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFGroupsSubsystem.h"
#include "Auth/EPFAuthSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFGroupsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFGroupsSubsystem::Deinitialize()
{
	CachedMemberships.Empty();
	Super::Deinitialize();
}


// ── Helpers ──────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> UEPFGroupsSubsystem::MakeEntityKey(const FString& EntityId, const FString& EntityType) const
{
	TSharedPtr<FJsonObject> Key = MakeShared<FJsonObject>();
	Key->SetStringField(TEXT("Id"), EntityId);
	Key->SetStringField(TEXT("Type"), EntityType);
	return Key;
}

TSharedPtr<FJsonObject> UEPFGroupsSubsystem::MakeGroupEntityKey(const FString& GroupId) const
{
	return MakeEntityKey(GroupId, TEXT("group"));
}


// ── Create Group ─────────────────────────────────────────────────────────────

void UEPFGroupsSubsystem::CreateGroup(const FString& GroupName)
{
	if (GroupName.IsEmpty())
	{
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFGroupsSubsystem::CreateGroup — GroupName cannot be empty"));
		OnGroupCreated.Broadcast(FEPFResult::Failure(TEXT("GroupName cannot be empty")), TEXT(""));
		return;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("GroupName"), GroupName);

	SendPlayFabRequestDetailed(
		TEXT("/Group/CreateGroup"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FString GroupId;
			if (Result.bSuccess && Response.IsValid())
			{
				const TSharedPtr<FJsonObject>* GroupObj = nullptr;
				if (Response->TryGetObjectField(TEXT("Group"), GroupObj) && GroupObj)
				{
					GroupId = (*GroupObj)->GetStringField(TEXT("Id"));
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFGroupsSubsystem — Created group: %s"), *GroupId);
			}
			OnGroupCreated.Broadcast(Result, GroupId);
		})
	);
}


// ── Delete Group ─────────────────────────────────────────────────────────────

void UEPFGroupsSubsystem::DeleteGroup(const FString& GroupId)
{
	if (GroupId.IsEmpty()) { OnGroupDeleted.Broadcast(FEPFResult::Failure(TEXT("GroupId cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetObjectField(TEXT("Group"), MakeGroupEntityKey(GroupId));

	SendPlayFabRequestDetailed(
		TEXT("/Group/DeleteGroup"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this, GroupId](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess) 			{
				CachedMemberships.RemoveAll([&](const FEPFGroupInfo& G) { return G.GroupId == GroupId; });
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFGroupsSubsystem — Deleted group: %s"), *GroupId);
			}
			OnGroupDeleted.Broadcast(Result);
		})
	);
}


// ── Get Group ────────────────────────────────────────────────────────────────

void UEPFGroupsSubsystem::GetGroup(const FString& GroupId)
{
	if (GroupId.IsEmpty()) { OnGroupInfoReceived.Broadcast(FEPFResult::Failure(TEXT("GroupId cannot be empty")), FEPFGroupInfo()); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetObjectField(TEXT("Group"), MakeGroupEntityKey(GroupId));

	SendPlayFabRequestDetailed(
		TEXT("/Group/GetGroup"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FEPFGroupInfo Info;
			if (Result.bSuccess && Response.IsValid())
			{
				Info.GroupName = Response->GetStringField(TEXT("GroupName"));
				Info.MemberCount = static_cast<int32>(Response->GetNumberField(TEXT("MemberCount")));
				const TSharedPtr<FJsonObject>* GroupObj = nullptr;
				if (Response->TryGetObjectField(TEXT("Group"), GroupObj) && GroupObj)
				{
					Info.GroupId = (*GroupObj)->GetStringField(TEXT("Id"));
				}
			}
			OnGroupInfoReceived.Broadcast(Result, Info);
		})
	);
}


// ── List Membership ──────────────────────────────────────────────────────────

void UEPFGroupsSubsystem::ListMembership()
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	// Entity key for the current player — built from auth subsystem
	if (UEPFAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UEPFAuthSubsystem>())
	{
		Body->SetObjectField(TEXT("Entity"), MakeEntityKey(Auth->GetEntityId(), Auth->GetEntityType()));
	}

	SendPlayFabRequestDetailed(
		TEXT("/Group/ListMembership"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				CachedMemberships.Empty();
				const TArray<TSharedPtr<FJsonValue>>* GroupsArr = nullptr;
				if (Response->TryGetArrayField(TEXT("Groups"), GroupsArr) && GroupsArr)
				{
					for (const auto& GroupVal : *GroupsArr)
					{
						const TSharedPtr<FJsonObject>* GroupObj = nullptr;
						if (GroupVal->TryGetObject(GroupObj) && GroupObj)
						{
							FEPFGroupInfo Info;
							Info.GroupName = (*GroupObj)->GetStringField(TEXT("GroupName"));
							const TSharedPtr<FJsonObject>* KeyObj = nullptr;
							if ((*GroupObj)->TryGetObjectField(TEXT("Group"), KeyObj) && KeyObj)
							{
								Info.GroupId = (*KeyObj)->GetStringField(TEXT("Id"));
							}
							CachedMemberships.Add(Info);
						}
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFGroupsSubsystem — Found %d group memberships"), CachedMemberships.Num());
			}
			OnGroupsListed.Broadcast(Result, CachedMemberships);
		})
	);
}


// ── List Group Members ───────────────────────────────────────────────────────

void UEPFGroupsSubsystem::ListGroupMembers(const FString& GroupId)
{
	if (GroupId.IsEmpty()) { OnGroupMembersReceived.Broadcast(FEPFResult::Failure(TEXT("GroupId cannot be empty")), TArray<FEPFGroupMember>()); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetObjectField(TEXT("Group"), MakeGroupEntityKey(GroupId));

	SendPlayFabRequestDetailed(
		TEXT("/Group/ListGroupMembers"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			TArray<FEPFGroupMember> Members;
			if (Result.bSuccess && Response.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* MembersArr = nullptr;
				if (Response->TryGetArrayField(TEXT("Members"), MembersArr) && MembersArr)
				{
					for (const auto& RoleVal : *MembersArr)
					{
						const TSharedPtr<FJsonObject>* RoleObj = nullptr;
						if (RoleVal->TryGetObject(RoleObj) && RoleObj)
						{
							FString RoleId = (*RoleObj)->GetStringField(TEXT("RoleId"));
							FString RoleName = (*RoleObj)->GetStringField(TEXT("RoleName"));

							const TArray<TSharedPtr<FJsonValue>>* RoleMembers = nullptr;
							if ((*RoleObj)->TryGetArrayField(TEXT("Members"), RoleMembers) && RoleMembers)
							{
								for (const auto& MemberVal : *RoleMembers)
								{
									const TSharedPtr<FJsonObject>* MemberObj = nullptr;
									if (MemberVal->TryGetObject(MemberObj) && MemberObj)
									{
										FEPFGroupMember Member;
										Member.RoleId = RoleId;
										Member.RoleName = RoleName;
										const TSharedPtr<FJsonObject>* KeyObj = nullptr;
										if ((*MemberObj)->TryGetObjectField(TEXT("Key"), KeyObj) && KeyObj)
										{
											Member.EntityId = (*KeyObj)->GetStringField(TEXT("Id"));
											Member.EntityType = (*KeyObj)->GetStringField(TEXT("Type"));
										}
										Members.Add(Member);
									}
								}
							}
						}
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFGroupsSubsystem — Found %d members"), Members.Num());
			}
			OnGroupMembersReceived.Broadcast(Result, Members);
		})
	);
}


// ── Apply / Accept / Remove / Leave ──────────────────────────────────────────

void UEPFGroupsSubsystem::ApplyToGroup(const FString& GroupId)
{
	if (GroupId.IsEmpty()) { OnMembershipChanged.Broadcast(FEPFResult::Failure(TEXT("GroupId cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetObjectField(TEXT("Group"), MakeGroupEntityKey(GroupId));

	if (UEPFAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UEPFAuthSubsystem>())
	{
		Body->SetObjectField(TEXT("Entity"), MakeEntityKey(Auth->GetEntityId(), Auth->GetEntityType()));
	}

	SendPlayFabRequestDetailed(TEXT("/Group/ApplyToGroup"), Body, EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			OnMembershipChanged.Broadcast(Result);
		}));
}

void UEPFGroupsSubsystem::AcceptGroupApplication(const FString& GroupId, const FString& EntityId, const FString& EntityType)
{
	if (GroupId.IsEmpty() || EntityId.IsEmpty()) { OnMembershipChanged.Broadcast(FEPFResult::Failure(TEXT("GroupId and EntityId are required"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetObjectField(TEXT("Group"), MakeGroupEntityKey(GroupId));
	Body->SetObjectField(TEXT("Entity"), MakeEntityKey(EntityId, EntityType));

	SendPlayFabRequestDetailed(TEXT("/Group/AcceptGroupApplication"), Body, EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			OnMembershipChanged.Broadcast(Result);
		}));
}

void UEPFGroupsSubsystem::RemoveMember(const FString& GroupId, const FString& EntityId, const FString& EntityType)
{
	if (GroupId.IsEmpty() || EntityId.IsEmpty()) { OnMembershipChanged.Broadcast(FEPFResult::Failure(TEXT("GroupId and EntityId are required"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetObjectField(TEXT("Group"), MakeGroupEntityKey(GroupId));

	TArray<TSharedPtr<FJsonValue>> MembersArr;
	MembersArr.Add(MakeShared<FJsonValueObject>(MakeEntityKey(EntityId, EntityType)));
	Body->SetArrayField(TEXT("Members"), MembersArr);

	SendPlayFabRequestDetailed(TEXT("/Group/RemoveMembers"), Body, EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			OnMembershipChanged.Broadcast(Result);
		}));
}

void UEPFGroupsSubsystem::LeaveGroup(const FString& GroupId)
{
	if (GroupId.IsEmpty()) { OnMembershipChanged.Broadcast(FEPFResult::Failure(TEXT("GroupId cannot be empty"))); return; }

	if (UEPFAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UEPFAuthSubsystem>())
	{
		RemoveMember(GroupId, Auth->GetEntityId(), Auth->GetEntityType());
	}
	else
	{
		OnMembershipChanged.Broadcast(FEPFResult::Failure(TEXT("Auth subsystem not available")));
	}
}


// ── Queries ──────────────────────────────────────────────────────────────────

TArray<FEPFGroupInfo> UEPFGroupsSubsystem::GetCachedMemberships() const
{
	return CachedMemberships;
}

bool UEPFGroupsSubsystem::IsInGroup(const FString& GroupId) const
{
	for (const auto& G : CachedMemberships)
	{
		if (G.GroupId == GroupId) return true;
	}
	return false;
}
