// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IEGQuestLoggerBase.h"


class UNREALEXTENDEDQUEST_API FEGQuestLogger : public IEGQuestLoggerBase
{
	typedef FEGQuestLogger Self;
	typedef IEGQuestLoggerBase Super;

protected:
	FEGQuestLogger();
	
public:
	virtual ~FEGQuestLogger() {}

	// Sync values with system UEGQuestPluginSettings values
	Self& SyncWithSettings();
	
	// Create a new logger
	static FEGQuestLogger New() { return Self{}; }
	static FEGQuestLogger& Get()
	{
		static FEGQuestLogger Instance;
		return Instance;
	}

	static void OnStart();
	static void OnShutdown();
};
