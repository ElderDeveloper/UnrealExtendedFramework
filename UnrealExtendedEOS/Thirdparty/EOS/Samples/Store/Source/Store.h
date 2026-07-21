// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "eos_sdk.h"

struct FEntitlementData
{
	/** User associated with this entitlement */
	FEpicAccountId UserId;
	/** The EOS_Ecom_EntitlementName */
	std::string Name;
	/** The EOS_Ecom_EntitlementInstanceId */
	std::string InstanceId;
	/** If true then this entitlement has been retrieved */
	bool bRedeemed;
};

struct FOwnershipData
{
	/** User associated with this ownwership */
	FEpicAccountId UserId;
	
	/** The EOS_Ecom_CatalogItemId */
	std::string CatalogItemId;
};

struct FOfferData
{
	/** User associated with this offer */
	FEpicAccountId UserId;
	/** The EOS_Ecom_CatalogOfferId */
	std::string Id;
	/** The title of the offer */
	std::wstring Title;
	/** True if the price was properly retrieved */
	bool bPriceValid;
	/** The current price of the offer (includes discounts) */
	uint64_t CurrentPrice;
	/** The original price of the offer */
	uint64_t OriginalPrice;
	/** The decimal point for given price. */
	uint32_t DecimalPoint;
};

/** Provides access to EOS_Ecom with an emphasis on the Catalog */
class FStore
{
public:
	FStore() noexcept(false);
	FStore(FStore const&) = delete;
	FStore& operator=(FStore const&) = delete;

	virtual ~FStore();

	/** Query the store for offer info and entitlement info */
	void Update();

	/** Buy current offers in the cart. */
	void Checkout();

	/** Adds an offer to the cart. */
	void AddToCart(const FOfferData& OfferData);

	/** Removes an offer from the cart. */
	void RemoveFromCart(const FOfferData& OfferData);

	/** Redeem an entitlement */
	// void RedeemEntitlement(const FEntitlementData& Entitlement);

	/** Retrieves the catalog cache */
	const std::vector<FOfferData>& GetCatalog() const { return Catalog; }
	/** Retrieves the entitlements cache */
	const std::vector<FEntitlementData>& GetEntitlements() const { return Entitlements; }
	/** Retrieves the ownership cache */
	const std::vector<FOwnershipData>& GetOwnerships() const { return Ownerships; }
	/** Retrieves the cart cache for the current user. */
	const std::list<FOfferData>& GetCart() const;

	/** Has the catalog data updated? */
	bool IsDirty() { return bDirty; };
	/** Change the dirty flag directly */
	void SetDirty(bool bNewIsDirty) { bDirty = bNewIsDirty; };

	/** Has the cart data updated? */
	bool IsCartDirty() { return bIsCartDirty; }
	/** Change the cart dirty flag directly */
	void SetCartDirty(bool bNewIsCartDirty) { bIsCartDirty = bNewIsCartDirty; }

	/** Is there a purchase in flight? */
	bool IsPurchaseProcessing() { return bPurchaseProcessing; }
	/** Set that there is a purchase in flight */
	void SetPurchaseProcessing(bool bNewPurchaseProcessing) { bPurchaseProcessing = bNewPurchaseProcessing; }

	/** Set the user currently associated with the store */
	void SetCurrentUser(FEpicAccountId UserId) { CurrentUserId = UserId; };
	/** Get the user currently associated with the store */
	FEpicAccountId GetCurrentUser() { return CurrentUserId; };

	/** Notify the store of game events. */
	void OnGameEvent(const FGameEvent& Event);

	/** Called on the completion of a successful checkout. */
	void OnCheckoutSuccessful();
private:
	struct FCart
	{
		/** Cart related Aliases. */
		using CartItemIter = std::list<FOfferData>::iterator;
		using CartItemToIterMap = std::map<std::string, CartItemIter>;

		/** The current cart cache */
		std::list<FOfferData> CartItems;
		/** A map that helps in O(1) removal of items from the cart */
		CartItemToIterMap CartItemMap;

		FCart() = default;
		~FCart()
		{
			CartItems.clear();
			CartItemMap.clear();
		}

		bool Add(const FOfferData& OfferData)
		{
			CartItemToIterMap::iterator It = CartItemMap.find(OfferData.Id);
			// Do not allow duplicates.
			if (It == CartItemMap.end())
			{
				CartItemIter ItemIter = CartItems.insert(CartItems.end(), OfferData);
				CartItemMap.insert({ OfferData.Id, ItemIter });

				return true;
			}

			return false;
		}

		bool Remove(const FOfferData& OfferData)
		{
			CartItemToIterMap::iterator It = CartItemMap.find(OfferData.Id);
			if (It != CartItemMap.end())
			{
				CartItems.erase(It->second);
				CartItemMap.erase(OfferData.Id);
				
				return true;
			}

			return false;
		}

		void Clear()
		{
			CartItems.clear();
			CartItemMap.clear();
		}
	};

	/** Respond to the logged in game event */
	void OnLoggedIn(FEpicAccountId UserId);
	/** Respond to the logged out game event */
	void OnLoggedOut(FEpicAccountId UserId);

	/** Start a store query for the user */
	void QueryStore(EOS_EpicAccountId LocalUserId);
	/** Start a entitlement query for the user */
	void QueryEntitlements(EOS_EpicAccountId LocalUser);
	/** Start a ownership query for the user */
	void QueryOwershipBySandboxIds(EOS_EpicAccountId LocalUser);

	/** Set the catalog data */
	void SetCatalog(FEpicAccountId InUserId, std::vector<FOfferData>&& InCatalog);
	/** Set the entitlement data */
	void SetEntitlements(FEpicAccountId InUserId, std::vector<FEntitlementData>&& InEntitlements);
	/** Set the ownership data */
	void SetOwnerships(FEpicAccountId InUserId, std::vector<FOwnershipData>&& InOwnerships);

	/** The cached data dirty flag */
	bool bDirty = false;
	/** The cached cart data dirty flag */
	bool bIsCartDirty = false;
	/** There is a purchase in flight */
	bool bPurchaseProcessing = false;
	/** How long until the next update */
	float UpdateStoreTimer = 0.f;
	/** The current user associated with the store */
	FEpicAccountId CurrentUserId;

	/** The current catalog cache */
	std::vector<FOfferData> Catalog;
	/** The current entitlement cache */
	std::vector<FEntitlementData> Entitlements;
	/** The current ownership cache */
	std::vector<FOwnershipData> Ownerships;
	/** The user's cart */
	FCart UserCart;

	/** The desired store update time */
	static constexpr float UpdateStoreTimeoutSeconds = 300.f;

	/** Amount of time to wait before querying entitlement as the checkout UI was closed while purchase was in flight */
	static constexpr float PurchaseProcessingTimeoutSeconds = 3.f;

	/** Static callback handler for Checkout complete */
	static void EOS_CALL CheckoutCompleteCallbackFn(const EOS_Ecom_CheckoutCallbackInfo* CheckoutData);

	/** Static callback handler for Query Offer complete */
	static void EOS_CALL QueryStoreCompleteCallbackFn(const EOS_Ecom_QueryOffersCallbackInfo* OfferData);

	/** Static callback handler for Query Entitlement complete */
	static void EOS_CALL QueryEntitlementsCompleteCallbackFn(const EOS_Ecom_QueryEntitlementsCallbackInfo* EntitlementData);

	/** Static callback handler for Query OwnershipBySandboxIds complete */
	static void EOS_CALL QueryOwnershipBySandboxIdsCompleteCallbackFn(const EOS_Ecom_QueryOwnershipBySandboxIdsCallbackInfo* OwnershipData);
};
