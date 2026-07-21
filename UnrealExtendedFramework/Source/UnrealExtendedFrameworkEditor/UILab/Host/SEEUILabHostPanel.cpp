// Copyright Moon Punch Games. All Rights Reserved.

#include "SEEUILabHostPanel.h"

#if WITH_EDITOR

#include "Blueprint/UserWidget.h"
#include "ContentBrowserModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/UserInterfaceSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/GameModeBase.h"
#include "IContentBrowserSingleton.h"
#include "Components/Widget.h"
#include "Internationalization/TextLocalizationManager.h"
#include "UILab/EEUILabUtils.h"
#include "UILab/Fixture/EFUIFixture.h"
#include "UILab/Focus/SEEUILabFocusOverlay.h"
#include "UILab/Input/EEUILabCaptureProcessor.h"
#include "UILab/Input/EEUILabInputLog.h"
#include "UILab/Input/SEEUILabEventScope.h"
#include "UILab/Input/SEEUILabInputInspector.h"
#include "UILab/Input/SEEUILabVirtualController.h"
#include "UILab/Providers/EEUIFixtureProviderRegistry.h"
#include "UILab/Scripting/SEEUILabScriptPanel.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "WidgetBlueprint.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SEEUILabHostPanel"

namespace
{
	/** Paints translucent margins marking the emulated safe zone. Hit-test invisible. */
	class SEESafeZoneOverlay : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SEESafeZoneOverlay) {}
			SLATE_ATTRIBUTE(float, SafeZonePercent)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			SafeZonePercent = InArgs._SafeZonePercent;
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
			const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
			const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			const float Percent = FMath::Clamp(SafeZonePercent.Get(0.0f), 0.0f, 25.0f) / 100.0f;
			if (Percent <= KINDA_SMALL_NUMBER)
			{
				return LayerId;
			}

			const FVector2D Size = AllottedGeometry.GetLocalSize();
			const FVector2D Margin(Size.X * Percent, Size.Y * Percent);
			const FSlateBrush* Brush = FAppStyle::GetBrush("WhiteBrush");
			const FLinearColor Tint(1.0f, 0.15f, 0.15f, 0.25f);

			auto DrawRect = [&](const FVector2D& Pos, const FVector2D& RectSize)
			{
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
					AllottedGeometry.ToPaintGeometry(RectSize, FSlateLayoutTransform(Pos)), Brush,
					ESlateDrawEffect::None, Tint);
			};

			DrawRect(FVector2D::ZeroVector, FVector2D(Size.X, Margin.Y));                        // top
			DrawRect(FVector2D(0.0f, Size.Y - Margin.Y), FVector2D(Size.X, Margin.Y));           // bottom
			DrawRect(FVector2D(0.0f, Margin.Y), FVector2D(Margin.X, Size.Y - 2.0f * Margin.Y));  // left
			DrawRect(FVector2D(Size.X - Margin.X, Margin.Y), FVector2D(Margin.X, Size.Y - 2.0f * Margin.Y)); // right

			return LayerId + 1;
		}

	private:
		TAttribute<float> SafeZonePercent;
	};
}

void SEEUILabHostPanel::Construct(const FArguments& InArgs)
{
	ResolutionOptions = {
		MakeShared<FResolutionPreset>(FResolutionPreset{LOCTEXT("Res1080", "1920 x 1080 (16:9)"), FIntPoint(1920, 1080)}),
		MakeShared<FResolutionPreset>(FResolutionPreset{LOCTEXT("Res1440", "2560 x 1440 (16:9)"), FIntPoint(2560, 1440)}),
		MakeShared<FResolutionPreset>(FResolutionPreset{LOCTEXT("Res4K", "3840 x 2160 (16:9)"), FIntPoint(3840, 2160)}),
		MakeShared<FResolutionPreset>(FResolutionPreset{LOCTEXT("Res720", "1280 x 720 (16:9)"), FIntPoint(1280, 720)}),
		MakeShared<FResolutionPreset>(FResolutionPreset{LOCTEXT("ResUltrawide", "3440 x 1440 (21:9)"), FIntPoint(3440, 1440)}),
		MakeShared<FResolutionPreset>(FResolutionPreset{LOCTEXT("Res1610", "1920 x 1200 (16:10)"), FIntPoint(1920, 1200)}),
		MakeShared<FResolutionPreset>(FResolutionPreset{LOCTEXT("ResHandheld", "1280 x 800 (handheld)"), FIntPoint(1280, 800)}),
	};

	// "None" plus every culture the game has localization data for.
	CultureOptions.Add(MakeShared<FString>(TEXT("")));
	TArray<FString> LocalizedCultures = FTextLocalizationManager::Get().GetLocalizedCultureNames(ELocalizationLoadFlags::Game);
	LocalizedCultures.Sort();
	for (const FString& Culture : LocalizedCultures)
	{
		CultureOptions.Add(MakeShared<FString>(Culture));
	}

	InputLog = MakeShared<FEEUILabInputLog>();

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			MakeToolbar()
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("Checkerboard"))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SScaleBox)
						.Stretch_Lambda([this]() { return bScaleToFit ? EStretch::ScaleToFit : EStretch::None; })
						[
							SNew(SOverlay)

							+ SOverlay::Slot()
							[
								SNew(SBox)
								.WidthOverride_Lambda([this]() { return static_cast<float>(EmulatedResolution.X); })
								.HeightOverride_Lambda([this]() { return static_cast<float>(EmulatedResolution.Y); })
								.Clipping(EWidgetClipping::ClipToBounds)
								[
									SNew(SDPIScaler)
									.DPIScale_Lambda([this]() { return GetEffectiveDPIScale(); })
									[
										SAssignNew(EventScope, SEEUILabEventScope, InputLog.ToSharedRef())
										[
											SAssignNew(WidgetSlot, SBox)
											.HAlign(HAlign_Fill)
											.VAlign(VAlign_Fill)
											[
												SNew(SBox)
												.HAlign(HAlign_Center)
												.VAlign(VAlign_Center)
												[
													SNew(STextBlock)
													.Text(LOCTEXT("NoWidget", "Pick a Widget Blueprint to host it here.\n\nYou can also use the \"UI Lab\" button in any Widget Blueprint's designer toolbar,\nor right-click a Widget Blueprint in the Content Browser and choose \"Open in Extended UI Lab\"."))
													.Justification(ETextJustify::Center)
												]
											]
										]
									]
								]
							]

							+ SOverlay::Slot()
							[
								SNew(SEESafeZoneOverlay)
								.SafeZonePercent_Lambda([this]() { return SafeZonePercent; })
							]

							+ SOverlay::Slot()
							[
								SNew(SEEUILabFocusOverlay, InputLog.ToSharedRef())
								.HostedWidget_Lambda([this]() { return HostedWidget.Get(); })
								.ShowOverlay_Lambda([this]() { return bShowFocusOverlay; })
								.ClickToFocus_Lambda([this]() { return bFocusClickMode; })
							]
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					SNew(SBox)
					.Visibility_Lambda([this]() { return bShowVirtualController ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SEEUILabVirtualController)
						.FocusTarget_Lambda([this]() { return GetHostedSlateWidget(); })
					]
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(340.0f)
				.Visibility_Lambda([this]() { return bShowInputInspector ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SEEUILabInputInspector, InputLog.ToSharedRef())
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(380.0f)
				.Visibility_Lambda([this]() { return bShowScriptPanel ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SAssignNew(ScriptPanel, SEEUILabScriptPanel, SharedThis(this))
				]
			]
		]
	];

	if (GEditor)
	{
		BlueprintReinstancedHandle = GEditor->OnBlueprintReinstanced().AddSP(this, &SEEUILabHostPanel::HandleBlueprintReinstanced);
	}

	if (FSlateApplication::IsInitialized())
	{
		CaptureProcessor = MakeShared<FEEUILabCaptureProcessor>(InputLog.ToSharedRef());
		FSlateApplication::Get().RegisterInputPreProcessor(CaptureProcessor);
	}
}

SEEUILabHostPanel::~SEEUILabHostPanel()
{
	if (CaptureProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(CaptureProcessor);
	}
	CaptureProcessor.Reset();

	if (GEditor && BlueprintReinstancedHandle.IsValid())
	{
		GEditor->OnBlueprintReinstanced().Remove(BlueprintReinstancedHandle);
	}

	if (bEnabledLocalizationPreview)
	{
		FTextLocalizationManager::Get().DisableGameLocalizationPreview();
	}

	ReleaseWidget();
}

void SEEUILabHostPanel::SetWidgetClass(UClass* InWidgetClass)
{
	if (InWidgetClass && InWidgetClass->IsChildOf(UUserWidget::StaticClass()))
	{
		ActiveFixture.Reset();
		HostedClassPath = FSoftClassPath(InWidgetClass);
		InstantiateWidget();
	}
}

void SEEUILabHostPanel::ApplyFixture(UEFUIFixture* Fixture)
{
	if (!Fixture)
	{
		return;
	}

	UClass* WidgetClass = Fixture->GetEffectiveWidgetClass().LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UI Lab: fixture '%s' resolves no widget class."), *Fixture->GetName());
		return;
	}

	ActiveFixture = Fixture;
	EmulatedResolution = Fixture->GetEffectiveResolution();
	bAutoDPI = !Fixture->HasEffectiveDPIOverride();
	DPIScale = Fixture->GetEffectiveDPIScale();
	SafeZonePercent = Fixture->GetEffectiveSafeZonePercent();
	SetPreviewCulture(Fixture->GetEffectiveCulture());

	// Runtime-Faithful Sandbox host mode arrives with UL-3; until then everything runs in the Fast Host.
	HostedClassPath = FSoftClassPath(WidgetClass);
	InstantiateWidget();
}

void SEEUILabHostPanel::InstantiateWidget()
{
	ReleaseWidget();

	UClass* WidgetClass = HostedClassPath.TryLoadClass<UUserWidget>();
	if (!WidgetClass || !GEditor)
	{
		return;
	}

	UEFUIFixture* Fixture = ActiveFixture.Get();
	const bool bWantsSandbox = Fixture && Fixture->GetEffectiveHostMode() == EEFUIFixtureHostMode::RuntimeFaithfulSandbox;

	UUserWidget* NewWidget = nullptr;

	if (bWantsSandbox)
	{
		FEEUILabSandboxParams Params;
		Params.GameInstanceClass = Fixture->GetEffectiveSandboxGameInstanceClass().LoadSynchronous();
		Params.GameModeClass = Fixture->GetEffectiveSandboxGameModeClass().LoadSynchronous();
		Fixture->GetEffectiveRequiredSubsystems(Params.RequiredSubsystems);

		Sandbox = MakeUnique<FEEUILabSandbox>();
		FString SandboxError;
		if (Sandbox->Initialize(Params, SandboxError))
		{
			NewWidget = CreateWidget<UUserWidget>(Sandbox->GetPlayerController(), WidgetClass);

			TArray<TSoftObjectPtr<UInputMappingContext>> InputContexts;
			Fixture->GetEffectiveInputMappingContexts(InputContexts);
			Sandbox->ApplyInputMappingContexts(InputContexts);
		}
		else
		{
			// Per plan UL-3: report which stage failed and fall back to the Fast Host, never crash.
			UE_LOG(LogTemp, Warning, TEXT("UI Lab: sandbox init failed (%s); falling back to Fast Host."), *SandboxError);
			Sandbox.Reset();
		}
	}

	if (!NewWidget)
	{
		UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
		if (!EditorWorld)
		{
			return;
		}
		NewWidget = CreateWidget<UUserWidget>(EditorWorld, WidgetClass);
	}

	if (!NewWidget)
	{
		return;
	}

	HostedWidget = TStrongObjectPtr<UUserWidget>(NewWidget);
	WidgetSlot->SetContent(NewWidget->TakeWidget());

	if (CaptureProcessor.IsValid())
	{
		CaptureProcessor->SetCaptureContext(AsShared(), GetHostedSlateWidget());
	}

	ApplyFixtureStateToWidget();
}

TSharedPtr<SWidget> SEEUILabHostPanel::GetHostedSlateWidget() const
{
	return HostedWidget.IsValid() ? HostedWidget->GetCachedWidget() : nullptr;
}

void SEEUILabHostPanel::StepNavigation(const FKey NavKey)
{
	if (const TSharedPtr<SWidget> Target = GetHostedSlateWidget())
	{
		FSlateApplication& SlateApp = FSlateApplication::Get();
		const TSharedPtr<SWidget> CurrentFocus = SlateApp.GetUserFocusedWidget(0);

		bool bFocusInside = false;
		for (TSharedPtr<SWidget> Walker = CurrentFocus; Walker.IsValid(); Walker = Walker->GetParentWidget())
		{
			if (Walker == Target)
			{
				bFocusInside = true;
				break;
			}
		}
		if (!bFocusInside)
		{
			SlateApp.SetUserFocus(0, Target, EFocusCause::SetDirectly);
		}

		const FKeyEvent DownEvent(NavKey, FModifierKeysState(), 0, false, 0, 0);
		SlateApp.ProcessKeyDownEvent(DownEvent);
		const FKeyEvent UpEvent(NavKey, FModifierKeysState(), 0, false, 0, 0);
		SlateApp.ProcessKeyUpEvent(UpEvent);
	}
}

void SEEUILabHostPanel::LogFocusReport()
{
	UUserWidget* Root = HostedWidget.Get();
	if (!Root || !InputLog.IsValid())
	{
		return;
	}

	int32 FocusableCount = 0;
	int32 IneligibleCount = 0;

	TFunction<void(UUserWidget&)> Walk = [&](UUserWidget& UserWidget)
	{
		if (!UserWidget.WidgetTree)
		{
			return;
		}
		UserWidget.WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (!Widget)
			{
				return;
			}
			if (UUserWidget* Nested = Cast<UUserWidget>(Widget))
			{
				Walk(*Nested);
				return;
			}

			const TSharedPtr<SWidget> SlateWidget = Widget->GetCachedWidget();
			if (!SlateWidget.IsValid() || !SlateWidget->SupportsKeyboardFocus())
			{
				return;
			}

			++FocusableCount;
			const bool bEligible = Widget->GetIsEnabled() && Widget->IsVisible();
			if (!bEligible)
			{
				++IneligibleCount;
			}

			FEEUILabInputEvent Event;
			Event.EventType = TEXT("Focusable");
			Event.bHandledByWidget = bEligible;
			Event.Detail = FString::Printf(TEXT("%s%s"), *Widget->GetName(),
				bEligible ? TEXT("") : TEXT("  — INELIGIBLE (disabled or hidden: unreachable by navigation)"));
			InputLog->Add(MoveTemp(Event));
		});
	};
	Walk(*Root);

	FEEUILabInputEvent Summary;
	Summary.EventType = TEXT("Focusable");
	Summary.Detail = FString::Printf(TEXT("Focus report: %d focusable, %d ineligible."), FocusableCount, IneligibleCount);
	InputLog->Add(MoveTemp(Summary));

	bShowInputInspector = true;
}

float SEEUILabHostPanel::GetEffectiveDPIScale() const
{
	if (bAutoDPI)
	{
		return GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(EmulatedResolution);
	}
	return DPIScale;
}

void SEEUILabHostPanel::SaveCurrentSetup()
{
	if (!HostedClassPath.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UI Lab: host a widget before saving a setup."));
		return;
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FSaveAssetDialogConfig DialogConfig;
	DialogConfig.DialogTitleOverride = LOCTEXT("SaveSetupTitle", "Save UI Lab Setup");
	DialogConfig.DefaultPath = TEXT("/Game");
	DialogConfig.DefaultAssetName = FPaths::GetBaseFilename(HostedClassPath.GetAssetName()).Replace(TEXT("_C"), TEXT("")) + TEXT("_Setup");
	DialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

	const FString ObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(DialogConfig);
	if (ObjectPath.IsEmpty())
	{
		return;
	}

	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);

	UPackage* Package = CreatePackage(*PackageName);
	UEFUIFixture* Fixture = NewObject<UEFUIFixture>(Package, FName(*AssetName), RF_Public | RF_Standalone);

	// Snapshot the live host state.
	Fixture->WidgetClass = TSoftClassPtr<UUserWidget>(HostedClassPath);
	Fixture->HostMode = Sandbox.IsValid() ? EEFUIFixtureHostMode::RuntimeFaithfulSandbox : EEFUIFixtureHostMode::FastHost;
	Fixture->bOverrideResolution = true;
	Fixture->Resolution = EmulatedResolution;
	Fixture->bOverrideDPIScale = !bAutoDPI;
	Fixture->DPIScale = DPIScale;
	Fixture->bOverrideSafeZonePercent = SafeZonePercent > 0.0f;
	Fixture->SafeZonePercent = SafeZonePercent;
	Fixture->Culture = PreviewCultureName;

	FAssetRegistryModule::AssetCreated(Fixture);
	Package->MarkPackageDirty();

	ActiveFixture = Fixture;
}

void SEEUILabHostPanel::HostWidgetWithCulture(UClass* InWidgetClass, const FString& CultureName)
{
	SetPreviewCulture(CultureName);
	SetWidgetClass(InWidgetClass);
}

void SEEUILabHostPanel::OpenScript(UEFUIInteractionScript* Script)
{
	bShowScriptPanel = true;
	if (ScriptPanel.IsValid())
	{
		ScriptPanel->SetActiveScript(Script);
	}
}

void SEEUILabHostPanel::ReleaseWidget()
{
	ResetProviders();

	if (WidgetSlot.IsValid())
	{
		WidgetSlot->SetContent(SNullWidget::NullWidget);
	}
	HostedWidget.Reset();

	// Widget released first, then its world: deterministic sandbox teardown.
	if (Sandbox.IsValid())
	{
		Sandbox->Shutdown();
		Sandbox.Reset();
	}
}

void SEEUILabHostPanel::ApplyFixtureStateToWidget()
{
	UEFUIFixture* Fixture = ActiveFixture.Get();
	if (!Fixture || !HostedWidget.IsValid())
	{
		return;
	}

	FEEUIFixtureContext Context;
	Context.Fixture = Fixture;
	Context.Widget = HostedWidget.Get();
	Context.World = HostedWidget->GetWorld();

	TArray<UEFUIFixtureProviderConfig*> ProviderConfigs;
	Fixture->GetEffectiveProviders(ProviderConfigs);

	for (UEFUIFixtureProviderConfig* Config : ProviderConfigs)
	{
		const FName ProviderType = Config->GetProviderType();
		if (ProviderType.IsNone())
		{
			// Built-in config: self-applies.
			Config->ApplyToWidget(*HostedWidget);
		}
		else if (TSharedPtr<IEEUIFixtureProvider> Provider = FEEUIFixtureProviderRegistry::Get().CreateProvider(ProviderType, Context))
		{
			Provider->ApplyState(Context, Config);
			ActiveProviders.Add(Provider);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UI Lab: no registered factory provides '%s' (fixture '%s')."),
				*ProviderType.ToString(), *Fixture->GetName());
		}
	}

	const FName FocusTarget = Fixture->GetEffectiveInitialFocusTarget();
	if (!FocusTarget.IsNone())
	{
		if (UWidget* Widget = EEUILabUtils::FindWidgetByAutomationIdentity(*HostedWidget, FocusTarget))
		{
			Widget->SetFocus();
		}
	}
}

void SEEUILabHostPanel::ResetProviders()
{
	for (const TSharedPtr<IEEUIFixtureProvider>& Provider : ActiveProviders)
	{
		if (Provider.IsValid())
		{
			Provider->Reset();
		}
	}
	ActiveProviders.Reset();

	if (const UEFUIFixture* Fixture = ActiveFixture.Get())
	{
		TArray<UEFUIFixtureProviderConfig*> ProviderConfigs;
		Fixture->GetEffectiveProviders(ProviderConfigs);
		for (UEFUIFixtureProviderConfig* Config : ProviderConfigs)
		{
			Config->Reset();
		}
	}
}

void SEEUILabHostPanel::HandleBlueprintReinstanced()
{
	// The hosted class may have been replaced by reinstancing; rebuild from the stable path.
	if (HostedClassPath.IsValid() && HostedWidget.IsValid())
	{
		InstantiateWidget();
	}
}

TSharedRef<SWidget> SEEUILabHostPanel::MakeToolbar()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
		[
			MakeWidgetPickerButton()
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			MakeResolutionCombo()
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(LOCTEXT("DPILabel", "DPI"))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]() { return bAutoDPI ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
			{
				bAutoDPI = NewState == ECheckBoxState::Checked;
				if (!bAutoDPI)
				{
					DPIScale = GetEffectiveDPIScale();
				}
			})
			[
				SNew(STextBlock).Text(LOCTEXT("AutoDPI", "Auto"))
				.ToolTipText(LOCTEXT("AutoDPITooltip", "Use the project's DPI curve for the emulated resolution (matches the UMG designer)."))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(70.0f)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.25f).MaxValue(4.0f).Delta(0.05f)
				.IsEnabled_Lambda([this]() { return !bAutoDPI; })
				.Value_Lambda([this]() { return GetEffectiveDPIScale(); })
				.OnValueChanged_Lambda([this](float NewValue) { DPIScale = NewValue; })
			]
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(LOCTEXT("SafeZoneLabel", "Safe Zone %"))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(70.0f)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.0f).MaxValue(25.0f).Delta(0.5f)
				.Value_Lambda([this]() { return SafeZonePercent; })
				.OnValueChanged_Lambda([this](float NewValue) { SafeZonePercent = NewValue; })
			]
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			MakeCultureCombo()
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.IsChecked_Lambda([this]() { return bScaleToFit ? ECheckBoxState::Unchecked : ECheckBoxState::Checked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { bScaleToFit = NewState != ECheckBoxState::Checked; })
			[
				SNew(STextBlock).Text(LOCTEXT("PixelMode", "1:1"))
				.ToolTipText(LOCTEXT("PixelModeTooltip", "Toggle between scale-to-fit and 1:1 pixel display."))
			]
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Text(LOCTEXT("Reinstantiate", "Reinstantiate"))
			.ToolTipText(LOCTEXT("ReinstantiateTooltip", "Destroy and recreate the hosted widget instance."))
			.OnClicked_Lambda([this]() { InstantiateWidget(); return FReply::Handled(); })
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(12.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			MakeFixturePickerButton()
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Text(LOCTEXT("SaveSetup", "Save Setup..."))
			.ToolTipText(LOCTEXT("SaveSetupTooltip", "Save the current widget, resolution, DPI, safe zone, and culture as a reusable Setup asset."))
			.IsEnabled_Lambda([this]() { return HostedClassPath.IsValid(); })
			.OnClicked_Lambda([this]() { SaveCurrentSetup(); return FReply::Handled(); })
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SComboButton)
			.OnGetMenuContent(FOnGetContent::CreateSP(this, &SEEUILabHostPanel::MakeToolsMenu))
			.ButtonContent()
			[
				SNew(STextBlock).Text(LOCTEXT("ToolsMenu", "Tools"))
				.ToolTipText(LOCTEXT("ToolsMenuTooltip", "Input capture, virtual gamepad, inspector, focus visualizer, and interaction scripts."))
			]
		];
}

TSharedRef<SWidget> SEEUILabHostPanel::MakeToolsMenu()
{
	// Keep the menu open so several tools can be toggled in one visit.
	FMenuBuilder MenuBuilder(/*bShouldCloseWindowAfterMenuSelection*/ false, nullptr);

	auto AddToggle = [&MenuBuilder](const FText& Label, const FText& Tooltip,
		const TFunction<bool()>& IsChecked, const TFunction<void()>& Toggle)
	{
		MenuBuilder.AddMenuEntry(Label, Tooltip, FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([Toggle]() { Toggle(); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([IsChecked]() { return IsChecked(); })),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);
	};

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("InputSection", "Input"));
	AddToggle(LOCTEXT("CapturePad", "Capture Gamepad"),
		LOCTEXT("CapturePadTooltip", "Route physical gamepad input to the hosted widget while this window is active. Release with View+Menu."),
		[this]() { return CaptureProcessor.IsValid() && CaptureProcessor->IsCaptureEnabled(); },
		[this]() { if (CaptureProcessor.IsValid()) { CaptureProcessor->SetCaptureEnabled(!CaptureProcessor->IsCaptureEnabled()); } });
	AddToggle(LOCTEXT("VirtualPad", "Virtual Gamepad"),
		LOCTEXT("VirtualPadTooltip", "On-screen gamepad controls that inject real input events."),
		[this]() { return bShowVirtualController; },
		[this]() { bShowVirtualController = !bShowVirtualController; });
	AddToggle(LOCTEXT("Inspector", "Input Inspector"),
		LOCTEXT("InspectorTooltip", "Focus path and the event route log with handled state."),
		[this]() { return bShowInputInspector; },
		[this]() { bShowInputInspector = !bShowInputInspector; });
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("FocusSection", "Focus"));
	AddToggle(LOCTEXT("FocusOverlay", "Focus Overlay"),
		LOCTEXT("FocusOverlayTooltip", "Show focusable widgets, current focus, eligibility, and navigation rules."),
		[this]() { return bShowFocusOverlay; },
		[this]() { bShowFocusOverlay = !bShowFocusOverlay; });
	AddToggle(LOCTEXT("FocusClick", "Click to Set Focus"),
		LOCTEXT("FocusClickTooltip", "Click a focusable widget in the viewport to give it focus (requires the Focus Overlay)."),
		[this]() { return bFocusClickMode; },
		[this]() { bFocusClickMode = !bFocusClickMode; });

	// A-2: single navigation steps through real routing.
	MenuBuilder.AddMenuEntry(LOCTEXT("StepUp", "Step Navigation Up"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { StepNavigation(EKeys::Gamepad_DPad_Up); })));
	MenuBuilder.AddMenuEntry(LOCTEXT("StepDown", "Step Navigation Down"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { StepNavigation(EKeys::Gamepad_DPad_Down); })));
	MenuBuilder.AddMenuEntry(LOCTEXT("StepLeft", "Step Navigation Left"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { StepNavigation(EKeys::Gamepad_DPad_Left); })));
	MenuBuilder.AddMenuEntry(LOCTEXT("StepRight", "Step Navigation Right"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { StepNavigation(EKeys::Gamepad_DPad_Right); })));

	MenuBuilder.AddMenuEntry(LOCTEXT("FocusReport", "Focus Report"),
		LOCTEXT("FocusReportTooltip", "List every focusable widget with its eligibility in the Inspector — ineligible entries are unreachable by navigation."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { LogFocusReport(); })));

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("TestingSection", "Testing"));
	AddToggle(LOCTEXT("Scripts", "Interaction Scripts"),
		LOCTEXT("ScriptsTooltip", "Record and replay semantic interaction scripts with assertions."),
		[this]() { return bShowScriptPanel; },
		[this]() { bShowScriptPanel = !bShowScriptPanel; });
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SEEUILabHostPanel::MakeFixturePickerButton()
{
	return SNew(SComboButton)
		.OnGetMenuContent(FOnGetContent::CreateSP(this, &SEEUILabHostPanel::MakeFixturePickerMenu))
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				const UEFUIFixture* Fixture = ActiveFixture.Get();
				return Fixture
					? FText::FromString(Fixture->GetName())
					: LOCTEXT("LoadSetup", "Load Setup...");
			})
		];
}

TSharedRef<SWidget> SEEUILabHostPanel::MakeFixturePickerMenu()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FAssetPickerConfig PickerConfig;
	PickerConfig.Filter.ClassPaths.Add(UEFUIFixture::StaticClass()->GetClassPathName());
	PickerConfig.Filter.bRecursiveClasses = true;
	PickerConfig.SelectionMode = ESelectionMode::Single;
	PickerConfig.InitialAssetViewType = EAssetViewType::List;
	PickerConfig.bAllowNullSelection = false;
	PickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda([this](const FAssetData& AssetData)
	{
		FSlateApplication::Get().DismissAllMenus();
		if (UEFUIFixture* Fixture = Cast<UEFUIFixture>(AssetData.GetAsset()))
		{
			ApplyFixture(Fixture);
		}
	});

	return SNew(SBox)
		.WidthOverride(350.0f)
		.HeightOverride(420.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(6.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SetupExplainer", "A Setup is a saved host configuration (widget, resolution, culture, mock data) used for repeatable previews, variants, and tests. Save the current state with \"Save Setup...\"."))
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))
			]

			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				ContentBrowserModule.Get().CreateAssetPicker(PickerConfig)
			]
		];
}

TSharedRef<SWidget> SEEUILabHostPanel::MakeWidgetPickerButton()
{
	return SNew(SComboButton)
		.OnGetMenuContent(FOnGetContent::CreateSP(this, &SEEUILabHostPanel::MakeWidgetPickerMenu))
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				return HostedClassPath.IsValid()
					? FText::FromString(FPaths::GetBaseFilename(HostedClassPath.GetAssetName()))
					: LOCTEXT("PickWidget", "Pick Widget...");
			})
		];
}

TSharedRef<SWidget> SEEUILabHostPanel::MakeWidgetPickerMenu()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FAssetPickerConfig PickerConfig;
	PickerConfig.Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
	PickerConfig.Filter.bRecursiveClasses = true;
	PickerConfig.SelectionMode = ESelectionMode::Single;
	PickerConfig.InitialAssetViewType = EAssetViewType::List;
	PickerConfig.bAllowNullSelection = false;
	PickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda([this](const FAssetData& AssetData)
	{
		FSlateApplication::Get().DismissAllMenus();
		if (const UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(AssetData.GetAsset()))
		{
			SetWidgetClass(WidgetBlueprint->GeneratedClass);
		}
	});

	return SNew(SBox)
		.WidthOverride(350.0f)
		.HeightOverride(400.0f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(PickerConfig)
		];
}

TSharedRef<SWidget> SEEUILabHostPanel::MakeResolutionCombo()
{
	return SNew(SComboBox<TSharedPtr<FResolutionPreset>>)
		.OptionsSource(&ResolutionOptions)
		.OnGenerateWidget_Lambda([](TSharedPtr<FResolutionPreset> Option)
		{
			return SNew(STextBlock).Text(Option->Label);
		})
		.OnSelectionChanged_Lambda([this](TSharedPtr<FResolutionPreset> Option, ESelectInfo::Type)
		{
			if (Option.IsValid())
			{
				EmulatedResolution = Option->Size;
			}
		})
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				return FText::Format(LOCTEXT("ResolutionFmt", "{0} x {1}"),
					FText::AsNumber(EmulatedResolution.X, &FNumberFormattingOptions::DefaultNoGrouping()),
					FText::AsNumber(EmulatedResolution.Y, &FNumberFormattingOptions::DefaultNoGrouping()));
			})
		];
}

TSharedRef<SWidget> SEEUILabHostPanel::MakeCultureCombo()
{
	return SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&CultureOptions)
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
		{
			return SNew(STextBlock).Text(Option->IsEmpty()
				? LOCTEXT("NoCulture", "Culture: None")
				: FText::FromString(*Option));
		})
		.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Option, ESelectInfo::Type)
		{
			if (Option.IsValid())
			{
				SetPreviewCulture(*Option);
			}
		})
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				return PreviewCultureName.IsEmpty()
					? LOCTEXT("NoCulture", "Culture: None")
					: FText::Format(LOCTEXT("CultureFmt", "Culture: {0}"), FText::FromString(PreviewCultureName));
			})
		];
}

void SEEUILabHostPanel::SetPreviewCulture(const FString& CultureName)
{
	PreviewCultureName = CultureName;

	// Same mechanism the UMG designer uses: a preview layer, never a persistent culture change.
	if (CultureName.IsEmpty())
	{
		FTextLocalizationManager::Get().DisableGameLocalizationPreview();
		bEnabledLocalizationPreview = false;
	}
	else
	{
		FTextLocalizationManager::Get().EnableGameLocalizationPreview(CultureName);
		bEnabledLocalizationPreview = true;
	}
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
