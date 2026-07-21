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
	// All async actions return true when the request was started (the result arrives on
	// the corresponding delegate) and false when it could not be. Pre-flight failures
	// broadcast a failure; an in-flight duplicate is rejected with a log ONLY — no
	// broadcast, so the legitimate in-flight operation's waiters are not misled.

	/** Query available store offers (catalog) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Ecom")
	bool QueryOffers();

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
	bool QueryEntitlements();

	/** Get cached entitlements (last-known-good — a failed refresh does NOT clear these) */
	UFUNCTION(BlueprintPure, Category = "EOS|Ecom")
	TArray<FEEOSEntitlement> GetCachedEntitlements() const;

	/** Check if user has a specific entitlement by name or catalog item ID */
	UFUNCTION(BlueprintPure, Category = "EOS|Ecom")
	bool HasEntitlement(const FString& EntitlementNameOrItemId) const;

	/** Get the number of cached entitlements */
	UFUNCTION(BlueprintPure, Category = "EOS|Ecom")
	int32 GetEntitlementCount() const;

	// ── Ownership ────────────────────────────────────────────────────────────

	/**
	 * Check ownership of specific catalog item IDs. When the fresh entitlements refresh
	 * fails, OnOwnershipQueried fires with bSuccess=false and results derived from the
	 * last-known-good entitlements cache — treat those as possibly stale, never as an
	 * authoritative "not owned".
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Ecom")
	bool QueryOwnership(const TArray<FString>& CatalogItemIds);

	/** Check if a specific item is owned (from cache) */
	UFUNCTION(BlueprintPure, Category = "EOS|Ecom")
	bool IsItemOwned(const FString& CatalogItemId) const;

	// ── Checkout / Purchase ──────────────────────────────────────────────────

	/** Initiate checkout for a specific offer */
	UFUNCTION(BlueprintCallable, Category = "EOS|Ecom")
	bool Checkout(const FString& OfferId);

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

	/** Last-known-good entitlements: replaced only by a SUCCESSFUL query — a failed refresh
	 *  keeps the previous contents so a transient backend error never turns into
	 *  "you own nothing". */
	TArray<FEEOSEntitlement> CachedEntitlements;

	TArray<FEEOSOwnership> CachedOwnership;

	/** Handle for QueryEntitlements' binding on the interface-wide OnQueryEntitlementsCompleteDelegates.
	 *  Doubles as the in-flight guard for QueryEntitlements; the handler removes ONLY this handle. */
	FDelegateHandle EntitlementsQueryHandle;

	/** Handle for QueryOwnership's binding on the same interface-wide list — kept separate so
	 *  overlapping entitlements/ownership queries never remove each other's binding.
	 *  Doubles as the in-flight guard for QueryOwnership. */
	FDelegateHandle OwnershipQueryHandle;
};
