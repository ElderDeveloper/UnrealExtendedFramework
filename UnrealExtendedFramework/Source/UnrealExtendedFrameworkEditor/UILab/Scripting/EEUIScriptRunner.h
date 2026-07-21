// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Containers/Ticker.h"
#include "UILab/Scripting/EFUIInteractionScript.h"
#include "UObject/StrongObjectPtr.h"

class SEEUILabHostPanel;
class UWidget;

/** Result of one executed script step. */
struct FEEUIScriptStepResult
{
	int32 StepIndex = INDEX_NONE;
	bool bPassed = false;
	FString Message;
};

/**
 * UL-6 script runner: executes a UEFUIInteractionScript against a UI Lab host panel,
 * one step per tick, translating semantic operations into real Slate events. Expect
 * steps poll until their timeout so animations/deferred layout don't cause flakes.
 * Assertions produce plain results consumable interactively or by automation.
 */
class UNREALEXTENDEDFRAMEWORKEDITOR_API FEEUIScriptRunner : public TSharedFromThis<FEEUIScriptRunner>
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnFinished);

	~FEEUIScriptRunner();

	/** Applies the script's fixture to the panel and starts stepping. */
	bool Start(UEFUIInteractionScript* Script, const TSharedRef<SEEUILabHostPanel>& Panel);

	/** Variant-matrix run: same script, explicit fixture override (null = script's own fixture). */
	bool Start(UEFUIInteractionScript* Script, const TSharedRef<SEEUILabHostPanel>& Panel, UEFUIFixture* FixtureOverride);
	void Stop();

	bool IsRunning() const { return TickHandle.IsValid(); }
	bool DidAllPass() const;
	const TArray<FEEUIScriptStepResult>& GetResults() const { return Results; }
	int32 GetCurrentStepIndex() const { return CurrentStep; }

	FOnFinished OnFinished;

private:
	bool Tick(float DeltaTime);

	enum class EStepStatus : uint8 { Passed, Failed, Pending };
	EStepStatus ExecuteStep(const FEFUIScriptStep& Step, FString& OutMessage);
	UWidget* ResolveTarget(FName Target, FString& OutError) const;

	void FinishStep(bool bPassed, const FString& Message);
	void Finish();

	TStrongObjectPtr<UEFUIInteractionScript> Script;
	TWeakPtr<SEEUILabHostPanel> Panel;
	TArray<FEEUIScriptStepResult> Results;
	FTSTicker::FDelegateHandle TickHandle;
	int32 CurrentStep = 0;
	double StepStartTime = 0.0;
	bool bStepActionDone = false;
};

#endif // WITH_EDITOR
