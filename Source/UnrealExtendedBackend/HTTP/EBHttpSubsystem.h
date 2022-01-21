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


	UFUNCTION(BlueprintCallable, Category = Http)
	FString JsonString(TArray<FString> StringArray);
};
