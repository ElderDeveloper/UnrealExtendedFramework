// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EFUIFixture.generated.h"

class AGameModeBase;
class UGameInstance;
class UGameInstanceSubsystem;
class UInputMappingContext;
class UUserWidget;

/** How a fixture hosts its widget (see plan UL-1/UL-3). */
UENUM()
enum class EEFUIFixtureHostMode : uint8
{
	/** Editor-owned Slate host: fast, no world. For presentation/self-contained widgets. */
	FastHost,
	/** Transient preview world with LocalPlayer/PlayerController. For subsystem-dependent widgets. */
	RuntimeFaithfulSandbox
};

/**
 * Base class for typed fixture provider configurations.
 *
 * Instanced inline on a fixture. Two dispatch paths:
 *  - GetProviderType() == NAME_None: the config self-applies via ApplyToWidget (built-in configs).
 *  - GetProviderType() != NAME_None: the UI Lab routes it through FEEUIFixtureProviderRegistry so a
 *    registered project factory (editor adapter module) can create the actual provider.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UNREALEXTENDEDFRAMEWORKEDITOR_API UEFUIFixtureProviderConfig : public UObject
{
	GENERATED_BODY()

public:
	virtual FName GetProviderType() const { return NAME_None; }
	virtual void ApplyToWidget(UUserWidget& Widget) const {}
	virtual void Reset() {}
};

/**
 * UL-2 fixture asset: a complete, repeatable description of how to host a widget in the UI Lab.
 *
 * Supports inheritance: unset values fall back to ParentFixture, and list-like fields
 * (input contexts, providers, tags) append to the parent's. One base fixture can therefore
 * generate culture/resolution/input/state variants without duplicating data.
 *
 * Lives in the UncookedOnly editor module, so fixture assets never enter cooked builds.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDFRAMEWORKEDITOR_API UEFUIFixture : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Optional base fixture this one derives from. */
	UPROPERTY(EditAnywhere, Category = "Fixture")
	TObjectPtr<UEFUIFixture> ParentFixture;

	/** Widget to host. Falls back to the parent's when unset. */
	UPROPERTY(EditAnywhere, Category = "Widget")
	TSoftClassPtr<UUserWidget> WidgetClass;

	UPROPERTY(EditAnywhere, Category = "Widget")
	EEFUIFixtureHostMode HostMode = EEFUIFixtureHostMode::FastHost;

	// --- Display ---

	UPROPERTY(EditAnywhere, Category = "Display", meta = (InlineEditConditionToggle))
	bool bOverrideResolution = false;

	UPROPERTY(EditAnywhere, Category = "Display", meta = (EditCondition = "bOverrideResolution"))
	FIntPoint Resolution = FIntPoint(1920, 1080);

	UPROPERTY(EditAnywhere, Category = "Display", meta = (InlineEditConditionToggle))
	bool bOverrideDPIScale = false;

	UPROPERTY(EditAnywhere, Category = "Display", meta = (EditCondition = "bOverrideDPIScale", ClampMin = "0.25", ClampMax = "4.0"))
	float DPIScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Display", meta = (InlineEditConditionToggle))
	bool bOverrideSafeZonePercent = false;

	UPROPERTY(EditAnywhere, Category = "Display", meta = (EditCondition = "bOverrideSafeZonePercent", ClampMin = "0.0", ClampMax = "25.0"))
	float SafeZonePercent = 0.0f;

	/** Optional platform profile label (e.g. "Desktop", "SteamDeck"). NAME_None inherits. */
	UPROPERTY(EditAnywhere, Category = "Display")
	FName PlatformProfile;

	// --- Localization ---

	/** Preview culture (e.g. "fr", "ja"). Empty inherits from parent; parent chain empty = no preview. */
	UPROPERTY(EditAnywhere, Category = "Localization")
	FString Culture;

	// --- Input ---

	/** Initial input device profile label (e.g. "KBM", "Gamepad_Xbox"). NAME_None inherits. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FName InitialInputDevice;

	/** Enhanced Input mapping contexts applied in the Runtime-Faithful Sandbox. Appends to parent's. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TArray<TSoftObjectPtr<UInputMappingContext>> InputMappingContexts;

	/** Automation identity of the widget to focus after instantiation. NAME_None inherits. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FName InitialFocusTarget;

	// --- Runtime-Faithful Sandbox ---

	/** GameInstance class for the sandbox. Null inherits; whole chain null = plain UGameInstance (never the project's by default). */
	UPROPERTY(EditAnywhere, Category = "Sandbox")
	TSoftClassPtr<UGameInstance> SandboxGameInstanceClass;

	/** GameMode class for the sandbox. Null inherits; whole chain null = AGameModeBase (never the project's by default). */
	UPROPERTY(EditAnywhere, Category = "Sandbox")
	TSoftClassPtr<AGameModeBase> SandboxGameModeClass;

	/**
	 * GameInstance subsystems this fixture's widget depends on. The sandbox verifies they exist
	 * after initialization and reports exactly which ones are missing (appends to parent's).
	 */
	UPROPERTY(EditAnywhere, Category = "Sandbox")
	TArray<TSoftClassPtr<UGameInstanceSubsystem>> RequiredSubsystems;

	// --- Providers ---

	/** Typed provider configurations. Appends to (runs after) the parent's providers. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Providers")
	TArray<TObjectPtr<UEFUIFixtureProviderConfig>> Providers;

	// --- Screenshot (A-4: advanced until screenshot assertions land) ---

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Screenshot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScreenshotTolerance = 0.02f;

	// --- Meta ---

	/** Tags/categories for the fixture browser (appends to parent's). */
	UPROPERTY(EditAnywhere, Category = "Meta")
	TArray<FName> Tags;

	// --- Effective (inheritance-resolved) accessors ---

	TSoftClassPtr<UUserWidget> GetEffectiveWidgetClass() const;
	EEFUIFixtureHostMode GetEffectiveHostMode() const;
	FIntPoint GetEffectiveResolution() const;
	float GetEffectiveDPIScale() const;
	/** True when any fixture in the chain explicitly overrides DPI (else hosts use auto DPI). */
	bool HasEffectiveDPIOverride() const;
	float GetEffectiveSafeZonePercent() const;
	FName GetEffectivePlatformProfile() const;
	FString GetEffectiveCulture() const;
	FName GetEffectiveInitialInputDevice() const;
	FName GetEffectiveInitialFocusTarget() const;
	TSoftClassPtr<UGameInstance> GetEffectiveSandboxGameInstanceClass() const;
	TSoftClassPtr<AGameModeBase> GetEffectiveSandboxGameModeClass() const;
	/** Parent-first, cycle-safe. */
	void GetEffectiveRequiredSubsystems(TArray<TSoftClassPtr<UGameInstanceSubsystem>>& OutSubsystems) const;
	/** Parent-first, cycle-safe. */
	void GetEffectiveInputMappingContexts(TArray<TSoftObjectPtr<UInputMappingContext>>& OutContexts) const;
	/** Parent-first, cycle-safe. */
	void GetEffectiveProviders(TArray<UEFUIFixtureProviderConfig*>& OutProviders) const;
	void GetEffectiveTags(TArray<FName>& OutTags) const;

private:
	/** Walks the parent chain root-first with cycle protection. */
	void GetChainRootFirst(TArray<const UEFUIFixture*>& OutChain) const;
};
