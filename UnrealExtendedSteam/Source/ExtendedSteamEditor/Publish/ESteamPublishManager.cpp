// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Publish/ESteamPublishManager.h"

#include "ExtendedSteamEditor.h"
#include "Publish/ESteamPublishCredentials.h"
#include "Publish/ESteamPublishSettings.h"

#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "ESteamPublishManager"

namespace
{
	/** Absolutize a possibly-relative path against the project directory. */
	FString ResolveAgainstProject(const FString& InPath)
	{
		if (InPath.IsEmpty())
		{
			return FString();
		}
		FString Path = InPath;
		if (FPaths::IsRelative(Path))
		{
			Path = FPaths::Combine(FPaths::ProjectDir(), Path);
		}
		return FPaths::ConvertRelativePathToFull(Path);
	}

	/** Quotes and strips embedded double-quotes so a value is safe inside a VDF "key" "value" pair. */
	FString SanitizeVdfValue(const FString& In)
	{
		return In.Replace(TEXT("\""), TEXT("'"));
	}
}

FString FESteamPublishManager::GetDefaultContentBuilderDirectory()
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT("Config"), TEXT("SteamPublish"), TEXT("ContentBuilder")));
}

FString FESteamPublishManager::ResolveContentBuilderDirectory(const UESteamPublishSettings* Settings)
{
	if (Settings && !Settings->ContentBuilderDirectory.Path.IsEmpty())
	{
		return ResolveAgainstProject(Settings->ContentBuilderDirectory.Path);
	}
	return GetDefaultContentBuilderDirectory();
}

FString FESteamPublishManager::ResolveContentRoot(const UESteamPublishSettings* Settings)
{
	return Settings ? ResolveAgainstProject(Settings->ContentRoot.Path) : FString();
}

FString FESteamPublishManager::GetSteamCmdExecutable(const FString& ContentBuilderDir)
{
#if PLATFORM_WINDOWS
	return FPaths::Combine(ContentBuilderDir, TEXT("builder"), TEXT("steamcmd.exe"));
#elif PLATFORM_MAC
	return FPaths::Combine(ContentBuilderDir, TEXT("builder_osx"), TEXT("steamcmd.sh"));
#else
	return FPaths::Combine(ContentBuilderDir, TEXT("builder_linux"), TEXT("steamcmd.sh"));
#endif
}

FString FESteamPublishManager::FindPluginContentBuilderSource()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UnrealExtendedSteam"));
	if (!Plugin.IsValid())
	{
		return FString();
	}
	return FPaths::Combine(Plugin->GetBaseDir(),
		TEXT("Source"), TEXT("ThirdParty"), TEXT("ExtendedSteamLibrary"),
		TEXT("SDK"), TEXT("tools"), TEXT("ContentBuilder"));
}

void FESteamPublishManager::ExtractContentBuilderTools(UESteamPublishSettings* Settings)
{
	const FString Source = FindPluginContentBuilderSource();
	if (Source.IsEmpty() || !FPaths::DirectoryExists(Source))
	{
		Notify(FString::Printf(TEXT("Could not find bundled ContentBuilder tools at: %s"), *Source), false);
		return;
	}

	const FString Dest = ResolveContentBuilderDirectory(Settings);
	if (!IFileManager::Get().MakeDirectory(*Dest, /*Tree*/ true))
	{
		Notify(FString::Printf(TEXT("Could not create ContentBuilder folder: %s"), *Dest), false);
		return;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.CopyDirectoryTree(*Dest, *Source, /*bOverwriteAllExisting*/ true))
	{
		Notify(FString::Printf(TEXT("Failed to copy ContentBuilder tools to: %s"), *Dest), false);
		return;
	}

	// Make sure scripts/output exist for later VDF generation and build logs.
	IFileManager::Get().MakeDirectory(*FPaths::Combine(Dest, TEXT("scripts")), true);
	IFileManager::Get().MakeDirectory(*FPaths::Combine(Dest, TEXT("output")), true);

	const FString SteamCmd = GetSteamCmdExecutable(Dest);
	const bool bHasSteamCmd = FPaths::FileExists(SteamCmd);

	// Remember the resolved working directory so the other actions can find it.
	if (Settings)
	{
		Settings->ContentBuilderDirectory.Path = Dest;
		Settings->TryUpdateDefaultConfigFile();
	}

	Notify(FString::Printf(TEXT("ContentBuilder extracted to %s (steamcmd %s)."),
		*Dest, bHasSteamCmd ? TEXT("found") : TEXT("MISSING")), bHasSteamCmd);
}

FString FESteamPublishManager::FindMarkerRoot(const FString& MarkerName)
{
	FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Dir);

	for (int32 Depth = 0; Depth < 8; ++Depth)
	{
		if (FPaths::DirectoryExists(FPaths::Combine(Dir, MarkerName)) ||
			FPaths::FileExists(FPaths::Combine(Dir, MarkerName)))
		{
			return Dir;
		}
		const FString Parent = FPaths::GetPath(Dir);
		if (Parent.IsEmpty() || Parent == Dir)
		{
			break;
		}
		Dir = Parent;
	}
	return FString();
}

bool FESteamPublishManager::AddIgnoreEntry(const FString& VcsRoot, const FString& IgnoreFileName, const FString& RelEntry, FString& OutStatus)
{
	const FString IgnorePath = FPaths::Combine(VcsRoot, IgnoreFileName);

	FString Existing;
	FFileHelper::LoadFileToString(Existing, *IgnorePath); // empty if the file does not exist yet

	// Idempotent: skip if any line already references the secrets folder.
	TArray<FString> Lines;
	Existing.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		const FString Trimmed = Line.TrimStartAndEnd().Replace(TEXT("\\"), TEXT("/"));
		if (Trimmed.Contains(TEXT("Config/SteamPublish")))
		{
			OutStatus = FString::Printf(TEXT("%s already ignores Config/SteamPublish."), *IgnoreFileName);
			return false;
		}
	}

	FString Updated = Existing;
	if (!Updated.IsEmpty() && !Updated.EndsWith(TEXT("\n")) && !Updated.EndsWith(LINE_TERMINATOR))
	{
		Updated += LINE_TERMINATOR;
	}
	Updated += FString::Printf(TEXT("# Extended Steam publish credentials + tools (do not commit)%s"), LINE_TERMINATOR);
	Updated += FString::Printf(TEXT("%s%s"), *RelEntry, LINE_TERMINATOR);

	if (!FFileHelper::SaveStringToFile(Updated, *IgnorePath))
	{
		OutStatus = FString::Printf(TEXT("FAILED to write %s"), *IgnorePath);
		return false;
	}

	OutStatus = FString::Printf(TEXT("Added '%s' to %s"), *RelEntry, *IgnoreFileName);
	return true;
}

void FESteamPublishManager::SecureForVersionControl()
{
	// Make sure the folder exists so there is something concrete to ignore.
	IFileManager::Get().MakeDirectory(*FESteamPublishCredentials::GetSecretsDirectory(), /*Tree*/ true);

	// Belt-and-suspenders for git: a nested .gitignore that hides everything except itself.
	{
		const FString NestedGitIgnore = FPaths::Combine(FESteamPublishCredentials::GetSecretsDirectory(), TEXT(".gitignore"));
		if (!FPaths::FileExists(NestedGitIgnore))
		{
			const FString Body = FString::Printf(TEXT("# Everything in this folder is local-only (credentials, tools, build logs).%s*%s!.gitignore%s"),
				LINE_TERMINATOR, LINE_TERMINATOR, LINE_TERMINATOR);
			FFileHelper::SaveStringToFile(Body, *NestedGitIgnore);
		}
	}

	struct FVcs { const TCHAR* Marker; const TCHAR* IgnoreFile; };
	const FVcs VcsList[] =
	{
		{ TEXT(".git"),      TEXT(".gitignore") },
		{ TEXT(".svn"),      TEXT(".svnignore") },
		{ TEXT(".p4config"), TEXT(".p4ignore")  },
		{ TEXT(".p4ignore"), TEXT(".p4ignore")  },
	};

	TArray<FString> Summary;
	TSet<FString> HandledIgnoreFiles;

	for (const FVcs& Vcs : VcsList)
	{
		const FString Root = FindMarkerRoot(Vcs.Marker);
		if (Root.IsEmpty())
		{
			continue;
		}

		// A repo can match more than one marker (e.g. both .p4config and .p4ignore) — only touch each file once.
		const FString IgnoreFullPath = FPaths::Combine(Root, Vcs.IgnoreFile);
		if (HandledIgnoreFiles.Contains(IgnoreFullPath))
		{
			continue;
		}
		HandledIgnoreFiles.Add(IgnoreFullPath);

		// Entry path relative to the VCS root, forward-slashed, trailing slash to ignore the whole folder.
		FString RelEntry = FESteamPublishCredentials::GetSecretsDirectory();
		FPaths::MakePathRelativeTo(RelEntry, *(Root / TEXT("")));
		RelEntry = RelEntry.Replace(TEXT("\\"), TEXT("/"));
		if (!RelEntry.EndsWith(TEXT("/")))
		{
			RelEntry += TEXT("/");
		}

		FString Status;
		AddIgnoreEntry(Root, Vcs.IgnoreFile, RelEntry, Status);
		Summary.Add(Status);
	}

	if (Summary.Num() == 0)
	{
		Notify(TEXT("No version control detected (.git / .svn / .p4config). Nothing to update — add Config/SteamPublish/ to your ignore list manually if needed."), false);
		return;
	}

	Notify(FString::Join(Summary, TEXT("  |  ")), true);
}

bool FESteamPublishManager::GenerateVdfScripts(const UESteamPublishSettings* Settings, const FString& ContentBuilderDir, FString& OutAppBuildVdfPath, FString& OutError)
{
	const FString ScriptsDir = FPaths::Combine(ContentBuilderDir, TEXT("scripts"));
	const FString OutputDir  = FPaths::Combine(ContentBuilderDir, TEXT("output"));
	IFileManager::Get().MakeDirectory(*ScriptsDir, true);
	IFileManager::Get().MakeDirectory(*OutputDir, true);

	const FString ContentRoot = ResolveContentRoot(Settings);

	// Per-depot scripts.
	FString DepotEntries;
	for (const FESteamPublishDepot& Depot : Settings->Depots)
	{
		if (Depot.DepotId <= 0)
		{
			continue;
		}

		const FString DepotFileName = FString::Printf(TEXT("depot_build_%d.vdf"), Depot.DepotId);

		FString DepotVdf;
		DepotVdf += TEXT("\"DepotBuild\"\n{\n");
		DepotVdf += FString::Printf(TEXT("\t\"DepotID\" \"%d\"\n"), Depot.DepotId);
		DepotVdf += TEXT("\t\"FileMapping\"\n\t{\n");
		DepotVdf += FString::Printf(TEXT("\t\t\"LocalPath\" \"%s\"\n"), *SanitizeVdfValue(Depot.LocalPath));
		DepotVdf += FString::Printf(TEXT("\t\t\"DepotPath\" \"%s\"\n"), *SanitizeVdfValue(Depot.DepotPath));
		DepotVdf += FString::Printf(TEXT("\t\t\"Recursive\" \"%d\"\n"), Depot.bRecursive ? 1 : 0);
		DepotVdf += TEXT("\t}\n}\n");

		if (!FFileHelper::SaveStringToFile(DepotVdf, *FPaths::Combine(ScriptsDir, DepotFileName)))
		{
			OutError = FString::Printf(TEXT("Could not write %s"), *DepotFileName);
			return false;
		}

		DepotEntries += FString::Printf(TEXT("\t\t\"%d\" \"%s\"\n"), Depot.DepotId, *DepotFileName);
	}

	if (DepotEntries.IsEmpty())
	{
		OutError = TEXT("No valid depots configured (each depot needs a DepotID >= 1).");
		return false;
	}

	// App build script.
	FString AppVdf;
	AppVdf += TEXT("\"AppBuild\"\n{\n");
	AppVdf += FString::Printf(TEXT("\t\"AppID\" \"%d\"\n"), Settings->AppId);
	AppVdf += FString::Printf(TEXT("\t\"Desc\" \"%s\"\n"), *SanitizeVdfValue(Settings->BuildDescription));
	AppVdf += FString::Printf(TEXT("\t\"Preview\" \"%d\"\n"), Settings->bPreviewBuild ? 1 : 0);
	AppVdf += FString::Printf(TEXT("\t\"ContentRoot\" \"%s\"\n"), *ContentRoot);
	AppVdf += FString::Printf(TEXT("\t\"BuildOutput\" \"%s\"\n"), *OutputDir);
	if (!Settings->bPreviewBuild && !Settings->SetLiveBranch.IsEmpty())
	{
		AppVdf += FString::Printf(TEXT("\t\"SetLive\" \"%s\"\n"), *SanitizeVdfValue(Settings->SetLiveBranch));
	}
	AppVdf += TEXT("\t\"Depots\"\n\t{\n");
	AppVdf += DepotEntries;
	AppVdf += TEXT("\t}\n}\n");

	OutAppBuildVdfPath = FPaths::Combine(ScriptsDir, FString::Printf(TEXT("app_build_%d.vdf"), Settings->AppId));
	if (!FFileHelper::SaveStringToFile(AppVdf, *OutAppBuildVdfPath))
	{
		OutError = FString::Printf(TEXT("Could not write %s"), *OutAppBuildVdfPath);
		return false;
	}

	return true;
}

void FESteamPublishManager::LaunchInteractiveLogin(const UESteamPublishSettings* Settings)
{
	const FString ContentBuilderDir = ResolveContentBuilderDirectory(Settings);
	const FString SteamCmd = GetSteamCmdExecutable(ContentBuilderDir);
	if (!FPaths::FileExists(SteamCmd))
	{
		Notify(TEXT("steamcmd not found. Run 'Extract ContentBuilder Tools' first."), false);
		return;
	}

	const FString User = Settings ? Settings->SteamUsername : FString();
	if (User.IsEmpty())
	{
		Notify(TEXT("Enter a Steam username in the settings first, then click Login & Authorize."), false);
		return;
	}

	const FString Args = FString::Printf(TEXT("+login \"%s\" +quit"), *User);
	const FString WorkingDir = FPaths::GetPath(SteamCmd);

	// Detached + visible so steamcmd gets its own console for the password + Steam Guard prompt.
	FProcHandle Handle = FPlatformProcess::CreateProc(
		*SteamCmd, *Args,
		/*bLaunchDetached*/ true, /*bLaunchHidden*/ false, /*bLaunchReallyHidden*/ false,
		nullptr, 0, *WorkingDir, nullptr, nullptr);

	if (!Handle.IsValid())
	{
		Notify(FString::Printf(TEXT("Failed to launch steamcmd: %s"), *SteamCmd), false);
		return;
	}
	FPlatformProcess::CloseProc(Handle);

	Notify(TEXT("steamcmd launched in a console window — enter your password and Steam Guard code there to authorize this machine."), true);
}

void FESteamPublishManager::PublishBuildAsync(const UESteamPublishSettings* Settings)
{
	if (!Settings)
	{
		return;
	}

	const FString ContentBuilderDir = ResolveContentBuilderDirectory(Settings);
	const FString SteamCmd = GetSteamCmdExecutable(ContentBuilderDir);
	if (!FPaths::FileExists(SteamCmd))
	{
		Notify(TEXT("steamcmd not found. Run 'Extract ContentBuilder Tools' first."), false);
		return;
	}

	if (Settings->AppId <= 0)
	{
		Notify(TEXT("Set a valid Steam App ID before publishing."), false);
		return;
	}

	const FString ContentRoot = ResolveContentRoot(Settings);
	if (ContentRoot.IsEmpty() || !FPaths::DirectoryExists(ContentRoot))
	{
		Notify(FString::Printf(TEXT("ContentRoot folder does not exist: %s"), *ContentRoot), false);
		return;
	}

	const FString User = Settings->SteamUsername;
	const FString Pass = Settings->SteamPassword;
	if (User.IsEmpty() || Pass.IsEmpty())
	{
		Notify(TEXT("Username and password are required for a non-interactive publish. Fill them in and click 'Save Credentials' first."), false);
		return;
	}

	FString AppBuildVdf;
	FString Error;
	if (!GenerateVdfScripts(Settings, ContentBuilderDir, AppBuildVdf, Error))
	{
		Notify(Error, false);
		return;
	}

	const bool bPreview = Settings->bPreviewBuild;
	const int32 AppId = Settings->AppId;
	const FString WorkingDir = FPaths::GetPath(SteamCmd);
	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y.%m.%d-%H.%M.%S"));
	const FString BuildLogPath = FPaths::Combine(ContentBuilderDir, TEXT("output"),
		FString::Printf(TEXT("publish_%d_%s.log"), AppId, *Timestamp));

	const FString Args = FString::Printf(TEXT("+login \"%s\" \"%s\" +run_app_build \"%s\" +quit"),
		*User, *Pass, *AppBuildVdf);

	Notify(FString::Printf(TEXT("Publishing app %d (%s)... watch the Output Log; a toast will report the result."),
		AppId, bPreview ? TEXT("PREVIEW") : TEXT("LIVE UPLOAD")), true);

	UE_LOG(LogExtendedSteamEditor, Display, TEXT("SteamPublish: running steamcmd for app %d (preview=%d). VDF: %s"),
		AppId, bPreview ? 1 : 0, *AppBuildVdf);

	// steamcmd can take minutes; run off the game thread so the editor stays responsive.
	Async(EAsyncExecution::Thread, [SteamCmd, Args, WorkingDir, BuildLogPath, AppId, bPreview]()
	{
		FString Output;
		int32 ExitCode = -1;
		bool bTimedOut = false;

		void* PipeRead = nullptr;
		void* PipeWrite = nullptr;
		if (!FPlatformProcess::CreatePipe(PipeRead, PipeWrite))
		{
			AsyncTask(ENamedThreads::GameThread, [](){ Notify(TEXT("Failed to create steamcmd output pipe."), false); });
			return;
		}

		FProcHandle Handle = FPlatformProcess::CreateProc(
			*SteamCmd, *Args,
			/*bLaunchDetached*/ false, /*bLaunchHidden*/ true, /*bLaunchReallyHidden*/ true,
			nullptr, 0, *WorkingDir, PipeWrite, PipeRead);

		if (!Handle.IsValid())
		{
			FPlatformProcess::ClosePipe(PipeRead, PipeWrite);
			AsyncTask(ENamedThreads::GameThread, [](){ Notify(TEXT("Failed to launch steamcmd."), false); });
			return;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		const double TimeoutSeconds = 30.0 * 60.0; // 30 minutes
		while (FPlatformProcess::IsProcRunning(Handle))
		{
			Output += FPlatformProcess::ReadPipe(PipeRead);
			if ((FPlatformTime::Seconds() - StartSeconds) >= TimeoutSeconds)
			{
				FPlatformProcess::TerminateProc(Handle, true);
				bTimedOut = true;
				break;
			}
			FPlatformProcess::Sleep(0.1f);
		}
		Output += FPlatformProcess::ReadPipe(PipeRead);

		FPlatformProcess::GetProcReturnCode(Handle, &ExitCode);
		FPlatformProcess::CloseProc(Handle);
		FPlatformProcess::ClosePipe(PipeRead, PipeWrite);

		FFileHelper::SaveStringToFile(Output, *BuildLogPath);

		const bool bSuccess = !bTimedOut && ExitCode == 0;
		FString Message;
		if (bTimedOut)
		{
			Message = FString::Printf(TEXT("steamcmd timed out. If this is a new machine, run 'Login & Authorize' once for Steam Guard. Log: %s"), *BuildLogPath);
		}
		else if (bSuccess)
		{
			Message = FString::Printf(TEXT("%s for app %d succeeded. Log: %s"),
				bPreview ? TEXT("Preview build") : TEXT("Build upload"), AppId, *BuildLogPath);
		}
		else
		{
			Message = FString::Printf(TEXT("steamcmd failed (exit %d). Common cause: Steam Guard not yet authorized on this machine — run 'Login & Authorize'. Log: %s"),
				ExitCode, *BuildLogPath);
		}

		UE_LOG(LogExtendedSteamEditor, Display, TEXT("SteamPublish: %s\n%s"), *Message, *Output);
		AsyncTask(ENamedThreads::GameThread, [Message, bSuccess]() { Notify(Message, bSuccess); });
	});
}

void FESteamPublishManager::Notify(const FString& Message, bool bSuccess)
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [Message, bSuccess]() { Notify(Message, bSuccess); });
		return;
	}

	if (bSuccess)
	{
		UE_LOG(LogExtendedSteamEditor, Display, TEXT("%s"), *Message);
	}
	else
	{
		UE_LOG(LogExtendedSteamEditor, Warning, TEXT("%s"), *Message);
	}

	FNotificationInfo Info(FText::FromString(Message));
	Info.ExpireDuration = bSuccess ? 6.0f : 9.0f;
	Info.bFireAndForget = true;
	Info.bUseSuccessFailIcons = true;

	const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	if (Item.IsValid())
	{
		Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
}

#undef LOCTEXT_NAMESPACE
