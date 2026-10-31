// Copyright Moon Punch Games. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Async/EFPendingCalls.h"
#include "Http/EFHttpTypes.h"
#include "Result/EFResult.h"
#include "EFTestAsyncAction.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFResultTest, "ExtendedFoundation.Result",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFResultTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("plain success"), FEFResult::Success().ToString(), FString(TEXT("Success")));
	TestEqual(TEXT("success with a source"), FEFResult::Success(TEXT("PlayFab")).ToString(), FString(TEXT("PlayFab: Success")));

	const FEFResult Failure = FEFResult::Failure(TEXT("PlayFab"), TEXT("InvalidParams"), TEXT("Name too long"), false, 400);
	TestEqual(TEXT("full failure line"), Failure.ToString(), FString(TEXT("PlayFab InvalidParams (HTTP 400): Name too long")));
	TestTrue(TEXT("codes compare without case"), Failure.HasCode(TEXT("invalidparams")));
	TestFalse(TEXT("a failure is not a success"), Failure.IsSuccess());

	TestEqual(TEXT("an empty code becomes Failed"), FEFResult::Failure(TEXT("X"), TEXT(""), TEXT("")).Code, FString(EFResultCodes::Failed));
	TestTrue(TEXT("Cancelled helper"), FEFResult::Cancelled(TEXT("X")).IsCancelled());
	TestTrue(TEXT("TimedOut helper is retryable"), FEFResult::TimedOut(TEXT("X"), 5.f).bRetryable);
	TestTrue(TEXT("TimedOut helper"), FEFResult::TimedOut(TEXT("X"), 5.f).IsTimedOut());

	FEFHttpResponse Http;
	Http.Result = EEFHttpResult::Success;
	Http.StatusCode = 204;
	TestTrue(TEXT("HTTP 2xx maps to success"), FEFResult::FromHttp(Http).IsSuccess());

	Http.Result = EEFHttpResult::HttpError;
	Http.StatusCode = 503;
	const FEFResult ServerError = FEFResult::FromHttp(Http, TEXT("SteamWeb"));
	TestFalse(TEXT("HTTP 503 fails"), ServerError.IsSuccess());
	TestTrue(TEXT("HTTP 503 is retryable"), ServerError.bRetryable);
	TestEqual(TEXT("HTTP status is kept"), ServerError.HttpStatus, 503);
	TestEqual(TEXT("source is kept"), ServerError.Source, FName(TEXT("SteamWeb")));

	Http.StatusCode = 404;
	TestFalse(TEXT("HTTP 404 is not retryable"), FEFResult::FromHttp(Http).bRetryable);

	Http.Result = EEFHttpResult::Cancelled;
	TestTrue(TEXT("HTTP cancellation maps to Cancelled"), FEFResult::FromHttp(Http).IsCancelled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFPendingCallsTest, "ExtendedFoundation.Async.PendingCalls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFPendingCallsTest::RunTest(const FString& Parameters)
{
	TEFPendingCalls<int32> Calls;
	int32 First = 0;
	int32 Second = 0;

	const int64 FirstId = Calls.Add([&First](int32 Value) { First = Value; });
	const int64 SecondId = Calls.Add([&Second](int32 Value) { Second = Value; });
	TestNotEqual(TEXT("ids are unique"), FirstId, SecondId);
	TestEqual(TEXT("two calls pending"), Calls.Num(), 2);

	// Results arriving out of order still reach the right caller.
	TestTrue(TEXT("second completes"), Calls.Complete(SecondId, 2));
	TestTrue(TEXT("first completes"), Calls.Complete(FirstId, 1));
	TestEqual(TEXT("first got its own result"), First, 1);
	TestEqual(TEXT("second got its own result"), Second, 2);
	TestFalse(TEXT("a completed call cannot complete twice"), Calls.Complete(FirstId, 99));
	TestEqual(TEXT("nothing pending"), Calls.Num(), 0);

	int32 Cancelled = 0;
	Calls.Add([&Cancelled](int32 Value) { Cancelled += Value; });
	Calls.Add([&Cancelled](int32 Value) { Cancelled += Value; });
	Calls.CompleteAll(-1);
	TestEqual(TEXT("CompleteAll runs every callback"), Cancelled, -2);
	TestEqual(TEXT("CompleteAll clears"), Calls.Num(), 0);

	TestFalse(TEXT("an empty callback is not stored"), Calls.Contains(Calls.Add(nullptr)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFAsyncActionBaseTest, "ExtendedFoundation.Async.ActionBase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFAsyncActionBaseTest::RunTest(const FString& Parameters)
{
	// Normal run: executes once, finishes once, callbacks stop after finishing.
	{
		UEFTestAsyncAction* Action = NewObject<UEFTestAsyncAction>();
		Action->Init();
		const TFunction<void()> Callback = Action->MakeTestCallback();

		Action->Activate();
		Action->Activate();
		TestEqual(TEXT("executes once"), Action->ExecuteCount, 1);
		TestTrue(TEXT("active while running"), Action->IsActive());

		Callback();
		TestEqual(TEXT("a callback runs while active"), Action->CallbackRuns, 1);

		TestTrue(TEXT("first finish succeeds"), Action->Finish());
		TestFalse(TEXT("second finish is refused"), Action->Finish());
		TestFalse(TEXT("inactive after finishing"), Action->IsActive());

		Callback();
		TestEqual(TEXT("a callback does not run after finishing"), Action->CallbackRuns, 1);

		Action->Cancel();
		TestEqual(TEXT("cancel after finishing does nothing"), Action->FailureCount, 0);
	}

	// Cancellation: stops the work and reports Cancelled once.
	{
		UEFTestAsyncAction* Action = NewObject<UEFTestAsyncAction>();
		Action->Init();
		Action->Activate();
		Action->Cancel();
		Action->Cancel();
		TestEqual(TEXT("cancel stops the work once"), Action->CancelWorkCount, 1);
		TestEqual(TEXT("cancel reports once"), Action->FailureCount, 1);
		TestTrue(TEXT("cancel reports Cancelled"), Action->LastFailure.IsCancelled());
		TestEqual(TEXT("the node's source is used"), Action->LastFailure.Source, FName(TEXT("Test")));
	}

	// A failure found in the factory waits for Activate, then skips the work.
	{
		UEFTestAsyncAction* Action = NewObject<UEFTestAsyncAction>();
		Action->Init();
		Action->FailEarly(TEXT("GroupId is empty"));
		TestEqual(TEXT("no broadcast before activation"), Action->FailureCount, 0);

		Action->Activate();
		TestEqual(TEXT("the held failure is broadcast on activation"), Action->FailureCount, 1);
		TestTrue(TEXT("it is an InvalidArgument result"), Action->LastFailure.HasCode(EFResultCodes::InvalidArgument));
		TestEqual(TEXT("the work never starts"), Action->ExecuteCount, 0);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
