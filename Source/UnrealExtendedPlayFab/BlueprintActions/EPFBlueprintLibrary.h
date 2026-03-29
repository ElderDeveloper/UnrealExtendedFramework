// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Shared/EPFTypes.h"
#include "TitleNews/EPFTitleNewsSubsystem.h"
#include "EPFBlueprintLibrary.generated.h"


/**
 * Static Blueprint helper functions for PlayFab.
 * Available from any Blueprint without a subsystem reference.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ── Status Queries ───────────────────────────────────────────────────────

	/** Check if PlayFab is configured (TitleId set in Project Settings) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility", meta = (WorldContext = "WorldContext"))
	static bool IsPlayFabConfigured();

	/** Check if the player is currently logged in to PlayFab */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility", meta = (WorldContext = "WorldContext"))
	static bool IsPlayFabLoggedIn(const UObject* WorldContext);

	/** Get the current PlayFab Player ID (empty if not logged in) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility", meta = (WorldContext = "WorldContext"))
	static FString GetPlayFabId(const UObject* WorldContext);

	/** Get the current display name (empty if not set) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility", meta = (WorldContext = "WorldContext"))
	static FString GetPlayFabDisplayName(const UObject* WorldContext);


	// ── PlayerData Type Helpers ──────────────────────────────────────────────

	/** Convert an int to a string for PlayerData storage */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility|Conversion", meta = (DisplayName = "Int to PlayFab String"))
	static FString IntToPlayFabString(int32 Value);

	/** Convert a float to a string for PlayerData storage */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility|Conversion", meta = (DisplayName = "Float to PlayFab String"))
	static FString FloatToPlayFabString(float Value);

	/** Convert a bool to a string for PlayerData storage */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility|Conversion", meta = (DisplayName = "Bool to PlayFab String"))
	static FString BoolToPlayFabString(bool bValue);

	/** Parse an int from a PlayerData string value (returns DefaultValue on failure) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility|Conversion", meta = (DisplayName = "PlayFab String to Int"))
	static int32 PlayFabStringToInt(const FString& Value, int32 DefaultValue = 0);

	/** Parse a float from a PlayerData string value (returns DefaultValue on failure) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility|Conversion", meta = (DisplayName = "PlayFab String to Float"))
	static float PlayFabStringToFloat(const FString& Value, float DefaultValue = 0.0f);

	/** Parse a bool from a PlayerData string value (returns DefaultValue on failure) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility|Conversion", meta = (DisplayName = "PlayFab String to Bool"))
	static bool PlayFabStringToBool(const FString& Value, bool DefaultValue = false);


	// ── Cached Data Quick Access ─────────────────────────────────────────────

	/** Get a cached player data value as int (reads from local cache, no server call) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility|CachedData", meta = (WorldContext = "WorldContext", DisplayName = "Get Cached Player Int"))
	static int32 GetCachedPlayerInt(const UObject* WorldContext, const FString& Key, int32 DefaultValue = 0);

	/** Get a cached player data value as float */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility|CachedData", meta = (WorldContext = "WorldContext", DisplayName = "Get Cached Player Float"))
	static float GetCachedPlayerFloat(const UObject* WorldContext, const FString& Key, float DefaultValue = 0.0f);

	/** Get a cached player data value as bool */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility|CachedData", meta = (WorldContext = "WorldContext", DisplayName = "Get Cached Player Bool"))
	static bool GetCachedPlayerBool(const UObject* WorldContext, const FString& Key, bool DefaultValue = false);

	/** Get a cached player data value as string */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility|CachedData", meta = (WorldContext = "WorldContext", DisplayName = "Get Cached Player String"))
	static FString GetCachedPlayerString(const UObject* WorldContext, const FString& Key);

	/** Get a cached stat value by name (-1 if not found) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility|CachedData", meta = (WorldContext = "WorldContext", DisplayName = "Get Cached Stat"))
	static int32 GetCachedStat(const UObject* WorldContext, const FString& StatName);


	// ── Struct Constructors ──────────────────────────────────────────────────

	/** Create an analytics event struct for batch logging */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility|Construct", meta = (DisplayName = "Make Analytics Event"))
	static FEPFAnalyticsEvent MakeAnalyticsEvent(const FString& EventName, const TMap<FString, FString>& Body);

	/** Create a stat entry for the Make Map node */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility|Construct", meta = (DisplayName = "Make Stat Update", CompactNodeTitle = "STAT"))
	static void MakeStatUpdate(const FString& StatName, int32 Value, FString& OutKey, int32& OutValue);


	// ── Result Helpers ───────────────────────────────────────────────────────

	/** Check if a result struct represents success */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility", meta = (DisplayName = "Was Successful"))
	static bool WasSuccessful(const FEPFResult& Result);

	/** Get the error message from a result struct */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Utility", meta = (DisplayName = "Get Error Message"))
	static FString GetErrorMessage(const FEPFResult& Result);
};
