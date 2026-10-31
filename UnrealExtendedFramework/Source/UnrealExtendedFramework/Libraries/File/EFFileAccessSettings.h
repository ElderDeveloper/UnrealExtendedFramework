// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Settings/EFDeveloperSettings.h"
#include "EFFileAccessSettings.generated.h"

/** When the Extended file library limits the paths it reads and writes. */
UENUM(BlueprintType)
enum class EEFFileAccessRestriction : uint8
{
	/** Every path is allowed in every build. */
	Off,
	/** Paths are limited in Shipping builds only. The editor, Development and Test builds allow every path. */
	ShippingOnly UMETA(DisplayName = "Shipping Builds Only"),
	/** Paths are limited in every build, the editor included. */
	Always
};

/**
 * Project Settings > Extended Framework > Extended File Access.
 *
 * Limits the paths the Extended file library (UEFFileLibrary) and the other Extended nodes that
 * take a file path may use, so a shipped game's Blueprints cannot read or overwrite arbitrary files
 * on the player's machine. While the restriction applies:
 * - reading, listing and querying are allowed under the project directory, the project Saved
 *   directory, the project user directory, the platform user and user settings directories, and
 *   AdditionalAllowedDirectories;
 * - writing, moving, copying to, renaming, deleting, changing the read-only flag and creating or
 *   removing directories are allowed only under the project Saved directory and
 *   AdditionalAllowedDirectories.
 *
 * Paths are made absolute and normalised before they are checked, so ".." cannot leave an allowed
 * directory. A refused call fails the way the node already reports failure (false, an empty result
 * or an error string) and logs one warning naming the function and the reason.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Extended File Access"))
class UNREALEXTENDEDFRAMEWORK_API UEFFileAccessSettings : public UEFDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * When the restriction applies. Shipping Builds Only (the default) keeps editor tools and
	 * development builds unrestricted and protects players of the shipped game.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Access")
	EEFFileAccessRestriction RestrictionMode = EEFFileAccessRestriction::ShippingOnly;

	/**
	 * Further directories that may be read and written while the restriction applies. A relative
	 * path is relative to the project directory; an absolute path is used as it is. File system
	 * roots such as / or C:/ are ignored: set the mode to Off to allow every path.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Access")
	TArray<FString> AdditionalAllowedDirectories;

	/** True when the restriction applies to this build. */
	bool IsRestrictionActive() const;
};
