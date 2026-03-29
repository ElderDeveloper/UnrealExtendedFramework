// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSUserInfoSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSUserInfoQueried, bool, bSuccess, const FEEOSUserInfo&, UserInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSUserSearchComplete, bool, bSuccess, const TArray<FEEOSUserInfo>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSExternalMappingsQueried, bool, bSuccess, const TArray<FEEOSExternalAccountMapping>&, Mappings);

/**
 * User lookup, search, and external account mapping.
 * Uses IOnlineUser for display name queries and user info retrieval.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSUserInfoSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── User Lookup ──────────────────────────────────────────────────────────

	/** Query user info by Epic Account ID */
	UFUNCTION(BlueprintCallable, Category = "EOS|UserInfo")
	void QueryUserInfo(const FString& EpicAccountId);

	/** Query user info by display name (search) */
	UFUNCTION(BlueprintCallable, Category = "EOS|UserInfo")
	void FindUserByDisplayName(const FString& DisplayName);

	/** Query multiple users' info at once */
	UFUNCTION(BlueprintCallable, Category = "EOS|UserInfo")
	void QueryUserInfoBatch(const TArray<FString>& EpicAccountIds);

	// ── External Account Mapping ─────────────────────────────────────────────

	/** Query external account mappings (Epic ↔ Steam/PSN/Xbox) */
	UFUNCTION(BlueprintCallable, Category = "EOS|UserInfo")
	void QueryExternalAccountMappings(const TArray<FString>& ExternalAccountIds, EEOSExternalCredentialType AccountType);

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

private:

	FEEOSUserInfo CachedUserInfo;
	TArray<FEEOSUserInfo> CachedSearchResults;
	TArray<FEEOSExternalAccountMapping> CachedMappings;

	/** Delegate handles for query cleanup — prevents accumulation */
	FDelegateHandle QueryUserInfoDelegateHandle;
	FDelegateHandle QueryBatchDelegateHandle;
};
