// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianTypes.h"

class FJsonObject;

DECLARE_DELEGATE_ThreeParams(
	FExtendedAtlassianPinsDelegate,
	bool /*bSuccess*/,
	const TArray<FExtendedAtlassianPin>&,
	const FExtendedAtlassianError&);
DECLARE_DELEGATE_ThreeParams(
	FExtendedAtlassianIssueCommentMetadataDelegate,
	bool /*bSuccess*/,
	const TArray<FExtendedAtlassianIssueCommentMetadata>&,
	const FExtendedAtlassianError&);

enum class EExtendedAtlassianPinStoreMutation : uint8
{
	CreatePin,
	UpdatePin,
	DeletePin,
	CreateMessage,
	UpdateMessage,
	DeleteMessage,
	ToggleResolved,
};

/** Runtime-safe mutation applied to the shared Backlot Pin envelope. */
struct FExtendedAtlassianPinStoreMutation
{
	EExtendedAtlassianPinStoreMutation Type =
		EExtendedAtlassianPinStoreMutation::CreatePin;
	FString PinId;
	FString MessageId;
	FString DisplayName;
	FString Body;
	FExtendedAtlassianPinTarget Target;
	FString Color;
	FString AuthorAccountId;
	FString AuthorDisplayName;
	bool bResolved = false;
};

enum class EExtendedAtlassianIssueCommentMetadataMutation : uint8
{
	Upsert,
	Remove,
};

/** One atomic update to the Jira-comment companion property. */
struct FExtendedAtlassianIssueCommentMetadataStoreMutation
{
	EExtendedAtlassianIssueCommentMetadataMutation Type =
		EExtendedAtlassianIssueCommentMetadataMutation::Upsert;
	FString IssueKey;
	FString CommentId;
	FString ParentId;
	bool bResolved = false;
	FString UpdatedBy;
};

/**
 * Versioned collaborative Pin store backed by a Confluence page content property.
 *
 * The property value remains an object envelope so unknown top-level fields survive
 * read-modify-write. A 409 reloads, reapplies the mutation, and retries exactly once.
 */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianBacklotStore
{
public:
	static constexpr const TCHAR* PinsPropertyKey = TEXT("ue.backlot.pins.v1");
	static constexpr const TCHAR* IssueCommentsPropertyKey =
		TEXT("ue.backlot.issue-comments.v1");

	static void LoadPins(
		const FString& MetadataPageId,
		FExtendedAtlassianPinsDelegate OnComplete);

	static void MutatePins(
		const FString& MetadataPageId,
		const FExtendedAtlassianPinStoreMutation& Mutation,
		FExtendedAtlassianPinsDelegate OnComplete);

	static void LoadIssueCommentMetadata(
		const FString& MetadataPageId,
		FExtendedAtlassianIssueCommentMetadataDelegate OnComplete);

	/** Conflict-safe read/modify/write with one 409 retry. */
	static void MutateIssueCommentMetadata(
		const FString& MetadataPageId,
		const FExtendedAtlassianIssueCommentMetadataStoreMutation& Mutation,
		FExtendedAtlassianIssueCommentMetadataDelegate OnComplete);

	/** Pure helpers used by contract tests and offline cache validation. */
	static bool ParsePinsEnvelope(
		const TSharedPtr<FJsonObject>& Value,
		TArray<FExtendedAtlassianPin>& OutPins,
		FExtendedAtlassianError& OutError);

	static TSharedRef<FJsonObject> BuildPinsEnvelope(
		const TArray<FExtendedAtlassianPin>& Pins,
		const TSharedPtr<FJsonObject>& PreviousValue,
		const FString& UpdatedBy);

	static bool ApplyPinMutation(
		TArray<FExtendedAtlassianPin>& Pins,
		const FExtendedAtlassianPinStoreMutation& Mutation,
		FExtendedAtlassianError& OutError);

	static FString MakeStablePinId(const FExtendedAtlassianPinTarget& Target);

	/** Machine-local stale cache and durable offline mutation queue. */
	static FString PinsCachePath(const FString& MetadataPageId);
	static bool LoadPinsCache(
		const FString& MetadataPageId,
		TArray<FExtendedAtlassianPin>& OutPins,
		TArray<FExtendedAtlassianPinStoreMutation>& OutPending,
		FString& OutError);
	static bool SavePinsCache(
		const FString& MetadataPageId,
		const TArray<FExtendedAtlassianPin>& Pins,
		const TArray<FExtendedAtlassianPinStoreMutation>& Pending,
		FString& OutError);

	static bool ParseIssueCommentMetadataEnvelope(
		const TSharedPtr<FJsonObject>& Value,
		TArray<FExtendedAtlassianIssueCommentMetadata>& OutMetadata,
		FExtendedAtlassianError& OutError);

	static TSharedRef<FJsonObject> BuildIssueCommentMetadataEnvelope(
		const TArray<FExtendedAtlassianIssueCommentMetadata>& Metadata,
		const TSharedPtr<FJsonObject>& PreviousValue,
		const FString& UpdatedBy);
};
