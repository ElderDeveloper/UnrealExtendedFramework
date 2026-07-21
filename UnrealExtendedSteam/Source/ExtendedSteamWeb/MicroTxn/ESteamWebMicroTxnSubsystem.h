// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebMicroTxnSubsystem.generated.h"

/**
 * ISteamMicroTxn — in-game microtransactions (partner host, publisher Web API key required).
 * Purchases are a three-way handshake: InitTxn creates the order, the user approves it in the
 * Steam overlay/client, then FinalizeTxn settles it (or RefundTxn reverses it).
 *
 * Sandbox: when UESteamWebSettings::bMicroTxnSandbox is true, every call is routed to the
 * ISteamMicroTxnSandbox interface instead — identical methods, but no real money moves,
 * so purchase flows can be tested end to end safely.
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebMicroTxnSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * ISteamMicroTxn/GetUserInfo/v2 — the user's wallet country, currency and status,
	 * needed to price InitTxn correctly. An empty SteamId falls back to the configured DevSteamId;
	 * IpAddress (the user's IP, for web-based purchases) is omitted when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|MicroTxn", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetUserInfo(FString SteamId, FString IpAddress, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamMicroTxn/InitTxn/v3 (POST) — creates a new purchase order. OrderId must be a unique
	 * 64-bit id supplied by the game. ItemIds/Quantities/Amounts/Descriptions are parallel arrays
	 * (one entry per line item) sent as itemcount + indexed itemid[N]/qty[N]/amount[N]/description[N];
	 * Amounts are totals in the currency's smallest unit (e.g. cents) as strings.
	 * UserSession ("client" or "web") and IpAddress (required for web sessions) are omitted when empty.
	 * AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|MicroTxn", meta = (AutoCreateRefTerm = "OnResponse"))
	void InitTxn(FString OrderId, FString SteamId, int32 AppId, FString Language, FString Currency, FString UserSession, FString IpAddress, const TArray<int32>& ItemIds, const TArray<int32>& Quantities, const TArray<FString>& Amounts, const TArray<FString>& Descriptions, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamMicroTxn/FinalizeTxn/v2 (POST) — completes a purchase the user authorized in Steam.
	 * Must be called for the transaction to settle; unauthorized orders fail here.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|MicroTxn", meta = (AutoCreateRefTerm = "OnResponse"))
	void FinalizeTxn(FString OrderId, int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamMicroTxn/QueryTxn/v3 — the current status of a transaction, looked up by
	 * game OrderId or Steam TransId (pass one; the empty one is omitted).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|MicroTxn", meta = (AutoCreateRefTerm = "OnResponse"))
	void QueryTxn(int32 AppId, FString OrderId, FString TransId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamMicroTxn/RefundTxn/v2 (POST) — refunds a previously finalized transaction in full.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|MicroTxn", meta = (AutoCreateRefTerm = "OnResponse"))
	void RefundTxn(FString OrderId, int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamMicroTxn/GetReport/v5 — transaction report for reconciliation. Time is RFC 3339 UTC
	 * (report entries at or after this time). Type is GAMESALES, STEAMSTORE, SETTLEMENT or
	 * CHARGEBACK (omitted when empty — the endpoint defaults to GAMESALES);
	 * MaxResults <= 0 uses the endpoint default (1000, max 100000).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|MicroTxn", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetReport(int32 AppId, FString Time, FString Type, int32 MaxResults, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamMicroTxn/GetUserAgreementInfo/v2 — the recurring-billing agreements (subscriptions) a user
	 * holds for the app. Note this is v2, not the v1 the sketch listed. An empty SteamId falls back to
	 * the configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 * Honors the sandbox switch (ISteamMicroTxnSandbox when bMicroTxnSandbox).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|MicroTxn", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetUserAgreementInfo(int32 AppId, FString SteamId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamMicroTxn/ProcessAgreement/v1 (POST) — charges a due recurring-billing agreement, creating a
	 * new order. Amount is a total in the currency's smallest unit (e.g. cents) passed as a string
	 * (matching InitTxn's amount[N]) so large totals do not overflow; Currency is ISO 4217.
	 * The docs require amount/currency the sketch omitted (added here). An empty SteamId falls back to
	 * the configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 * Honors the sandbox switch (ISteamMicroTxnSandbox when bMicroTxnSandbox).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|MicroTxn", meta = (AutoCreateRefTerm = "OnResponse"))
	void ProcessAgreement(int32 AppId, FString SteamId, FString AgreementId, FString OrderId, FString Amount, FString Currency, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamMicroTxn/AdjustAgreement/v1 (POST) — reschedules a recurring-billing agreement's next
	 * charge. NextProcessDate is the new date (YYYYMMDD); OldNextProcessDate, when supplied, guards
	 * against a stale update and is omitted when empty. An empty SteamId falls back to the configured
	 * DevSteamId; AppId <= 0 falls back to the configured AppId.
	 * Honors the sandbox switch (ISteamMicroTxnSandbox when bMicroTxnSandbox).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|MicroTxn", meta = (AutoCreateRefTerm = "OnResponse"))
	void AdjustAgreement(int32 AppId, FString SteamId, FString AgreementId, FString NextProcessDate, FString OldNextProcessDate, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamMicroTxn/CancelAgreement/v1 (POST) — cancels a user's recurring-billing agreement.
	 * An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 * Honors the sandbox switch (ISteamMicroTxnSandbox when bMicroTxnSandbox).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|MicroTxn", meta = (AutoCreateRefTerm = "OnResponse"))
	void CancelAgreement(int32 AppId, FString SteamId, FString AgreementId, const FOnSteamWebResponse& OnResponse);

private:
	/** Interface name honoring the sandbox switch: ISteamMicroTxnSandbox when bMicroTxnSandbox, else ISteamMicroTxn. */
	FString GetMicroTxnInterface() const;
};
