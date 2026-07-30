// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianEditorTargetService.h"

#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "IContentBrowserSingleton.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPath.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianEditorTargetService"

namespace ExtendedAtlassianEditorTargetPrivate
{
	bool ResolveSelectedAsset(
		EExtendedAtlassianPinKind Kind,
		FExtendedAtlassianPinTarget& OutTarget,
		FText& OutError)
	{
		FContentBrowserModule& ContentBrowser =
			FModuleManager::LoadModuleChecked<FContentBrowserModule>(
				TEXT("ContentBrowser"));
		TArray<FAssetData> SelectedAssets;
		ContentBrowser.Get().GetSelectedAssets(SelectedAssets);

		for (const FAssetData& AssetData : SelectedAssets)
		{
			UObject* Asset = AssetData.GetAsset();
			const bool bMatches =
				Kind == EExtendedAtlassianPinKind::Material
					? Asset && Asset->IsA<UMaterialInterface>()
					: Asset && Asset->IsA<UBlueprint>();
			if (!bMatches)
			{
				continue;
			}

			OutTarget.Kind = Kind;
			OutTarget.StableId = AssetData.GetSoftObjectPath().ToString();
			OutTarget.DisplayName = AssetData.AssetName.ToString();
			OutTarget.SecondaryId.Reset();
			return true;
		}

		OutError = Kind == EExtendedAtlassianPinKind::Material
			? LOCTEXT(
				"SelectMaterial",
				"Select a Material asset in the Content Browser.")
			: LOCTEXT(
				"SelectBlueprint",
				"Select a Blueprint asset in the Content Browser.");
		return false;
	}

	bool ResolveLevel(
		FExtendedAtlassianPinTarget& OutTarget,
		FText& OutError)
	{
		UWorld* World =
			GEditor ? GEditor->GetEditorWorldContext(false).World() : nullptr;
		if (!World || !World->GetPackage())
		{
			OutError = LOCTEXT(
				"NoEditorLevel",
				"No editable level is currently open.");
			return false;
		}

		OutTarget.Kind = EExtendedAtlassianPinKind::Level;
		OutTarget.StableId = FSoftObjectPath(World).ToString();
		OutTarget.DisplayName = World->GetName();
		OutTarget.SecondaryId.Reset();

		if (USelection* Selection = GEditor->GetSelectedActors())
		{
			for (FSelectionIterator It(*Selection); It; ++It)
			{
				if (const AActor* Actor = Cast<AActor>(*It))
				{
					OutTarget.DisplayName = Actor->GetActorLabel();
					OutTarget.SecondaryId =
						Actor->GetActorGuid().ToString(EGuidFormats::DigitsWithHyphens);
					break;
				}
			}
		}
		return true;
	}
}

bool FExtendedAtlassianEditorTargetService::ResolveCurrentTarget(
	EExtendedAtlassianPinKind Kind,
	const FString& SelectedPageId,
	const FString& SelectedPageTitle,
	FExtendedAtlassianPinTarget& OutTarget,
	FText& OutError)
{
	OutTarget = FExtendedAtlassianPinTarget();
	OutTarget.Kind = Kind;
	OutError = FText::GetEmpty();

	switch (Kind)
	{
	case EExtendedAtlassianPinKind::Material:
	case EExtendedAtlassianPinKind::Blueprint:
		return ExtendedAtlassianEditorTargetPrivate::ResolveSelectedAsset(
			Kind,
			OutTarget,
			OutError);

	case EExtendedAtlassianPinKind::Level:
		return ExtendedAtlassianEditorTargetPrivate::ResolveLevel(
			OutTarget,
			OutError);

	case EExtendedAtlassianPinKind::Page:
		if (SelectedPageId.IsEmpty())
		{
			OutError = LOCTEXT(
				"NoSelectedPage",
				"Select a Confluence page in Docs first.");
			return false;
		}
		OutTarget.StableId = SelectedPageId;
		OutTarget.DisplayName =
			SelectedPageTitle.IsEmpty() ? SelectedPageId : SelectedPageTitle;
		OutTarget.SecondaryId.Reset();
		return true;
	}

	OutError = LOCTEXT("UnknownTargetKind", "Unsupported Pin target kind.");
	return false;
}

bool FExtendedAtlassianEditorTargetService::RevealUnrealTarget(
	const FExtendedAtlassianPinTarget& Target,
	FText& OutError)
{
	OutError = FText::GetEmpty();
	if (!GEditor)
	{
		OutError = LOCTEXT("EditorUnavailable", "Unreal Editor is unavailable.");
		return false;
	}

	if (Target.Kind == EExtendedAtlassianPinKind::Material
		|| Target.Kind == EExtendedAtlassianPinKind::Blueprint)
	{
		UObject* Asset = FSoftObjectPath(Target.StableId).TryLoad();
		if (!Asset)
		{
			OutError = FText::Format(
				LOCTEXT(
					"AssetMissing",
					"The asset no longer exists at {0}."),
				FText::FromString(Target.StableId));
			return false;
		}
		GEditor->SyncBrowserToObjects(TArray<UObject*>({ Asset }), true);
		return true;
	}

	if (Target.Kind == EExtendedAtlassianPinKind::Level)
	{
		UWorld* World = GEditor->GetEditorWorldContext(false).World();
		if (!World || FSoftObjectPath(World).ToString() != Target.StableId)
		{
			OutError = FText::Format(
				LOCTEXT(
					"LevelNotOpen",
					"Open level {0} before revealing this target."),
				FText::FromString(Target.StableId));
			return false;
		}

		if (Target.SecondaryId.IsEmpty())
		{
			GEditor->SyncBrowserToObjects(TArray<UObject*>({ World }), true);
			return true;
		}

		FGuid ActorGuid;
		if (!FGuid::Parse(Target.SecondaryId, ActorGuid))
		{
			OutError = LOCTEXT(
				"ActorIdentityInvalid",
				"The stored actor identity is invalid.");
			return false;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && Actor->GetActorGuid() == ActorGuid)
			{
				GEditor->SelectNone(false, true, false);
				GEditor->SelectActor(Actor, true, true, true);
				GEditor->MoveViewportCamerasToActor(*Actor, true);
				return true;
			}
		}

		OutError = FText::Format(
			LOCTEXT(
				"ActorMissing",
				"The actor “{0}” is missing from the current level."),
			FText::FromString(Target.DisplayName));
		return false;
	}

	OutError = LOCTEXT(
		"PageHandledByWorkspace",
		"Confluence pages open in the Backlot Docs workspace.");
	return false;
}

#undef LOCTEXT_NAMESPACE
