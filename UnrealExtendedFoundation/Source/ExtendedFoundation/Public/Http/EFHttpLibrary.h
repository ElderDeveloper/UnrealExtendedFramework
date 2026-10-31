// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Http/EFHttpTypes.h"
#include "Json/EFJsonTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EFHttpLibrary.generated.h"

/** Helpers for building HTTP requests and reading responses. */
UCLASS(meta = (ScriptName = "EFHttpLibrary"))
class EXTENDEDFOUNDATION_API UEFHttpLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** A request with a URL and verb and every other setting at its default. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Http")
	static FEFHttpRequest MakeHttpRequest(const FString& Url, EEFHttpVerb Verb = EEFHttpVerb::Get);

	/** A request that sends Body as JSON and asks for JSON back. An empty Body handle sends no body. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Http")
	static FEFHttpRequest MakeJsonHttpRequest(const FString& Url, EEFHttpVerb Verb, const FEFJsonObject& Body);

	/** Returns the request with a header added or replaced. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Http")
	static FEFHttpRequest SetRequestHeader(const FEFHttpRequest& Request, const FString& Name, const FString& Value);

	/** Returns the request with an "Authorization: Bearer <Token>" header. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Http")
	static FEFHttpRequest SetBearerToken(const FEFHttpRequest& Request, const FString& Token);

	/** Parses the response body as a JSON object. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Http")
	static bool GetResponseJson(const FEFHttpResponse& Response, FEFJsonObject& OutObject);

	/** Parses the response body as a JSON object or array. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Http")
	static bool GetResponseJsonValue(const FEFHttpResponse& Response, FEFJsonValue& OutValue);

	/** Looks up a response header, ignoring case. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Http")
	static bool GetResponseHeader(const FEFHttpResponse& Response, const FString& Name, FString& OutValue);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Http", meta = (DisplayName = "Is Success (Http Response)"))
	static bool IsHttpSuccess(const FEFHttpResponse& Response);

	/** True for 200-299. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Http")
	static bool IsSuccessStatusCode(int32 StatusCode);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Http")
	static FString HttpVerbToString(EEFHttpVerb Verb);

	/** Percent-encodes text for use in a URL query. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Http")
	static FString UrlEncode(const FString& Value);

	/** Appends URL-encoded query parameters, keeping any existing query string. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Http")
	static FString BuildUrl(const FString& BaseUrl, const TMap<FString, FString>& QueryParameters);
};
