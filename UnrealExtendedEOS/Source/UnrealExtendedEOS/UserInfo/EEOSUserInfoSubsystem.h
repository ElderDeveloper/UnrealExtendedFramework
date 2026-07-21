// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/CoreOnline.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSUserInfoSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSUserInfoQueried, bool, bSuccess, const FEEOSUserInfo&, UserInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSUserSearchComplete, bool, bSuccess, const TArray<FEEOSUserInfo>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSExternalMappingsQueried, bool, bSuccess, const TArray<FEEOSExternalAccountMapping>&, Mappings);

/**
 * User lookup, search, and external account mapping.
 * Uses IOnlineUser for user info queries; display-name search goes through the raw SDK.
 *
 * Entry-point contract (applies to every async entry point below):
 *  - returns true  → the operation started (or completed synchronously); its completion
 *    delegate will broadcast exactly once.
 *  - returns false with a failure broadcast → safe pre-flight failure (EOS unavailable,
 *    bad input, no login); waiters are released.
 *  - returns false WITHOUT any broadcast → rejected because the same kind of query is
 *    already in flight; the pending operation's completion is NOT disturbed.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSUserInfoSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── User Lookup ──────────────────────────────────────────────────────────

	/** Query user info by Epic Account ID. Local-user queries are served synchronously from
	 *  the engine cache (the engine never completes them). See class comment for the return
	 *  value contract. */
	UFUNCTION(BlueprintCallable, Category = "EOS|UserInfo")
	bool QueryUserInfo(const FString& EpicAccountId);

	/** Query user info by display name (search). See class comment for the return value contract. */
	UFUNCTION(BlueprintCallable, Category = "EOS|UserInfo")
	bool FindUserByDisplayName(const FString& DisplayName);

	/** Query multiple users' info at once. Local users are served from the engine cache;
	 *  only the remainder is queried. See class comment for the return value contract. */
	UFUNCTION(BlueprintCallable, Category = "EOS|UserInfo")
	bool QueryUserInfoBatch(const TArray<FString>& EpicAccountIds);

	// ── External Account Mapping ─────────────────────────────────────────────

	/** Query external account mappings (Epic ↔ Steam/PSN/Xbox). See class comment for the
	 *  return value contract. */
	UFUNCTION(BlueprintCallable, Category = "EOS|UserInfo")
	bool QueryExternalAccountMappings(const TArray<FString>& ExternalAccountIds, EEOSExternalCredentialType AccountType);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the cached user info from the last single-user query */
	UFUNCTION(BlueprintPure, Category = "EOS|UserInfo")
	FEEOSUserInfo GetCachedUserInfo() const;

	/** Get all cached user info from batch or search queries */
	UFUNCTION(BlueprintPure, Category = "EOS|UserInfo")
	TArray<FEEOSUserInfo> GetCachedSearchResults() const;

	/** Get cached external account mappings */
	UFUNCTION(BlueprintPure, Category = "EOS|UserInfo")
	TArray<FEEOSExternalAccountMapping> GetCachedMappings() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|UserInfo")
	FOnEOSUserInfoQueried OnUserInfoQueried;

	UPROPERTY(BlueprintAssignable, Category = "EOS|UserInfo")
	FOnEOSUserSearchComplete OnUserSearchComplete;

	UPROPERTY(BlueprintAssignable, Category = "EOS|UserInfo")
	FOnEOSExternalMappingsQueried OnExternalMappingsQueried;

	// ── Internal (raw SDK callback thunk — not for game code) ────────────────

	/** Completion of the raw display-name search. Clears the in-flight guard BEFORE
	 *  broadcasting so a retry from the handler is not rejected. Game thread only. */
	void HandleFindUserByDisplayNameComplete(bool bSuccess, const FEEOSUserInfo& FoundUser, const FString& SearchedName);

private:

	FEEOSUserInfo CachedUserInfo;
	TArray<FEEOSUserInfo> CachedSearchResults;
	TArray<FEEOSExternalAccountMapping> CachedMappings;

	/** Delegate handles for query cleanup — prevents accumulation.
	 *  Also act as the in-flight guards: a second query of the same kind is rejected while
	 *  valid (log + return false, NO broadcast — the pending caller owns the delegate). */
	FDelegateHandle QueryUserInfoDelegateHandle;
	FDelegateHandle QueryBatchDelegateHandle;

	/** In-flight guard for FindUserByDisplayName: it broadcasts on the shared
	 *  OnUserSearchComplete, so a concurrent search would clobber CachedSearchResults and
	 *  confuse waiters. Cleared before the completion broadcast. */
	bool bFindUserByDisplayNameInFlight = false;

	/** The net id requested by the pending QueryUserInfo, held as the REGISTRY INSTANCE.
	 *  The EOS registry mutates ids in place when a missing half becomes known
	 *  (OnlineSubsystemEOSTypes.cpp FindOrAddImpl), so ToString() snapshots taken at request
	 *  time can stop matching the completion echo. Instance compare survives the mutation. */
	FUniqueNetIdPtr PendingQueryUserInfoId;

	/** The net ids actually sent to the engine by the pending QueryUserInfoBatch (local
	 *  users and unqueryable ids are pre-filtered out) — same instance-compare purpose. */
	TArray<FUniqueNetIdRef> PendingBatchQueryIds;
};
