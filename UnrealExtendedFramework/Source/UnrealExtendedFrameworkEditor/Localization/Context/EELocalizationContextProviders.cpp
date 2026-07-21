// Copyright Moon Punch Games. All Rights Reserved.

#include "EELocalizationContextProvider.h"

#if WITH_EDITOR

#include "Editor.h"
#include "Engine/DataTable.h"
#include "Localization/Model/EELocalizationSession.h"
#include "SourceCodeNavigation.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Systems/Subtitle/Data/EFSubtitleData.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	/** "/Game/UI/W_Menu.W_Menu" (optionally with trailing sub-object/property info). */
	FString ExtractAssetPath(const FString& SourceLocation)
	{
		if (!SourceLocation.StartsWith(TEXT("/")))
		{
			return FString();
		}
		FString Path = SourceLocation;
		int32 SpaceIndex;
		if (Path.FindChar(TEXT(' '), SpaceIndex))
		{
			Path.LeftInline(SpaceIndex);
		}
		// Strip a sub-object suffix after the second ':' if present ("/Game/A.A:Prop").
		int32 ColonIndex;
		if (Path.FindChar(TEXT(':'), ColonIndex))
		{
			Path.LeftInline(ColonIndex);
		}
		return Path;
	}

	UObject* LoadOwningAsset(const FEELocEntry& Entry)
	{
		const FString AssetPath = ExtractAssetPath(Entry.SourceLocation);
		return AssetPath.IsEmpty() ? nullptr : FSoftObjectPath(AssetPath).TryLoad();
	}

	/** C++ source usages: "<path> - line <N>" (classic gather format) or "<path>(<N>)". */
	class FEESourceCodeContextProvider final : public IEELocalizationContextProvider
	{
	public:
		virtual FName GetProviderName() const override { return TEXT("SourceCode"); }

		static bool ParseLocation(const FString& SourceLocation, FString& OutFile, int32& OutLine)
		{
			int32 MarkerIndex = SourceLocation.Find(TEXT(" - line "));
			if (MarkerIndex != INDEX_NONE)
			{
				OutFile = SourceLocation.Left(MarkerIndex);
				OutLine = FCString::Atoi(*SourceLocation.Mid(MarkerIndex + 8));
				return true;
			}

			// "<path>(<N>)"
			if (SourceLocation.EndsWith(TEXT(")")))
			{
				const int32 OpenIndex = SourceLocation.Find(TEXT("("), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				if (OpenIndex != INDEX_NONE)
				{
					OutFile = SourceLocation.Left(OpenIndex);
					OutLine = FCString::Atoi(*SourceLocation.Mid(OpenIndex + 1));
					return OutFile.Contains(TEXT("."));
				}
			}
			return false;
		}

		virtual bool CanDescribe(const FEELocEntry& Entry) const override
		{
			FString File;
			int32 Line;
			return ParseLocation(Entry.SourceLocation, File, Line);
		}

		virtual void BuildContext(const FEELocEntry& Entry, FEELocContext& OutContext) const override
		{
			FString File;
			int32 Line = 0;
			if (ParseLocation(Entry.SourceLocation, File, Line))
			{
				OutContext.Add(TEXT("Source type"), TEXT("C++ (LOCTEXT/NSLOCTEXT)"));
				OutContext.Add(TEXT("File"), File);
				OutContext.Add(TEXT("Line"), FString::FromInt(Line));
			}
		}

		virtual bool OpenUsage(const FEELocEntry& Entry) const override
		{
			FString File;
			int32 Line = 0;
			if (!ParseLocation(Entry.SourceLocation, File, Line))
			{
				return false;
			}
			const FString AbsolutePath = FPaths::ConvertRelativePathToFull(File);
			return FSourceCodeNavigation::OpenSourceFile(AbsolutePath, Line);
		}
	};

	/** Asset-owned text: Widget Blueprints, DataTables, StringTables, generic assets. */
	class FEEAssetContextProvider final : public IEELocalizationContextProvider
	{
	public:
		virtual FName GetProviderName() const override { return TEXT("Asset"); }

		virtual bool CanDescribe(const FEELocEntry& Entry) const override
		{
			return !ExtractAssetPath(Entry.SourceLocation).IsEmpty();
		}

		virtual void BuildContext(const FEELocEntry& Entry, FEELocContext& OutContext) const override
		{
			const FString AssetPath = ExtractAssetPath(Entry.SourceLocation);
			OutContext.Add(TEXT("Owner asset"), AssetPath);

			if (const UObject* Asset = FSoftObjectPath(AssetPath).TryLoad())
			{
				OutContext.Add(TEXT("Asset class"), Asset->GetClass()->GetName());
			}
		}

		virtual bool OpenUsage(const FEELocEntry& Entry) const override
		{
			UObject* Asset = LoadOwningAsset(Entry);
			if (!Asset || !GEditor)
			{
				return false;
			}
			UAssetEditorSubsystem* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
			return AssetEditor && AssetEditor->OpenEditorForAsset(Asset);
		}
	};

	/** ExtendedFramework subtitle rows: speaker, duration, priority, per-culture voice sounds. */
	class FEESubtitleContextProvider final : public IEELocalizationContextProvider
	{
	public:
		virtual FName GetProviderName() const override { return TEXT("Subtitle"); }

		static const UDataTable* GetSubtitleTable(const FEELocEntry& Entry)
		{
			const UObject* Asset = LoadOwningAsset(Entry);
			const UDataTable* Table = Cast<UDataTable>(Asset);
			return Table && Table->GetRowStruct() && Table->GetRowStruct()->IsChildOf(FEFSubtitleEntry::StaticStruct())
				? Table
				: nullptr;
		}

		virtual bool CanDescribe(const FEELocEntry& Entry) const override
		{
			return GetSubtitleTable(Entry) != nullptr;
		}

		virtual void BuildContext(const FEELocEntry& Entry, FEELocContext& OutContext) const override
		{
			const UDataTable* Table = GetSubtitleTable(Entry);
			if (!Table)
			{
				return;
			}

			// Best effort: locate the row whose text matches this entry's source.
			for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
			{
				const FEFSubtitleEntry* Subtitle = reinterpret_cast<const FEFSubtitleEntry*>(Row.Value);
				if (!Subtitle || Subtitle->Text.ToString() != Entry.SourceText)
				{
					continue;
				}

				OutContext.Add(TEXT("Content type"), TEXT("Subtitle"));
				OutContext.Add(TEXT("Row"), Row.Key.ToString());
				OutContext.Add(TEXT("Speaker"), Subtitle->SpeakerName.ToString());
				OutContext.Add(TEXT("Duration"), Subtitle->Duration > 0.0f
					? FString::Printf(TEXT("%.1fs"), Subtitle->Duration)
					: TEXT("auto"));
				OutContext.Add(TEXT("Priority"), FString::FromInt(Subtitle->Priority));

				FString VoiceCultures;
				for (const FEFCultureSound& CultureSound : Subtitle->CultureSounds)
				{
					VoiceCultures += CultureSound.CultureCode + TEXT(" ");
				}
				OutContext.Add(TEXT("Voice cultures"), VoiceCultures.TrimEnd());
				break;
			}
		}
	};
}

FEELocContextProviderRegistry::FEELocContextProviderRegistry()
{
	// Built-in providers. Order matters for OpenUsage: most specific first.
	Providers.Add(MakeShared<FEESubtitleContextProvider>());
	Providers.Add(MakeShared<FEESourceCodeContextProvider>());
	Providers.Add(MakeShared<FEEAssetContextProvider>());
}

FEELocContextProviderRegistry& FEELocContextProviderRegistry::Get()
{
	static FEELocContextProviderRegistry Instance;
	return Instance;
}

void FEELocContextProviderRegistry::Register(const TSharedRef<IEELocalizationContextProvider>& Provider)
{
	// Project providers get priority over the generic built-ins.
	Providers.Insert(Provider, 0);
}

void FEELocContextProviderRegistry::Unregister(const TSharedRef<IEELocalizationContextProvider>& Provider)
{
	Providers.Remove(Provider);
}

void FEELocContextProviderRegistry::BuildContext(const FEELocEntry& Entry, FEELocContext& OutContext) const
{
	for (const TSharedRef<IEELocalizationContextProvider>& Provider : Providers)
	{
		if (Provider->CanDescribe(Entry))
		{
			Provider->BuildContext(Entry, OutContext);
		}
	}
}

bool FEELocContextProviderRegistry::OpenUsage(const FEELocEntry& Entry) const
{
	for (const TSharedRef<IEELocalizationContextProvider>& Provider : Providers)
	{
		if (Provider->CanDescribe(Entry) && Provider->OpenUsage(Entry))
		{
			return true;
		}
	}
	return false;
}

#endif // WITH_EDITOR
