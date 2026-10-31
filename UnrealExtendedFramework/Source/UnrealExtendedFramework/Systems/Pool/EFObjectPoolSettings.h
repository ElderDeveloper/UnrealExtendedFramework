// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Settings/EFDeveloperSettings.h"
#include "EFObjectPoolSettings.generated.h"

/** Pool size for one actor class. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDFRAMEWORK_API FEFPoolClassSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pool")
	TSoftClassPtr<AActor> ActorClass;

	/** Spawned hidden when a game world begins play, so the first uses do not spawn mid-game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pool", meta = (ClampMin = "0"))
	int32 PrewarmCount = 0;

	/** Inactive actors kept at most; released actors beyond this are destroyed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pool", meta = (ClampMin = "0"))
	int32 MaxPooled = 32;
};

/** Project Settings > Extended Framework > Extended Object Pool. */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Extended Object Pool"))
class UNREALEXTENDEDFRAMEWORK_API UEFObjectPoolSettings : public UEFDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UEFObjectPoolSettings* Get() { return GetDefault<UEFObjectPoolSettings>(); }

	/** Inactive actors kept per class when the class has no entry below. */
	UPROPERTY(config, EditAnywhere, Category = "Pool", meta = (ClampMin = "0"))
	int32 DefaultMaxPooled = 32;

	/** Classes with their own size or prewarm count. */
	UPROPERTY(config, EditAnywhere, Category = "Pool", meta = (TitleProperty = "ActorClass"))
	TArray<FEFPoolClassSettings> Classes;

	/** Settings for Class, or null when it uses the defaults. */
	const FEFPoolClassSettings* FindClassSettings(const UClass* Class) const;
};
