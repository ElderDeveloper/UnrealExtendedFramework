// EFSubtitleQueuePolicy.h - Swappable subtitle scheduling policy
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleData.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Subsystem/EFSubtitleQueue.h"
#include "EFSubtitleQueuePolicy.generated.h"

/**
 * Abstract queue policy. LocalSubsystem owns one instance selected by project settings.
 */
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleQueuePolicy : public UObject
{
	GENERATED_BODY()

public:
	virtual void Configure(EEFSubtitleQueueMode Mode, int32 MaxStacked) {}

	virtual int32 Enqueue(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request)
	PURE_VIRTUAL(UEFSubtitleQueuePolicy::Enqueue, return 0;);

	virtual void Cancel(int32 RequestId)
	PURE_VIRTUAL(UEFSubtitleQueuePolicy::Cancel, );

	virtual void ClearAll()
	PURE_VIRTUAL(UEFSubtitleQueuePolicy::ClearAll, );

	virtual void Tick(float DeltaTime)
	PURE_VIRTUAL(UEFSubtitleQueuePolicy::Tick, );

	virtual const FEFActiveSubtitle& GetActive() const
	PURE_VIRTUAL(UEFSubtitleQueuePolicy::GetActive, return InvalidSubtitle;);

	virtual const TArray<FEFActiveSubtitle>& GetAllActive() const
	PURE_VIRTUAL(UEFSubtitleQueuePolicy::GetAllActive, return EmptyActive;);

	virtual bool IsEmpty() const
	PURE_VIRTUAL(UEFSubtitleQueuePolicy::IsEmpty, return true;);

	virtual EEFSubtitleQueueMode GetQueueMode() const
	PURE_VIRTUAL(UEFSubtitleQueuePolicy::GetQueueMode, return EEFSubtitleQueueMode::Replace;);

	FOnActiveSubtitleChanged OnActiveChanged;
	FOnSubtitleExpired OnExpired;

protected:
	static FEFActiveSubtitle InvalidSubtitle;
	static TArray<FEFActiveSubtitle> EmptyActive;
};

/**
 * Default policy wrapping FEFSubtitleQueue (Replace / Queue / PriorityQueue / Stack).
 */
UCLASS(BlueprintType, meta = (DisplayName = "Default Subtitle Queue Policy"))
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleQueuePolicy_Default : public UEFSubtitleQueuePolicy
{
	GENERATED_BODY()

public:
	virtual void Configure(EEFSubtitleQueueMode Mode, int32 MaxStacked) override;
	virtual int32 Enqueue(const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request) override;
	virtual void Cancel(int32 RequestId) override;
	virtual void ClearAll() override;
	virtual void Tick(float DeltaTime) override;
	virtual const FEFActiveSubtitle& GetActive() const override;
	virtual const TArray<FEFActiveSubtitle>& GetAllActive() const override;
	virtual bool IsEmpty() const override;
	virtual EEFSubtitleQueueMode GetQueueMode() const override;

private:
	void BindQueueDelegates();

	FEFSubtitleQueue Queue;
	bool bDelegatesBound = false;
};
