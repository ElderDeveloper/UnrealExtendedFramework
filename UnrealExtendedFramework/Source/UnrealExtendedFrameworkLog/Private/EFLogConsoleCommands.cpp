// Copyright Moon Punch Games. All Rights Reserved.

#include "EFLog.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDevice.h"

namespace EFLogConsoleCommandsPrivate
{
	static void ListCategories(FOutputDevice& Output)
	{
		const TArray<FString> Names = EFLog::GetCategoryNames();
		if (Names.Num() == 0)
		{
			Output.Log(TEXT("EF.Log.Verbosity: no EF_LOG category exists yet."));
			return;
		}

		for (const FString& Name : Names)
		{
			EEFLogVerbosity Verbosity = EEFLogVerbosity::Log;
			EFLog::GetCategoryVerbosity(FName(*Name), Verbosity);
			Output.Logf(TEXT("  %s = %s"), *Name, LexToString(Verbosity));
		}
	}

	static void HandleVerbosity(const TArray<FString>& Args, FOutputDevice& Output)
	{
		if (Args.Num() == 0)
		{
			ListCategories(Output);
			return;
		}

		if (Args.Num() != 2)
		{
			Output.Log(TEXT("Usage: EF.Log.Verbosity [<Category>|* <Error|Warning|Display|Log|Verbose|VeryVerbose>]"));
			return;
		}

		EEFLogVerbosity Verbosity = EEFLogVerbosity::Log;
		if (!EFLog::ParseVerbosity(Args[1], Verbosity))
		{
			Output.Logf(TEXT("EF.Log.Verbosity: unknown level '%s'."), *Args[1]);
			return;
		}

		if (Args[0] == TEXT("*"))
		{
			EFLog::SetAllCategoriesVerbosity(Verbosity);
			Output.Logf(TEXT("EF.Log.Verbosity: every category is now %s."), LexToString(Verbosity));
			return;
		}

		EFLog::SetCategoryVerbosity(FName(*Args[0]), Verbosity);
		Output.Logf(TEXT("EF.Log.Verbosity: %s is now %s."), *EFLog::SanitizeCategoryName(Args[0]), LexToString(Verbosity));
	}

	// Runtime only: this changes the running session, not the project's settings.
	static FAutoConsoleCommandWithArgsAndOutputDevice VerbosityCommand(
		TEXT("EF.Log.Verbosity"),
		TEXT("Shows or sets EF_LOG category thresholds for this session. ")
		TEXT("EF.Log.Verbosity lists them; EF.Log.Verbosity <Category> <Level> sets one; EF.Log.Verbosity * <Level> sets all."),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(&HandleVerbosity));
}
