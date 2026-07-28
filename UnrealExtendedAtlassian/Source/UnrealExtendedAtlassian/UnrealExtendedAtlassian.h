// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FExtendedAtlassianClient;

/**
 * Transport module for the Extended Atlassian integration.
 *
 * Owns the REST client, credential store and document-format conversion, and deliberately
 * carries no UnrealEd dependency so the same transport can back an in-game bug reporter in
 * playtest builds later without restructuring. All editor UI lives in
 * UnrealExtendedAtlassianEditor.
 */
class UNREALEXTENDEDATLASSIAN_API FUnrealExtendedAtlassianModule : public IModuleInterface
{
public:
	virtual ~FUnrealExtendedAtlassianModule() override;
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FUnrealExtendedAtlassianModule* Get();

	/** The shared REST client. Null before startup and after shutdown. */
	static TSharedPtr<FExtendedAtlassianClient> GetClient();

private:
	TSharedPtr<FExtendedAtlassianClient> Client;

	static FUnrealExtendedAtlassianModule* Instance;
};
