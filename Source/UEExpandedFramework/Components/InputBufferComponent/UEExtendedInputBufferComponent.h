// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/UserDefinedEnum.h"
#include "UEExtendedInputBufferComponent.generated.h"




DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExtendedInputBufferOpened);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExtendedInputBufferClosed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExtendedInputBufferConsumed , FName , Input);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UEEXPANDEDFRAMEWORK_API UUEExtendedInputBufferComponent : public UActorComponent
{
	GENERATED_BODY()
	
	// Sets default values for this component's properties
	UUEExtendedInputBufferComponent()
	{
		PrimaryComponentTick.bCanEverTick = false;
	}
	
	FName StoredKey;
	bool IsOpen;

public:
	
	UPROPERTY(BlueprintAssignable)
	FOnExtendedInputBufferOpened OnExtendedInputBufferOpened;

	UPROPERTY(BlueprintAssignable)
	FOnExtendedInputBufferClosed OnExtendedInputBufferClosed;

	UPROPERTY(BlueprintAssignable)
	FOnExtendedInputBufferConsumed OnExtendedInputBufferConsumed;

	UFUNCTION(BlueprintCallable , Category="Input Buffer")
	void ConsumeInputBuffer() {	OnExtendedInputBufferConsumed.Broadcast(StoredKey); StoredKey = FName(); }

	UFUNCTION(BlueprintCallable , Category="Input Buffer")
	void OpenInputBuffer() { IsOpen = true; OnExtendedInputBufferOpened.Broadcast(); } 

	UFUNCTION(BlueprintCallable , Category="Input Buffer")
	void CloseInputBuffer() { IsOpen = false; OnExtendedInputBufferClosed.Broadcast(); ConsumeInputBuffer(); }

	UFUNCTION(BlueprintCallable , Category="Input Buffer")
	void UpdateInputBuffer(const FName Key) { StoredKey = Key; if(!IsOpen) ConsumeInputBuffer();	}

	UFUNCTION(BlueprintCallable , Category="Input Buffer")
	FName GetStoredKey() const { return StoredKey; }
	
};
