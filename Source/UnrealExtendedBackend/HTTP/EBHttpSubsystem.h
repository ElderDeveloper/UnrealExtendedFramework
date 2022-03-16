// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "EBHttpSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDBACKEND_API UEBHttpSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	void CreatePostRequest(FString Data, FString Api, TMemFunPtrType<false, UEBHttpSubsystem, void(FHttpRequestPtr, FHttpResponsePtr, bool)>::Type InFunc);
	void CreateGetRequest(FString Api, TMemFunPtrType<false, UEBHttpSubsystem, void(FHttpRequestPtr, FHttpResponsePtr, bool)>::Type InFunc);


	/*Sends an http request*/
	UFUNCTION(BlueprintCallable, Category = Http)
	void SendHttpRequest(const FString& Url, const FString& RequestContent);

	/*Called when the server has responded to our http request*/
	void OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	
	UFUNCTION(BlueprintCallable, Category = Http)
	FString JsonString(TArray<FString> StringArray);
};
