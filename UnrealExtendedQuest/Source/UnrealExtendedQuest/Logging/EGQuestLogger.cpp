// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestLogger.h"
#include "UnrealExtendedQuest/EGQuestPluginModule.h"
#include "UnrealExtendedQuest/EGQuestPluginSettings.h"

#define LOCTEXT_NAMESPACE "QuestLogger"

static const FName MESSAGE_LOG_NAME{TEXT("Quest Plugin")};

FEGQuestLogger::FEGQuestLogger() : Super()
{
	static constexpr bool bOwnMessageLogMirrorToOutputLog = true;
	EnableMessageLog(bOwnMessageLogMirrorToOutputLog);
	SetMessageLogMirrorToOutputLog(true);

	DisableOutputLog();
	DisableOnScreen();
	DisableClientConsole();

	// We mirror everything to the output log so that is why we disabled the output log above
	SetOutputLogCategory(LogEGQuestPlugin);
	SetMessageLogName(MESSAGE_LOG_NAME, false);
	SetMessageLogOpenOnNewMessage(true);
	SetRedirectMessageLogLevelsHigherThan(EEGQuestLogLevel::Warning);
	SetOpenMessageLogLevelsHigherThan(EEGQuestLogLevel::NoLogging);
}

FEGQuestLogger& FEGQuestLogger::SyncWithSettings()
{
	const UEGQuestPluginSettings* Settings = GetDefault<UEGQuestPluginSettings>();

	UseMessageLog(Settings->bEnableMessageLog);
	SetMessageLogMirrorToOutputLog(Settings->bMessageLogMirrorToOutputLog);
	UseOutputLog(Settings->bEnableOutputLog);
	SetRedirectMessageLogLevelsHigherThan(Settings->RedirectMessageLogLevelsHigherThan);
	SetOpenMessageLogLevelsHigherThan(Settings->OpenMessageLogLevelsHigherThan);
	SetMessageLogOpenOnNewMessage(Settings->bMessageLogOpen);

	return *this;
}

void FEGQuestLogger::OnStart()
{
	MessageLogRegisterLogName(MESSAGE_LOG_NAME, LOCTEXT("quest_key", "Quest System Plugin"));
	Get().SyncWithSettings();
}

void FEGQuestLogger::OnShutdown()
{
	MessageLogUnregisterLogName(MESSAGE_LOG_NAME);
}

#undef  LOCTEXT_NAMESPACE
