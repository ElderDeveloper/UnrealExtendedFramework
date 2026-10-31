// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Analytics/EFAnalyticsTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StructUtils/InstancedStruct.h"
#include "EFAnalyticsLibrary.generated.h"

/** Blueprint shortcuts to UEFAnalyticsSubsystem that work from any world context. */
UCLASS(meta = (ScriptName = "EFAnalyticsLibrary"))
class EXTENDEDFOUNDATION_API UEFAnalyticsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Records an analytics event, e.g. level_completed with level and seconds attributes. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Analytics", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Attributes"))
	static void RecordAnalyticsEvent(const UObject* WorldContextObject, const FString& Name, const TMap<FString, FString>& Attributes);

	/** Records an event whose attributes are the fields of the struct in Payload (Make Instanced Struct). */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Analytics", meta = (WorldContext = "WorldContextObject"))
	static void RecordAnalyticsStructEvent(const UObject* WorldContextObject, const FString& Name, const FInstancedStruct& Payload);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Analytics", meta = (WorldContext = "WorldContextObject"))
	static void SetAnalyticsConsent(const UObject* WorldContextObject, EEFAnalyticsConsent Consent);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Analytics", meta = (WorldContext = "WorldContextObject"))
	static EEFAnalyticsConsent GetAnalyticsConsent(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Analytics", meta = (WorldContext = "WorldContextObject"))
	static void SetAnalyticsUserId(const UObject* WorldContextObject, const FString& UserId);

	/** Adds an attribute to every following event; an empty Value removes it. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Analytics", meta = (WorldContext = "WorldContextObject"))
	static void SetAnalyticsGlobalAttribute(const UObject* WorldContextObject, const FString& Key, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Analytics", meta = (WorldContext = "WorldContextObject"))
	static void FlushAnalytics(const UObject* WorldContextObject);

	/** The most recent events, oldest first, for debug UI. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Analytics", meta = (WorldContext = "WorldContextObject"))
	static TArray<FEFAnalyticsEvent> GetRecentAnalyticsEvents(const UObject* WorldContextObject);
};
