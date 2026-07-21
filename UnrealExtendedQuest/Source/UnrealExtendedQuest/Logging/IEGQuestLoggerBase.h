// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/LogCategory.h"

#include "UnrealExtendedQuest/NYEngineVersionHelpers.h"

#include "IEGQuestLoggerBase.generated.h"

class FOutputDevice;
class APlayerController;
class FMessageLogModule;
class IMessageLogListing;


/**
 * Options for setting up a message log's UI
 */
struct FEGQuestMessageLogInitializationOptions
{
	FEGQuestMessageLogInitializationOptions() {}

	// Whether to show the filters menu
	bool bShowFilters = true;

	// Whether to initially  show the pages widget. Setting this to false will allow the user to manually clear the log.
	// If this is not set & NewPage() is called on the log, the pages widget will show itself
	bool bShowPages = false;

	// Whether to allow the user to clear this log.
	bool bAllowClear = true;

	/// Whether to check for duplicate messages & discard them
	bool bDiscardDuplicates = false;

	// The maximum number of pages this log can hold. Pages are managed in a first in, last out manner
	uint32 MaxPageCount = 20;

	// Whether to show this log in the main log window
	bool bShowInLogWindow = true;
};

UENUM()
enum class EEGQuestLogLevel : uint8
{
	NoLogging = 0,

	// Kills the program
	// TODO use
	// Fatal,

	Error,
	Warning,

	// Log
	Info,
	//Log = Info,

	// Verbose
	Debug,
	//Verbose = Debug,

	// VeryVerbose
	// Used for
	Trace,
	//VeryVerbose = Trace
};


/**
 * The following output are available:
 * - output log, also file on your filesystem which corresponds to the output log
 * - message log, only available in editor
 * - in the client console, press ~ (tilde) to see it
 * - on screen
 */
class UNREALEXTENDEDQUEST_API IEGQuestLoggerBase
{
	typedef IEGQuestLoggerBase Self;
protected:
	IEGQuestLoggerBase() {}

public:
	virtual ~IEGQuestLoggerBase() {}

	// Create a new logger
	static IEGQuestLoggerBase New() { return Self{}; }
	static IEGQuestLoggerBase& Get()
	{
		static IEGQuestLoggerBase Instance;
		return Instance;
	}

	// Exclusive enables
	Self& OnlyEnableClientConsole(APlayerController* PC)
	{
		EnableClientConsole(PC);
		DisableOnScreen();
		DisableMessageLog();
		DisableOutputLog();
		return *this;
	}
	Self& OnlyEnableOnScreen(bool bInForceEnableScreenMessages = false)
	{
		EnableOnScreen(bInForceEnableScreenMessages);
		DisableClientConsole();
		DisableMessageLog();
		DisableOutputLog();
		return *this;
	}
	Self& OnlyEnableOutputLog()
	{
		EnableOutputLog();
		DisableOnScreen();
		DisableClientConsole();
		DisableMessageLog();
		return *this;
	}

	Self& OnlyEnableMessageLog(bool bSuppressLoggingToOutputLog = false)
	{
		EnableMessageLog(bSuppressLoggingToOutputLog);
		DisableOnScreen();
		DisableClientConsole();
		DisableOutputLog();
		return *this;
	}


	//
	// Client console
	//

	Self& EnableClientConsole(APlayerController* PC)
	{
		UseClientConsole(true);
		SetClientConsolePlayerController(PC);
		return *this;
	}
	Self& DisableClientConsole() { return UseClientConsole(false); }
	Self& UseClientConsole(bool bValue)
	{
		bClientConsole = bValue;
		return *this;
	}
	Self& SetClientConsolePlayerController(APlayerController* PC);

	//
	// On screen
	//

	// bInForceEnableScreenMessages - if true, even if the screen messages are disabled we will force display it
	Self& EnableOnScreen(bool bInForceEnableScreenMessages = false) { return UseOnScreen(true, bInForceEnableScreenMessages); }
	Self& DisableOnScreen() { return UseOnScreen(false); }
	Self& UseOnScreen(bool bValue, bool bInForceEnableScreenMessages = false)
	{
		bOnScreen = bValue;
		bForceEnableScreenMessages = bInForceEnableScreenMessages;
		return *this;
	}

	// How long to display the on screen log messages
	Self& SetOnScreenTimeToDisplay(float Seconds)
	{
		ScreenLogDisplayTimeSeconds = Seconds;
		return *this;
	}

	// Should newer messages appear on top
	Self& SetOnScreenNewerOnTop(bool bValue)
	{
		bScreenNewerOnTop = bValue;
		return *this;
	}

	// Clears all the on screen messages
	static void ClearAllOnScreenLogs();
	FORCEINLINE static bool AreAllOnScreenMessagesEnabled() { return GAreScreenMessagesEnabled; }
	FORCEINLINE static void SetAreAllOnScreenMessagesEnabled(bool bValue)
	{
		GAreScreenMessagesEnabled = bValue;
	}
	FORCEINLINE static void DisableAllOnScreenMessages() { SetAreAllOnScreenMessagesEnabled(false); }
	FORCEINLINE static void EnableAllOnScreenMessages() { SetAreAllOnScreenMessagesEnabled(true); }


	//
	// Output log
	//

	Self& EnableOutputLog() { return UseOutputLog(true); }
	Self& DisableOutputLog() { return UseOutputLog(false); }
	Self& UseOutputLog(bool bValue)
	{
		bOutputLog = bValue;
		return *this;
	}

	// The log category must exist
	Self& SetNoOutputLogCategory() { return SetOutputLogCategory(NAME_None); }
	Self& SetOutputLogCategory(const FLogCategoryBase& NewCategory) { return SetOutputLogCategory(NewCategory.GetCategoryName()); }
	Self& SetOutputLogCategory(FName NewCategory)
	{
		PreviousOutputLogCategory = OutputLogCategory;
		OutputLogCategory = NewCategory;
		return *this;
	}

	// Special case for no logging, aka shipping build
#if NO_LOGGING
	Self& SetOutputLogCategory(FNoLoggingCategory NoLogging)
	{
		OutputLogCategory = NAME_None;
		bOutputLog = false;
		return *this;
	}
#endif // NO_LOGGING

	//
	// Message log
	//

	Self& EnableMessageLog(bool bSuppressLoggingToOutputLog = false) { return UseMessageLog(true, bSuppressLoggingToOutputLog); }
	Self& DisableMessageLog() { return UseMessageLog(true); }
	Self& UseMessageLog(bool bValue, bool bInMessageLogMirrorToOutputLog = true)
	{
		bMessageLog = bValue;
		return SetMessageLogMirrorToOutputLog(bInMessageLogMirrorToOutputLog);
	}

	// Opens the log for display to the user given certain conditions.
	// Set filter with SetOpenMessageLogLevelsHigherThan
	Self& SetMessageLogOpenOnNewMessage(bool bValue)
	{
		bMessageLogOpen = bValue;
		return *this;
	}

	// Should we mirror message log messages from this instance to the output log during flush?
	Self& SetMessageLogMirrorToOutputLog(bool bValue)
	{
		bMessageLogMirrorToOutputLog = bValue;
		return *this;
	}

	Self& DisableRedirectMessageLogLevels() { return SetRedirectMessageLogLevelsHigherThan(EEGQuestLogLevel::NoLogging); }
	Self& SetRedirectMessageLogLevelsHigherThan(EEGQuestLogLevel AfterOrEqualLevel)
	{
		RedirectMessageLogLevelsHigherThan = AfterOrEqualLevel;
		return *this;
	}

	// Only useful if bMessageLogOpen is set to true
	Self& SetOpenMessageLogLevelsHigherThan(EEGQuestLogLevel AfterOrEqualLevel)
	{
		OpenMessageLogLevelsHigherThan = AfterOrEqualLevel;
		return *this;
	}

	static bool IsMessageLogNameRegistered(FName LogName);
	static bool MessageLogUnregisterLogName(FName LogName);
	static void MessageLogRegisterLogName(FName LogName, const FText& LogLabel, const FEGQuestMessageLogInitializationOptions& InitOptions = {});
#if WITH_UNREAL_DEVELOPER_TOOLS
	static TSharedPtr<IMessageLogListing> MessageLogGetLogNameListing(FName LogName);
#endif // WITH_UNREAL_DEVELOPER_TOOLS
	static void MessageLogOpenLogName(FName LogName);


	// Registers the new Message log name
	// NOTE: Call MessageLogRegisterLogName before calling this
	Self& SetMessageLogName(FName LogName, bool bVerify = true)
	{
#if WITH_UNREAL_DEVELOPER_TOOLS
		if (bVerify && !IsMessageLogNameRegistered(LogName))
		{
			Warning(TEXT("SetMessageLogName: Failed to register the message log name"));
		}
#endif // WITH_UNREAL_DEVELOPER_TOOLS

		MessageLogName = LogName;
		return *this;
	}

	//
	// Public accessors
	//

	FORCEINLINE FName GetOutputLogCategory() const { return OutputLogCategory; }
	FORCEINLINE bool IsClientConsoleEnabled() const { return bClientConsole; }
	FORCEINLINE bool IsOnScreenEnabled() const { return bOnScreen; }
	FORCEINLINE bool IsOutputLogEnabled() const { return bOutputLog; }
	FORCEINLINE bool IsMessageLogEnabled() const { return bMessageLog; }

	template <typename FmtType, typename... Types>
	void Logf(EEGQuestLogLevel Level, const FmtType& Fmt, Types... Args)
	{
#if NY_ENGINE_VERSION >= 501
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
#else
		static_assert(TIsArrayOrRefOfType<FmtType, TCHAR>::Value, "Formatting string must be a TCHAR array.");
#endif
		static_assert(TAnd<TIsValidVariadicFunctionArg<Types>...>::Value, "Invalid argument(s) passed to IEGQuestLoggerBase::Logf");
		LogfImplementation(Level, Fmt, Args...);
	}

	template <typename FmtType, typename... Types>
	void Errorf(const FmtType& Fmt, Types... Args) { Logf(EEGQuestLogLevel::Error, Fmt, Args...); }

	template <typename FmtType, typename... Types>
	void Warningf(const FmtType& Fmt, Types... Args) { Logf(EEGQuestLogLevel::Warning, Fmt, Args...); }

	template <typename FmtType, typename... Types>
	void Infof(const FmtType& Fmt, Types... Args) { Logf(EEGQuestLogLevel::Info, Fmt, Args...); }

	template <typename FmtType, typename... Types>
	void Debugf(const FmtType& Fmt, Types... Args) { Logf(EEGQuestLogLevel::Debug, Fmt, Args...); }

	template <typename FmtType, typename... Types>
	void Tracef(const FmtType& Fmt, Types... Args) { Logf(EEGQuestLogLevel::Trace, Fmt, Args...); }


	// void Fatal(const ANSICHAR* File, int32 Line, const FString& Message);
	void Log(EEGQuestLogLevel Level, const FString& Message);

	// TODO implement
	// void Fatal(const FString& Message) { Log(EEGQuestLogLevel::Fatal, Message); }
	FORCEINLINE void Error(const FString& Message) { Log(EEGQuestLogLevel::Error, Message); }
	FORCEINLINE void Warning(const FString& Message) { Log(EEGQuestLogLevel::Warning, Message); }
	FORCEINLINE void Info(const FString& Message) { Log(EEGQuestLogLevel::Info, Message); }
	FORCEINLINE void Debug(const FString& Message) { Log(EEGQuestLogLevel::Debug, Message); }
	FORCEINLINE void Trace(const FString& Message) { Log(EEGQuestLogLevel::Trace, Message); }

protected:
	void VARARGS LogfImplementation(EEGQuestLogLevel Level, const TCHAR* Fmt, ...);

#if WITH_UNREAL_DEVELOPER_TOOLS
	static FMessageLogModule* GetMessageLogModule();
#endif // WITH_UNREAL_DEVELOPER_TOOLS

	virtual void LogScreen(EEGQuestLogLevel Level, const FString& Message);
	virtual void LogOutputLog(EEGQuestLogLevel Level, const FString& Message);
	virtual void LogMessageLog(EEGQuestLogLevel Level, const FString& Message);
	virtual void LogClientConsole(EEGQuestLogLevel Level, const FString& Message);

	static ELogVerbosity::Type GetUnrealLogTypeForLogLevel(EEGQuestLogLevel Level)
	{
	 	switch (Level)
		{
		// case EEGQuestLogLevel::Fatal:
		// 	return ELogVerbosity::Fatal;

		case EEGQuestLogLevel::Error:
			return ELogVerbosity::Error;

		case EEGQuestLogLevel::Warning:
			return ELogVerbosity::Warning;

		case EEGQuestLogLevel::Debug:
			return ELogVerbosity::Verbose;

		case EEGQuestLogLevel::Trace:
			return ELogVerbosity::VeryVerbose;

		default:
			return ELogVerbosity::Log;
		}
	}
	static EMessageSeverity::Type GetMessageSeverityForLogLevel(EEGQuestLogLevel Level)
	{
		switch (Level)
		{
		// case EEGQuestLogLevel::Fatal:
		// 	return EMessageSeverity::CriticalError;

		case EEGQuestLogLevel::Error:
			return EMessageSeverity::Error;

		case EEGQuestLogLevel::Warning:
			return EMessageSeverity::Warning;

		default:
			return EMessageSeverity::Info;
		}
	}

	FColor GetColorForLogLevel(EEGQuestLogLevel Level) const
	{
		switch (Level)
		{
		// case EEGQuestLogLevel::Fatal:
		// 	return ColorFatal;

		case EEGQuestLogLevel::Error:
			return ColorError;

		case EEGQuestLogLevel::Warning:
			return ColorWarning;

		case EEGQuestLogLevel::Debug:
			return ColorDebug;

		case EEGQuestLogLevel::Trace:
			return ColorTrace;

		default:
			return ColorInfo;
		}
	}
	static FOutputDevice* GetOutputDeviceFromLogLevel(EEGQuestLogLevel Level);

protected:
	//
	// On screen
	//

	// Output to the screen
	bool bOnScreen = false;

	// Time to stay on screen
	float ScreenLogDisplayTimeSeconds = 5.f;

	// How to scale the text for the on screen messages
	FVector2D ScreenTextScale = FVector2D::UnitVector;

	// Newer screen messages appear on top
	bool bScreenNewerOnTop = true;

	// If bScreen == true and this is true, we force enable the screen messages
	bool bForceEnableScreenMessages = false;

	//
	// Output log
	//

	// Output to the output log and log file
	bool bOutputLog = false;

	// Category for output log
	FName OutputLogCategory = NAME_None;

	// Useful when temporarily using another category
	FName PreviousOutputLogCategory = NAME_None;

	//
	// Message log
	//

	// Output to the message log
	bool bMessageLog = true;

	// Category for message log
	FName MessageLogName = TEXT("PIE");

	// Should we mirror message log messages from this instance to the output log during flush?
	bool bMessageLogMirrorToOutputLog = true;

	// By default the message log does not support debug output, latest is info.
	// For the sake of sanity we redirect all levels higher than RedirectMessageLogLevelsHigherThan to the output log
	// even if the output log is disabled.
	// So that not to output for example debug output to the message log only to the output log.
	// NOTE: A value of EEGQuestLogLevel::NoLogging means no log level will get redirected
	EEGQuestLogLevel RedirectMessageLogLevelsHigherThan = EEGQuestLogLevel::Warning;

	// Opens the log for display to the user given certain conditions.
	// See OpenMessageLogLevelsHigherThan for the filter
	bool bMessageLogOpen = true;

	// All the log levels messages that will open the message log window if bMessageLogOpen is true
	// NOTE: A value of  EEGQuestLogLevel::NoLogging means all log levels will be opened if bMessageLogOpen is true
	EEGQuestLogLevel OpenMessageLogLevelsHigherThan = EEGQuestLogLevel::NoLogging;

	//
	// Client console
	//

	// Output to the dropdown in game console, requires the PlayerController to be set
	bool bClientConsole = false;

	// Required to print to client console
	APlayerController* PlayerController = nullptr;

	//
	// Colors
	//
	//
	FColor ColorFatal = FColor::Red;
	FColor ColorError = FColor::Red;
	FColor ColorWarning = FColor::Yellow;
	FColor ColorInfo = FColor::White;
	FColor ColorDebug = FColor::Blue;
	FColor ColorTrace = FColor::Cyan;
};
