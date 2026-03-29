# 15 — Ecom / Store

## Subsystem: `UEEOSEcomSubsystem`

Manages EOS Commerce — store catalog, entitlements, ownership, and checkout.

### Offers / Catalog
| Function | Description |
|---|---|
| `QueryOffers()` | Fetch available store offers via `IOnlineStoreV2` |
| `GetCachedOffers()` | `TArray<FEEOSCatalogOffer>` |
| `GetOfferById(OfferId)` | Single offer lookup |
| `GetOfferCount()` | int32 |

### Entitlements
| Function | Description |
|---|---|
| `QueryEntitlements()` | Fetch owned entitlements via `IOnlineEntitlements` |
| `GetCachedEntitlements()` | `TArray<FEEOSEntitlement>` |
| `HasEntitlement(Name)` | bool — check by entitlement name |
| `GetEntitlementCount()` | int32 |

### Ownership
| Function | Description |
|---|---|
| `QueryOwnership(CatalogItemIds)` | Check if items are owned |
| `IsItemOwned(CatalogItemId)` | bool — from cached ownership |

### Checkout / Purchase
| Function | Description |
|---|---|
| `Checkout(OfferId)` | Initiate purchase via `IOnlinePurchase` |

### Delegates
| Delegate | Params |
|---|---|
| `OnOffersQueried` | bSuccess, Offers array |
| `OnEntitlementsQueried` | bSuccess, Entitlements array |
| `OnOwnershipQueried` | bSuccess, OwnedItems array |
| `OnCheckoutComplete` | bSuccess, TransactionId |

### Types

**`FEEOSCatalogOffer`**: OfferId, Title, Description, LongDescription, FormattedPrice, PriceInSmallestUnit, OriginalPriceInSmallestUnit, CurrencyCode, ItemCount, bAvailableForPurchase

**`FEEOSEntitlement`**: EntitlementId, EntitlementName, CatalogItemId, bRedeemed

**`FEEOSOwnership`**: ItemId, bOwned

### Flow
```
QueryOffers() → OnOffersQueried → browse catalog
Checkout("offer123") → OnCheckoutComplete(true, "txn456")
QueryEntitlements() → verify purchases
```
