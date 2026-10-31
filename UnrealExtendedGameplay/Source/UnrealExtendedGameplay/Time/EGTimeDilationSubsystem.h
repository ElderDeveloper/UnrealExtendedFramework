// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGTimeDilationStack.h"
#include "Subsystems/WorldSubsystem.h"

#include "EGTimeDilationSubsystem.generated.h"

class USkeletalMeshComponent;

/** Identifies one time dilation request. Keep it to remove the request; a default handle is invalid. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGTimeDilationHandle
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	bool IsValid() const { return Id != 0; }
	void Invalidate() { Id = 0; }
};

/** How a time dilation request blends and how long it lasts. Times are real (undilated) seconds. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGTimeDilationParams
{
	GENERATED_BODY()

	/** Higher wins. Requests with the same priority: the newest wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Dilation")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Dilation", meta = (ClampMin = "0", Units = "s"))
	float BlendInTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Dilation", meta = (ClampMin = "0", Units = "s"))
	float BlendOutTime = 0.0f;

	/** The request removes itself after this long. 0 lasts until removed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Dilation", meta = (ClampMin = "0", Units = "s"))
	float Duration = 0.0f;
};

/**
 * UEGTimeDilationSubsystem
 *
 * The one owner of time scaling, so slow motion, hit pause and pause-like effects stack instead
 * of overwriting each other. Three kinds of target, each with its own stack (FEGTimeDilationStack):
 *
 *  - The world's global time dilation. Server / standalone only: it replicates from the server,
 *    so client requests are refused.
 *  - An actor's CustomTimeDilation. Local to the machine that asks.
 *  - A skeletal mesh's GlobalAnimRateScale: slows only the animation, not movement or physics,
 *    which makes it the safe choice for hit pause in multiplayer. Local to the machine that asks.
 *    Root motion comes from the animation and slows with it.
 *
 * Something else setting the same value while a request is active is overwritten the next time
 * the stack's value changes, and the value from before the first request is restored at the end.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGTimeDilationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UEGTimeDilationSubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Extended|Time Dilation")
	FEGTimeDilationHandle PushGlobalTimeDilation(float Dilation, FEGTimeDilationParams Params);

	UFUNCTION(BlueprintCallable, Category = "Extended|Time Dilation")
	FEGTimeDilationHandle PushActorTimeDilation(AActor* Actor, float Dilation, FEGTimeDilationParams Params);

	UFUNCTION(BlueprintCallable, Category = "Extended|Time Dilation")
	FEGTimeDilationHandle PushAnimRateScale(USkeletalMeshComponent* Mesh, float RateScale, FEGTimeDilationParams Params);

	/** Removes the request and invalidates the handle. Safe on an expired or invalid handle. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Time Dilation")
	void RemoveTimeDilation(UPARAM(ref) FEGTimeDilationHandle& Handle);

	UFUNCTION(BlueprintPure, Category = "Extended|Time Dilation")
	bool IsTimeDilationActive(const FEGTimeDilationHandle& Handle) const;

	/**
	 * The global time dilation to return to after the last global request ends (instead of the
	 * value from before the first). Applied at once when there are no global requests.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Time Dilation")
	void SetGlobalRestoreValue(float Dilation, float BlendTime = 0.0f);

	UFUNCTION(BlueprintPure, Category = "Extended|Time Dilation")
	bool HasGlobalRequests() const;

	/** Freezes the animation of each mesh for Duration real seconds (hit stop). */
	UFUNCTION(BlueprintCallable, Category = "Extended|Time Dilation")
	void HitPause(const TArray<USkeletalMeshComponent*>& Meshes, float Duration = 0.08f, float AnimRateScale = 0.02f);

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	enum class ETarget : uint8
	{
		World,
		Actor,
		AnimRate
	};

	struct FHandleTarget
	{
		ETarget Kind = ETarget::World;
		TWeakObjectPtr<UObject> Object;
	};

	struct FChannel
	{
		ETarget Kind = ETarget::World;
		TWeakObjectPtr<UObject> Object;
		FEGTimeDilationStack Stack;
		float LastApplied = 1.0f;
	};

	FEGTimeDilationHandle PushInternal(ETarget Kind, UObject* Object, float Value, const FEGTimeDilationParams& Params);
	FChannel* FindChannel(ETarget Kind, const UObject* Object);
	FChannel& FindOrAddChannel(ETarget Kind, UObject* Object);

	float ReadValue(ETarget Kind, UObject* Object) const;
	void WriteValue(ETarget Kind, UObject* Object, float Value) const;
	void ApplyChannel(FChannel& Channel, bool bForce);
	void ApplyAndPrune();

	TArray<FChannel> Channels;
	TMap<int32, FHandleTarget> Handles;
	double RealTime = 0.0;
	int32 NextHandleId = 0;
};
