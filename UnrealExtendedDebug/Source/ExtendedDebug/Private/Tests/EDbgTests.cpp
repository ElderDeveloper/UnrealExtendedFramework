// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && EXTENDED_DEBUG_ENABLED

#include "EDbgSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Testing/EFTestHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEDbgCheatTest, "UnrealExtendedDebug.Cheats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEDbgCheatTest::RunTest(const FString& Parameters)
{
	EFTest::FScopedGameInstance Game;
	UEDbgSubsystem* Debug = Game.GetSubsystem<UEDbgSubsystem>();
	if (!Debug)
	{
		AddInfo(TEXT("Extended Debug is turned off in the project settings; nothing to test."));
		return true;
	}

	TestTrue(TEXT("the built-in cheats are there"), Debug->HasCheat(TEXT("TimeDilation")) && Debug->HasCheat(TEXT("ShowStatus")));

	// A game-instance cheat runs here with its arguments.
	int32 Runs = 0;
	TArray<FString> Received;
	FEDbgCheatInfo Info;
	Info.Name = TEXT("ExtendedTestCheat");
	Info.Category = TEXT("Test");
	Info.ArgumentHint = TEXT("<amount>");
	Debug->RegisterCheat(Info, [&Runs, &Received](const FEDbgCheatContext& Context)
	{
		++Runs;
		Received = Context.Arguments;
	});
	TestTrue(TEXT("a registered cheat is listed"), Debug->GetCheats().ContainsByPredicate([](const FEDbgCheatInfo& Candidate)
	{
		return Candidate.Name == TEXT("ExtendedTestCheat") && Candidate.Category == TEXT("Test");
	}));
	TestTrue(TEXT("it runs"), Debug->RunCheat(nullptr, TEXT("ExtendedTestCheat"), { TEXT("500") }));
	TestEqual(TEXT("once"), Runs, 1);
	TestTrue(TEXT("with its arguments"), Received.Num() == 1 && Received[0] == TEXT("500"));

	Debug->UnregisterCheat(TEXT("ExtendedTestCheat"));
	TestFalse(TEXT("an unregistered cheat is gone"), Debug->RunCheat(nullptr, TEXT("ExtendedTestCheat"), {}));

	// A cheat owned by an object disappears with it.
	AActor* Owner = Game.GetWorld()->SpawnActor<AActor>();
	FEDbgCheatInfo OwnedInfo;
	OwnedInfo.Name = TEXT("ExtendedOwnedCheat");
	Debug->RegisterCheat(OwnedInfo, [](const FEDbgCheatContext&) {}, Owner);
	TestTrue(TEXT("an owned cheat exists while its owner does"), Debug->HasCheat(TEXT("ExtendedOwnedCheat")));
	Owner->Destroy();
	Owner->MarkAsGarbage();
	TestFalse(TEXT("and goes with its owner"), Debug->HasCheat(TEXT("ExtendedOwnedCheat")));

	// Global cheats reach every game instance.
	FEDbgCheatInfo GlobalInfo;
	GlobalInfo.Name = TEXT("ExtendedGlobalTestCheat");
	int32 GlobalRuns = 0;
	UEDbgSubsystem::RegisterGlobalCheat(GlobalInfo, [&GlobalRuns](const FEDbgCheatContext&) { ++GlobalRuns; });
	TestTrue(TEXT("a global cheat runs"), Debug->RunCheat(nullptr, TEXT("ExtendedGlobalTestCheat"), {}) && GlobalRuns == 1);
	UEDbgSubsystem::UnregisterGlobalCheat(TEXT("ExtendedGlobalTestCheat"));
	TestFalse(TEXT("and is removed everywhere"), Debug->HasCheat(TEXT("ExtendedGlobalTestCheat")));

	// Arguments are read leniently.
	FEDbgCheatContext Context;
	Context.Arguments = { TEXT("2.5"), TEXT("7") };
	TestEqual(TEXT("float argument"), Context.GetFloat(0), 2.5f);
	TestEqual(TEXT("int argument"), Context.GetInt(1), 7);
	TestEqual(TEXT("missing argument uses the default"), Context.GetInt(5, 42), 42);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEDbgWatchTest, "UnrealExtendedDebug.Watches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEDbgWatchTest::RunTest(const FString& Parameters)
{
	EFTest::FScopedGameInstance Game;
	UEDbgSubsystem* Debug = Game.GetSubsystem<UEDbgSubsystem>();
	if (!Debug)
	{
		AddInfo(TEXT("Extended Debug is turned off in the project settings; nothing to test."));
		return true;
	}

	EDBG_WATCH(Game.Get(), TEXT("Speed"), TEXT("%d cm/s"), 350);
	const FEDbgWatch* Watch = Debug->GetWatches().Find(TEXT("Speed"));
	TestTrue(TEXT("EDBG_WATCH sets a watch"), Watch && Watch->Value == TEXT("350 cm/s"));
	Debug->SetWatch(TEXT("Speed"), TEXT("400 cm/s"));
	TestEqual(TEXT("setting it again replaces the value"), Debug->GetWatches().FindChecked(TEXT("Speed")).Value, FString(TEXT("400 cm/s")));
	Debug->ClearWatch(TEXT("Speed"));
	TestEqual(TEXT("a cleared watch is gone"), Debug->GetWatches().Num(), 0);
	TestFalse(TEXT("the menu is closed without a viewport"), Debug->IsMenuOpen());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && EXTENDED_DEBUG_ENABLED
