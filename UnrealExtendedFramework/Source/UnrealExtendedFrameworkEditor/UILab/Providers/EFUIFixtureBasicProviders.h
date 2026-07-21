// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UILab/Fixture/EFUIFixture.h"
#include "EFUIFixtureBasicProviders.generated.h"

/**
 * Sets named reflected properties on the hosted widget from string values
 * (imported via the property's ImportText path — supports numbers, text, enums,
 * structs in export syntax). The generic "typed fixture state" workhorse.
 */
UCLASS(meta = (DisplayName = "Set Widget Properties"))
class UNREALEXTENDEDFRAMEWORKEDITOR_API UEFUIFixtureSetPropertiesConfig : public UEFUIFixtureProviderConfig
{
	GENERATED_BODY()

public:
	/** Property name -> value in property export syntax. */
	UPROPERTY(EditAnywhere, Category = "Provider")
	TMap<FName, FString> PropertyValues;

	virtual void ApplyToWidget(UUserWidget& Widget) const override;
};

/**
 * Calls a parameterless function/custom event on the hosted widget after instantiation,
 * e.g. a Blueprint "SetupPreviewState" event that fills the widget with sample data.
 */
UCLASS(meta = (DisplayName = "Call Widget Function"))
class UNREALEXTENDEDFRAMEWORKEDITOR_API UEFUIFixtureCallFunctionConfig : public UEFUIFixtureProviderConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Provider")
	FName FunctionName;

	virtual void ApplyToWidget(UUserWidget& Widget) const override;
};

/**
 * Routes to a registered project provider factory (FEEUIFixtureProviderRegistry) by type name.
 * Project adapters subclass this to add their own typed payload fields.
 */
UCLASS(meta = (DisplayName = "Registered Provider (Advanced)"))
class UNREALEXTENDEDFRAMEWORKEDITOR_API UEFUIFixtureRegisteredProviderConfig : public UEFUIFixtureProviderConfig
{
	GENERATED_BODY()

public:
	/** Provider type resolved through the provider registry (e.g. "Inventory", "Lobby"). */
	UPROPERTY(EditAnywhere, Category = "Provider")
	FName ProviderType;

	virtual FName GetProviderType() const override { return ProviderType; }
};
