// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSEcomSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSOffersQueried, bool, bSuccess, const TArray<FEEOSCatalogOffer>&, Offers);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSEntitlementsQueried, bool, bSuccess, const TArray<FEEOSEntitlement>&, Entitlements);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSOwnershipQueried, bool, bSuccess, const TArray<FEEOSOwnership>&, OwnedItems);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSCheckoutComplete, bool, bSuccess, const FString&, TransactionId);

/**
 * Manages EOS Commerce — store offers, entitlements, ownership, and checkout.
 * Uses IOnlinePurchase and IOnlineStore interfaces.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSEcomSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Offers / Catalog ─────────────────────────────────────────────────────

	/** Query available store offers (catalog) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Ecom")
	void QueryOffers();

	/** Get cached offers from last query */
	UFUNCTION(BlueprintPure, Category = "EOS|Ecom")
	TArray<FEEOSCatalogOffer> GetCachedOffers() const;

	/** Get a specific offer by ID */
	UFUNCTION(BlueprintPure, Category = "EOS|Ecom")
	FEEOSCatalogOffer GetOfferById(const FString& OfferId) const;

	/** Get the number of cached offers */
	UFUNCTION(BlueprintPure, Category = "EOS|Ecom")
	int32 GetOfferCount() const;

	// ── Entitlements ─────────────────────────────────────────────────────────

	/** Query the user's entitlements (owned items) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Ecom")
	void QueryEntitlements();

	/** Get cached entitlements */
	UFUNCTION(BlueprintPure, Category = "EOS|Ecom")
	TArray<FEEOSEntitlement> GetCachedEntitlements() const;

	/** Check if user has a specific entitlement by name or catalog item ID */
	UFUNCTION(BlueprintPure, Category = "EOS|Ecom")
	bool HasEntitlement(const FString& EntitlementNameOrItemId) const;

	/** Get the number of cached entitlements */
	UFUNCTION(BlueprintPure, Category = "EOS|Ecom")
	int32 GetEntitlementCount() const;

	// ── Ownership ────────────────────────────────────────────────────────────

	/** Check ownership of specific catalog item IDs */
	UFUNCTION(BlueprintCallable, Category = "EOS|Ecom")
	void QueryOwnership(const TArray<FString>& CatalogItemIds);

	/** Check if a specific item is owned (from cache) */
	UFUNCTION(BlueprintPure, Category = "EOS|Ecom")
	bool IsItemOwned(const FString& CatalogItemId) const;

	// ── Checkout / Purchase ──────────────────────────────────────────────────

	/** Initiate checkout for a specific offer */
	UFUNCTION(BlueprintCallable, Category = "EOS|Ecom")
	void Checkout(const FString& OfferId);

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Ecom")
	FOnEOSOffersQueried OnOffersQueried;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Ecom")
	FOnEOSEntitlementsQueried OnEntitlementsQueried;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Ecom")
	FOnEOSOwnershipQueried OnOwnershipQueried;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Ecom")
	FOnEOSCheckoutComplete OnCheckoutComplete;

private:

	TArray<FEEOSCatalogOffer> CachedOffers;
	TArray<FEEOSEntitlement> CachedEntitlements;
	TArray<FEEOSOwnership> CachedOwnership;

	/** Delegate handle for entitlements query cleanup */
	FDelegateHandle EntitlementsQueryHandle;
};
