// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianContextCapture.h"

#include "Camera/PlayerCameraManager.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "LevelEditorViewport.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace ExtendedAtlassianContextPrivate
{
	/** Beyond this the list stops being useful; the remainder is reported as a count. */
	constexpr int32 MaxSelectedActors = 20;

	/** How far up from the project directory to look for a .git entry. */
	constexpr int32 MaxGitSearchDepth = 5;

	/** Resolves a .git path that may be a directory or a "gitdir:" pointer file. */
	bool ResolveGitDirectory(const FString& GitPath, FString& OutGitDirectory)
	{
		if (IFileManager::Get().DirectoryExists(*GitPath))
		{
			OutGitDirectory = GitPath;
			return true;
		}

		// Worktrees and submodules use a file containing "gitdir: <path>".
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *GitPath))
		{
			return false;
		}

		Contents.TrimStartAndEndInline();
		if (!Contents.StartsWith(TEXT("gitdir:")))
		{
			return false;
		}

		FString Target = Contents.RightChop(7).TrimStartAndEnd();
		if (FPaths::IsRelative(Target))
		{
			Target = FPaths::Combine(FPaths::GetPath(GitPath), Target);
			FPaths::CollapseRelativeDirectories(Target);
		}

		OutGitDirectory = Target;
		return IFileManager::Get().DirectoryExists(*OutGitDirectory);
	}
}

FExtendedAtlassianCapturedContext FExtendedAtlassianContextCapture::Capture()
{
	using namespace ExtendedAtlassianContextPrivate;

	FExtendedAtlassianCapturedContext Context;

	Context.EngineVersion = FEngineVersion::Current().ToString();
	Context.Platform = FString::Printf(TEXT("%s / %s"),
		ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()),
		*FPlatformMisc::GetOSVersion());
	Context.GraphicsRhi = FApp::GetGraphicsRHI();
	Context.Gpu = FPlatformMisc::GetPrimaryGPUBrand();

	ReadGitRevision(Context.GitBranch, Context.GitCommit);

	if (!GEditor)
	{
		return Context;
	}

	UWorld* World = nullptr;
	if (GEditor->PlayWorld)
	{
		World = GEditor->PlayWorld;
		Context.WorldContext = GEditor->bIsSimulatingInEditor ? TEXT("Simulate In Editor") : TEXT("Play In Editor");
	}
	else
	{
		World = GEditor->GetEditorWorldContext().World();
		Context.WorldContext = TEXT("Editor");
	}

	if (World)
	{
		Context.LevelName = UWorld::RemovePIEPrefix(World->GetMapName());
	}

	// During play the tester was looking through the player camera, not the editor viewport.
	if (GEditor->PlayWorld)
	{
		if (const APlayerController* PlayerController = GEditor->PlayWorld->GetFirstPlayerController())
		{
			if (PlayerController->PlayerCameraManager)
			{
				Context.CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
				Context.CameraRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
				Context.bHasCamera = true;
			}
		}
	}

	if (!Context.bHasCamera && GCurrentLevelEditingViewportClient)
	{
		Context.CameraLocation = GCurrentLevelEditingViewportClient->GetViewLocation();
		Context.CameraRotation = GCurrentLevelEditingViewportClient->GetViewRotation();
		Context.bHasCamera = true;
	}

	if (USelection* Selection = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator It(*Selection); It; ++It)
		{
			const AActor* Actor = Cast<AActor>(*It);
			if (!Actor)
			{
				continue;
			}

			if (Context.SelectedActors.Num() >= MaxSelectedActors)
			{
				Context.SelectedActorOverflow++;
				continue;
			}

			Context.SelectedActors.Add(FString::Printf(TEXT("%s [%s] @ %s"),
				*Actor->GetActorNameOrLabel(),
				*Actor->GetClass()->GetName(),
				*Actor->GetActorLocation().ToCompactString()));
		}
	}

	return Context;
}

FString FExtendedAtlassianCapturedContext::ToContextBlock() const
{
	TArray<FString> Lines;

	Lines.Add(FString::Printf(TEXT("Level:        %s"), *LevelName));
	Lines.Add(FString::Printf(TEXT("Context:      %s"), *WorldContext));

	if (bHasCamera)
	{
		Lines.Add(FString::Printf(TEXT("Camera Loc:   %s"), *CameraLocation.ToCompactString()));
		Lines.Add(FString::Printf(TEXT("Camera Rot:   %s"), *CameraRotation.ToCompactString()));
	}

	Lines.Add(FString::Printf(TEXT("Engine:       %s"), *EngineVersion));
	Lines.Add(FString::Printf(TEXT("Platform:     %s"), *Platform));
	Lines.Add(FString::Printf(TEXT("RHI:          %s"), *GraphicsRhi));
	Lines.Add(FString::Printf(TEXT("GPU:          %s"), *Gpu));

	if (!GitBranch.IsEmpty() || !GitCommit.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("Revision:     %s @ %s"), *GitBranch, *GitCommit));
	}

	if (SelectedActors.Num() > 0)
	{
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("Selected actors:"));
		for (const FString& Actor : SelectedActors)
		{
			Lines.Add(FString::Printf(TEXT("  %s"), *Actor));
		}

		if (SelectedActorOverflow > 0)
		{
			Lines.Add(FString::Printf(TEXT("  ... and %d more"), SelectedActorOverflow));
		}
	}

	return FString::Join(Lines, TEXT("\n"));
}

FString FExtendedAtlassianContextCapture::ReadLogTail(int32 Kilobytes)
{
	const FString EditorLogPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectLogDir() / (FString(FApp::GetProjectName()) + TEXT(".log")));

	// The editor holds the log open for writing; opening it without FILEREAD_AllowWrite just fails.
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*EditorLogPath, FILEREAD_AllowWrite | FILEREAD_Silent));
	if (!Reader)
	{
		return FString();
	}

	const int64 TotalSize = Reader->TotalSize();
	const int64 WantedBytes = FMath::Min<int64>(TotalSize, static_cast<int64>(FMath::Max(1, Kilobytes)) * 1024);
	if (WantedBytes <= 0)
	{
		return FString();
	}

	Reader->Seek(TotalSize - WantedBytes);

	TArray<uint8> Raw;
	Raw.SetNumUninitialized(static_cast<int32>(WantedBytes));
	Reader->Serialize(Raw.GetData(), WantedBytes);
	Reader->Close();

	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Raw.GetData()), Raw.Num());
	FString Text(Converted.Length(), Converted.Get());

	// Seeking into the middle of the file almost certainly lands mid-line; drop the partial one.
	int32 FirstNewline = INDEX_NONE;
	if (TotalSize > WantedBytes && Text.FindChar(TEXT('\n'), FirstNewline))
	{
		Text = Text.RightChop(FirstNewline + 1);
	}

	return Text;
}

bool FExtendedAtlassianContextCapture::ReadGitRevision(FString& OutBranch, FString& OutShortSha)
{
	using namespace ExtendedAtlassianContextPrivate;

	OutBranch.Reset();
	OutShortSha.Reset();

	FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Directory);

	FString GitPath;
	for (int32 Depth = 0; Depth < MaxGitSearchDepth && !Directory.IsEmpty(); ++Depth)
	{
		const FString Candidate = Directory / TEXT(".git");
		if (IFileManager::Get().DirectoryExists(*Candidate) || IFileManager::Get().FileExists(*Candidate))
		{
			GitPath = Candidate;
			break;
		}

		const FString Parent = FPaths::GetPath(Directory);
		if (Parent == Directory)
		{
			break;
		}
		Directory = Parent;
	}

	if (GitPath.IsEmpty())
	{
		return false;
	}

	FString GitDirectory;
	if (!ResolveGitDirectory(GitPath, GitDirectory))
	{
		return false;
	}

	FString Head;
	if (!FFileHelper::LoadFileToString(Head, *(GitDirectory / TEXT("HEAD"))))
	{
		return false;
	}
	Head.TrimStartAndEndInline();

	if (!Head.StartsWith(TEXT("ref:")))
	{
		// Detached HEAD stores the SHA directly.
		OutBranch = TEXT("(detached)");
		OutShortSha = Head.Left(8);
		return true;
	}

	const FString RefPath = Head.RightChop(4).TrimStartAndEnd();
	OutBranch = RefPath.StartsWith(TEXT("refs/heads/")) ? RefPath.RightChop(11) : RefPath;

	FString LooseSha;
	if (FFileHelper::LoadFileToString(LooseSha, *(GitDirectory / RefPath)))
	{
		OutShortSha = LooseSha.TrimStartAndEnd().Left(8);
		return true;
	}

	// No loose ref: the branch has been packed into packed-refs.
	FString PackedRefs;
	if (FFileHelper::LoadFileToString(PackedRefs, *(GitDirectory / TEXT("packed-refs"))))
	{
		TArray<FString> Lines;
		PackedRefs.ParseIntoArrayLines(Lines);

		// Match on " <ref>" so refs/heads/foo cannot match refs/heads/barfoo.
		const FString Suffix = FString(TEXT(" ")) + RefPath;
		for (const FString& Line : Lines)
		{
			if (Line.EndsWith(Suffix))
			{
				OutShortSha = Line.Left(8);
				return true;
			}
		}
	}

	// The branch name alone is still worth reporting.
	return !OutBranch.IsEmpty();
}
