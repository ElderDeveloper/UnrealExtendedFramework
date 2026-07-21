// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFGroupsSubsystem.generated.h"


USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFGroupInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Groups")
	FString GroupId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Groups")
	FString GroupName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Groups")
	int32 MemberCount = 0;
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFGroupMember
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Groups")
	FString EntityId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Groups")
	FString EntityType;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Groups")
	FString RoleId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Groups")
	FString RoleName;
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFGroupRole
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Groups")
	FString RoleId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Groups")
	FString RoleName;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFGroupCreated, const FEPFResult&, Result, const FString&, GroupId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFGroupDeleted, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFGroupMembersReceived, const FEPFResult&, Result, const TArray<FEPFGroupMember>&, Members);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFGroupsListed, const FEPFResult&, Result, const TArray<FEPFGroupInfo>&, Groups);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFGroupMembershipChanged, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFGroupInfoReceived, const FEPFResult&, Result, const FEPFGroupInfo&, GroupInfo);

/**
 * Manages PlayFab Entity Groups — clans, guilds, teams.
 * Uses the Entity Groups API for cross-platform group management.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFGroupsSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Create a new group (clan/guild) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Groups")
	void CreateGroup(const FString& GroupName);

	/** Delete a group (must be admin) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Groups")
	void DeleteGroup(const FString& GroupId);

	/** Get group info by ID */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Groups")
	void GetGroup(const FString& GroupId);

	/** List all groups the current player belongs to */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Groups")
	void ListMembership();

	/** List members of a specific group */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Groups")
	void ListGroupMembers(const FString& GroupId);

	/** Apply to join a group */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Groups")
	void ApplyToGroup(const FString& GroupId);

	/** Accept a group application (admin action) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Groups")
	void AcceptGroupApplication(const FString& GroupId, const FString& EntityId, const FString& EntityType = TEXT("title_player_account"));

	/** Remove a member from a group */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Groups")
	void RemoveMember(const FString& GroupId, const FString& EntityId, const FString& EntityType = TEXT("title_player_account"));

	/** Leave a group */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Groups")
	void LeaveGroup(const FString& GroupId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get locally cached group memberships */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Groups")
	TArray<FEPFGroupInfo> GetCachedMemberships() const;

	/** Check if the player is in a specific group */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Groups")
	bool IsInGroup(const FString& GroupId) const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Groups")
	FOnEPFGroupCreated OnGroupCreated;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Groups")
	FOnEPFGroupDeleted OnGroupDeleted;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Groups")
	FOnEPFGroupInfoReceived OnGroupInfoReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Groups")
	FOnEPFGroupsListed OnGroupsListed;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Groups")
	FOnEPFGroupMembersReceived OnGroupMembersReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Groups")
	FOnEPFGroupMembershipChanged OnMembershipChanged;

private:

	TArray<FEPFGroupInfo> CachedMemberships;

	/** Build the entity key JSON for the current player */
	TSharedPtr<FJsonObject> MakeEntityKey(const FString& EntityId, const FString& EntityType) const;
	TSharedPtr<FJsonObject> MakeGroupEntityKey(const FString& GroupId) const;
};
