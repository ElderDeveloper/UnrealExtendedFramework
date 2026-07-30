// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianModelUtils.h"

#include "ExtendedAtlassianMarkdown.h"
#include "ExtendedAtlassianWorkspaceData.h"

namespace ExtendedAtlassianModelUtils
{
	namespace
	{
		FString Field(
			const FExtendedAtlassianWorkspaceMutation& Mutation,
			const TCHAR* Name)
		{
			if (const FString* Value = Mutation.Fields.Find(Name))
			{
				return *Value;
			}
			return FString();
		}

		FString NextDocumentId(
			const FExtendedAtlassianWorkspaceSnapshot& Snapshot,
			const TCHAR* Prefix)
		{
			for (int32 Number = 1; ; ++Number)
			{
				const FString Candidate = FString::Printf(TEXT("%s%d"), Prefix, Number);
				if (!Snapshot.DocumentTree.ContainsByPredicate(
					[&Candidate](const FExtendedAtlassianDocumentTreeNode& Node)
					{
						return Node.Id == Candidate;
					}))
				{
					return Candidate;
				}
			}
		}

		void RemovePageComments(
			FExtendedAtlassianWorkspaceSnapshot& Snapshot,
			const FString& PageId)
		{
			Snapshot.CommentCollections.RemoveAll(
				[&PageId](const FExtendedAtlassianCommentCollection& Collection)
				{
					return Collection.TargetId == TEXT("page:") + PageId;
				});
		}

		int32 IndexAfterSectionChildren(
			const TArray<FExtendedAtlassianDocumentTreeNode>& Tree,
			int32 SectionIndex)
		{
			int32 InsertIndex = SectionIndex + 1;
			while (InsertIndex < Tree.Num() && Tree[InsertIndex].Depth == 1)
			{
				++InsertIndex;
			}
			return InsertIndex;
		}
	}

	FExtendedAtlassianComment* FindComment(
		TArray<FExtendedAtlassianComment>& Comments,
		const FString& CommentId)
	{
		for (FExtendedAtlassianComment& Comment : Comments)
		{
			if (Comment.Id == CommentId)
			{
				return &Comment;
			}
			if (FExtendedAtlassianComment* Reply = FindComment(Comment.Replies, CommentId))
			{
				return Reply;
			}
		}
		return nullptr;
	}

	const FExtendedAtlassianComment* FindComment(
		const TArray<FExtendedAtlassianComment>& Comments,
		const FString& CommentId)
	{
		for (const FExtendedAtlassianComment& Comment : Comments)
		{
			if (Comment.Id == CommentId)
			{
				return &Comment;
			}
			if (const FExtendedAtlassianComment* Reply = FindComment(Comment.Replies, CommentId))
			{
				return Reply;
			}
		}
		return nullptr;
	}

	bool RemoveComment(
		TArray<FExtendedAtlassianComment>& Comments,
		const FString& CommentId)
	{
		const int32 Removed = Comments.RemoveAll(
			[&CommentId](const FExtendedAtlassianComment& Comment)
			{
				return Comment.Id == CommentId;
			});
		if (Removed > 0)
		{
			return true;
		}

		for (FExtendedAtlassianComment& Comment : Comments)
		{
			if (RemoveComment(Comment.Replies, CommentId))
			{
				return true;
			}
		}
		return false;
	}

	int32 CountComments(
		const TArray<FExtendedAtlassianComment>& Comments,
		bool bIncludeReplies)
	{
		int32 Count = Comments.Num();
		if (bIncludeReplies)
		{
			for (const FExtendedAtlassianComment& Comment : Comments)
			{
				Count += CountComments(Comment.Replies, true);
			}
		}
		return Count;
	}

	int32 CountOpenComments(const TArray<FExtendedAtlassianComment>& Comments)
	{
		int32 Count = 0;
		for (const FExtendedAtlassianComment& Comment : Comments)
		{
			Count += Comment.bResolved ? 0 : 1;
			Count += CountOpenComments(Comment.Replies);
		}
		return Count;
	}

	bool ApplyDocumentMutation(
		FExtendedAtlassianWorkspaceSnapshot& Snapshot,
		const FExtendedAtlassianWorkspaceMutation& Mutation)
	{
		switch (Mutation.Type)
		{
		case EExtendedAtlassianWorkspaceMutation::CreatePage:
			{
				const FString PageId = Mutation.TargetId.IsEmpty()
					? NextDocumentId(Snapshot, TEXT("new"))
					: Mutation.TargetId;
				FString Title = Field(Mutation, TEXT("title")).TrimStartAndEnd();
				if (Title.IsEmpty())
				{
					Title = TEXT("Untitled page");
				}

				FExtendedAtlassianPage Page;
				Page.Id = PageId;
				Page.ParentId = Mutation.ParentId;
				Page.Title = Title;
				Page.Version = 1;
				Page.EditedByLabel = Snapshot.CurrentUser.Initials;
				Page.EditedAtLabel = TEXT("JUST NOW");
				Snapshot.Pages.Add(MoveTemp(Page));

				FExtendedAtlassianDocumentTreeNode Node;
				Node.Id = PageId;
				Node.Label = Title;
				Node.ParentId = Mutation.ParentId;
				int32 InsertIndex = Snapshot.DocumentTree.Num();
				if (!Mutation.ParentId.IsEmpty())
				{
					const int32 SectionIndex =
						Snapshot.DocumentTree.IndexOfByPredicate(
							[&Mutation](const FExtendedAtlassianDocumentTreeNode& Candidate)
							{
								return Candidate.Id == Mutation.ParentId && Candidate.bSection;
							});
					if (SectionIndex != INDEX_NONE)
					{
						Snapshot.DocumentTree[SectionIndex].bExpanded = true;
						Node.Depth = 1;
						InsertIndex = IndexAfterSectionChildren(
							Snapshot.DocumentTree,
							SectionIndex);
					}
					else
					{
						Node.ParentId.Reset();
					}
				}
				Snapshot.DocumentTree.Insert(MoveTemp(Node), InsertIndex);
			}
			return true;

		case EExtendedAtlassianWorkspaceMutation::UpdatePage:
			{
				const FString Title = Field(Mutation, TEXT("title"));
				for (FExtendedAtlassianPage& Page : Snapshot.Pages)
				{
					if (Page.Id != Mutation.TargetId)
					{
						continue;
					}
					if (Mutation.Fields.Contains(TEXT("title")))
					{
						Page.Title = Title;
					}
					if (Mutation.Fields.Contains(TEXT("body")))
					{
						Page.Body = Field(Mutation, TEXT("body"));
						Page.Markdown = Page.Body;
						Page.Blocks = FExtendedAtlassianMarkdown::ToBlocks(Page.Markdown);
						++Page.Version;
					}
					Page.EditedByLabel = Snapshot.CurrentUser.Initials;
					Page.EditedAtLabel = TEXT("JUST NOW");
					break;
				}
				if (Mutation.Fields.Contains(TEXT("title")))
				{
					for (FExtendedAtlassianDocumentTreeNode& Node : Snapshot.DocumentTree)
					{
						if (Node.Id == Mutation.TargetId)
						{
							Node.Label = Title;
							break;
						}
					}
				}
			}
			return true;

		case EExtendedAtlassianWorkspaceMutation::TogglePageTask:
			{
				const int32 BlockIndex =
					FCString::Atoi(*Field(Mutation, TEXT("blockIndex")));
				for (FExtendedAtlassianPage& Page : Snapshot.Pages)
				{
					if (Page.Id != Mutation.TargetId
						|| !Page.Blocks.IsValidIndex(BlockIndex)
						|| Page.Blocks[BlockIndex].Kind
							!= EExtendedAtlassianBlockKind::TaskItem)
					{
						continue;
					}
					Page.Blocks[BlockIndex].bChecked =
						!Page.Blocks[BlockIndex].bChecked;
					Page.Markdown =
						FExtendedAtlassianMarkdown::FromBlocks(Page.Blocks);
					Page.Body = Page.Markdown;
					break;
				}
			}
			return true;

		case EExtendedAtlassianWorkspaceMutation::MovePage:
			{
				const int32 SourceIndex = Snapshot.DocumentTree.IndexOfByPredicate(
					[&Mutation](const FExtendedAtlassianDocumentTreeNode& Node)
					{
						return Node.Id == Mutation.TargetId && !Node.bSection;
					});
				if (SourceIndex == INDEX_NONE)
				{
					return true;
				}

				FExtendedAtlassianDocumentTreeNode Node =
					MoveTemp(Snapshot.DocumentTree[SourceIndex]);
				Snapshot.DocumentTree.RemoveAt(SourceIndex);
				Node.ParentId = Mutation.ParentId;
				Node.Depth = 0;
				int32 InsertIndex = Snapshot.DocumentTree.Num();
				if (!Mutation.ParentId.IsEmpty())
				{
					const int32 SectionIndex =
						Snapshot.DocumentTree.IndexOfByPredicate(
							[&Mutation](const FExtendedAtlassianDocumentTreeNode& Candidate)
							{
								return Candidate.Id == Mutation.ParentId && Candidate.bSection;
							});
					if (SectionIndex != INDEX_NONE)
					{
						Snapshot.DocumentTree[SectionIndex].bExpanded = true;
						Node.Depth = 1;
						InsertIndex = IndexAfterSectionChildren(
							Snapshot.DocumentTree,
							SectionIndex);
					}
					else
					{
						Node.ParentId.Reset();
					}
				}
				Snapshot.DocumentTree.Insert(MoveTemp(Node), InsertIndex);
				for (FExtendedAtlassianPage& Page : Snapshot.Pages)
				{
					if (Page.Id == Mutation.TargetId)
					{
						Page.ParentId = Mutation.ParentId;
						Page.EditedByLabel = Snapshot.CurrentUser.Initials;
						Page.EditedAtLabel = TEXT("JUST NOW");
						break;
					}
				}
			}
			return true;

		case EExtendedAtlassianWorkspaceMutation::DuplicatePage:
			{
				const int32 SourceTreeIndex = Snapshot.DocumentTree.IndexOfByPredicate(
					[&Mutation](const FExtendedAtlassianDocumentTreeNode& Node)
					{
						return Node.Id == Mutation.TargetId && !Node.bSection;
					});
				const FExtendedAtlassianPage* SourcePage = Snapshot.Pages.FindByPredicate(
					[&Mutation](const FExtendedAtlassianPage& Page)
					{
						return Page.Id == Mutation.TargetId;
					});
				if (SourceTreeIndex == INDEX_NONE || !SourcePage)
				{
					return true;
				}

				const FString NewId = Field(Mutation, TEXT("newId")).IsEmpty()
					? NextDocumentId(Snapshot, TEXT("new"))
					: Field(Mutation, TEXT("newId"));
				FExtendedAtlassianPage Copy = *SourcePage;
				Copy.Id = NewId;
				Copy.Title = Field(Mutation, TEXT("title"));
				if (Copy.Title.IsEmpty())
				{
					Copy.Title = SourcePage->Title + TEXT(" copy");
				}
				Copy.Version = 1;
				Copy.EditedByLabel = Snapshot.CurrentUser.Initials;
				Copy.EditedAtLabel = TEXT("JUST NOW");
				Snapshot.Pages.Add(MoveTemp(Copy));

				FExtendedAtlassianDocumentTreeNode CopyNode =
					Snapshot.DocumentTree[SourceTreeIndex];
				CopyNode.Id = NewId;
				CopyNode.Label = Snapshot.Pages.Last().Title;
				CopyNode.CommentBadge = 0;
				Snapshot.DocumentTree.Insert(MoveTemp(CopyNode), SourceTreeIndex + 1);
			}
			return true;

		case EExtendedAtlassianWorkspaceMutation::DeletePage:
			Snapshot.Pages.RemoveAll(
				[&Mutation](const FExtendedAtlassianPage& Page)
				{
					return Page.Id == Mutation.TargetId;
				});
			Snapshot.DocumentTree.RemoveAll(
				[&Mutation](const FExtendedAtlassianDocumentTreeNode& Node)
				{
					return Node.Id == Mutation.TargetId;
				});
			RemovePageComments(Snapshot, Mutation.TargetId);
			return true;

		case EExtendedAtlassianWorkspaceMutation::CreateSection:
			{
				FExtendedAtlassianDocumentTreeNode Section;
				Section.Id = Mutation.TargetId.IsEmpty()
					? NextDocumentId(Snapshot, TEXT("sec"))
					: Mutation.TargetId;
				Section.Label = Field(Mutation, TEXT("title")).TrimStartAndEnd();
				if (Section.Label.IsEmpty())
				{
					Section.Label = TEXT("New section");
				}
				Section.bSection = true;
				Section.bExpanded = true;
				Snapshot.DocumentTree.Add(MoveTemp(Section));
			}
			return true;

		case EExtendedAtlassianWorkspaceMutation::RenameSection:
			for (FExtendedAtlassianDocumentTreeNode& Node : Snapshot.DocumentTree)
			{
				if (Node.Id == Mutation.TargetId && Node.bSection)
				{
					Node.Label = Field(Mutation, TEXT("title"));
					break;
				}
			}
			return true;

		case EExtendedAtlassianWorkspaceMutation::DeleteSection:
			{
				const int32 SectionIndex = Snapshot.DocumentTree.IndexOfByPredicate(
					[&Mutation](const FExtendedAtlassianDocumentTreeNode& Node)
					{
						return Node.Id == Mutation.TargetId && Node.bSection;
					});
				if (SectionIndex == INDEX_NONE)
				{
					return true;
				}
				const int32 EndIndex = IndexAfterSectionChildren(
					Snapshot.DocumentTree,
					SectionIndex);
				TSet<FString> RemovedPageIds;
				for (int32 Index = SectionIndex + 1; Index < EndIndex; ++Index)
				{
					RemovedPageIds.Add(Snapshot.DocumentTree[Index].Id);
				}
				Snapshot.DocumentTree.RemoveAt(
					SectionIndex,
					EndIndex - SectionIndex);
				Snapshot.Pages.RemoveAll(
					[&RemovedPageIds](const FExtendedAtlassianPage& Page)
					{
						return RemovedPageIds.Contains(Page.Id);
					});
				Snapshot.CommentCollections.RemoveAll(
					[&RemovedPageIds](const FExtendedAtlassianCommentCollection& Collection)
					{
						return Collection.TargetId.StartsWith(TEXT("page:"))
							&& RemovedPageIds.Contains(Collection.TargetId.Mid(5));
					});
			}
			return true;

		case EExtendedAtlassianWorkspaceMutation::ReorderPage:
			if (Mutation.OrderedIds.Num() == Snapshot.DocumentTree.Num())
			{
				Snapshot.DocumentTree.StableSort(
					[&Mutation](
						const FExtendedAtlassianDocumentTreeNode& Left,
						const FExtendedAtlassianDocumentTreeNode& Right)
					{
						return Mutation.OrderedIds.IndexOfByKey(Left.Id)
							< Mutation.OrderedIds.IndexOfByKey(Right.Id);
					});
			}
			return true;

		default:
			return false;
		}
	}

	void RefreshCommentPresentation(FExtendedAtlassianWorkspaceSnapshot& Snapshot)
	{
		for (FExtendedAtlassianPage& Page : Snapshot.Pages)
		{
			const FString Scope = TEXT("page:") + Page.Id;
			const FExtendedAtlassianCommentCollection* Collection =
				Snapshot.CommentCollections.FindByPredicate(
					[&Scope](const FExtendedAtlassianCommentCollection& Candidate)
					{
						return Candidate.TargetId == Scope;
					});
			Page.CommentCount = Collection ? Collection->Comments.Num() : 0;
			int32 OpenCount = 0;
			if (Collection)
			{
				for (const FExtendedAtlassianComment& Comment : Collection->Comments)
				{
					OpenCount += Comment.bResolved ? 0 : 1;
				}
			}
			if (FExtendedAtlassianDocumentTreeNode* Node =
				Snapshot.DocumentTree.FindByPredicate(
					[&Page](const FExtendedAtlassianDocumentTreeNode& Candidate)
					{
						return Candidate.Id == Page.Id;
					}))
			{
				Node->CommentBadge = OpenCount;
			}
		}

		for (FExtendedAtlassianIssue& Issue : Snapshot.Issues)
		{
			const FString Scope = TEXT("issue:") + Issue.Key;
			const FExtendedAtlassianCommentCollection* Collection =
				Snapshot.CommentCollections.FindByPredicate(
					[&Scope](const FExtendedAtlassianCommentCollection& Candidate)
					{
						return Candidate.TargetId == Scope;
					});
			Issue.CommentCount = Collection ? Collection->Comments.Num() : 0;
		}
	}

	FString RelativeAge(const FDateTime& Timestamp)
	{
		if (Timestamp == FDateTime::MinValue())
		{
			return FString();
		}
		const FTimespan Age = FDateTime::UtcNow() - Timestamp;
		if (Age.GetTotalMinutes() < 1.0)
		{
			return TEXT("now");
		}
		if (Age.GetTotalHours() < 1.0)
		{
			return FString::Printf(
				TEXT("%dm"),
				FMath::Max(1, FMath::FloorToInt(Age.GetTotalMinutes())));
		}
		if (Age.GetTotalDays() < 1.0)
		{
			return FString::Printf(
				TEXT("%dh"),
				FMath::Max(1, FMath::FloorToInt(Age.GetTotalHours())));
		}
		return FString::Printf(
			TEXT("%dd"),
			FMath::Max(1, FMath::FloorToInt(Age.GetTotalDays())));
	}
}
