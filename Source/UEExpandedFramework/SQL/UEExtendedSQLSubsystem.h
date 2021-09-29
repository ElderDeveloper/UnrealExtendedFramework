// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Runtime/Online/HTTP/Public/Interfaces/IHttpBase.h"
#include "UEExtendedSQLSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedSQLSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	void CreatePostRequest(FString Data, FString Api, TMemFunPtrType<false, UUEExtendedSQLSubsystem, void(FHttpRequestPtr, FHttpResponsePtr, bool)>::Type InFunc);
	void CreateGetRequest(FString Api, TMemFunPtrType<false, UUEExtendedSQLSubsystem, void(FHttpRequestPtr, FHttpResponsePtr, bool)>::Type InFunc);


	UFUNCTION(BlueprintCallable, Category = Http)
	FString JsonString(TArray<FString> StringArray);
};
