// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EEOSTypes.generated.h"


// ── Login Types ──────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EEOSLoginType : uint8
{
	Password			UMETA(DisplayName = "Password (Email + Password)"),
	ExchangeCode		UMETA(DisplayName = "Exchange Code"),
	PersistentAuth		UMETA(DisplayName = "Persistent Auth (Saved Token)"),
	/** Not supported by the EOS SDK (superseded by ExternalAuth) — kept only so serialized settings don't break */
	DeviceCode			UMETA(DisplayName = "Device Code (Unsupported)", Hidden),
	Developer			UMETA(DisplayName = "Developer (DevAuth Tool)"),
	AccountPortal		UMETA(DisplayName = "Account Portal (Browser)"),
	ExternalAuth		UMETA(DisplayName = "External Auth (Steam, PSN, etc.)")
};

UENUM(BlueprintType)
enum class EEOSExternalCredentialType : uint8
{
	None				UMETA(DisplayName = "None"),
	Steam				UMETA(DisplayName = "Steam"),
	PSN					UMETA(DisplayName = "PlayStation Network"),
	XboxLive			UMETA(DisplayName = "Xbox Live"),
	Nintendo			UMETA(DisplayName = "Nintendo"),
	Discord				UMETA(DisplayName = "Discord"),
	OpenID				UMETA(DisplayName = "OpenID"),
	Apple				UMETA(DisplayName = "Apple"),
	Google				UMETA(DisplayName = "Google")
};

/**
 * Login type for EOS Connect (Game Services).
 * Maps to EOS_EExternalCredentialType values.
 * Use this for platform-specific auth without requiring an Epic Games account.
 */
UENUM(BlueprintType)
enum class EEOSConnectLoginType : uint8
{
	/** Uses Epic ID Token from an existing EOS Auth login */
	Epic			UMETA(DisplayName = "Epic (from Auth login)"),
	/** Steam Auth Session Ticket — ISteamUser::GetAuthTicketForWebApi */
	Steam			UMETA(DisplayName = "Steam Session Ticket"),
	/** PlayStation Network ID Token */
	PSN				UMETA(DisplayName = "PlayStation Network"),
	/** Xbox Live XSTS Token */
	XboxLive		UMETA(DisplayName = "Xbox Live"),
	/** Nintendo Account ID Token */
	Nintendo		UMETA(DisplayName = "Nintendo"),
	/** Discord Access Token */
	Discord			UMETA(DisplayName = "Discord"),
	/** Anonymous device-based login — no platform credentials needed */
	DeviceId		UMETA(DisplayName = "Device ID (Anonymous)"),
	/** OpenID Provider Access Token */
	OpenID			UMETA(DisplayName = "OpenID"),
	/** Apple ID Token */
	Apple			UMETA(DisplayName = "Apple"),
	/** Google ID Token */
	Google			UMETA(DisplayName = "Google")
};


// ── Region Info ──────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EEOSRegionInfo : uint8
{
	NoSelection			UMETA(DisplayName = "Any Region"),
	Asia				UMETA(DisplayName = "Asia"),
	NorthAmerica		UMETA(DisplayName = "North America"),
	SouthAmerica		UMETA(DisplayName = "South America"),
	Africa				UMETA(DisplayName = "Africa"),
	Europe				UMETA(DisplayName = "Europe"),
	Australia			UMETA(DisplayName = "Australia")
};


// ── Session State ────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EEOSSessionState : uint8
{
	NoSession			UMETA(DisplayName = "No Session"),
	Creating			UMETA(DisplayName = "Creating"),
	Pending				UMETA(DisplayName = "Pending"),
	Starting			UMETA(DisplayName = "Starting"),
	InProgress			UMETA(DisplayName = "In Progress"),
	Ending				UMETA(DisplayName = "Ending"),
	Ended				UMETA(DisplayName = "Ended"),
	Destroying			UMETA(DisplayName = "Destroying")
};


// ── Sanction Appeal Type ─────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EEOSSanctionAppealType : uint8
{
	IncorrectSanction		UMETA(DisplayName = "Incorrect Sanction"),
	CompromisedAccount		UMETA(DisplayName = "Compromised Account"),
	UnfairPunishment		UMETA(DisplayName = "Unfair Punishment"),
	AppealForForgiveness	UMETA(DisplayName = "Appeal for Forgiveness")
};


// ── Login Status ─────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EEOSLoginStatus : uint8
{
	NotLoggedIn,
	LoggingIn,
	LoggedIn,
	Failed
};


// ── Generic Result ───────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "EOS")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadOnly, Category = "EOS")
	FString ErrorCode;

	static FEEOSResult Success()
	{
		FEEOSResult Result;
		Result.bSuccess = true;
		return Result;
	}

	static FEEOSResult Failure(const FString& InErrorMessage, const FString& InErrorCode = TEXT(""))
	{
		FEEOSResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = InErrorMessage;
		Result.ErrorCode = InErrorCode;
		return Result;
	}
};


// ── Session Settings (Advanced) ──────────────────────────────────────────────

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSSessionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	int32 MaxPlayers = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	int32 NumPrivateConnections = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	bool bIsDedicatedServer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	bool bIsLANMatch = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	bool bShouldAdvertise = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	bool bAllowJoinInProgress = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	bool bUsesPresence = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	bool bAllowJoinViaPresence = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	bool bAllowJoinViaPresenceFriendsOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	bool bAllowInvites = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	bool bUseLobbiesIfAvailable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	bool bUseLobbiesVoiceChatIfAvailable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	bool bUsesStats = false;

	/**
	 * Preferred region for this session. EOS sessions have no native region field, so when this
	 * is not NoSelection it is advertised as a custom "REGION" session attribute with a stable
	 * string value ("Asia", "NorthAmerica", "SouthAmerica", "Africa", "Europe", "Australia").
	 * Searches can filter on it by passing "REGION" with one of those values in
	 * FindSessionsFiltered's filter map — no region filter is added to searches automatically.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	EEOSRegionInfo Region = EEOSRegionInfo::NoSelection;

	/** Custom key-value attributes advertised for search filtering */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Sessions")
	TMap<FString, FString> CustomSettings;
};


// ── Session Search Result ────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSSessionSearchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Sessions")
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Sessions")
	FString OwnerName;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Sessions")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Sessions")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Sessions")
	int32 Ping = 0;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Sessions")
	bool bIsDedicatedServer = false;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Sessions")
	TMap<FName, FString> Settings;
};


// ── Leaderboard Entry ────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSLeaderboardEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Leaderboards")
	int32 Rank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Leaderboards")
	FString UserId;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Leaderboards")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Leaderboards")
	int32 Score = 0;
};


// ── Achievement ──────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSAchievement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Achievements")
	FString AchievementId;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Achievements")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Achievements")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Achievements")
	float Progress = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Achievements")
	bool bUnlocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Achievements")
	FDateTime UnlockTime;
};


// ── Friend Info ──────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSFriendInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Friends")
	FString UserId;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Friends")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Friends")
	FString PresenceStatus;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Friends")
	bool bIsOnline = false;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Friends")
	bool bIsPlayingThisGame = false;
};


// ── Presence Info ────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSPresenceInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Presence")
	FString UserId;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Presence")
	FString Status;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Presence")
	FString RichText;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Presence")
	bool bIsOnline = false;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Presence")
	bool bIsPlaying = false;
};


// ── Stat Entry ───────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSStat
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Stats")
	FString StatName;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Stats")
	int32 Value = 0;
};


// ── Sanction Info ────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSSanction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Sanctions")
	FString SanctionId;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Sanctions")
	FString Action;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Sanctions")
	FDateTime TimePlaced;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Sanctions")
	FDateTime TimeExpires;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Sanctions")
	bool bIsPermanent = false;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Sanctions")
	FString Reason;
};


// ── Voice Device Info ────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSVoiceDeviceInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Voice")
	FString DeviceId;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Voice")
	FString DisplayName;
};


// ── Ecom / Store Types ───────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSCatalogOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	FString OfferId;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	FString LongDescription;

	/** Price as formatted string (e.g., "$9.99") */
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	FString FormattedPrice;

	/** Price in smallest currency unit (e.g., 999 = $9.99) */
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	int64 PriceInSmallestUnit = 0;

	/** Original price before discount */
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	int64 OriginalPriceInSmallestUnit = 0;

	/** Currency code (e.g., "USD") */
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	FString CurrencyCode;

	/** Number of items in this offer */
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	int32 ItemCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	bool bAvailableForPurchase = false;
};

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSEntitlement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	FString EntitlementId;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	FString EntitlementName;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	FString CatalogItemId;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	bool bRedeemed = false;
};

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSOwnership
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	FString ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Ecom")
	bool bOwned = false;
};


// ── User Info Types ──────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSUserInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS|UserInfo")
	FString EpicAccountId;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|UserInfo")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|UserInfo")
	FString Nickname;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|UserInfo")
	FString Country;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|UserInfo")
	FString PreferredLanguage;
};

USTRUCT(BlueprintType)
struct EXTENDEDEOSSHARED_API FEEOSExternalAccountMapping
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS|UserInfo")
	FString ProductUserId;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|UserInfo")
	FString ExternalAccountId;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|UserInfo")
	FString ExternalAccountType;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|UserInfo")
	FString DisplayName;
};


// ── Common Delegates ─────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSOperationComplete, const FEEOSResult&, Result);
