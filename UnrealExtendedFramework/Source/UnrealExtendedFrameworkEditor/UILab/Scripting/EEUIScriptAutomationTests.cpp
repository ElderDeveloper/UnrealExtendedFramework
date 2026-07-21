// Copyright Moon Punch Games. All Rights Reserved.

#if WITH_EDITOR && WITH_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "UILab/Fixture/EFUIFixture.h"
#include "UILab/Host/SEEUILabHostPanel.h"
#include "UILab/Scripting/EEUIScriptRunner.h"
#include "UILab/Scripting/EFUIInteractionScript.h"
#include "Widgets/SWindow.h"

/**
 * UL-6 automation integration: every UEFUIInteractionScript asset in the project appears as an
 * automation test under ExtendedFramework.UILab. The runner drives a real host panel in a
 * transient window; step failures surface as ordinary automation errors. No generated C++ per
 * recorded script.
 */
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FEEUILabScriptAutomationTest, "ExtendedFramework.UILab.Scripts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace
{
	struct FEEUIScriptTestSession
	{
		TSharedPtr<SWindow> Window;
		TSharedPtr<SEEUILabHostPanel> Panel;
		TSharedPtr<FEEUIScriptRunner> Runner;

		void Close()
		{
			if (Runner.IsValid())
			{
				Runner->Stop();
			}
			if (Window.IsValid() && FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
			}
			Window.Reset();
			Panel.Reset();
			Runner.Reset();
		}
	};
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FEEUIWaitForScriptCommand,
	TSharedPtr<FEEUIScriptTestSession>, Session,
	FAutomationTestBase*, Test);

bool FEEUIWaitForScriptCommand::Update()
{
	if (!Session.IsValid() || !Session->Runner.IsValid())
	{
		return true;
	}

	if (Session->Runner->IsRunning())
	{
		return false;
	}

	for (const FEEUIScriptStepResult& Result : Session->Runner->GetResults())
	{
		if (!Result.bPassed)
		{
			Test->AddError(FString::Printf(TEXT("Step %d failed: %s"), Result.StepIndex + 1, *Result.Message));
		}
	}
	if (Session->Runner->GetResults().Num() == 0)
	{
		Test->AddError(TEXT("Script produced no results."));
	}

	Session->Close();
	return true;
}

void FEEUILabScriptAutomationTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	const FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter Filter;
	Filter.ClassPaths.Add(UEFUIInteractionScript::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	AssetRegistry.Get().GetAssets(Filter, Assets);

	for (const FAssetData& Asset : Assets)
	{
		OutBeautifiedNames.Add(Asset.AssetName.ToString());
		OutTestCommands.Add(Asset.GetObjectPathString());

		// UL-7 variant matrix: one additional test per matrix fixture.
		if (const UEFUIInteractionScript* Script = Cast<UEFUIInteractionScript>(Asset.GetAsset()))
		{
			for (const TObjectPtr<UEFUIFixture>& Variant : Script->MatrixFixtures)
			{
				if (Variant)
				{
					OutBeautifiedNames.Add(FString::Printf(TEXT("%s (%s)"), *Asset.AssetName.ToString(), *Variant->GetName()));
					OutTestCommands.Add(FString::Printf(TEXT("%s|%s"), *Asset.GetObjectPathString(), *Variant->GetPathName()));
				}
			}
		}
	}
}

bool FEEUILabScriptAutomationTest::RunTest(const FString& Parameters)
{
	FString ScriptPath = Parameters;
	FString VariantPath;
	Parameters.Split(TEXT("|"), &ScriptPath, &VariantPath);

	UEFUIInteractionScript* Script = LoadObject<UEFUIInteractionScript>(nullptr, *ScriptPath);
	if (!Script)
	{
		AddError(FString::Printf(TEXT("Failed to load script '%s'."), *ScriptPath));
		return false;
	}

	UEFUIFixture* FixtureOverride = nullptr;
	if (!VariantPath.IsEmpty())
	{
		FixtureOverride = LoadObject<UEFUIFixture>(nullptr, *VariantPath);
		if (!FixtureOverride)
		{
			AddError(FString::Printf(TEXT("Failed to load variant fixture '%s'."), *VariantPath));
			return false;
		}
	}

	if (Script->bSkipInAutomation)
	{
		AddInfo(TEXT("Skipped (bSkipInAutomation)."));
		return true;
	}

	if (!FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Slate is not initialized; UI Lab scripts need an interactive editor."));
		return false;
	}

	const TSharedPtr<FEEUIScriptTestSession> Session = MakeShared<FEEUIScriptTestSession>();

	TSharedRef<SEEUILabHostPanel> Panel = SNew(SEEUILabHostPanel);
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(NSLOCTEXT("EEUILab", "AutomationWindow", "UI Lab Automation"))
		.ClientSize(FVector2D(1280.0f, 800.0f));
	Window->SetContent(Panel);
	FSlateApplication::Get().AddWindow(Window);

	Session->Window = Window;
	Session->Panel = Panel;
	Session->Runner = MakeShared<FEEUIScriptRunner>();

	if (!Session->Runner->Start(Script, Panel, FixtureOverride))
	{
		AddError(TEXT("Failed to start the script runner."));
		Session->Close();
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEEUIWaitForScriptCommand(Session, this));
	return true;
}

#endif // WITH_EDITOR && WITH_AUTOMATION_TESTS
