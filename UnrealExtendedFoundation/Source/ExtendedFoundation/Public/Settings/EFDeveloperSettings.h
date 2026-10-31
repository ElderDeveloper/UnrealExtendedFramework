// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EFDeveloperSettings.generated.h"

/**
 * Base for every Extended plugin's Project Settings page.
 *
 * - Places the page under Project Settings > Extended Framework, so all plugins sit together.
 * - OpenInEditor() jumps to the page from a menu entry, notification or error message.
 * - Warns in the editor when a property marked meta = (EFSecret) holds a value and is saved to a
 *   Default*.ini file, which ships inside packaged builds. Secrets belong in FEFCredentialStore
 *   (per developer, never committed) or on a server; mark the property Transient if it only
 *   mirrors a value kept there.
 *
 *     UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Extended PlayFab"))
 *     class UEPFSettings : public UEFDeveloperSettings
 *     {
 *         UPROPERTY(config, EditAnywhere, Category = "PlayFab", meta = (EFSecret))
 *         FString DeveloperSecretKey;   // warned about: it would ship in DefaultGame.ini
 *     };
 */
UCLASS(Abstract)
class EXTENDEDFOUNDATION_API UEFDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Extended Framework"); }

	/** Opens this page in Project Settings. Does nothing outside the editor. */
	void OpenInEditor() const;

	/**
	 * Names of properties marked EFSecret that hold a value and are saved to a Default*.ini file.
	 * Always empty outside the editor, where property metadata is not available.
	 */
	TArray<FString> FindShippedSecrets() const;

#if WITH_EDITOR
	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void WarnAboutShippedSecrets() const;
};
