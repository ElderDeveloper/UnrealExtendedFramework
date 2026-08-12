// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSDevAuthTool.h"

#include "Shared/EEOSLog.h"

#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

const TCHAR* FEEOSDevAuthTool::ToString(EEnvironment Environment)
{
	return Environment == EEnvironment::GameDev ? TEXT("gamedev") : TEXT("prod");
}

FString FEEOSDevAuthTool::FindToolExecutable()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UnrealExtendedEOS"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("DevAuthTool: plugin 'UnrealExtendedEOS' not found — cannot locate the tool."));
		return FString();
	}

	const FString ToolsDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Thirdparty"), TEXT("EOS"), TEXT("SDK"), TEXT("Tools"));

	// The tool ships in a version-stamped folder (EOS_DevAuthTool-win32-x64-1.2.1). Discover it
	// rather than hardcoding, so bumping the SDK does not silently disable the launcher.
	TArray<FString> CandidateDirs;
	IFileManager::Get().FindFiles(CandidateDirs, *(ToolsDir / TEXT("EOS_DevAuthTool*")), false, true);

	// Newest-looking first: the names sort lexicographically by version well enough for this,
	// and a single entry is the overwhelmingly common case.
	CandidateDirs.Sort([](const FString& A, const FString& B) { return A > B; });

	for (const FString& Dir : CandidateDirs)
	{
		const FString ExePath = ToolsDir / Dir / TEXT("EOS_DevAuthTool.exe");
		if (FPaths::FileExists(ExePath))
		{
			return FPaths::ConvertRelativePathToFull(ExePath);
		}
	}

	UE_LOG(LogExtendedEOS, Warning, TEXT("DevAuthTool: no EOS_DevAuthTool.exe under '%s'."), *ToolsDir);
	return FString();
}

bool FEEOSDevAuthTool::IsToolListening(int32 Port, int32 TimeoutMs)
{
	if (Port < 1024 || Port > 65535)
	{
		return false;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		return false;
	}

	const TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
	bool bAddrValid = false;
	Addr->SetIp(TEXT("127.0.0.1"), bAddrValid);
	if (!bAddrValid)
	{
		return false;
	}
	Addr->SetPort(Port);

	FSocket* Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("EEOSDevAuthProbe"), Addr->GetProtocolType());
	if (!Socket)
	{
		return false;
	}

	// Non-blocking connect + bounded wait: a blocking connect to a dead local port is fast on
	// Windows, but this runs on the game thread and must never be able to stall the editor.
	Socket->SetNonBlocking(true);
	Socket->Connect(*Addr);
	const bool bConnected = Socket->Wait(ESocketWaitConditions::WaitForWrite, FTimespan::FromMilliseconds(TimeoutMs));

	Socket->Close();
	SocketSubsystem->DestroySocket(Socket);
	return bConnected;
}

bool FEEOSDevAuthTool::LaunchTool(EEnvironment Environment)
{
	const FString ExePath = FindToolExecutable();
	if (ExePath.IsEmpty())
	{
		return false;
	}

	// "-gamedev" is the tool's own switch for the non-prod account service; it also selects
	// which credentials_<env>.json it reads and writes.
	const FString Args = (Environment == EEnvironment::GameDev) ? TEXT("-gamedev") : TEXT("");
	const FString WorkingDir = FPaths::GetPath(ExePath);

	FProcHandle Handle = FPlatformProcess::CreateProc(
		*ExePath, *Args,
		/*bLaunchDetached*/ true, /*bLaunchHidden*/ false, /*bLaunchReallyHidden*/ false,
		/*OutProcessID*/ nullptr, /*PriorityModifier*/ 0, *WorkingDir, /*PipeWrite*/ nullptr);

	if (!Handle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("DevAuthTool: failed to launch '%s'."), *ExePath);
		return false;
	}

	// Detached: we do not own the process lifetime, so release our handle immediately. The user
	// closes the tool themselves; killing it on editor shutdown would drop their logins.
	FPlatformProcess::CloseProc(Handle);
	UE_LOG(LogExtendedEOS, Log, TEXT("DevAuthTool: launched '%s' %s"), *ExePath, *Args);
	return true;
}

bool FEEOSDevAuthTool::WaitForTool(int32 Port, float TimeoutSeconds)
{
	const double Deadline = FPlatformTime::Seconds() + FMath::Max(0.f, TimeoutSeconds);
	do
	{
		if (IsToolListening(Port))
		{
			return true;
		}
		FPlatformProcess::Sleep(0.25f);
	}
	while (FPlatformTime::Seconds() < Deadline);

	return IsToolListening(Port);
}

FString FEEOSDevAuthTool::GetCredentialsFilePath(EEnvironment Environment)
{
	// Electron's app.getPath("userData") on Windows is %APPDATA% (Roaming) + productName.
	// FPlatformProcess::UserSettingsDir() is AppData/Local, which is the WRONG root here, so
	// read APPDATA directly and fall back to composing the Roaming path from the user dir.
	FString RoamingDir = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
	if (RoamingDir.IsEmpty())
	{
		RoamingDir = FPaths::Combine(FPlatformProcess::UserDir(), TEXT(".."), TEXT("AppData"), TEXT("Roaming"));
	}

	return FPaths::ConvertRelativePathToFull(
		FPaths::Combine(RoamingDir, TEXT("EOS_DevAuthTool"), FString::Printf(TEXT("credentials_%s.json"), ToString(Environment))));
}

TArray<FString> FEEOSDevAuthTool::ReadCredentialNames(EEnvironment Environment)
{
	TArray<FString> Names;

	const FString FilePath = GetCredentialsFilePath(Environment);
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *FilePath))
	{
		UE_LOG(LogExtendedEOS, Verbose, TEXT("DevAuthTool: no credential store at '%s' (tool never run for this environment)."), *FilePath);
		return Names;
	}

	// The tool truncates the file to "" while saving, and writes "[]" when no accounts exist.
	Raw.TrimStartAndEndInline();
	if (Raw.IsEmpty())
	{
		return Names;
	}

	TArray<TSharedPtr<FJsonValue>> Entries;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Entries))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("DevAuthTool: could not parse '%s' as JSON."), *FilePath);
		return Names;
	}

	for (const TSharedPtr<FJsonValue>& Entry : Entries)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Entry.IsValid() || !Entry->TryGetObject(Obj) || !Obj)
		{
			continue;
		}

		FString Name;
		if ((*Obj)->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty())
		{
			Names.Add(Name);
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("DevAuthTool: %d credential(s) in '%s'."), Names.Num(), *FilePath);
	return Names;
}
