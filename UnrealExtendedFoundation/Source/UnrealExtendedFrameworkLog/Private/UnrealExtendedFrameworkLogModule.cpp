// Copyright Moon Punch Games. All Rights Reserved.

#include "EFLog.h"
#include "EFLogSettings.h"
#include "EFLogWriter.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

class FUnrealExtendedFrameworkLogModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if EF_LOG_ENABLED
		// Taking the folder here rather than at the first EF_LOG line is what archives the previous
		// session's files even in a session that never logs. A module that logs before this runs is
		// still fine: the first line starts the writer on its own.
		FEFLogWriter::Get().Startup();

		// Categories created before this point started at the default threshold; this also brings
		// them to their configured ones.
		GetDefault<UEFLogSettings>()->ApplyToRuntime();

		// A crash is exactly when the last lines matter most.
		SystemErrorHandle = FCoreDelegates::OnHandleSystemError.AddStatic(&FUnrealExtendedFrameworkLogModule::HandleSystemError);

		// A forced exit terminates the process without ShutdownModule; this is its last call.
		WillTerminateHandle = FCoreDelegates::GetApplicationWillTerminateDelegate().AddStatic(&FUnrealExtendedFrameworkLogModule::HandleApplicationWillTerminate);
#endif
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::OnHandleSystemError.Remove(SystemErrorHandle);
		FCoreDelegates::GetApplicationWillTerminateDelegate().Remove(WillTerminateHandle);
		FEFLogWriter::Get().Shutdown();
	}

private:
	static void HandleSystemError()
	{
		FEFLogWriter::Get().FlushForCrash();
	}

	static void HandleApplicationWillTerminate()
	{
		FEFLogWriter::Get().FlushBeforeExit();
	}

	FDelegateHandle SystemErrorHandle;
	FDelegateHandle WillTerminateHandle;
};

IMPLEMENT_MODULE(FUnrealExtendedFrameworkLogModule, UnrealExtendedFrameworkLog)
