// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Blueprint/UserWidget.h"
#include "UILab/Host/EEUILabSandbox.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class FEEUILabCaptureProcessor;
class FEEUILabInputLog;
class IEEUIFixtureProvider;
class SBox;
class SEEUILabEventScope;
class SScaleBox;
class UEFUIFixture;

/**
 * UL-1 Fast Widget Host.
 *
 * Hosts a UUserWidget instance in an editor-owned Slate panel with emulated resolution, DPI
 * scale, safe zone, and preview culture — no PIE, no gameplay map. The hosted widget receives
 * real Slate mouse/keyboard events and its animations/timers tick through the normal
 * SObjectWidget/editor-world paths. Reinstantiates automatically after Blueprint compilation.
 */
class SEEUILabHostPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEEUILabHostPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SEEUILabHostPanel() override;

	/** Sets and instantiates the hosted widget class (UUserWidget subclass). Leaves fixture mode. */
	void SetWidgetClass(UClass* InWidgetClass);

	/** Applies a fixture: display/culture emulation, widget class, providers, and initial focus. */
	void ApplyFixture(UEFUIFixture* Fixture);

	UUserWidget* GetHostedWidget() const { return HostedWidget.Get(); }
	TSharedPtr<FEEUILabInputLog> GetInputLog() const { return InputLog; }

	/** Integration hook: hosts a widget class with a preview culture (Localization Workbench preview). */
	void HostWidgetWithCulture(UClass* InWidgetClass, const FString& CultureName);

	/** UX-2: shows the Scripts panel with the given script selected. */
	void OpenScript(class UEFUIInteractionScript* Script);

private:
	// --- Widget lifecycle ---
	void InstantiateWidget();
	void ReleaseWidget();
	void HandleBlueprintReinstanced();

	// --- Fixture state ---
	void ApplyFixtureStateToWidget();
	void ResetProviders();

	// --- Toolbar ---
	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeToolsMenu();
	TSharedRef<SWidget> MakeFixturePickerButton();
	TSharedRef<SWidget> MakeFixturePickerMenu();
	TSharedRef<SWidget> MakeWidgetPickerButton();
	TSharedRef<SWidget> MakeWidgetPickerMenu();
	TSharedRef<SWidget> MakeResolutionCombo();
	TSharedRef<SWidget> MakeCultureCombo();

	// --- Emulation state ---
	struct FResolutionPreset
	{
		FText Label;
		FIntPoint Size;
	};

	void SetPreviewCulture(const FString& CultureName);

	/** The hosted widget's Slate representation, for focus steering and input injection. */
	TSharedPtr<SWidget> GetHostedSlateWidget() const;

	/** Class currently hosted; resolved from path after Blueprint reinstancing. */
	FSoftClassPath HostedClassPath;
	TStrongObjectPtr<UUserWidget> HostedWidget;

	/** Live sandbox session when the fixture requires the Runtime-Faithful host mode. */
	TUniquePtr<FEEUILabSandbox> Sandbox;

	/** Fixture currently applied (null when hosting a bare widget class). */
	TWeakObjectPtr<UEFUIFixture> ActiveFixture;
	/** Registry-created providers live for the fixture session and are Reset on release. */
	TArray<TSharedPtr<IEEUIFixtureProvider>> ActiveProviders;

	/** Slot that receives the hosted widget's Slate representation. */
	TSharedPtr<SBox> WidgetSlot;

	/** A-1: current DPI — the project's DPI curve for the emulated resolution unless overridden. */
	float GetEffectiveDPIScale() const;

	/** A-2: injects one navigation event (D-pad key) through real routing. */
	void StepNavigation(FKey NavKey);

	/** A-2: logs every focusable widget (identity, eligibility) into the input inspector. */
	void LogFocusReport();

	/** UX-4: snapshots the current host state into a new fixture asset via a save dialog. */
	void SaveCurrentSetup();

	FIntPoint EmulatedResolution = FIntPoint(1920, 1080);
	bool bAutoDPI = true;
	float DPIScale = 1.0f;
	float SafeZonePercent = 0.0f;
	/** true = scale-to-fit the panel (design mode), false = 1:1 pixels. */
	bool bScaleToFit = true;
	/** Empty = no localization preview. */
	FString PreviewCultureName;
	bool bEnabledLocalizationPreview = false;

	TArray<TSharedPtr<FResolutionPreset>> ResolutionOptions;
	TArray<TSharedPtr<FString>> CultureOptions;

	// --- Input tooling (UL-4) ---
	TSharedPtr<FEEUILabInputLog> InputLog;
	TSharedPtr<FEEUILabCaptureProcessor> CaptureProcessor;
	TSharedPtr<SEEUILabEventScope> EventScope;
	bool bShowVirtualController = false;
	bool bShowInputInspector = false;

	// --- Focus visualizer (UL-5) ---
	bool bShowFocusOverlay = false;
	bool bFocusClickMode = false;

	// --- Interaction scripts (UL-6) ---
	TSharedPtr<class SEEUILabScriptPanel> ScriptPanel;
	bool bShowScriptPanel = false;

	FDelegateHandle BlueprintReinstancedHandle;
};

#endif // WITH_EDITOR
