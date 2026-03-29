// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSEcomSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineStoreInterfaceV2.h"
#include "Interfaces/OnlinePurchaseInterface.h"
#include "Interfaces/OnlineEntitlementsInterface.h"
#include "UnrealExtendedEOS.h"

void UEEOSEcomSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSEcomSubsystem::Deinitialize()
{
	CachedOffers.Empty();
	CachedEntitlements.Empty();
	CachedOwnership.Empty();
	Super::Deinitialize();
}

// ── Offers / Catalog ─────────────────────────────────────────────────────────

void UEEOSEcomSubsystem::QueryOffers()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryOffers"));
		OnOffersQueried.Broadcast(false, TArray<FEEOSCatalogOffer>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineStoreV2Ptr StoreInterface = EOSSub->GetStoreV2Interface();
	if (!StoreInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSEcomSubsystem::QueryOffers — StoreV2 interface not available"));
		OnOffersQueried.Broadcast(false, TArray<FEEOSCatalogOffer>());
		return;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnOffersQueried.Broadcast(false, TArray<FEEOSCatalogOffer>());
		return;
	}

	// FOnQueryOnlineStoreOffersComplete is a regular delegate, so CreateLambda works
	StoreInterface->QueryOffersById(*UserId, TArray<FUniqueOfferId>(),
		FOnQueryOnlineStoreOffersComplete::CreateLambda(
			[this, StoreInterface](bool bWasSuccessful, const TArray<FUniqueOfferId>& OfferIds, const FString& Error)
			{
				CachedOffers.Empty();

				if (bWasSuccessful)
				{
					TArray<FOnlineStoreOfferRef> Offers;
					StoreInterface->GetOffers(Offers);

					for (const auto& Offer : Offers)
					{
						FEEOSCatalogOffer EInfo;
						EInfo.OfferId = Offer->OfferId;
						EInfo.Title = Offer->Title.ToString();
						EInfo.Description = Offer->Description.ToString();
						EInfo.LongDescription = Offer->LongDescription.ToString();
						EInfo.PriceInSmallestUnit = Offer->NumericPrice;
						EInfo.OriginalPriceInSmallestUnit = Offer->RegularPrice;
						EInfo.FormattedPrice = Offer->GetDisplayPrice().ToString();
						EInfo.CurrencyCode = Offer->CurrencyCode;
						EInfo.bAvailableForPurchase = Offer->IsPurchaseable();
						CachedOffers.Add(EInfo);
					}

					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSEcomSubsystem: Queried %d offers"), CachedOffers.Num());
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSEcomSubsystem: QueryOffers failed — %s"), *Error);
				}

				OnOffersQueried.Broadcast(bWasSuccessful, CachedOffers);
			}));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSEcomSubsystem::QueryOffers — Querying catalog..."));
}

TArray<FEEOSCatalogOffer> UEEOSEcomSubsystem::GetCachedOffers() const
{
	return CachedOffers;
}

FEEOSCatalogOffer UEEOSEcomSubsystem::GetOfferById(const FString& OfferId) const
{
	for (const auto& Offer : CachedOffers)
	{
		if (Offer.OfferId == OfferId) return Offer;
	}
	return FEEOSCatalogOffer();
}

int32 UEEOSEcomSubsystem::GetOfferCount() const
{
	return CachedOffers.Num();
}

// ── Entitlements ─────────────────────────────────────────────────────────────

void UEEOSEcomSubsystem::QueryEntitlements()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryEntitlements"));
		OnEntitlementsQueried.Broadcast(false, TArray<FEEOSEntitlement>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineEntitlementsPtr EntitlementsInterface = EOSSub->GetEntitlementsInterface();
	if (!EntitlementsInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSEcomSubsystem::QueryEntitlements — Entitlements interface not available"));
		OnEntitlementsQueried.Broadcast(false, TArray<FEEOSEntitlement>());
		return;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnEntitlementsQueried.Broadcast(false, TArray<FEEOSEntitlement>());
		return;
	}

	// FOnQueryEntitlementsComplete is a MULTICAST delegate — bind via the interface
	// Store the handle so we can remove it in the callback to avoid accumulation
	EntitlementsQueryHandle = EntitlementsInterface->OnQueryEntitlementsCompleteDelegates.AddLambda(
		[this, EntitlementsInterface, UserId](bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& Namespace, const FString& Error)
		{
			// Remove ourselves from the multicast delegate to prevent accumulation
			EntitlementsInterface->OnQueryEntitlementsCompleteDelegates.Remove(EntitlementsQueryHandle);

			CachedEntitlements.Empty();

			if (bWasSuccessful)
			{
				TArray<TSharedRef<FOnlineEntitlement>> AllEntitlements;
				EntitlementsInterface->GetAllEntitlements(InUserId, Namespace, AllEntitlements);

				for (const auto& Ent : AllEntitlements)
				{
					FEEOSEntitlement EInfo;
					EInfo.EntitlementId = Ent->Id;
					EInfo.EntitlementName = Ent->Name;
					EInfo.CatalogItemId = Ent->ItemId;
					EInfo.bRedeemed = (Ent->ConsumedCount > 0);
					CachedEntitlements.Add(EInfo);
				}

				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSEcomSubsystem: Queried %d entitlements"), CachedEntitlements.Num());
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSEcomSubsystem: QueryEntitlements failed — %s"), *Error);
			}

			OnEntitlementsQueried.Broadcast(bWasSuccessful, CachedEntitlements);
		});

	EntitlementsInterface->QueryEntitlements(*UserId, TEXT(""));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSEcomSubsystem::QueryEntitlements — Querying..."));
}

TArray<FEEOSEntitlement> UEEOSEcomSubsystem::GetCachedEntitlements() const
{
	return CachedEntitlements;
}

bool UEEOSEcomSubsystem::HasEntitlement(const FString& EntitlementNameOrItemId) const
{
	for (const auto& Ent : CachedEntitlements)
	{
		// Check both EntitlementName AND CatalogItemId — QueryOwnership passes catalog item IDs,
		// while other callers may pass entitlement names. Matching either prevents false negatives.
		if (Ent.EntitlementName == EntitlementNameOrItemId || Ent.CatalogItemId == EntitlementNameOrItemId)
		{
			return true;
		}
	}
	return false;
}

int32 UEEOSEcomSubsystem::GetEntitlementCount() const
{
	return CachedEntitlements.Num();
}

// ── Ownership ────────────────────────────────────────────────────────────────

void UEEOSEcomSubsystem::QueryOwnership(const TArray<FString>& CatalogItemIds)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryOwnership"));
		OnOwnershipQueried.Broadcast(false, TArray<FEEOSOwnership>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnOwnershipQueried.Broadcast(false, TArray<FEEOSOwnership>());
		return;
	}

	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnOwnershipQueried.Broadcast(false, TArray<FEEOSOwnership>());
		return;
	}

	IOnlineEntitlementsPtr EntitlementsInterface = EOSSub->GetEntitlementsInterface();
	if (!EntitlementsInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSEcomSubsystem::QueryOwnership — Entitlements interface not available, falling back to cached data"));
		// Fallback to cache if entitlements interface unavailable
		CachedOwnership.Empty();
		for (const FString& ItemId : CatalogItemIds)
		{
			FEEOSOwnership Ownership;
			Ownership.ItemId = ItemId;
			Ownership.bOwned = HasEntitlement(ItemId);
			CachedOwnership.Add(Ownership);
		}
		OnOwnershipQueried.Broadcast(true, CachedOwnership);
		return;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSEcomSubsystem::QueryOwnership — Refreshing entitlements before checking %d items"), CatalogItemIds.Num());

	// Query fresh entitlements from EOS, then check ownership in the callback
	TArray<FString> ItemIdsCopy = CatalogItemIds;

	// Use the same delegate pattern as QueryEntitlements
	FDelegateHandle OwnershipQueryHandle = EntitlementsInterface->OnQueryEntitlementsCompleteDelegates.AddLambda(
		[this, ItemIdsCopy, EntitlementsInterface](bool bWasSuccessful, const FUniqueNetId& EntUserId, const FString& Namespace, const FString& Error)
		{
			if (bWasSuccessful)
			{
				// Refresh cached entitlements from the fresh query
				CachedEntitlements.Empty();
				IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
				if (EOSSub)
				{
					IOnlineEntitlementsPtr EntIf = EOSSub->GetEntitlementsInterface();
					if (EntIf.IsValid())
					{
						TArray<TSharedRef<FOnlineEntitlement>> Entitlements;
						EntIf->GetAllEntitlements(EntUserId, Namespace, Entitlements);
						for (const auto& Ent : Entitlements)
						{
							FEEOSEntitlement CachedEnt;
							CachedEnt.EntitlementId = Ent->Id;
							CachedEnt.EntitlementName = Ent->Name;
							CachedEnt.CatalogItemId = Ent->ItemId;
							CachedEnt.bRedeemed = (Ent->ConsumedCount > 0);
							CachedEntitlements.Add(CachedEnt);
						}
					}
				}
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSEcomSubsystem::QueryOwnership — Entitlement refresh failed (%s), using cached data"), *Error);
			}

			// Now build ownership results from the (refreshed or cached) entitlements
			CachedOwnership.Empty();
			for (const FString& ItemId : ItemIdsCopy)
			{
				FEEOSOwnership Ownership;
				Ownership.ItemId = ItemId;
				Ownership.bOwned = HasEntitlement(ItemId);
				CachedOwnership.Add(Ownership);
			}

			OnOwnershipQueried.Broadcast(true, CachedOwnership);

			// Clean up this delegate
			EntitlementsInterface->OnQueryEntitlementsCompleteDelegates.RemoveAll(this);
		});

	EntitlementsInterface->QueryEntitlements(*UserId, TEXT(""));
}

bool UEEOSEcomSubsystem::IsItemOwned(const FString& CatalogItemId) const
{
	for (const auto& Ownership : CachedOwnership)
	{
		if (Ownership.ItemId == CatalogItemId) return Ownership.bOwned;
	}
	return false;
}

// ── Checkout / Purchase ──────────────────────────────────────────────────────

void UEEOSEcomSubsystem::Checkout(const FString& OfferId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("Checkout"));
		OnCheckoutComplete.Broadcast(false, TEXT(""));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlinePurchasePtr PurchaseInterface = EOSSub->GetPurchaseInterface();
	if (!PurchaseInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSEcomSubsystem::Checkout — Purchase interface not available"));
		OnCheckoutComplete.Broadcast(false, TEXT(""));
		return;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnCheckoutComplete.Broadcast(false, TEXT(""));
		return;
	}

	FPurchaseCheckoutRequest Request;
	Request.AddPurchaseOffer(TEXT(""), OfferId, 1);

	// FOnPurchaseCheckoutComplete is a regular delegate — CreateLambda works
	PurchaseInterface->Checkout(*UserId, Request,
		FOnPurchaseCheckoutComplete::CreateLambda(
			[this, OfferId](const FOnlineError& Result, const TSharedRef<FPurchaseReceipt>& Receipt)
			{
				if (Result.bSucceeded)
				{
					FString TransactionId = Receipt->TransactionId;
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSEcomSubsystem: Checkout succeeded — TransactionId=%s"), *TransactionId);
					OnCheckoutComplete.Broadcast(true, TransactionId);
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSEcomSubsystem: Checkout failed — %s"), *Result.ErrorMessage.ToString());
					OnCheckoutComplete.Broadcast(false, TEXT(""));
				}
			}));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSEcomSubsystem::Checkout — Purchasing offer '%s'..."), *OfferId);
}
