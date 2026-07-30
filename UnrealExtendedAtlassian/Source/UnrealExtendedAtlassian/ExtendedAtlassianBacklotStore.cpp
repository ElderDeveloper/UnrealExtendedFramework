// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianBacklotStore.h"

#include "ExtendedAtlassianConfluenceProperties.h"
#include "UnrealExtendedAtlassian.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

THIRD_PARTY_INCLUDES_START
#include <openssl/sha.h>
THIRD_PARTY_INCLUDES_END

namespace ExtendedAtlassianBacklotStorePrivate
{
	constexpr int32 SchemaVersion = 1;

	FString KindToString(EExtendedAtlassianPinKind Kind)
	{
		switch (Kind)
		{
		case EExtendedAtlassianPinKind::Level: return TEXT("LEVEL");
		case EExtendedAtlassianPinKind::Blueprint: return TEXT("BLUEPRINT");
		case EExtendedAtlassianPinKind::Page: return TEXT("PAGE");
		default: return TEXT("MATERIAL");
		}
	}

	EExtendedAtlassianPinKind KindFromString(const FString& Kind)
	{
		if (Kind == TEXT("LEVEL"))
		{
			return EExtendedAtlassianPinKind::Level;
		}
		if (Kind == TEXT("BLUEPRINT"))
		{
			return EExtendedAtlassianPinKind::Blueprint;
		}
		if (Kind == TEXT("PAGE"))
		{
			return EExtendedAtlassianPinKind::Page;
		}
		return EExtendedAtlassianPinKind::Material;
	}

	FString StringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
	{
		FString Value;
		if (Object.IsValid())
		{
			Object->TryGetStringField(Name, Value);
		}
		return Value;
	}

	int32 IntField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Name,
		int32 Default = 0)
	{
		double Value = Default;
		if (Object.IsValid())
		{
			Object->TryGetNumberField(Name, Value);
		}
		return static_cast<int32>(Value);
	}

	bool BoolField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Name,
		bool Default = false)
	{
		bool Value = Default;
		if (Object.IsValid())
		{
			Object->TryGetBoolField(Name, Value);
		}
		return Value;
	}

	TSharedRef<FJsonObject> CloneObject(const TSharedPtr<FJsonObject>& Source)
	{
		const TSharedRef<FJsonObject> Clone = MakeShared<FJsonObject>();
		if (Source.IsValid())
		{
			Clone->Values = Source->Values;
		}
		return Clone;
	}

	TSharedRef<FJsonObject> MessageToJson(const FExtendedAtlassianPinThread& Message)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("id"), Message.Id);
		Object->SetStringField(TEXT("authorAccountId"), Message.AuthorAccountId);
		Object->SetStringField(TEXT("authorDisplayName"), Message.AuthorDisplayName);
		Object->SetStringField(TEXT("body"), Message.Body);
		Object->SetStringField(
			TEXT("createdAt"),
			Message.Created == FDateTime::MinValue()
				? FString()
				: Message.Created.ToIso8601());
		Object->SetStringField(
			TEXT("updatedAt"),
			Message.Updated == FDateTime::MinValue()
				? FString()
				: Message.Updated.ToIso8601());
		Object->SetStringField(TEXT("relativeTime"), Message.RelativeTime);
		Object->SetStringField(TEXT("linkedLabel"), Message.LinkedLabel);
		Object->SetBoolField(TEXT("resolved"), Message.bResolved);
		return Object;
	}

	TSharedRef<FJsonObject> PinToJson(const FExtendedAtlassianPin& Pin)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("id"), Pin.Id);
		Object->SetStringField(TEXT("displayName"), Pin.DisplayName);
		Object->SetStringField(TEXT("color"), Pin.Color);
		Object->SetNumberField(TEXT("version"), Pin.Version);
		const TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("kind"), KindToString(Pin.Target.Kind));
		Target->SetStringField(TEXT("stableId"), Pin.Target.StableId);
		Target->SetStringField(TEXT("displayName"), Pin.Target.DisplayName);
		Target->SetStringField(TEXT("secondaryId"), Pin.Target.SecondaryId);
		Object->SetObjectField(TEXT("target"), Target);
		TArray<TSharedPtr<FJsonValue>> Messages;
		for (const FExtendedAtlassianPinThread& Message : Pin.Threads)
		{
			Messages.Add(MakeShared<FJsonValueObject>(MessageToJson(Message)));
		}
		Object->SetArrayField(TEXT("messages"), MoveTemp(Messages));
		return Object;
	}

	TSharedRef<FJsonObject> PinMutationToJson(
		const FExtendedAtlassianPinStoreMutation& Mutation)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(
			TEXT("type"),
			static_cast<uint8>(Mutation.Type));
		Object->SetStringField(TEXT("pinId"), Mutation.PinId);
		Object->SetStringField(TEXT("messageId"), Mutation.MessageId);
		Object->SetStringField(TEXT("displayName"), Mutation.DisplayName);
		Object->SetStringField(TEXT("body"), Mutation.Body);
		Object->SetStringField(TEXT("color"), Mutation.Color);
		Object->SetStringField(
			TEXT("authorAccountId"),
			Mutation.AuthorAccountId);
		Object->SetStringField(
			TEXT("authorDisplayName"),
			Mutation.AuthorDisplayName);
		Object->SetBoolField(TEXT("resolved"), Mutation.bResolved);
		const TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(
			TEXT("kind"),
			KindToString(Mutation.Target.Kind));
		Target->SetStringField(TEXT("stableId"), Mutation.Target.StableId);
		Target->SetStringField(
			TEXT("displayName"),
			Mutation.Target.DisplayName);
		Target->SetStringField(
			TEXT("secondaryId"),
			Mutation.Target.SecondaryId);
		Object->SetObjectField(TEXT("target"), Target);
		return Object;
	}

	bool PinMutationFromJson(
		const TSharedPtr<FJsonObject>& Object,
		FExtendedAtlassianPinStoreMutation& OutMutation)
	{
		if (!Object.IsValid())
		{
			return false;
		}
		double Type = -1.0;
		if (!Object->TryGetNumberField(TEXT("type"), Type)
			|| Type < 0.0
			|| Type > static_cast<uint8>(
				EExtendedAtlassianPinStoreMutation::ToggleResolved))
		{
			return false;
		}
		OutMutation.Type =
			static_cast<EExtendedAtlassianPinStoreMutation>(
				static_cast<uint8>(Type));
		OutMutation.PinId = StringField(Object, TEXT("pinId"));
		OutMutation.MessageId = StringField(Object, TEXT("messageId"));
		OutMutation.DisplayName =
			StringField(Object, TEXT("displayName"));
		OutMutation.Body = StringField(Object, TEXT("body"));
		OutMutation.Color = StringField(Object, TEXT("color"));
		OutMutation.AuthorAccountId =
			StringField(Object, TEXT("authorAccountId"));
		OutMutation.AuthorDisplayName =
			StringField(Object, TEXT("authorDisplayName"));
		OutMutation.bResolved =
			BoolField(Object, TEXT("resolved"));
		const TSharedPtr<FJsonObject>* Target = nullptr;
		if (Object->TryGetObjectField(TEXT("target"), Target)
			&& Target->IsValid())
		{
			OutMutation.Target.Kind =
				KindFromString(StringField(*Target, TEXT("kind")));
			OutMutation.Target.StableId =
				StringField(*Target, TEXT("stableId"));
			OutMutation.Target.DisplayName =
				StringField(*Target, TEXT("displayName"));
			OutMutation.Target.SecondaryId =
				StringField(*Target, TEXT("secondaryId"));
		}
		return true;
	}

	bool IsOfflineRetryable(const FExtendedAtlassianError& Error)
	{
		return Error.bRetryable
			|| Error.HttpStatus == 0
			|| Error.HttpStatus == 429
			|| Error.HttpStatus >= 500;
	}

	void WritePins(
		const FString& MetadataPageId,
		const FExtendedAtlassianContentProperty& Current,
		const TSharedRef<FJsonObject>& Value,
		FExtendedAtlassianContentPropertyDelegate OnComplete)
	{
		if (Current.IsValid())
		{
			FExtendedAtlassianConfluenceProperties::UpdatePageProperty(
				MetadataPageId,
				Current,
				Value,
				MoveTemp(OnComplete));
		}
		else
		{
			FExtendedAtlassianConfluenceProperties::CreatePageProperty(
				MetadataPageId,
				FExtendedAtlassianBacklotStore::PinsPropertyKey,
				Value,
				MoveTemp(OnComplete));
		}
	}

	void MutateAttempt(
		const FString& MetadataPageId,
		const FExtendedAtlassianPinStoreMutation& Mutation,
		const FExtendedAtlassianPinsDelegate& OnComplete,
		bool bAllowConflictRetry)
	{
		FExtendedAtlassianConfluenceProperties::GetPageProperty(
			MetadataPageId,
			FExtendedAtlassianBacklotStore::PinsPropertyKey,
			FExtendedAtlassianContentPropertyDelegate::CreateLambda(
				[
					MetadataPageId,
					Mutation,
					OnComplete,
					bAllowConflictRetry
				](
					bool bReadSuccess,
					const FExtendedAtlassianContentProperty& Current,
					const FExtendedAtlassianError& ReadError)
				{
					if (!bReadSuccess)
					{
						OnComplete.ExecuteIfBound(
							false,
							TArray<FExtendedAtlassianPin>(),
							ReadError);
						return;
					}
					TArray<FExtendedAtlassianPin> Pins;
					FExtendedAtlassianError ParseError;
					if (Current.IsValid()
						&& !FExtendedAtlassianBacklotStore::ParsePinsEnvelope(
							Current.Value,
							Pins,
							ParseError))
					{
						OnComplete.ExecuteIfBound(false, Pins, ParseError);
						return;
					}
					FExtendedAtlassianError MutationError;
					if (!FExtendedAtlassianBacklotStore::ApplyPinMutation(
						Pins,
						Mutation,
						MutationError))
					{
						OnComplete.ExecuteIfBound(false, Pins, MutationError);
						return;
					}
					const TSharedRef<FJsonObject> Value =
						FExtendedAtlassianBacklotStore::BuildPinsEnvelope(
							Pins,
							Current.Value,
							Mutation.AuthorAccountId);
					WritePins(
						MetadataPageId,
						Current,
						Value,
						FExtendedAtlassianContentPropertyDelegate::CreateLambda(
							[
								MetadataPageId,
								Mutation,
								Pins,
								OnComplete,
								bAllowConflictRetry
							](
								bool bWriteSuccess,
								const FExtendedAtlassianContentProperty&,
								const FExtendedAtlassianError& WriteError)
							{
								if (!bWriteSuccess
									&& bAllowConflictRetry
									&& WriteError.HttpStatus == 409)
								{
									MutateAttempt(
										MetadataPageId,
										Mutation,
										OnComplete,
										false);
									return;
								}
								OnComplete.ExecuteIfBound(
									bWriteSuccess,
									Pins,
									WriteError);
							}));
				}));
	}

	struct FPinFlushState
	{
		FString MetadataPageId;
		TArray<FExtendedAtlassianPin> Pins;
		TArray<FExtendedAtlassianPin> OptimisticPins;
		TArray<FExtendedAtlassianPinStoreMutation> Pending;
		FExtendedAtlassianPinsDelegate OnComplete;
	};

	void FlushNextPinMutation(const TSharedRef<FPinFlushState>& State)
	{
		if (State->Pending.IsEmpty())
		{
			FString CacheError;
			FExtendedAtlassianBacklotStore::SavePinsCache(
				State->MetadataPageId,
				State->Pins,
				State->Pending,
				CacheError);
			State->OnComplete.ExecuteIfBound(
				true,
				State->Pins,
				FExtendedAtlassianError());
			return;
		}
		const FExtendedAtlassianPinStoreMutation Mutation =
			State->Pending[0];
		MutateAttempt(
			State->MetadataPageId,
			Mutation,
			FExtendedAtlassianPinsDelegate::CreateLambda(
				[State](
					bool bSuccess,
					const TArray<FExtendedAtlassianPin>& Pins,
					const FExtendedAtlassianError& Error)
				{
					if (!bSuccess)
					{
						if (IsOfflineRetryable(Error))
						{
							FString CacheError;
							FExtendedAtlassianBacklotStore::SavePinsCache(
								State->MetadataPageId,
								State->OptimisticPins,
								State->Pending,
								CacheError);
							FExtendedAtlassianError Warning = Error;
							Warning.Code = TEXT("OfflinePinQueue");
							Warning.Message = FString::Printf(
								TEXT("%d Pin change(s) remain queued until Atlassian reconnects."),
								State->Pending.Num());
							State->OnComplete.ExecuteIfBound(
								true,
								State->OptimisticPins,
								Warning);
							return;
						}
						State->OnComplete.ExecuteIfBound(
							false,
							State->Pins,
							Error);
						return;
					}
					State->Pins = Pins;
					State->Pending.RemoveAt(0);
					FString CacheError;
					FExtendedAtlassianBacklotStore::SavePinsCache(
						State->MetadataPageId,
						State->Pins,
						State->Pending,
						CacheError);
					FlushNextPinMutation(State);
				}),
			true);
	}

	void FlushPinQueue(
		const FString& MetadataPageId,
		const TArray<FExtendedAtlassianPin>& RemotePins,
		const TArray<FExtendedAtlassianPin>& OptimisticPins,
		const TArray<FExtendedAtlassianPinStoreMutation>& Pending,
		FExtendedAtlassianPinsDelegate OnComplete)
	{
		const TSharedRef<FPinFlushState> State =
			MakeShared<FPinFlushState>();
		State->MetadataPageId = MetadataPageId;
		State->Pins = RemotePins;
		State->OptimisticPins = OptimisticPins;
		State->Pending = Pending;
		State->OnComplete = MoveTemp(OnComplete);
		FlushNextPinMutation(State);
	}

	void WriteIssueCommentMetadata(
		const FString& MetadataPageId,
		const FExtendedAtlassianContentProperty& Current,
		const TSharedRef<FJsonObject>& Value,
		FExtendedAtlassianContentPropertyDelegate OnComplete)
	{
		if (Current.IsValid())
		{
			FExtendedAtlassianConfluenceProperties::UpdatePageProperty(
				MetadataPageId,
				Current,
				Value,
				MoveTemp(OnComplete));
		}
		else
		{
			FExtendedAtlassianConfluenceProperties::CreatePageProperty(
				MetadataPageId,
				FExtendedAtlassianBacklotStore::IssueCommentsPropertyKey,
				Value,
				MoveTemp(OnComplete));
		}
	}

	void MutateIssueCommentMetadataAttempt(
		const FString& MetadataPageId,
		const FExtendedAtlassianIssueCommentMetadataStoreMutation& Mutation,
		const FExtendedAtlassianIssueCommentMetadataDelegate& OnComplete,
		bool bAllowConflictRetry)
	{
		FExtendedAtlassianConfluenceProperties::GetPageProperty(
			MetadataPageId,
			FExtendedAtlassianBacklotStore::IssueCommentsPropertyKey,
			FExtendedAtlassianContentPropertyDelegate::CreateLambda(
				[
					MetadataPageId,
					Mutation,
					OnComplete,
					bAllowConflictRetry
				](
					bool bReadSuccess,
					const FExtendedAtlassianContentProperty& Current,
					const FExtendedAtlassianError& ReadError)
				{
					if (!bReadSuccess)
					{
						OnComplete.ExecuteIfBound(
							false,
							TArray<FExtendedAtlassianIssueCommentMetadata>(),
							ReadError);
						return;
					}
					TArray<FExtendedAtlassianIssueCommentMetadata> Metadata;
					FExtendedAtlassianError ParseError;
					if (Current.IsValid()
						&& !FExtendedAtlassianBacklotStore::
							ParseIssueCommentMetadataEnvelope(
								Current.Value,
								Metadata,
								ParseError))
					{
						OnComplete.ExecuteIfBound(false, Metadata, ParseError);
						return;
					}
					if (Mutation.Type
						== EExtendedAtlassianIssueCommentMetadataMutation::Remove)
					{
						Metadata.RemoveAll(
							[&Mutation](
								const FExtendedAtlassianIssueCommentMetadata& Entry)
							{
								return Entry.IssueKey == Mutation.IssueKey
									&& (Entry.CommentId == Mutation.CommentId
										|| Entry.ParentId
											== Mutation.CommentId);
							});
					}
					else
					{
						FExtendedAtlassianIssueCommentMetadata* Entry =
							Metadata.FindByPredicate(
								[&Mutation](
									const FExtendedAtlassianIssueCommentMetadata&
										Candidate)
								{
									return Candidate.IssueKey
											== Mutation.IssueKey
										&& Candidate.CommentId
											== Mutation.CommentId;
								});
						if (!Entry)
						{
							FExtendedAtlassianIssueCommentMetadata NewEntry;
							NewEntry.IssueKey = Mutation.IssueKey;
							NewEntry.CommentId = Mutation.CommentId;
							Metadata.Add(MoveTemp(NewEntry));
							Entry = &Metadata.Last();
						}
						Entry->ParentId = Mutation.ParentId;
						Entry->bResolved = Mutation.bResolved;
						Entry->Updated = FDateTime::UtcNow();
					}
					const TSharedRef<FJsonObject> Value =
						FExtendedAtlassianBacklotStore::
							BuildIssueCommentMetadataEnvelope(
								Metadata,
								Current.Value,
								Mutation.UpdatedBy);
					WriteIssueCommentMetadata(
						MetadataPageId,
						Current,
						Value,
						FExtendedAtlassianContentPropertyDelegate::CreateLambda(
							[
								MetadataPageId,
								Mutation,
								Metadata,
								OnComplete,
								bAllowConflictRetry
							](
								bool bWriteSuccess,
								const FExtendedAtlassianContentProperty&,
								const FExtendedAtlassianError& WriteError)
							{
								if (!bWriteSuccess
									&& bAllowConflictRetry
									&& WriteError.HttpStatus == 409)
								{
									MutateIssueCommentMetadataAttempt(
										MetadataPageId,
										Mutation,
										OnComplete,
										false);
									return;
								}
								OnComplete.ExecuteIfBound(
									bWriteSuccess,
									Metadata,
									WriteError);
							}));
				}));
	}
}

void FExtendedAtlassianBacklotStore::LoadPins(
	const FString& MetadataPageId,
	FExtendedAtlassianPinsDelegate OnComplete)
{
	if (MetadataPageId.IsEmpty())
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("SharedMetadataNotConfigured");
		Error.Message =
			TEXT("Set a Backlot metadata page ID before loading shared Pins.");
		OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianPin>(), Error);
		return;
	}
	FExtendedAtlassianConfluenceProperties::GetPageProperty(
		MetadataPageId,
		PinsPropertyKey,
		FExtendedAtlassianContentPropertyDelegate::CreateLambda(
			[MetadataPageId, OnComplete](
				bool bSuccess,
				const FExtendedAtlassianContentProperty& Property,
				const FExtendedAtlassianError& Error)
			{
				if (!bSuccess)
				{
					TArray<FExtendedAtlassianPin> CachedPins;
					TArray<FExtendedAtlassianPinStoreMutation> Pending;
					FString CacheError;
					if (ExtendedAtlassianBacklotStorePrivate::
							IsOfflineRetryable(Error)
						&& LoadPinsCache(
							MetadataPageId,
							CachedPins,
							Pending,
							CacheError))
					{
						FExtendedAtlassianError Warning = Error;
						Warning.Code = TEXT("OfflinePins");
						Warning.Message = Pending.IsEmpty()
							? TEXT("Showing cached Pins while Atlassian is offline.")
							: FString::Printf(
								TEXT("Showing cached Pins; %d change(s) remain queued."),
								Pending.Num());
						OnComplete.ExecuteIfBound(
							true,
							CachedPins,
							Warning);
						return;
					}
					OnComplete.ExecuteIfBound(
						false,
						TArray<FExtendedAtlassianPin>(),
						Error);
					return;
				}
				TArray<FExtendedAtlassianPin> Pins;
				FExtendedAtlassianError ParseError;
				if (Property.IsValid()
					&& !ParsePinsEnvelope(Property.Value, Pins, ParseError))
				{
					OnComplete.ExecuteIfBound(false, Pins, ParseError);
					return;
				}
				TArray<FExtendedAtlassianPin> CachedPins;
				TArray<FExtendedAtlassianPinStoreMutation> Pending;
				FString CacheError;
				LoadPinsCache(
					MetadataPageId,
					CachedPins,
					Pending,
					CacheError);
				SavePinsCache(
					MetadataPageId,
					Pins,
					Pending,
					CacheError);
				if (Pending.IsEmpty())
				{
					OnComplete.ExecuteIfBound(
						true,
						Pins,
						FExtendedAtlassianError());
					return;
				}
				ExtendedAtlassianBacklotStorePrivate::FlushPinQueue(
					MetadataPageId,
					Pins,
					CachedPins,
					Pending,
					OnComplete);
			}));
}

void FExtendedAtlassianBacklotStore::MutatePins(
	const FString& MetadataPageId,
	const FExtendedAtlassianPinStoreMutation& Mutation,
	FExtendedAtlassianPinsDelegate OnComplete)
{
	if (MetadataPageId.IsEmpty())
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("SharedMetadataNotConfigured");
		Error.Message =
			TEXT("Set a Backlot metadata page ID before changing shared Pins.");
		OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianPin>(), Error);
		return;
	}
	TArray<FExtendedAtlassianPin> CachedPins;
	TArray<FExtendedAtlassianPinStoreMutation> Pending;
	FString CacheError;
	const bool bHasCache = LoadPinsCache(
		MetadataPageId,
		CachedPins,
		Pending,
		CacheError);
	FExtendedAtlassianError MutationError;
	if (bHasCache
		&& ApplyPinMutation(CachedPins, Mutation, MutationError))
	{
		Pending.Add(Mutation);
		if (!SavePinsCache(
			MetadataPageId,
			CachedPins,
			Pending,
			CacheError))
		{
			FExtendedAtlassianError Error;
			Error.Code = TEXT("PinQueueSaveFailed");
			Error.Message = CacheError;
			OnComplete.ExecuteIfBound(false, CachedPins, Error);
			return;
		}
		// LoadPins reconciles the remote value and drains queued mutations in order.
		LoadPins(MetadataPageId, MoveTemp(OnComplete));
		return;
	}
	if (bHasCache)
	{
		OnComplete.ExecuteIfBound(false, CachedPins, MutationError);
		return;
	}
	// First use has no safe local base yet; perform the conflict-safe remote mutation,
	// then establish the cache for future offline work.
	ExtendedAtlassianBacklotStorePrivate::MutateAttempt(
		MetadataPageId,
		Mutation,
		FExtendedAtlassianPinsDelegate::CreateLambda(
			[MetadataPageId, OnComplete](
				bool bSuccess,
				const TArray<FExtendedAtlassianPin>& Pins,
				const FExtendedAtlassianError& Error)
			{
				if (bSuccess)
				{
					FString SaveError;
					SavePinsCache(
						MetadataPageId,
						Pins,
						TArray<FExtendedAtlassianPinStoreMutation>(),
						SaveError);
				}
				OnComplete.ExecuteIfBound(bSuccess, Pins, Error);
			}),
		true);
}

void FExtendedAtlassianBacklotStore::LoadIssueCommentMetadata(
	const FString& MetadataPageId,
	FExtendedAtlassianIssueCommentMetadataDelegate OnComplete)
{
	if (MetadataPageId.IsEmpty())
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("SharedMetadataNotConfigured");
		Error.Message =
			TEXT("Set a Backlot metadata page ID before loading issue-comment metadata.");
		OnComplete.ExecuteIfBound(
			false,
			TArray<FExtendedAtlassianIssueCommentMetadata>(),
			Error);
		return;
	}
	FExtendedAtlassianConfluenceProperties::GetPageProperty(
		MetadataPageId,
		IssueCommentsPropertyKey,
		FExtendedAtlassianContentPropertyDelegate::CreateLambda(
			[OnComplete](
				bool bSuccess,
				const FExtendedAtlassianContentProperty& Property,
				const FExtendedAtlassianError& Error)
			{
				if (!bSuccess)
				{
					OnComplete.ExecuteIfBound(
						false,
						TArray<FExtendedAtlassianIssueCommentMetadata>(),
						Error);
					return;
				}
				TArray<FExtendedAtlassianIssueCommentMetadata> Metadata;
				FExtendedAtlassianError ParseError;
				if (Property.IsValid()
					&& !ParseIssueCommentMetadataEnvelope(
						Property.Value,
						Metadata,
						ParseError))
				{
					OnComplete.ExecuteIfBound(false, Metadata, ParseError);
					return;
				}
				OnComplete.ExecuteIfBound(
					true,
					Metadata,
					FExtendedAtlassianError());
			}));
}

void FExtendedAtlassianBacklotStore::MutateIssueCommentMetadata(
	const FString& MetadataPageId,
	const FExtendedAtlassianIssueCommentMetadataStoreMutation& Mutation,
	FExtendedAtlassianIssueCommentMetadataDelegate OnComplete)
{
	if (MetadataPageId.IsEmpty())
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("SharedMetadataNotConfigured");
		Error.Message =
			TEXT("Set a Backlot metadata page ID before changing issue-comment metadata.");
		OnComplete.ExecuteIfBound(
			false,
			TArray<FExtendedAtlassianIssueCommentMetadata>(),
			Error);
		return;
	}
	if (Mutation.IssueKey.IsEmpty() || Mutation.CommentId.IsEmpty())
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("InvalidIssueCommentMetadata");
		Error.Message =
			TEXT("Issue-comment metadata requires an issue key and comment id.");
		OnComplete.ExecuteIfBound(
			false,
			TArray<FExtendedAtlassianIssueCommentMetadata>(),
			Error);
		return;
	}
	ExtendedAtlassianBacklotStorePrivate::
		MutateIssueCommentMetadataAttempt(
			MetadataPageId,
			Mutation,
			OnComplete,
			true);
}

bool FExtendedAtlassianBacklotStore::ParsePinsEnvelope(
	const TSharedPtr<FJsonObject>& Value,
	TArray<FExtendedAtlassianPin>& OutPins,
	FExtendedAtlassianError& OutError)
{
	using namespace ExtendedAtlassianBacklotStorePrivate;
	OutPins.Reset();
	OutError.Reset();
	if (!Value.IsValid())
	{
		return true;
	}
	const FString Schema = StringField(Value, TEXT("schema"));
	const int32 Version = IntField(Value, TEXT("version"));
	if (!Schema.IsEmpty() && Schema != TEXT("ue.backlot.pins"))
	{
		OutError.Code = TEXT("UnsupportedSchema");
		OutError.Message = TEXT("The shared Pin property has an unexpected schema.");
		return false;
	}
	if (Version > SchemaVersion)
	{
		OutError.Code = TEXT("NewerSchema");
		OutError.Message = TEXT("Update Backlot to edit this shared Pin data.");
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!Value->TryGetArrayField(TEXT("entries"), Entries))
	{
		return true;
	}
	for (const TSharedPtr<FJsonValue>& EntryValue : *Entries)
	{
		const TSharedPtr<FJsonObject> Entry = EntryValue.IsValid()
			? EntryValue->AsObject()
			: nullptr;
		if (!Entry.IsValid())
		{
			continue;
		}
		FExtendedAtlassianPin Pin;
		Pin.Id = StringField(Entry, TEXT("id"));
		Pin.DisplayName = StringField(Entry, TEXT("displayName"));
		Pin.Color = StringField(Entry, TEXT("color"));
		Pin.Version = IntField(Entry, TEXT("version"), 1);
		const TSharedPtr<FJsonObject>* Target = nullptr;
		if (Entry->TryGetObjectField(TEXT("target"), Target) && Target->IsValid())
		{
			Pin.Target.Kind = KindFromString(StringField(*Target, TEXT("kind")));
			Pin.Target.StableId = StringField(*Target, TEXT("stableId"));
			Pin.Target.DisplayName = StringField(*Target, TEXT("displayName"));
			Pin.Target.SecondaryId = StringField(*Target, TEXT("secondaryId"));
		}
		const TArray<TSharedPtr<FJsonValue>>* Messages = nullptr;
		if (Entry->TryGetArrayField(TEXT("messages"), Messages))
		{
			for (const TSharedPtr<FJsonValue>& MessageValue : *Messages)
			{
				const TSharedPtr<FJsonObject> MessageObject =
					MessageValue.IsValid() ? MessageValue->AsObject() : nullptr;
				if (!MessageObject.IsValid())
				{
					continue;
				}
				FExtendedAtlassianPinThread Message;
				Message.Id = StringField(MessageObject, TEXT("id"));
				Message.AuthorAccountId =
					StringField(MessageObject, TEXT("authorAccountId"));
				Message.AuthorDisplayName =
					StringField(MessageObject, TEXT("authorDisplayName"));
				Message.Body = StringField(MessageObject, TEXT("body"));
				FDateTime::ParseIso8601(
					*StringField(MessageObject, TEXT("createdAt")),
					Message.Created);
				FDateTime::ParseIso8601(
					*StringField(MessageObject, TEXT("updatedAt")),
					Message.Updated);
				Message.RelativeTime =
					StringField(MessageObject, TEXT("relativeTime"));
				Message.LinkedLabel =
					StringField(MessageObject, TEXT("linkedLabel"));
				Message.bResolved =
					BoolField(MessageObject, TEXT("resolved"));
				if (!Message.Id.IsEmpty())
				{
					Pin.Threads.Add(MoveTemp(Message));
				}
			}
		}
		if (!Pin.Id.IsEmpty() && Pin.Target.IsValid())
		{
			OutPins.Add(MoveTemp(Pin));
		}
	}
	return true;
}

TSharedRef<FJsonObject> FExtendedAtlassianBacklotStore::BuildPinsEnvelope(
	const TArray<FExtendedAtlassianPin>& Pins,
	const TSharedPtr<FJsonObject>& PreviousValue,
	const FString& UpdatedBy)
{
	using namespace ExtendedAtlassianBacklotStorePrivate;
	const TSharedRef<FJsonObject> Value = CloneObject(PreviousValue);
	Value->SetStringField(TEXT("schema"), TEXT("ue.backlot.pins"));
	Value->SetNumberField(TEXT("version"), SchemaVersion);
	Value->SetNumberField(
		TEXT("revision"),
		IntField(PreviousValue, TEXT("revision")) + 1);
	Value->SetStringField(TEXT("updatedAt"), FDateTime::UtcNow().ToIso8601());
	Value->SetStringField(TEXT("updatedBy"), UpdatedBy);
	TArray<FExtendedAtlassianPin> Sorted = Pins;
	Sorted.Sort(
		[](const FExtendedAtlassianPin& Left, const FExtendedAtlassianPin& Right)
		{
			return Left.Id < Right.Id;
		});
	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const FExtendedAtlassianPin& Pin : Sorted)
	{
		Entries.Add(MakeShared<FJsonValueObject>(PinToJson(Pin)));
	}
	Value->SetArrayField(TEXT("entries"), MoveTemp(Entries));
	return Value;
}

bool FExtendedAtlassianBacklotStore::ApplyPinMutation(
	TArray<FExtendedAtlassianPin>& Pins,
	const FExtendedAtlassianPinStoreMutation& Mutation,
	FExtendedAtlassianError& OutError)
{
	OutError.Reset();
	FExtendedAtlassianPin* Pin = Pins.FindByPredicate(
		[&Mutation](const FExtendedAtlassianPin& Candidate)
		{
			return Candidate.Id == Mutation.PinId;
		});
	if (Mutation.Type == EExtendedAtlassianPinStoreMutation::CreatePin)
	{
		if (Pin)
		{
			if (Pin->Target.StableId == Mutation.Target.StableId
				&& Pin->Target.Kind == Mutation.Target.Kind)
			{
				// A retry after an ambiguous network failure is idempotent.
				return true;
			}
			OutError.Code = TEXT("PinAlreadyExists");
			OutError.Message = TEXT("A Pin already exists for this stable target.");
			return false;
		}
		FExtendedAtlassianPin NewPin;
		NewPin.Id = Mutation.PinId.IsEmpty()
			? MakeStablePinId(Mutation.Target)
			: Mutation.PinId;
		NewPin.DisplayName = Mutation.DisplayName;
		NewPin.Target = Mutation.Target;
		NewPin.Color = Mutation.Color;
		NewPin.Version = 1;
		Pins.Add(MoveTemp(NewPin));
		return true;
	}
	if (!Pin)
	{
		if (Mutation.Type == EExtendedAtlassianPinStoreMutation::DeletePin)
		{
			return true;
		}
		OutError.Code = TEXT("PinNotFound");
		OutError.Message = TEXT("Refresh Backlot; the shared Pin no longer exists.");
		return false;
	}
	switch (Mutation.Type)
	{
	case EExtendedAtlassianPinStoreMutation::UpdatePin:
		Pin->DisplayName = Mutation.DisplayName;
		++Pin->Version;
		return true;
	case EExtendedAtlassianPinStoreMutation::DeletePin:
		Pins.RemoveAll(
			[&Mutation](const FExtendedAtlassianPin& Candidate)
			{
				return Candidate.Id == Mutation.PinId;
			});
		return true;
	case EExtendedAtlassianPinStoreMutation::CreateMessage:
		{
			if (Pin->Threads.ContainsByPredicate(
				[&Mutation](const FExtendedAtlassianPinThread& Existing)
				{
					return Existing.Id == Mutation.MessageId;
				}))
			{
				return true;
			}
			if (Mutation.Body.Len() > 8192)
			{
				OutError.Code = TEXT("MessageTooLarge");
				OutError.Message = TEXT("Pin replies are limited to 8 KiB.");
				return false;
			}
			FExtendedAtlassianPinThread Message;
			Message.Id = Mutation.MessageId;
			Message.AuthorAccountId = Mutation.AuthorAccountId;
			Message.AuthorDisplayName = Mutation.AuthorDisplayName;
			Message.Body = Mutation.Body;
			Message.Created = FDateTime::UtcNow();
			Message.Updated = Message.Created;
			Message.RelativeTime = TEXT("now");
			Pin->Threads.Add(MoveTemp(Message));
			++Pin->Version;
			return true;
		}
	default:
		break;
	}
	FExtendedAtlassianPinThread* Message = Pin->Threads.FindByPredicate(
		[&Mutation](const FExtendedAtlassianPinThread& Candidate)
		{
			return Candidate.Id == Mutation.MessageId;
		});
	if (!Message)
	{
		if (Mutation.Type
			== EExtendedAtlassianPinStoreMutation::DeleteMessage)
		{
			return true;
		}
		OutError.Code = TEXT("PinMessageNotFound");
		OutError.Message = TEXT("Refresh Backlot; the shared Pin reply no longer exists.");
		return false;
	}
	switch (Mutation.Type)
	{
	case EExtendedAtlassianPinStoreMutation::UpdateMessage:
		if (Mutation.Body.Len() > 8192)
		{
			OutError.Code = TEXT("MessageTooLarge");
			OutError.Message = TEXT("Pin replies are limited to 8 KiB.");
			return false;
		}
		Message->Body = Mutation.Body;
		Message->Updated = FDateTime::UtcNow();
		Message->RelativeTime = TEXT("now");
		break;
	case EExtendedAtlassianPinStoreMutation::DeleteMessage:
		Pin->Threads.RemoveAll(
			[&Mutation](const FExtendedAtlassianPinThread& Candidate)
			{
				return Candidate.Id == Mutation.MessageId;
			});
		break;
	case EExtendedAtlassianPinStoreMutation::ToggleResolved:
		Message->bResolved = Mutation.bResolved;
		Message->Updated = FDateTime::UtcNow();
		Message->RelativeTime = TEXT("now");
		break;
	default:
		OutError.Code = TEXT("UnsupportedPinMutation");
		OutError.Message = TEXT("The shared Pin mutation is not supported.");
		return false;
	}
	++Pin->Version;
	return true;
}

FString FExtendedAtlassianBacklotStore::MakeStablePinId(
	const FExtendedAtlassianPinTarget& Target)
{
	const FString Source =
		ExtendedAtlassianBacklotStorePrivate::KindToString(Target.Kind)
		+ TEXT("\n")
		+ Target.StableId
		+ (Target.SecondaryId.IsEmpty()
			? FString()
			: TEXT("\n") + Target.SecondaryId);
	FTCHARToUTF8 Utf8(*Source);
	uint8 Hash[SHA256_DIGEST_LENGTH];
	if (!SHA256(
		reinterpret_cast<const uint8*>(Utf8.Get()),
		static_cast<size_t>(Utf8.Length()),
		Hash))
	{
		return FString();
	}
	return BytesToHex(Hash, SHA256_DIGEST_LENGTH).ToLower();
}

FString FExtendedAtlassianBacklotStore::PinsCachePath(
	const FString& MetadataPageId)
{
	const FString CacheKey =
		FMD5::HashAnsiString(*MetadataPageId).ToLower();
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("UnrealExtendedAtlassian"),
		TEXT("PinCache"),
		CacheKey + TEXT(".json"));
}

bool FExtendedAtlassianBacklotStore::LoadPinsCache(
	const FString& MetadataPageId,
	TArray<FExtendedAtlassianPin>& OutPins,
	TArray<FExtendedAtlassianPinStoreMutation>& OutPending,
	FString& OutError)
{
	using namespace ExtendedAtlassianBacklotStorePrivate;
	OutPins.Reset();
	OutPending.Reset();
	OutError.Reset();
	const FString Path = PinsCachePath(MetadataPageId);
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		OutError = TEXT("No Pin cache exists yet.");
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("The local Pin cache is not valid JSON.");
		return false;
	}
	if (StringField(Root, TEXT("schema")) != TEXT("ue.backlot.pins-cache")
		|| IntField(Root, TEXT("version")) > SchemaVersion)
	{
		OutError = TEXT("The local Pin cache uses an unsupported schema.");
		return false;
	}
	const TSharedPtr<FJsonObject>* PinsValue = nullptr;
	FExtendedAtlassianError ParseError;
	if (Root->TryGetObjectField(TEXT("pins"), PinsValue)
		&& PinsValue->IsValid()
		&& !ParsePinsEnvelope(*PinsValue, OutPins, ParseError))
	{
		OutError = ParseError.Message;
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Pending = nullptr;
	if (Root->TryGetArrayField(TEXT("pending"), Pending))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Pending)
		{
			const TSharedPtr<FJsonObject> Object =
				Value.IsValid() ? Value->AsObject() : nullptr;
			FExtendedAtlassianPinStoreMutation Mutation;
			if (PinMutationFromJson(Object, Mutation))
			{
				OutPending.Add(MoveTemp(Mutation));
			}
		}
	}
	return true;
}

bool FExtendedAtlassianBacklotStore::SavePinsCache(
	const FString& MetadataPageId,
	const TArray<FExtendedAtlassianPin>& Pins,
	const TArray<FExtendedAtlassianPinStoreMutation>& Pending,
	FString& OutError)
{
	using namespace ExtendedAtlassianBacklotStorePrivate;
	OutError.Reset();
	const FString Path = PinsCachePath(MetadataPageId);
	const FString Directory = FPaths::GetPath(Path);
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		OutError = TEXT("Could not create the local Pin cache directory.");
		return false;
	}
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("ue.backlot.pins-cache"));
	Root->SetNumberField(TEXT("version"), SchemaVersion);
	Root->SetStringField(TEXT("savedAt"), FDateTime::UtcNow().ToIso8601());
	Root->SetObjectField(
		TEXT("pins"),
		BuildPinsEnvelope(Pins, nullptr, TEXT("local-cache")));
	TArray<TSharedPtr<FJsonValue>> PendingValues;
	for (const FExtendedAtlassianPinStoreMutation& Mutation : Pending)
	{
		PendingValues.Add(
			MakeShared<FJsonValueObject>(PinMutationToJson(Mutation)));
	}
	Root->SetArrayField(TEXT("pending"), MoveTemp(PendingValues));
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer =
		TJsonWriterFactory<>::Create(&Json);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		OutError = TEXT("Could not serialize the local Pin cache.");
		return false;
	}
	const FString TemporaryPath = Path + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(
		Json,
		*TemporaryPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = TEXT("Could not write the local Pin cache.");
		return false;
	}
	if (!IFileManager::Get().Move(
		*Path,
		*TemporaryPath,
		true,
		true))
	{
		OutError = TEXT("Could not replace the local Pin cache.");
		return false;
	}
	return true;
}

bool FExtendedAtlassianBacklotStore::ParseIssueCommentMetadataEnvelope(
	const TSharedPtr<FJsonObject>& Value,
	TArray<FExtendedAtlassianIssueCommentMetadata>& OutMetadata,
	FExtendedAtlassianError& OutError)
{
	using namespace ExtendedAtlassianBacklotStorePrivate;
	OutMetadata.Reset();
	OutError.Reset();
	if (!Value.IsValid())
	{
		return true;
	}
	const FString Schema = StringField(Value, TEXT("schema"));
	const int32 Version = IntField(Value, TEXT("version"));
	if (!Schema.IsEmpty() && Schema != TEXT("ue.backlot.issue-comments"))
	{
		OutError.Code = TEXT("UnsupportedSchema");
		OutError.Message =
			TEXT("The issue-comment companion property has an unexpected schema.");
		return false;
	}
	if (Version > SchemaVersion)
	{
		OutError.Code = TEXT("NewerSchema");
		OutError.Message =
			TEXT("Update Backlot to edit this issue-comment metadata.");
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!Value->TryGetArrayField(TEXT("entries"), Entries))
	{
		return true;
	}
	for (const TSharedPtr<FJsonValue>& EntryValue : *Entries)
	{
		const TSharedPtr<FJsonObject> Entry =
			EntryValue.IsValid() ? EntryValue->AsObject() : nullptr;
		if (!Entry.IsValid())
		{
			continue;
		}
		FExtendedAtlassianIssueCommentMetadata Metadata;
		Metadata.IssueKey = StringField(Entry, TEXT("issueKey"));
		Metadata.CommentId = StringField(Entry, TEXT("commentId"));
		Metadata.ParentId = StringField(Entry, TEXT("parentId"));
		Metadata.bResolved = BoolField(Entry, TEXT("resolved"));
		FDateTime::ParseIso8601(
			*StringField(Entry, TEXT("updatedAt")),
			Metadata.Updated);
		if (!Metadata.IssueKey.IsEmpty() && !Metadata.CommentId.IsEmpty())
		{
			OutMetadata.Add(MoveTemp(Metadata));
		}
	}
	return true;
}

TSharedRef<FJsonObject>
FExtendedAtlassianBacklotStore::BuildIssueCommentMetadataEnvelope(
	const TArray<FExtendedAtlassianIssueCommentMetadata>& Metadata,
	const TSharedPtr<FJsonObject>& PreviousValue,
	const FString& UpdatedBy)
{
	using namespace ExtendedAtlassianBacklotStorePrivate;
	const TSharedRef<FJsonObject> Value = CloneObject(PreviousValue);
	Value->SetStringField(TEXT("schema"), TEXT("ue.backlot.issue-comments"));
	Value->SetNumberField(TEXT("version"), SchemaVersion);
	Value->SetNumberField(
		TEXT("revision"),
		IntField(PreviousValue, TEXT("revision")) + 1);
	Value->SetStringField(TEXT("updatedAt"), FDateTime::UtcNow().ToIso8601());
	Value->SetStringField(TEXT("updatedBy"), UpdatedBy);
	TArray<FExtendedAtlassianIssueCommentMetadata> Sorted = Metadata;
	Sorted.Sort(
		[](const FExtendedAtlassianIssueCommentMetadata& Left,
			const FExtendedAtlassianIssueCommentMetadata& Right)
		{
			return Left.IssueKey == Right.IssueKey
				? Left.CommentId < Right.CommentId
				: Left.IssueKey < Right.IssueKey;
		});
	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const FExtendedAtlassianIssueCommentMetadata& Entry : Sorted)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("issueKey"), Entry.IssueKey);
		Object->SetStringField(TEXT("commentId"), Entry.CommentId);
		Object->SetStringField(TEXT("parentId"), Entry.ParentId);
		Object->SetBoolField(TEXT("resolved"), Entry.bResolved);
		Object->SetStringField(
			TEXT("updatedAt"),
			Entry.Updated == FDateTime::MinValue()
				? FString()
				: Entry.Updated.ToIso8601());
		Entries.Add(MakeShared<FJsonValueObject>(Object));
	}
	Value->SetArrayField(TEXT("entries"), MoveTemp(Entries));
	return Value;
}
