// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianJira.h"
#include "ExtendedAtlassianTypes.h"

DECLARE_DELEGATE_ThreeParams(
	FExtendedAtlassianContentPropertyDelegate,
	bool /*bSuccess*/,
	const FExtendedAtlassianContentProperty&,
	const FExtendedAtlassianError&);

/** Versioned Confluence v2 page content properties with compare-and-swap updates. */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianConfluenceProperties
{
public:
	/** Missing key is a successful result with an invalid property. */
	static void GetPageProperty(
		const FString& PageId,
		const FString& Key,
		FExtendedAtlassianContentPropertyDelegate OnComplete);

	static void CreatePageProperty(
		const FString& PageId,
		const FString& Key,
		const TSharedRef<FJsonObject>& Value,
		FExtendedAtlassianContentPropertyDelegate OnComplete);

	static void UpdatePageProperty(
		const FString& PageId,
		const FExtendedAtlassianContentProperty& Current,
		const TSharedRef<FJsonObject>& Value,
		FExtendedAtlassianContentPropertyDelegate OnComplete);

	/** Reads by key then creates or version-updates. */
	static void UpsertPageProperty(
		const FString& PageId,
		const FString& Key,
		const TSharedRef<FJsonObject>& Value,
		FExtendedAtlassianContentPropertyDelegate OnComplete);

	/** Pure content-property codec used by live requests and contract tests. */
	static FExtendedAtlassianContentProperty ParseProperty(
		const TSharedPtr<FJsonObject>& Object);
	static FString BuildPropertyBody(
		const FString& Key,
		const TSharedRef<FJsonObject>& Value,
		int32 Version);
};
