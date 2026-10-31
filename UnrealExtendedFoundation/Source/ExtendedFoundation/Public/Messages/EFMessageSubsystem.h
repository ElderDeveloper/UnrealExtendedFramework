// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EFMessageSubsystem.generated.h"

class UEFMessageSubsystem;

/** Which channels a listener hears. */
UENUM(BlueprintType)
enum class EEFMessageMatch : uint8
{
	/** Only messages sent on exactly this channel. */
	ExactMatch,
	/** Messages on this channel and on every channel below it: Game.Combat hears Game.Combat.EnemyKilled. */
	PartialMatch
};

/** One message that was sent, kept for debug tools. */
USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFMessageRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Messages")
	FGameplayTag Channel;

	/** The payload's struct name, or None for a message without payload. */
	UPROPERTY(BlueprintReadOnly, Category = "Messages")
	FName PayloadType;

	/** Listeners that received it. */
	UPROPERTY(BlueprintReadOnly, Category = "Messages")
	int32 ListenerCount = 0;

	/** FPlatformTime::Seconds() when it was sent. */
	UPROPERTY(BlueprintReadOnly, Category = "Messages")
	double TimeSeconds = 0.0;
};

/** Identifies a listener so it can be removed. Removing it twice, or after the subsystem is gone, is harmless. */
struct EXTENDEDFOUNDATION_API FEFMessageListenerHandle
{
	bool IsValid() const { return Id != 0; }

	/** Removes the listener and clears the handle. */
	void Unregister();

private:
	friend class UEFMessageSubsystem;

	TWeakObjectPtr<UEFMessageSubsystem> Subsystem;
	FGameplayTag Channel;
	int64 Id = 0;
};

/**
 * A message bus for one game instance: code that sends a message does not need to know who listens.
 *
 * Messages travel on gameplay-tag channels and carry any USTRUCT as payload. One "enemy killed"
 * message can update a quest, an achievement, the HUD and analytics without any of them holding a
 * reference to the others.
 *
 *     // Sender
 *     UEFMessageSubsystem::Get(this)->BroadcastMessage(TAG_Game_Combat_EnemyKilled, FMyEnemyKilled{ Enemy, Weapon });
 *
 *     // Listener
 *     Handle = UEFMessageSubsystem::Get(this)->RegisterListener<FMyEnemyKilled>(TAG_Game_Combat_EnemyKilled,
 *         this, &UMyQuestTracker::HandleEnemyKilled);
 *     ...
 *     Handle.Unregister();
 *
 * Blueprint: Broadcast Message (payload from Make Instanced Struct) and the Listen For Message async
 * node (read the payload with Get Instanced Struct Value).
 *
 * - Listeners run on the game thread, before BroadcastMessage returns, in the order they registered
 *   (exact channel first, then each parent channel).
 * - Listeners may send messages and add or remove listeners while they run. Messages sent from a
 *   listener are delivered at once, up to a nesting depth of 16.
 * - Messages are local to this game instance. They are not replicated; send an RPC first if other
 *   machines need to know.
 */
UCLASS()
class EXTENDEDFOUNDATION_API UEFMessageSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	using FListener = TFunction<void(FGameplayTag Channel, const FInstancedStruct& Payload)>;

	/** Every message, after its listeners ran. For tools such as the debug menu and analytics forwarding. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAnyMessage, FGameplayTag /*Channel*/, const FInstancedStruct& /*Payload*/);

	/** The subsystem of WorldContextObject's game instance, or null. */
	static UEFMessageSubsystem* Get(const UObject* WorldContextObject);

	virtual void Deinitialize() override;

	/** Sends Payload to every listener that hears Channel. Payload may be empty. */
	void BroadcastMessage(FGameplayTag Channel, const FInstancedStruct& Payload);

	template <typename TPayload>
	void BroadcastMessage(FGameplayTag Channel, const TPayload& Payload)
	{
		BroadcastMessage(Channel, FInstancedStruct::Make(Payload));
	}

	/**
	 * Calls Callback for messages on Channel. With PayloadType set, only messages whose payload is
	 * that struct or a child of it are delivered. With Owner set, the listener is removed once Owner
	 * is destroyed, so a forgotten handle cannot call into a dead object.
	 */
	FEFMessageListenerHandle RegisterListener(FGameplayTag Channel, FListener Callback, EEFMessageMatch Match = EEFMessageMatch::ExactMatch,
		const UScriptStruct* PayloadType = nullptr, const UObject* Owner = nullptr);

	/** Typed listener: receives only payloads of type TPayload (or a child struct). */
	template <typename TPayload>
	FEFMessageListenerHandle RegisterListener(FGameplayTag Channel, TFunction<void(FGameplayTag, const TPayload&)> Callback,
		EEFMessageMatch Match = EEFMessageMatch::ExactMatch, const UObject* Owner = nullptr)
	{
		return RegisterListener(Channel, [Callback = MoveTemp(Callback)](FGameplayTag ActualChannel, const FInstancedStruct& Payload)
		{
			if (const TPayload* Typed = Payload.GetPtr<TPayload>())
			{
				Callback(ActualChannel, *Typed);
			}
		}, Match, TBaseStructure<TPayload>::Get(), Owner);
	}

	/** Typed listener bound to a member function. Removed automatically when Owner is destroyed. */
	template <typename TPayload, typename TOwner>
	FEFMessageListenerHandle RegisterListener(FGameplayTag Channel, TOwner* Owner, void (TOwner::*Function)(FGameplayTag, const TPayload&),
		EEFMessageMatch Match = EEFMessageMatch::ExactMatch)
	{
		TWeakObjectPtr<TOwner> WeakOwner(Owner);
		return RegisterListener<TPayload>(Channel, [WeakOwner, Function](FGameplayTag ActualChannel, const TPayload& Payload)
		{
			if (TOwner* StrongOwner = WeakOwner.Get())
			{
				(StrongOwner->*Function)(ActualChannel, Payload);
			}
		}, Match, Owner);
	}

	/** Removes the listener and clears Handle. */
	void UnregisterListener(FEFMessageListenerHandle& Handle);

	/** Listeners that would receive a message sent on Channel now. */
	int32 CountListeners(FGameplayTag Channel) const;

	/** The most recent messages, oldest first. */
	const TArray<FEFMessageRecord>& GetRecentMessages() const { return RecentMessages; }

	FOnAnyMessage OnAnyMessage;

private:
	struct FListenerEntry
	{
		int64 Id = 0;
		EEFMessageMatch Match = EEFMessageMatch::ExactMatch;
		TWeakObjectPtr<const UScriptStruct> PayloadType;
		TWeakObjectPtr<const UObject> Owner;
		bool bHasPayloadType = false;
		bool bHasOwner = false;
		FListener Callback;
	};

	/** Whether Entry, registered on a channel that is ExactLevel or a parent, hears a message with PayloadStruct. */
	static bool Hears(const FListenerEntry& Entry, bool bExactLevel, const UScriptStruct* PayloadStruct);

	void RemoveListener(FGameplayTag Channel, int64 Id);
	void PruneDeadListeners();
	void RecordMessage(FGameplayTag Channel, const UScriptStruct* PayloadStruct, int32 ListenerCount);

	TMap<FGameplayTag, TArray<FListenerEntry>> ListenersByChannel;
	TArray<FEFMessageRecord> RecentMessages;
	int64 LastListenerId = 0;
	int32 BroadcastDepth = 0;
};
