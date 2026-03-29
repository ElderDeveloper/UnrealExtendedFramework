// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "Groups/EPFGroupsSubsystem.h"
#include "EPFAsyncGroups.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncGroupCreated, const FString&, GroupId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncGroupAction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncGroupInfoSuccess, const FEPFGroupInfo&, GroupInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncGroupsListSuccess, const TArray<FEPFGroupInfo>&, Groups);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncGroupMembersSuccess, const TArray<FEPFGroupMember>&, Members);

// ── Create ───────────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Create Group"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncCreateGroup : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Create Group"), Category = "PlayFab|Async|Groups")
	static UEPFAsyncCreateGroup* CreateGroup(UObject* WorldContext, const FString& GroupName);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncGroupCreated OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString GroupName; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& GroupId);
};

// ── Delete ───────────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Delete Group"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncDeleteGroup : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Delete Group"), Category = "PlayFab|Async|Groups")
	static UEPFAsyncDeleteGroup* DeleteGroup(UObject* WorldContext, const FString& GroupId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncGroupAction OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString GroupId; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// ── Get Group Info ───────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Get Group Info"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetGroupInfo : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Group Info"), Category = "PlayFab|Async|Groups")
	static UEPFAsyncGetGroupInfo* GetGroupInfo(UObject* WorldContext, const FString& GroupId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncGroupInfoSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString GroupId; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FEPFGroupInfo& Info);
};

// ── List Memberships ─────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: List Group Memberships"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncListMemberships : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: List Group Memberships"), Category = "PlayFab|Async|Groups")
	static UEPFAsyncListMemberships* ListMemberships(UObject* WorldContext);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncGroupsListSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFGroupInfo>& Groups);
};

// ── List Group Members ───────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: List Group Members"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncListGroupMembers : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: List Group Members"), Category = "PlayFab|Async|Groups")
	static UEPFAsyncListGroupMembers* ListGroupMembers(UObject* WorldContext, const FString& GroupId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncGroupMembersSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString GroupId; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFGroupMember>& Members);
};

// ── Apply / Leave ────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Apply To Group"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncApplyToGroup : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Apply To Group"), Category = "PlayFab|Async|Groups")
	static UEPFAsyncApplyToGroup* ApplyToGroup(UObject* WorldContext, const FString& GroupId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncGroupAction OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString GroupId; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

UCLASS(meta = (DisplayName = "PlayFab: Leave Group"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncLeaveGroup : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Leave Group"), Category = "PlayFab|Async|Groups")
	static UEPFAsyncLeaveGroup* LeaveGroup(UObject* WorldContext, const FString& GroupId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncGroupAction OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString GroupId; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
