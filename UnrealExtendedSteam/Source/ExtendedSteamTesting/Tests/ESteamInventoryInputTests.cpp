// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ExtendedSteamSharedModule.h"
#include "Inventory/ESteamInventorySubsystem.h"
#include "Input/ESteamInputSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// Steam may genuinely be initialized when tests run interactively (auto-init in the editor),
// so each test branches: strict offline defaults when Steam is down, Steam-independent
// invariants when up. Subsystems are constructed directly without Initialize — every method
// is guarded by IsSteamAvailable(), which only consults the shared module.
namespace ESteamInventoryInputTestHelpers
{
	bool IsSteamClientUp()
	{
		return FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamInventoryOfflineStateTest,
	"UnrealExtendedSteam.Inventory.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamInventoryOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamInventorySubsystem* Inventory = NewObject<UESteamInventorySubsystem>(GameInstance);
	TestNotNull(TEXT("Inventory subsystem constructed"), Inventory);

	const bool bSteamUp = ESteamInventoryInputTestHelpers::IsSteamClientUp();

	if (bSteamUp)
	{
		// Input-validation guards reject bad arguments before any Steam call.
		AddExpectedMessage(TEXT("GetItemsById"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);
		AddExpectedMessage(TEXT("StartPurchase"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

		TestFalse(TEXT("GetItemsById rejects an empty id array"), Inventory->GetItemsById(TArray<int64>()));
		TestFalse(TEXT("StartPurchase rejects mismatched array lengths"),
			Inventory->StartPurchase({ 1, 2 }, { 1 }));
		return true;
	}

	// The unavailable guards log a standard warning; that is the expected behavior under test.
	AddExpectedMessage(TEXT("Steam is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	TestFalse(TEXT("GetAllItems fails offline"), Inventory->GetAllItems());
	TestFalse(TEXT("TriggerItemDrop fails offline"), Inventory->TriggerItemDrop(1));

	int64 CurrentPrice = 0;
	int64 BasePrice = 0;
	TestFalse(TEXT("GetItemPrice fails offline"), Inventory->GetItemPrice(1, CurrentPrice, BasePrice));
	TestEqual(TEXT("Current price is 0 offline"), CurrentPrice, static_cast<int64>(0));
	TestEqual(TEXT("Base price is 0 offline"), BasePrice, static_cast<int64>(0));

	TestTrue(TEXT("GetItemDefinitionProperty is empty offline"),
		Inventory->GetItemDefinitionProperty(1, TEXT("name")).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamInputOfflineStateTest,
	"UnrealExtendedSteam.Input.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamInputOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamInputSubsystem* Input = NewObject<UESteamInputSubsystem>(GameInstance);
	TestNotNull(TEXT("Input subsystem constructed"), Input);

	const bool bSteamUp = ESteamInventoryInputTestHelpers::IsSteamClientUp();

	// Steam Input requires an explicit InitSteamInput opt-in, so these defaults hold whether
	// or not the Steam client is up — this subsystem instance was never initialized.
	TestFalse(TEXT("Steam Input starts uninitialized"), Input->IsSteamInputInitialized());

	TArray<int64> Controllers;
	TestEqual(TEXT("GetConnectedControllers returns 0 uninitialized"), Input->GetConnectedControllers(Controllers), 0);
	TestEqual(TEXT("GetConnectedControllers outputs no handles uninitialized"), Controllers.Num(), 0);

	TestEqual(TEXT("GetActionSetHandle is 0 uninitialized"), Input->GetActionSetHandle(TEXT("MenuControls")), static_cast<int64>(0));

	const FESteamDigitalActionData DigitalData = Input->GetDigitalActionData(0, 0);
	TestFalse(TEXT("Digital action state is off uninitialized"), DigitalData.bState);
	TestFalse(TEXT("Digital action is inactive uninitialized"), DigitalData.bActive);

	TestEqual(TEXT("Input type is Unknown uninitialized"), Input->GetInputTypeForHandle(0), EESteamInputType::Unknown);

	const FESteamAnalogActionData AnalogData = Input->GetAnalogActionData(0, 0);
	TestEqual(TEXT("Analog action source mode is None uninitialized"), AnalogData.Mode, EESteamInputSourceMode::None);

	// Action-set layers, origins and glyphs all short-circuit while Steam Input is uninitialized.
	TArray<int64> Layers;
	TestEqual(TEXT("GetActiveActionSetLayers returns 0 uninitialized"), Input->GetActiveActionSetLayers(0, Layers), 0);

	TArray<int32> Origins;
	TestEqual(TEXT("GetDigitalActionOrigins returns 0 uninitialized"), Input->GetDigitalActionOrigins(0, 0, 0, Origins), 0);
	TestEqual(TEXT("GetAnalogActionOrigins returns 0 uninitialized"), Input->GetAnalogActionOrigins(0, 0, 0, Origins), 0);

	TestTrue(TEXT("Glyph PNG path is empty uninitialized"),
		Input->GetGlyphPNGForActionOrigin(0, EESteamInputGlyphSize::Small, 0).IsEmpty());
	TestTrue(TEXT("Action origin string is empty uninitialized"), Input->GetStringForActionOrigin(0).IsEmpty());

	TestEqual(TEXT("GetGamepadIndexForController is -1 uninitialized"), Input->GetGamepadIndexForController(0), -1);
	TestEqual(TEXT("GetControllerForGamepadIndex is 0 uninitialized"), Input->GetControllerForGamepadIndex(0), static_cast<int64>(0));
	TestEqual(TEXT("GetRemotePlaySessionID is 0 uninitialized"), Input->GetRemotePlaySessionID(0), 0);

	const FESteamInputMotionData MotionData = Input->GetMotionData(0);
	TestTrue(TEXT("Motion acceleration is zero uninitialized"), MotionData.Accel.IsZero());
	TestTrue(TEXT("Motion angular velocity is zero uninitialized"), MotionData.RotVel.IsZero());

	if (!bSteamUp)
	{
		// The unavailable guard logs a standard warning; that is the expected behavior under test.
		AddExpectedMessage(TEXT("Steam is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);
		TestFalse(TEXT("InitSteamInput fails offline"), Input->InitSteamInput());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
