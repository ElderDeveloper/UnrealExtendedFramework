// Copyright Moon Punch Games. All Rights Reserved.

#include "EEUIScriptRunner.h"

#if WITH_EDITOR

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Framework/Application/SlateApplication.h"
#include "UILab/EEUILabUtils.h"
#include "UILab/Fixture/EFUIFixture.h"
#include "UILab/Host/SEEUILabHostPanel.h"
#include "UILab/Scripting/EFUIInteractionScript.h"

FEEUIScriptRunner::~FEEUIScriptRunner()
{
	Stop();
}

bool FEEUIScriptRunner::Start(UEFUIInteractionScript* InScript, const TSharedRef<SEEUILabHostPanel>& InPanel)
{
	return Start(InScript, InPanel, nullptr);
}

bool FEEUIScriptRunner::Start(UEFUIInteractionScript* InScript, const TSharedRef<SEEUILabHostPanel>& InPanel, UEFUIFixture* FixtureOverride)
{
	if (IsRunning() || !InScript)
	{
		return false;
	}

	Script = TStrongObjectPtr<UEFUIInteractionScript>(InScript);
	Panel = InPanel;
	Results.Reset();
	CurrentStep = 0;
	StepStartTime = FPlatformTime::Seconds();
	bStepActionDone = false;

	if (UEFUIFixture* Fixture = FixtureOverride ? FixtureOverride : InScript->Fixture.Get())
	{
		InPanel->ApplyFixture(Fixture);
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(this, &FEEUIScriptRunner::Tick));
	return true;
}

void FEEUIScriptRunner::Stop()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	Script.Reset();
}

bool FEEUIScriptRunner::DidAllPass() const
{
	for (const FEEUIScriptStepResult& Result : Results)
	{
		if (!Result.bPassed)
		{
			return false;
		}
	}
	return Results.Num() > 0;
}

void FEEUIScriptRunner::FinishStep(const bool bPassed, const FString& Message)
{
	FEEUIScriptStepResult Result;
	Result.StepIndex = CurrentStep;
	Result.bPassed = bPassed;
	Result.Message = Message;
	Results.Add(MoveTemp(Result));

	++CurrentStep;
	StepStartTime = FPlatformTime::Seconds();
	bStepActionDone = false;
}

void FEEUIScriptRunner::Finish()
{
	Stop();
	OnFinished.Broadcast();
}

bool FEEUIScriptRunner::Tick(float DeltaTime)
{
	UEFUIInteractionScript* ScriptPtr = Script.Get();
	const TSharedPtr<SEEUILabHostPanel> PanelPtr = Panel.Pin();

	if (!ScriptPtr || !PanelPtr.IsValid())
	{
		Finish();
		return false;
	}

	if (CurrentStep >= ScriptPtr->Steps.Num())
	{
		Finish();
		return false;
	}

	const FEFUIScriptStep& Step = ScriptPtr->Steps[CurrentStep];
	FString Message;
	const EStepStatus Status = ExecuteStep(Step, Message);

	switch (Status)
	{
	case EStepStatus::Passed:
		FinishStep(true, Message);
		break;
	case EStepStatus::Failed:
		FinishStep(false, Message);
		break;
	case EStepStatus::Pending:
		if (FPlatformTime::Seconds() - StepStartTime > FMath::Max(0.05f, Step.TimeoutSeconds))
		{
			FinishStep(false, FString::Printf(TEXT("Timed out: %s"), *Message));
		}
		break;
	}

	return true;
}

UWidget* FEEUIScriptRunner::ResolveTarget(const FName Target, FString& OutError) const
{
	const TSharedPtr<SEEUILabHostPanel> PanelPtr = Panel.Pin();
	UUserWidget* Hosted = PanelPtr.IsValid() ? PanelPtr->GetHostedWidget() : nullptr;
	if (!Hosted)
	{
		OutError = TEXT("No hosted widget.");
		return nullptr;
	}

	UWidget* Found = EEUILabUtils::FindWidgetByAutomationIdentity(*Hosted, Target);
	if (!Found)
	{
		OutError = FString::Printf(TEXT("No widget with automation identity '%s'."), *Target.ToString());
	}
	return Found;
}

FEEUIScriptRunner::EStepStatus FEEUIScriptRunner::ExecuteStep(const FEFUIScriptStep& Step, FString& OutMessage)
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	FString Error;

	auto InjectKey = [&SlateApp, &Step](const FKey& Key)
	{
		for (int32 Repeat = 0; Repeat < FMath::Max(1, Step.RepeatCount); ++Repeat)
		{
			const FKeyEvent DownEvent(Key, FModifierKeysState(), 0, false, 0, 0);
			SlateApp.ProcessKeyDownEvent(DownEvent);
			const FKeyEvent UpEvent(Key, FModifierKeysState(), 0, false, 0, 0);
			SlateApp.ProcessKeyUpEvent(UpEvent);
		}
	};

	switch (Step.Op)
	{
	case EEFUIScriptOp::SetFocus:
	{
		UWidget* Target = ResolveTarget(Step.Target, Error);
		if (!Target)
		{
			OutMessage = Error;
			return EStepStatus::Pending; // widget may appear after an animation; retry until timeout
		}
		const TSharedPtr<SWidget> SlateWidget = Target->GetCachedWidget();
		if (!SlateWidget.IsValid())
		{
			OutMessage = TEXT("Target has no live Slate widget.");
			return EStepStatus::Pending;
		}
		SlateApp.SetUserFocus(0, SlateWidget, EFocusCause::SetDirectly);
		OutMessage = FString::Printf(TEXT("Focus set to %s"), *Step.Target.ToString());
		return EStepStatus::Passed;
	}

	case EEFUIScriptOp::ExpectFocus:
	{
		UWidget* Target = ResolveTarget(Step.Target, Error);
		if (!Target)
		{
			OutMessage = Error;
			return EStepStatus::Pending;
		}
		const TSharedPtr<SWidget> Focused = SlateApp.GetUserFocusedWidget(0);
		const TSharedPtr<SWidget> TargetSlate = Target->GetCachedWidget();
		if (Focused.IsValid() && TargetSlate.IsValid())
		{
			for (TSharedPtr<SWidget> Walker = Focused; Walker.IsValid(); Walker = Walker->GetParentWidget())
			{
				if (Walker == TargetSlate)
				{
					OutMessage = FString::Printf(TEXT("Focus is on %s"), *Step.Target.ToString());
					return EStepStatus::Passed;
				}
			}
		}
		OutMessage = FString::Printf(TEXT("Focus is not on %s"), *Step.Target.ToString());
		return EStepStatus::Pending;
	}

	case EEFUIScriptOp::Navigate:
	{
		if (bStepActionDone)
		{
			return EStepStatus::Passed;
		}
		bStepActionDone = true;
		FKey Key = EKeys::Gamepad_DPad_Down;
		if (Step.Value == TEXT("Up")) { Key = EKeys::Gamepad_DPad_Up; }
		else if (Step.Value == TEXT("Left")) { Key = EKeys::Gamepad_DPad_Left; }
		else if (Step.Value == TEXT("Right")) { Key = EKeys::Gamepad_DPad_Right; }
		else if (Step.Value != TEXT("Down"))
		{
			OutMessage = FString::Printf(TEXT("Unknown direction '%s'."), *Step.Value);
			return EStepStatus::Failed;
		}
		InjectKey(Key);
		OutMessage = FString::Printf(TEXT("Navigated %s"), *Step.Value);
		return EStepStatus::Passed;
	}

	case EEFUIScriptOp::PressKey:
	{
		const FKey Key(*Step.Value);
		if (!Key.IsValid())
		{
			OutMessage = FString::Printf(TEXT("Invalid key '%s'."), *Step.Value);
			return EStepStatus::Failed;
		}
		InjectKey(Key);
		OutMessage = FString::Printf(TEXT("Pressed %s"), *Step.Value);
		return EStepStatus::Passed;
	}

	case EEFUIScriptOp::Click:
	{
		UWidget* Target = ResolveTarget(Step.Target, Error);
		if (!Target || !Target->GetCachedWidget().IsValid())
		{
			OutMessage = Error.IsEmpty() ? TEXT("Target has no live Slate widget.") : Error;
			return EStepStatus::Pending;
		}
		const FGeometry& Geometry = Target->GetCachedWidget()->GetCachedGeometry();
		const FVector2D Center = FVector2D(Geometry.GetAbsolutePosition()) + FVector2D(Geometry.GetAbsoluteSize()) * 0.5f;

		const FPointerEvent DownEvent(0, Center, Center, TSet<FKey>(), EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
		SlateApp.ProcessMouseButtonDownEvent(nullptr, DownEvent);
		const FPointerEvent UpEvent(0, Center, Center, TSet<FKey>{ EKeys::LeftMouseButton }, EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
		SlateApp.ProcessMouseButtonUpEvent(UpEvent);

		OutMessage = FString::Printf(TEXT("Clicked %s"), *Step.Target.ToString());
		return EStepStatus::Passed;
	}

	case EEFUIScriptOp::TypeText:
	{
		for (const TCHAR Char : Step.Value)
		{
			const FCharacterEvent CharEvent(Char, FModifierKeysState(), 0, false);
			SlateApp.ProcessKeyCharEvent(CharEvent);
		}
		OutMessage = FString::Printf(TEXT("Typed \"%s\""), *Step.Value);
		return EStepStatus::Passed;
	}

	case EEFUIScriptOp::Wait:
	{
		const float WaitSeconds = FCString::Atof(*Step.Value);
		if (FPlatformTime::Seconds() - StepStartTime >= WaitSeconds)
		{
			OutMessage = FString::Printf(TEXT("Waited %.2fs"), WaitSeconds);
			return EStepStatus::Passed;
		}
		OutMessage = TEXT("Waiting");
		return EStepStatus::Pending;
	}

	case EEFUIScriptOp::ExpectVisible:
	case EEFUIScriptOp::ExpectEnabled:
	{
		UWidget* Target = ResolveTarget(Step.Target, Error);
		if (!Target)
		{
			OutMessage = Error;
			return EStepStatus::Pending;
		}
		const bool bExpected = !Step.Value.Equals(TEXT("false"), ESearchCase::IgnoreCase);
		const bool bActual = Step.Op == EEFUIScriptOp::ExpectVisible ? Target->IsVisible() : Target->GetIsEnabled();
		if (bActual == bExpected)
		{
			OutMessage = FString::Printf(TEXT("%s is %s"), *Step.Target.ToString(), bActual ? TEXT("true") : TEXT("false"));
			return EStepStatus::Passed;
		}
		OutMessage = FString::Printf(TEXT("%s expected %s, got %s"), *Step.Target.ToString(),
			bExpected ? TEXT("true") : TEXT("false"), bActual ? TEXT("true") : TEXT("false"));
		return EStepStatus::Pending;
	}

	case EEFUIScriptOp::ExpectText:
	{
		UWidget* Target = ResolveTarget(Step.Target, Error);
		if (!Target)
		{
			OutMessage = Error;
			return EStepStatus::Pending;
		}
		FString ActualText;
		if (const UTextBlock* TextBlock = Cast<UTextBlock>(Target))
		{
			ActualText = TextBlock->GetText().ToString();
		}
		else if (const FTextProperty* TextProperty = CastField<FTextProperty>(Target->GetClass()->FindPropertyByName(TEXT("Text"))))
		{
			ActualText = TextProperty->GetPropertyValue_InContainer(Target).ToString();
		}
		else
		{
			OutMessage = FString::Printf(TEXT("%s has no readable Text."), *Step.Target.ToString());
			return EStepStatus::Failed;
		}
		if (ActualText == Step.Value)
		{
			OutMessage = FString::Printf(TEXT("%s text matches"), *Step.Target.ToString());
			return EStepStatus::Passed;
		}
		OutMessage = FString::Printf(TEXT("%s text is \"%s\", expected \"%s\""), *Step.Target.ToString(), *ActualText, *Step.Value);
		return EStepStatus::Pending;
	}

	case EEFUIScriptOp::ExpectValue:
	{
		UWidget* Target = ResolveTarget(Step.Target, Error);
		if (!Target)
		{
			OutMessage = Error;
			return EStepStatus::Pending;
		}
		FString PropertyName, Expected;
		if (!Step.Value.Split(TEXT("="), &PropertyName, &Expected))
		{
			OutMessage = TEXT("ExpectValue needs \"Property=Value\".");
			return EStepStatus::Failed;
		}
		const FProperty* Property = Target->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!Property)
		{
			OutMessage = FString::Printf(TEXT("Property '%s' not found on %s."), *PropertyName, *Step.Target.ToString());
			return EStepStatus::Failed;
		}
		FString Actual;
		Property->ExportTextItem_InContainer(Actual, Target, nullptr, nullptr, PPF_None);
		if (Actual == Expected)
		{
			OutMessage = FString::Printf(TEXT("%s.%s matches"), *Step.Target.ToString(), *PropertyName);
			return EStepStatus::Passed;
		}
		OutMessage = FString::Printf(TEXT("%s.%s is \"%s\", expected \"%s\""), *Step.Target.ToString(), *PropertyName, *Actual, *Expected);
		return EStepStatus::Pending;
	}

	case EEFUIScriptOp::Capture:
	{
		const TSharedPtr<SWidget> Focused = SlateApp.GetUserFocusedWidget(0);
		OutMessage = FString::Printf(TEXT("Capture: focus=%s"),
			Focused.IsValid() ? *Focused->GetTypeAsString() : TEXT("(none)"));
		return EStepStatus::Passed;
	}

	default:
		OutMessage = TEXT("Unknown operation.");
		return EStepStatus::Failed;
	}
}

#endif // WITH_EDITOR
