// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Shared/EPFSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Containers/Ticker.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Inventory/EPFInventorySubsystem.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	class FInventoryResponse final : public IHttpResponse
	{
	public:
		virtual const FString& GetURL() const override { return URL; }
		virtual const FString& GetEffectiveURL() const override { return URL; }
		virtual EHttpRequestStatus::Type GetStatus() const override { return EHttpRequestStatus::Succeeded; }
		virtual EHttpFailureReason GetFailureReason() const override { return EHttpFailureReason::None; }
		virtual FString GetURLParameter(const FString&) const override { return {}; }
		virtual FString GetHeader(const FString&) const override { return {}; }
		virtual TArray<FString> GetAllHeaders() const override { return {}; }
		virtual FString GetContentType() const override { return TEXT("application/json"); }
		virtual uint64 GetContentLength() const override { return 0; }
		virtual const TArray<uint8>& GetContent() const override { return Content; }
		virtual TArray<uint8> TakeContent() override { return {}; }
		virtual int32 GetResponseCode() const override { return 200; }
		virtual FString GetContentAsString() const override
		{
			return TEXT("{\"code\":200,\"data\":{\"Items\":[{\"Id\":\"Outfit.Test\"}]}}");
		}
		virtual FUtf8StringView GetContentAsUtf8StringView() const override { return {}; }
	private:
		FString URL = TEXT("https://example.invalid/Inventory/GetInventoryItems");
		TArray<uint8> Content;
	};

	// Reproduce the game-thread-as-loading-thread branch of CoreGlobals without loading a world
	// or modifying either PIE ID. No engine ticks or UObject operations may run inside this scope.
	struct FSimulateLoadingThread
	{
		bool (*Previous)() = IsInAsyncLoadingThread;
		FSimulateLoadingThread() { IsInAsyncLoadingThread = [] { return true; }; }
		~FSimulateLoadingThread() { IsInAsyncLoadingThread = Previous; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEPFHttpLoadingResponseTest,
	"UnrealExtendedPlayFab.Transport.ResponseDuringPackageLoading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEPFHttpLoadingResponseTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
	TStrongObjectPtr<UEPFInventorySubsystem> Subsystem(NewObject<UEPFInventorySubsystem>(Instance.Get()));
	TStrongObjectPtr<UDataTable> Table(NewObject<UDataTable>());
	const TSoftObjectPtr<UDataTable> SoftTable(Table.Get());
	const FHttpResponsePtr Response = MakeShared<FInventoryResponse, ESPMode::ThreadSafe>();
	int32 CompletionCount = 0;
	const UEPFSubsystem::FOnPlayFabResponseDetailed OnComplete =
		UEPFSubsystem::FOnPlayFabResponseDetailed::CreateLambda(
			[&](const FEPFResult& Result, TSharedPtr<FJsonObject> Data)
			{
				++CompletionCount;
				TestTrue(TEXT("Successful response is preserved"), Result.bSuccess);
				TestFalse(TEXT("Completion runs outside package loading"), IsInAsyncLoadingThread());
				TestEqual(TEXT("Soft catalog reference resolves without the PIE assertion"), SoftTable.Get(), Table.Get());
				TestTrue(TEXT("Response data survives deferral"), Data.IsValid() && Data->HasTypedField<EJson::Array>(TEXT("Items")));
			});

	{
		FSimulateLoadingThread LoadingScope;
		Subsystem->HandleHttpResponse(nullptr, Response, true, TEXT("/Inventory/GetInventoryItems"),
			TEXT("{}"), EEPFAuthMode::None, OnComplete, 0);
	}
	TestEqual(TEXT("Loading callback does not enter game/UI code"), CompletionCount, 0);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Response is delivered once after loading unwinds"), CompletionCount, 1);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Deferred response is not delivered twice"), CompletionCount, 1);

	// The subsystem can remain a valid UObject after PIE/GameInstance shutdown.
	{
		FSimulateLoadingThread LoadingScope;
		Subsystem->HandleHttpResponse(nullptr, Response, true, TEXT("/Inventory/GetInventoryItems"),
			TEXT("{}"), EEPFAuthMode::None, OnComplete, 0);
	}
	Subsystem->Deinitialize();
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Shutdown cancels queued completion even before GC"), CompletionCount, 1);
	Subsystem->HandleHttpResponse(nullptr, Response, true, TEXT("/Inventory/GetInventoryItems"),
		TEXT("{}"), EEPFAuthMode::None, OnComplete, 0);
	TestEqual(TEXT("Late HTTP completion after shutdown is ignored"), CompletionCount, 1);
	return true;
}
#endif
