// Copyright Moon Punch Games. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Messages/EFMessageSubsystem.h"
#include "NativeGameplayTags.h"
#include "Testing/EFTestHelpers.h"
#include "EFTestAsyncAction.h"
#include "EFTestMessageTypes.h"

namespace EFMessageTestTags
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Root, "Extended.Test.Messages");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Child, "Extended.Test.Messages.Child");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Other, "Extended.Test.Other");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFMessageRoutingTest, "ExtendedFoundation.Messages.Routing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFMessageRoutingTest::RunTest(const FString& Parameters)
{
	using namespace EFMessageTestTags;

	EFTest::FScopedGameInstance Game;
	UEFMessageSubsystem* Messages = Game.GetSubsystem<UEFMessageSubsystem>();
	if (!TestNotNull(TEXT("the test game instance has the message subsystem"), Messages))
	{
		return false;
	}

	int32 ExactHits = 0;
	int32 PartialHits = 0;
	int32 TypedHits = 0;
	int32 OtherTypeHits = 0;
	int32 LastValue = 0;
	FGameplayTag LastChannel;

	FEFMessageListenerHandle Exact = Messages->RegisterListener(Root.GetTag(), [&ExactHits](FGameplayTag, const FInstancedStruct&) { ++ExactHits; });
	FEFMessageListenerHandle Partial = Messages->RegisterListener(Root.GetTag(), [&PartialHits, &LastChannel](FGameplayTag Channel, const FInstancedStruct&)
	{
		++PartialHits;
		LastChannel = Channel;
	}, EEFMessageMatch::PartialMatch);
	FEFMessageListenerHandle Typed = Messages->RegisterListener<FEFTestMessage>(Child.GetTag(), [&TypedHits, &LastValue](FGameplayTag, const FEFTestMessage& Received)
	{
		++TypedHits;
		LastValue = Received.Value;
	});
	FEFMessageListenerHandle OtherTyped = Messages->RegisterListener<FEFTestOtherMessage>(Child.GetTag(), [&OtherTypeHits](FGameplayTag, const FEFTestOtherMessage&) { ++OtherTypeHits; });

	TestTrue(TEXT("handles are valid"), Exact.IsValid() && Partial.IsValid() && Typed.IsValid() && OtherTyped.IsValid());

	FEFTestMessage Message;
	Message.Value = 7;
	Messages->BroadcastMessage(Child.GetTag(), Message);
	TestEqual(TEXT("an exact listener on the parent does not hear the child channel"), ExactHits, 0);
	TestEqual(TEXT("a partial listener on the parent hears the child channel"), PartialHits, 1);
	TestTrue(TEXT("the partial listener is told the channel the message was sent on"), LastChannel == Child.GetTag());
	TestEqual(TEXT("the typed listener hears its payload type"), TypedHits, 1);
	TestEqual(TEXT("the typed listener reads the payload"), LastValue, 7);
	TestEqual(TEXT("a listener for another payload type is skipped"), OtherTypeHits, 0);

	FEFTestChildMessage ChildMessage;
	ChildMessage.Value = 9;
	Messages->BroadcastMessage(Child.GetTag(), ChildMessage);
	TestEqual(TEXT("a child struct reaches a listener for its parent struct"), TypedHits, 2);
	TestEqual(TEXT("and the parent's fields are read"), LastValue, 9);

	Messages->BroadcastMessage(Root.GetTag(), FInstancedStruct());
	TestEqual(TEXT("an exact listener hears its own channel"), ExactHits, 1);
	TestEqual(TEXT("a partial listener hears its own channel too"), PartialHits, 3);
	TestEqual(TEXT("a typed listener does not hear a message without payload"), TypedHits, 2);

	Messages->BroadcastMessage(Other.GetTag(), FInstancedStruct());
	TestEqual(TEXT("unrelated channels reach nobody"), ExactHits + PartialHits + TypedHits + OtherTypeHits, 6);

	TestEqual(TEXT("two listeners hear the child channel exactly, one through its parent"), Messages->CountListeners(Child.GetTag()), 3);

	Partial.Unregister();
	TestFalse(TEXT("an unregistered handle is cleared"), Partial.IsValid());
	Partial.Unregister();
	Messages->BroadcastMessage(Root.GetTag(), FInstancedStruct());
	TestEqual(TEXT("a removed listener is not called"), PartialHits, 3);
	TestEqual(TEXT("the others still are"), ExactHits, 2);

	const TArray<FEFMessageRecord>& Recent = Messages->GetRecentMessages();
	if (TestTrue(TEXT("sent messages are recorded"), Recent.Num() >= 5))
	{
		TestTrue(TEXT("the newest record is last"), Recent.Last().Channel == Root.GetTag());
		TestEqual(TEXT("the record counts the listeners reached"), Recent.Last().ListenerCount, 1);
	}

	Exact.Unregister();
	Typed.Unregister();
	OtherTyped.Unregister();
	TestEqual(TEXT("no listeners remain"), Messages->CountListeners(Child.GetTag()), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFMessageReentrancyTest, "ExtendedFoundation.Messages.Reentrancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFMessageReentrancyTest::RunTest(const FString& Parameters)
{
	using namespace EFMessageTestTags;

	EFTest::FScopedGameInstance Game;
	UEFMessageSubsystem* Messages = Game.GetSubsystem<UEFMessageSubsystem>();
	if (!TestNotNull(TEXT("the test game instance has the message subsystem"), Messages))
	{
		return false;
	}

	// A listener that removes itself and a later listener while the message is being delivered.
	FEFMessageListenerHandle First;
	FEFMessageListenerHandle Second;
	int32 FirstHits = 0;
	int32 SecondHits = 0;
	First = Messages->RegisterListener(Root.GetTag(), [&FirstHits, &First, &Second](FGameplayTag, const FInstancedStruct&)
	{
		++FirstHits;
		First.Unregister();
		Second.Unregister();
	});
	Second = Messages->RegisterListener(Root.GetTag(), [&SecondHits](FGameplayTag, const FInstancedStruct&) { ++SecondHits; });

	Messages->BroadcastMessage(Root.GetTag(), FInstancedStruct());
	TestEqual(TEXT("the first listener ran once"), FirstHits, 1);
	TestEqual(TEXT("a listener removed earlier in the same broadcast is not called"), SecondHits, 0);
	Messages->BroadcastMessage(Root.GetTag(), FInstancedStruct());
	TestEqual(TEXT("a listener that removed itself is gone"), FirstHits, 1);

	// A listener that sends the message it listens to stops at the nesting limit.
	int32 Depth = 0;
	FEFMessageListenerHandle Loop = Messages->RegisterListener(Other.GetTag(), [&Depth, Messages](FGameplayTag, const FInstancedStruct&)
	{
		++Depth;
		Messages->BroadcastMessage(Other.GetTag(), FInstancedStruct());
	});
	AddExpectedMessage(TEXT("dropped to stop a loop"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1, false);
	Messages->BroadcastMessage(Other.GetTag(), FInstancedStruct());
	TestEqual(TEXT("nested delivery stops at 16 levels"), Depth, 16);
	Loop.Unregister();

	// A listener bound to an owner disappears with it.
	UEFTestAsyncAction* Owner = NewObject<UEFTestAsyncAction>();
	int32 OwnedHits = 0;
	Messages->RegisterListener(Root.GetTag(), [&OwnedHits](FGameplayTag, const FInstancedStruct&) { ++OwnedHits; }, EEFMessageMatch::ExactMatch, nullptr, Owner);
	Messages->BroadcastMessage(Root.GetTag(), FInstancedStruct());
	TestEqual(TEXT("the owned listener runs while its owner lives"), OwnedHits, 1);
	Owner->MarkAsGarbage();
	Messages->BroadcastMessage(Root.GetTag(), FInstancedStruct());
	TestEqual(TEXT("the owned listener stops when its owner is destroyed"), OwnedHits, 1);
	TestEqual(TEXT("and is no longer counted"), Messages->CountListeners(Root.GetTag()), 0);

	// Invalid input is refused without side effects.
	AddExpectedMessage(TEXT("the channel is empty"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2, false);
	TestFalse(TEXT("a listener without a channel is refused"), Messages->RegisterListener(FGameplayTag(), [](FGameplayTag, const FInstancedStruct&) {}).IsValid());
	Messages->BroadcastMessage(FGameplayTag(), FInstancedStruct());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
