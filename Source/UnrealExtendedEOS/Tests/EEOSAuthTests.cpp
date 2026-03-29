// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Auth/EEOSAuthSubsystem.h"
#include "Engine/GameInstance.h"


// ═══════════════════════════════════════════════════════════════════════════
// Initial State (without live EOS backend)
// ═══════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSAuthInitialLoginStatusTest,
	"UnrealExtendedEOS.Auth.InitialState.LoginStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSAuthInitialLoginStatusTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSAuthSubsystem* AuthSub = NewObject<UEEOSAuthSubsystem>(GI);
	TestNotNull(TEXT("Should be able to create AuthSubsystem"), AuthSub);

	TestEqual(TEXT("Initial login status should be NotLoggedIn"),
		AuthSub->GetLoginStatus(), EEOSLoginStatus::NotLoggedIn);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSAuthInitialIsLoggedInTest,
	"UnrealExtendedEOS.Auth.InitialState.IsLoggedIn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSAuthInitialIsLoggedInTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSAuthSubsystem* AuthSub = NewObject<UEEOSAuthSubsystem>(GI);

	TestFalse(TEXT("Should not be logged in initially"), AuthSub->IsLoggedIn());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSAuthInitialConnectStateTest,
	"UnrealExtendedEOS.Auth.InitialState.ConnectState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSAuthInitialConnectStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSAuthSubsystem* AuthSub = NewObject<UEEOSAuthSubsystem>(GI);

	TestFalse(TEXT("Should not be connected to game services initially"),
		AuthSub->IsConnectedToGameServices());
	TestTrue(TEXT("ProductUserId should be empty initially"),
		AuthSub->GetProductUserId().IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSAuthInitialLoginTypeTest,
	"UnrealExtendedEOS.Auth.InitialState.LoginType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSAuthInitialLoginTypeTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSAuthSubsystem* AuthSub = NewObject<UEEOSAuthSubsystem>(GI);

	TestEqual(TEXT("Default login type should be AccountPortal"),
		AuthSub->GetCurrentLoginType(), EEOSLoginType::AccountPortal);

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════
// SetConnectLoginResult State Transitions
// ═══════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSAuthSetConnectLoginSuccessTest,
	"UnrealExtendedEOS.Auth.SetConnectLoginResult.Success",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSAuthSetConnectLoginSuccessTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSAuthSubsystem* AuthSub = NewObject<UEEOSAuthSubsystem>(GI);

	// Simulate a successful Connect login
	AuthSub->SetConnectLoginResult(true, TEXT("abc123def456"), TEXT(""));

	TestTrue(TEXT("Should be connected after success"), AuthSub->IsConnectedToGameServices());
	TestEqual(TEXT("ProductUserId should be set"), AuthSub->GetProductUserId(), FString(TEXT("abc123def456")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSAuthSetConnectLoginFailureTest,
	"UnrealExtendedEOS.Auth.SetConnectLoginResult.Failure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSAuthSetConnectLoginFailureTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSAuthSubsystem* AuthSub = NewObject<UEEOSAuthSubsystem>(GI);

	// Simulate a failed Connect login
	AuthSub->SetConnectLoginResult(false, TEXT(""), TEXT("Connection refused"));

	TestFalse(TEXT("Should not be connected after failure"), AuthSub->IsConnectedToGameServices());
	TestTrue(TEXT("ProductUserId should remain empty after failure"), AuthSub->GetProductUserId().IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSAuthSetConnectLoginOverwriteTest,
	"UnrealExtendedEOS.Auth.SetConnectLoginResult.Overwrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSAuthSetConnectLoginOverwriteTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UEEOSAuthSubsystem* AuthSub = NewObject<UEEOSAuthSubsystem>(GI);

	// First: successful login
	AuthSub->SetConnectLoginResult(true, TEXT("user_001"), TEXT(""));
	TestTrue(TEXT("Should be connected"), AuthSub->IsConnectedToGameServices());
	TestEqual(TEXT("First PUID"), AuthSub->GetProductUserId(), FString(TEXT("user_001")));

	// Second: another successful login (overwrites)
	AuthSub->SetConnectLoginResult(true, TEXT("user_002"), TEXT(""));
	TestTrue(TEXT("Should still be connected"), AuthSub->IsConnectedToGameServices());
	TestEqual(TEXT("PUID should be overwritten"), AuthSub->GetProductUserId(), FString(TEXT("user_002")));

	return true;
}
