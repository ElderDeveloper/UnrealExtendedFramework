// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "SExtendedAtlassianWorkspace.h"

#include "ExtendedAtlassianBacklotStore.h"
#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianContextCapture.h"
#include "ExtendedAtlassianDocumentStore.h"
#include "ExtendedAtlassianEditorTargetService.h"
#include "ExtendedAtlassianFixtureWorkspaceData.h"
#include "ExtendedAtlassianLiveWorkspaceData.h"
#include "ExtendedAtlassianScreenshot.h"
#include "ExtendedAtlassianSettings.h"
#include "ExtendedAtlassianStyle.h"
#include "ExtendedAtlassianWorkspaceController.h"
#include "ExtendedAtlassianWorkspaceHostServices.h"
#include "ExtendedAtlassianMarkdown.h"
#include "SExtendedAtlassianDocumentEditor.h"
#include "SExtendedAtlassianDocumentView.h"
#include "SBacklotStylePrimitives.h"
#include "SBacklotSurfaces.h"
#include "UnrealExtendedAtlassian.h"

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "DirectoryWatcherModule.h"
#include "Editor.h"
#include "EditorBuildUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformProperties.h"
#include "HAL/PlatformTime.h"
#include "IDirectoryWatcher.h"
#include "InputCoreTypes.h"
#include "ISettingsModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Internationalization/Regex.h"
#include "Modules/ModuleManager.h"
#include "Rendering/DrawElements.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "Types/ISlateMetaData.h"

#define LOCTEXT_NAMESPACE "SExtendedAtlassianWorkspace"

namespace ExtendedAtlassianWorkspacePrivate
{
	bool IsActionableWidgetType(const FString& Type)
	{
		return Type.Contains(TEXT("Button"))
			|| Type.Contains(TEXT("CheckBox"))
			|| Type.Contains(TEXT("EditableText"))
			|| Type.Contains(TEXT("Combo"))
			|| Type.Contains(TEXT("ScrollBar"))
			|| Type.Contains(TEXT("BoardCard"))
			|| Type.Contains(TEXT("AnnotationSurface"))
			|| Type.Contains(TEXT("DocumentEditor"));
	}

	void ApplyAutomationMetadata(
		const TSharedRef<SWidget>& Widget,
		const FString& Path = TEXT("0"))
	{
		const FString Type = Widget->GetTypeAsString();
		if (Type.Contains(TEXT("Button")))
		{
			Widget->SetCursor(
				TAttribute<TOptional<EMouseCursor::Type>>(
					TOptional<EMouseCursor::Type>(EMouseCursor::Hand)));
		}
		if (IsActionableWidgetType(Type))
		{
			const FString AutomationId =
				TEXT("Backlot.Action.") + Path + TEXT(".") + Type;
			Widget->AddMetadata(
				MakeShared<FTagMetaData>(FName(*AutomationId)));

			FString Accessible = Widget->GetAccessibleText().ToString();
			Accessible.TrimStartAndEndInline();
			bool bHasAlpha = false;
			for (int32 Index = 0; Index < Accessible.Len(); ++Index)
			{
				if (FChar::IsAlpha(Accessible[Index]))
				{
					bHasAlpha = true;
					break;
				}
			}
			const bool bHasWord = Accessible.Len() >= 3 && bHasAlpha;
			if (!bHasWord)
			{
				Accessible = FString::Printf(
					TEXT("Backlot %s control %s"),
					*Type,
					*Path);
			}
			if (!Widget->IsEnabled())
			{
				Accessible += TEXT(", disabled");
			}
			Widget->SetAccessibleBehavior(
				EAccessibleBehavior::Custom,
				FText::FromString(Accessible));
		}

		FChildren* Children = Widget->GetChildren();
		if (!Children)
		{
			return;
		}
		for (int32 Index = 0; Index < Children->Num(); ++Index)
		{
			ApplyAutomationMetadata(
				Children->GetChildAt(Index),
				FString::Printf(TEXT("%s.%d"), *Path, Index));
		}
	}

	const FButtonStyle& Button(const TCHAR* Name)
	{
		return FExtendedAtlassianStyle::Get().GetWidgetStyle<FButtonStyle>(Name);
	}

	const FTextBlockStyle& Text(const TCHAR* Name)
	{
		return FExtendedAtlassianStyle::Get().GetWidgetStyle<FTextBlockStyle>(Name);
	}

	TSharedRef<SMultiLineEditableText> SelectableText(
		const FText& Value,
		const TCHAR* StyleName,
		bool bAutoWrap = false,
		float LineHeight = 1.0f,
		TOptional<FLinearColor> Color = {})
	{
		FTextBlockStyle SelectableStyle = Text(StyleName);
		if (Color.IsSet())
		{
			SelectableStyle.SetColorAndOpacity(FSlateColor(Color.GetValue()));
		}
		return SNew(SMultiLineEditableText)
			.Text(Value)
			.TextStyle(&SelectableStyle)
			.AutoWrapText(bAutoWrap)
			.LineHeightPercentage(LineHeight)
			.IsReadOnly(true)
			.AllowContextMenu(true);
	}

	const FSlateBrush* Brush(const TCHAR* Name)
	{
		return FExtendedAtlassianStyle::Get().GetBrush(Name);
	}

	const FSlateBrush* Brush(const FName& Name)
	{
		return FExtendedAtlassianStyle::Get().GetBrush(Name);
	}

	/**
	 * Named geometry generated from the frozen HTML. Every authored size in this file
	 * reads through here so a source change fails the metric contract instead of
	 * silently disagreeing with a transcribed literal.
	 */
	float Metric(const TCHAR* Name)
	{
		return FExtendedAtlassianStyle::Metric(Name);
	}

	const FSlateBrush* AvatarBrush(const FString& Initials)
	{
		if (Initials.IsEmpty())
		{
			return Brush(TEXT("Backlot.Brush.Avatar"));
		}
		const int32 First = Initials[0] % 5;
		const int32 Second =
			Initials.Len() > 1 ? Initials[1] % 5 : First;
		const int32 PaletteIndex =
			(3 * First + 3 * First * First + 2 * Second * Second) % 5;
		static const FName Palette[] = {
			TEXT("Backlot.Brush.Avatar.Blue"),
			TEXT("Backlot.Brush.Avatar.Purple"),
			TEXT("Backlot.Brush.Avatar.Green"),
			TEXT("Backlot.Brush.Avatar.Amber"),
			TEXT("Backlot.Brush.Avatar.Red")
		};
		return Brush(Palette[PaletteIndex]);
	}

	const FSlateBrush* AvatarBrush(
		const FExtendedAtlassianUser* User,
		const FString& FallbackInitials)
	{
		if (!User || User->AvatarBackground.IsEmpty())
		{
			return AvatarBrush(FallbackInitials);
		}

		static TMap<FString, TSharedPtr<FSlateRoundedBoxBrush>> AvatarBrushes;
		const FString PaletteKey = User->AvatarBackground.ToLower();
		TSharedPtr<FSlateRoundedBoxBrush>& Avatar =
			AvatarBrushes.FindOrAdd(PaletteKey);
		if (!Avatar.IsValid())
		{
			Avatar = MakeShared<FSlateRoundedBoxBrush>(
				FExtendedAtlassianStyle::FromHex(*User->AvatarBackground),
				13.0f);
		}
		return Avatar.Get();
	}

	FLinearColor AvatarForeground(const FExtendedAtlassianUser* User)
	{
		return FExtendedAtlassianStyle::FromHex(
			User && !User->AvatarForeground.IsEmpty()
				? *User->AvatarForeground
				: TEXT("#cfe0ff"));
	}

	FString AuthConfigurationSignature()
	{
		const UExtendedAtlassianSettings* Settings =
			UExtendedAtlassianSettings::Get();
		const TSharedPtr<FExtendedAtlassianClient> Client =
			FUnrealExtendedAtlassianModule::GetClient();
		if (!Settings)
		{
			return TEXT("NoSettings");
		}
		const FExtendedAtlassianUser VerifiedUser =
			Client.IsValid()
				? Client->GetVerifiedUser()
				: FExtendedAtlassianUser();
		return FString::Printf(
			TEXT("%s|%s|%s|%s|%s|%s|%d|%s"),
			*Settings->GetNormalizedSiteUrl(),
			*Settings->ProjectKey,
			*Settings->BoardId,
			*Settings->SprintSelection,
			*Settings->PrimarySpaceKey,
			*Settings->BacklotMetadataPageId,
			Client.IsValid() && Client->HasCredentials() ? 1 : 0,
			*VerifiedUser.AccountId);
	}

	FText MutationTooltip(
		const TSharedPtr<FExtendedAtlassianWorkspaceController>& Controller,
		EExtendedAtlassianWorkspaceMutation Type,
		const FText& AllowedTooltip = FText::GetEmpty())
	{
		FText Reason;
		return Controller.IsValid()
			&& Controller->CanExecuteMutation(Type, &Reason)
				? AllowedTooltip
				: Reason;
	}

	FString RouteLabel(EExtendedAtlassianWorkspaceRoute Route)
	{
		switch (Route)
		{
		case EExtendedAtlassianWorkspaceRoute::Docs: return TEXT("Docs");
		case EExtendedAtlassianWorkspaceRoute::Issues: return TEXT("Issues");
		case EExtendedAtlassianWorkspaceRoute::IssueDetail: return TEXT("Issue");
		case EExtendedAtlassianWorkspaceRoute::Board: return TEXT("Board");
		case EExtendedAtlassianWorkspaceRoute::Pins: return TEXT("Pins");
		case EExtendedAtlassianWorkspaceRoute::Inbox: return TEXT("Inbox");
		default: return FString();
		}
	}

	FString ProjectDisplayName(
		const FExtendedAtlassianWorkspaceSnapshot& Snapshot)
	{
		for (const FExtendedAtlassianBoard& Board : Snapshot.Boards)
		{
			if (!Board.Name.IsEmpty())
			{
				return Board.Name;
			}
		}
		const UExtendedAtlassianSettings* Settings =
			UExtendedAtlassianSettings::Get();
		return Settings && !Settings->ProjectKey.IsEmpty()
			? Settings->ProjectKey
			: FString(TEXT("Unconfigured"));
	}

	FString ConfluenceSpaceDisplayName(
		const FExtendedAtlassianWorkspaceSnapshot& Snapshot)
	{
		if (!Snapshot.ConfluenceSpaceName.IsEmpty())
		{
			return Snapshot.ConfluenceSpaceName;
		}
		if (!Snapshot.ConfluenceSpaceKey.IsEmpty())
		{
			return Snapshot.ConfluenceSpaceKey;
		}
		return ProjectDisplayName(Snapshot);
	}

	FString ProjectKey(const FExtendedAtlassianWorkspaceSnapshot& Snapshot)
	{
		for (const FExtendedAtlassianBoard& Board : Snapshot.Boards)
		{
			if (!Board.ProjectKey.IsEmpty())
			{
				return Board.ProjectKey;
			}
		}
		const UExtendedAtlassianSettings* Settings =
			UExtendedAtlassianSettings::Get();
		if (Settings && !Settings->ProjectKey.IsEmpty())
		{
			return Settings->ProjectKey;
		}
		for (const FExtendedAtlassianIssue& Issue : Snapshot.Issues)
		{
			FString Prefix;
			FString Number;
			if (Issue.Key.Split(TEXT("-"), &Prefix, &Number)
				&& !Prefix.IsEmpty())
			{
				return Prefix;
			}
		}
		return TEXT("ISSUE");
	}

	FString NextIssueKey(const FExtendedAtlassianWorkspaceSnapshot& Snapshot)
	{
		const FString Prefix = ProjectKey(Snapshot);
		int32 NextNumber = 1;
		for (const FExtendedAtlassianIssue& Issue : Snapshot.Issues)
		{
			FString CandidatePrefix;
			FString Number;
			if (Issue.Key.Split(TEXT("-"), &CandidatePrefix, &Number)
				&& CandidatePrefix == Prefix)
			{
				NextNumber =
					FMath::Max(NextNumber, FCString::Atoi(*Number) + 1);
			}
		}
		return FString::Printf(TEXT("%s-%d"), *Prefix, NextNumber);
	}

	FString SelectedSprintName(
		const FExtendedAtlassianWorkspaceSnapshot& Snapshot)
	{
		if (!Snapshot.SelectedSprintId.IsEmpty())
		{
			if (const FExtendedAtlassianSprint* Sprint =
				Snapshot.Sprints.FindByPredicate(
					[&Snapshot](const FExtendedAtlassianSprint& Candidate)
					{
						return Candidate.Id == Snapshot.SelectedSprintId;
					}))
			{
				return Sprint->Name;
			}
		}
		if (const FExtendedAtlassianSprint* Sprint =
			Snapshot.Sprints.FindByPredicate(
				[](const FExtendedAtlassianSprint& Candidate)
				{
					return Candidate.State.Equals(
						TEXT("active"),
						ESearchCase::IgnoreCase);
				}))
		{
			return Sprint->Name;
		}
		return Snapshot.Sprints.IsEmpty()
			? FString(TEXT("No sprint"))
			: Snapshot.Sprints[0].Name;
	}

	FString SelectedDocumentSection(
		const FExtendedAtlassianWorkspaceSnapshot& Snapshot,
		const FString& SelectedPageId)
	{
		const FExtendedAtlassianDocumentTreeNode* PageNode =
			Snapshot.DocumentTree.FindByPredicate(
				[&SelectedPageId](const FExtendedAtlassianDocumentTreeNode& Node)
				{
					return Node.Id == SelectedPageId;
				});
		if (PageNode && !PageNode->ParentId.IsEmpty())
		{
			if (const FExtendedAtlassianDocumentTreeNode* Parent =
				Snapshot.DocumentTree.FindByPredicate(
					[PageNode](const FExtendedAtlassianDocumentTreeNode& Node)
					{
						return Node.Id == PageNode->ParentId;
					}))
			{
				return Parent->Label;
			}
		}
		if (const FExtendedAtlassianDocumentTreeNode* FirstSection =
			Snapshot.DocumentTree.FindByPredicate(
				[](const FExtendedAtlassianDocumentTreeNode& Node)
				{
					return Node.bSection;
				}))
		{
			return FirstSection->Label;
		}
		return TEXT("Documents");
	}

	FString LabelInitials(const FString& Label)
	{
		TArray<FString> Words;
		Label.ParseIntoArrayWS(Words);
		FString Result;
		for (const FString& Word : Words)
		{
			if (!Word.IsEmpty())
			{
				Result.AppendChar(FChar::ToUpper(Word[0]));
				if (Result.Len() == 2)
				{
					break;
				}
			}
		}
		return Result.IsEmpty() ? FString(TEXT("D")) : Result;
	}

	/**
	 * Initials for an issue's assignee, or empty when the issue is unassigned.
	 *
	 * Never the account id. An Atlassian account id is an opaque string like "557058:2a1f…", which
	 * inside a 20px circle rendered as unreadable overflow, and is absent on unassigned work, which
	 * rendered as nothing while still claiming to identify somebody. The display name is the field
	 * that actually names a person, so derive from that when the snapshot lookup misses — which it
	 * does for anyone not in the fetched user list.
	 */
	FString AssigneeInitials(
		const FExtendedAtlassianUser* Resolved,
		const FExtendedAtlassianIssue& Issue)
	{
		if (Resolved && !Resolved->Initials.IsEmpty())
		{
			return Resolved->Initials;
		}
		if (!Issue.AssigneeDisplayName.IsEmpty())
		{
			return LabelInitials(Issue.AssigneeDisplayName);
		}
		return FString();
	}

	/**
	 * Colour for a workflow status.
	 *
	 * The display name is matched first, so the five presentation states the design was drawn
	 * against keep their distinct colours where a workflow uses those names. Everything else falls
	 * back to the Jira status category, which is the field that exists so colouring never depends on
	 * workflow names: a Turkish, renamed, or custom workflow matched nothing here and rendered every
	 * status in the default grey.
	 *
	 * Category carries three values against the palette's five, so Blocked and In review are only
	 * reachable by name. That is the right trade: a category is always correct where it applies,
	 * whereas guessing them from a localised name would be wrong in a new way.
	 */
	FLinearColor StatusColor(
		const FString& Status,
		const FString& StatusCategoryKey = FString())
	{
		if (Status == TEXT("In progress")) { return FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")); }
		if (Status == TEXT("In review")) { return FExtendedAtlassianStyle::FromHex(TEXT("#c58fff")); }
		if (Status == TEXT("Blocked")) { return FExtendedAtlassianStyle::FromHex(TEXT("#f0665f")); }
		if (Status == TEXT("Done")) { return FExtendedAtlassianStyle::FromHex(TEXT("#57cc8a")); }

		if (StatusCategoryKey.Equals(TEXT("done"), ESearchCase::IgnoreCase))
		{
			return FExtendedAtlassianStyle::FromHex(TEXT("#57cc8a"));
		}
		if (StatusCategoryKey.Equals(TEXT("indeterminate"), ESearchCase::IgnoreCase))
		{
			return FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff"));
		}

		// "new" and an absent category both read as not-started.
		return FExtendedAtlassianStyle::FromHex(TEXT("#a2a9b4"));
	}

	const TCHAR* PinKindKey(EExtendedAtlassianPinKind Kind)
	{
		switch (Kind)
		{
		case EExtendedAtlassianPinKind::Material: return TEXT("MATERIAL");
		case EExtendedAtlassianPinKind::Level: return TEXT("LEVEL");
		case EExtendedAtlassianPinKind::Blueprint: return TEXT("BLUEPRINT");
		case EExtendedAtlassianPinKind::Page: return TEXT("PAGE");
		default: return TEXT("MATERIAL");
		}
	}

	const TCHAR* PinKindLabel(EExtendedAtlassianPinKind Kind)
	{
		switch (Kind)
		{
		case EExtendedAtlassianPinKind::Material: return TEXT("Materials");
		case EExtendedAtlassianPinKind::Level: return TEXT("Levels");
		case EExtendedAtlassianPinKind::Blueprint: return TEXT("Blueprints");
		case EExtendedAtlassianPinKind::Page: return TEXT("Pages");
		default: return TEXT("Materials");
		}
	}

	const TCHAR* PinKindGlyph(EExtendedAtlassianPinKind Kind)
	{
		switch (Kind)
		{
		case EExtendedAtlassianPinKind::Material: return TEXT("◈");
		case EExtendedAtlassianPinKind::Level: return TEXT("▣");
		case EExtendedAtlassianPinKind::Blueprint: return TEXT("◆");
		case EExtendedAtlassianPinKind::Page: return TEXT("≡");
		default: return TEXT("◈");
		}
	}

	const TCHAR* PinKindColor(EExtendedAtlassianPinKind Kind)
	{
		switch (Kind)
		{
		case EExtendedAtlassianPinKind::Material: return TEXT("#b6a9ff");
		case EExtendedAtlassianPinKind::Level: return TEXT("#58a6ff");
		case EExtendedAtlassianPinKind::Blueprint: return TEXT("#57cc8a");
		case EExtendedAtlassianPinKind::Page: return TEXT("#e3a54a");
		default: return TEXT("#b6a9ff");
		}
	}

	const TCHAR* NotificationKindLabel(EExtendedAtlassianNotificationKind Kind)
	{
		switch (Kind)
		{
		case EExtendedAtlassianNotificationKind::Mention: return TEXT("MENTION");
		case EExtendedAtlassianNotificationKind::Review: return TEXT("REVIEW");
		case EExtendedAtlassianNotificationKind::Pin: return TEXT("PIN");
		case EExtendedAtlassianNotificationKind::Assign: return TEXT("ASSIGN");
		case EExtendedAtlassianNotificationKind::Status: return TEXT("STATUS");
		case EExtendedAtlassianNotificationKind::Comment: return TEXT("COMMENT");
		case EExtendedAtlassianNotificationKind::Link: return TEXT("LINK");
		default: return TEXT("STATUS");
		}
	}

	const TCHAR* NotificationKindColor(EExtendedAtlassianNotificationKind Kind)
	{
		switch (Kind)
		{
		case EExtendedAtlassianNotificationKind::Mention: return TEXT("#b6a9ff");
		case EExtendedAtlassianNotificationKind::Review:
		case EExtendedAtlassianNotificationKind::Link: return TEXT("#58a6ff");
		case EExtendedAtlassianNotificationKind::Pin: return TEXT("#e3a54a");
		case EExtendedAtlassianNotificationKind::Assign: return TEXT("#57cc8a");
		default: return TEXT("#a2a9b4");
		}
	}

	bool InboxTabMatches(
		const FString& Tab,
		EExtendedAtlassianNotificationKind Kind)
	{
		return Tab == TEXT("All")
			|| (Tab == TEXT("Mentions") && Kind == EExtendedAtlassianNotificationKind::Mention)
			|| (Tab == TEXT("Reviews") && Kind == EExtendedAtlassianNotificationKind::Review)
			|| (Tab == TEXT("Pins") && Kind == EExtendedAtlassianNotificationKind::Pin);
	}

	bool ContainsSearch(const FString& Haystack, const FString& Search)
	{
		return Search.IsEmpty() || Haystack.Contains(Search, ESearchCase::IgnoreCase);
	}

	/** One-pixel dashed outline used by the authored add-card/add-pin controls. */
	class SBacklotDashedBorder final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBacklotDashedBorder) {}
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			SetCanTick(false);
			ChildSlot
			[
				InArgs._Content.Widget
			];
		}

		virtual int32 OnPaint(
			const FPaintArgs& Args,
			const FGeometry& AllottedGeometry,
			const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements,
			int32 LayerId,
			const FWidgetStyle& InWidgetStyle,
			bool bParentEnabled) const override
		{
			const int32 Result = SCompoundWidget::OnPaint(
				Args,
				AllottedGeometry,
				MyCullingRect,
				OutDrawElements,
				LayerId,
				InWidgetStyle,
				bParentEnabled);
			const FVector2D Size = AllottedGeometry.GetLocalSize();
			const FLinearColor Outline =
				FExtendedAtlassianStyle::FromHex(TEXT("#343a42"));
			auto DrawDashed = [
				&AllottedGeometry,
				&OutDrawElements,
				Result,
				Outline
			](const FVector2D& Start, const FVector2D& End)
			{
				const FVector2D Delta = End - Start;
				const float Length = Delta.Size();
				if (Length <= 0.0f)
				{
					return;
				}
				const FVector2D Direction = Delta / Length;
				for (float Offset = 0.0f; Offset < Length; Offset += 6.0f)
				{
					TArray<FVector2D> Segment;
					Segment.Add(Start + Direction * Offset);
					Segment.Add(
						Start + Direction * FMath::Min(Offset + 3.0f, Length));
					FSlateDrawElement::MakeLines(
						OutDrawElements,
						Result + 1,
						AllottedGeometry.ToPaintGeometry(),
						Segment,
						ESlateDrawEffect::None,
						Outline,
						true,
						1.0f);
				}
			};
			constexpr float Inset = 0.5f;
			DrawDashed(
				FVector2D(Inset, Inset),
				FVector2D(Size.X - Inset, Inset));
			DrawDashed(
				FVector2D(Size.X - Inset, Inset),
				FVector2D(Size.X - Inset, Size.Y - Inset));
			DrawDashed(
				FVector2D(Size.X - Inset, Size.Y - Inset),
				FVector2D(Inset, Size.Y - Inset));
			DrawDashed(
				FVector2D(Inset, Size.Y - Inset),
				FVector2D(Inset, Inset));
			return Result + 1;
		}
	};

	class FBacklotIssueDragDropOperation final : public FDecoratedDragDropOp
	{
	public:
		DRAG_DROP_OPERATOR_TYPE(
			FBacklotIssueDragDropOperation,
			FDecoratedDragDropOp)

		static TSharedRef<FBacklotIssueDragDropOperation> New(
			const FString& InIssueKey,
			FSimpleDelegate InOnEnded)
		{
			const TSharedRef<FBacklotIssueDragDropOperation> Operation =
				MakeShared<FBacklotIssueDragDropOperation>();
			Operation->IssueKey = InIssueKey;
			Operation->OnEnded = MoveTemp(InOnEnded);
			Operation->DefaultHoverText = FText::FromString(
				TEXT("MOVE  ") + InIssueKey);
			Operation->Construct();
			return Operation;
		}

		virtual ~FBacklotIssueDragDropOperation() override
		{
			OnEnded.ExecuteIfBound();
		}

		FString IssueKey;
		FSimpleDelegate OnEnded;
	};

	DECLARE_DELEGATE_ThreeParams(
		FOnBacklotIssueDropped,
		const FString&,
		const FString&,
		const FString&);

	class SBacklotBoardCard final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBacklotBoardCard) {}
			SLATE_ARGUMENT(FString, IssueKey)
			SLATE_ARGUMENT(FString, Status)
			SLATE_ARGUMENT(bool, CanDrag)
			SLATE_ARGUMENT(FText, DragTooltip)
			SLATE_EVENT(FSimpleDelegate, OnOpen)
			SLATE_EVENT(FOnBacklotIssueDropped, OnIssueDropped)
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			IssueKey = InArgs._IssueKey;
			Status = InArgs._Status;
			bCanDrag = InArgs._CanDrag;
			SetToolTipText(InArgs._DragTooltip);
			OnOpen = InArgs._OnOpen;
			OnIssueDropped = InArgs._OnIssueDropped;
			ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(2.0f)
					.Visibility_Lambda(
						[this]()
						{
							return bAcceptingDrop
								? EVisibility::Visible
								: EVisibility::Collapsed;
						})
					[
						SNew(SBorder)
							.BorderImage(Brush(TEXT("Backlot.Brush.BlueSolid")))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					InArgs._Content.Widget
				]
			];
		}

		virtual FReply OnMouseButtonDown(
			const FGeometry&,
			const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				return bCanDrag
					? FReply::Handled().DetectDrag(
						SharedThis(this),
						EKeys::LeftMouseButton)
					: FReply::Handled();
			}
			return FReply::Unhandled();
		}

		virtual FReply OnMouseButtonUp(
			const FGeometry&,
			const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				OnOpen.ExecuteIfBound();
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

		virtual FReply OnDragDetected(
			const FGeometry&,
			const FPointerEvent&) override
		{
			SetRenderOpacity(0.4f);
			return FReply::Handled().BeginDragDrop(
				FBacklotIssueDragDropOperation::New(
					IssueKey,
					FSimpleDelegate::CreateSP(
						SharedThis(this),
						&SBacklotBoardCard::ResetDragPresentation)));
		}

		virtual void OnDragEnter(
			const FGeometry& MyGeometry,
			const FDragDropEvent& DragDropEvent) override
		{
			SCompoundWidget::OnDragEnter(MyGeometry, DragDropEvent);
			const TSharedPtr<FBacklotIssueDragDropOperation> Operation =
				DragDropEvent.GetOperationAs<FBacklotIssueDragDropOperation>();
			bAcceptingDrop =
				Operation.IsValid() && Operation->IssueKey != IssueKey;
			Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
		}

		virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override
		{
			SCompoundWidget::OnDragLeave(DragDropEvent);
			bAcceptingDrop = false;
			Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
		}

		virtual FReply OnDrop(
			const FGeometry&,
			const FDragDropEvent& DragDropEvent) override
		{
			bAcceptingDrop = false;
			const TSharedPtr<FBacklotIssueDragDropOperation> Operation =
				DragDropEvent.GetOperationAs<FBacklotIssueDragDropOperation>();
			if (!Operation.IsValid() || Operation->IssueKey == IssueKey)
			{
				return FReply::Unhandled();
			}
			OnIssueDropped.ExecuteIfBound(
				Operation->IssueKey,
				Status,
				IssueKey);
			return FReply::Handled();
		}

	private:
		void ResetDragPresentation()
		{
			SetRenderOpacity(1.0f);
			bAcceptingDrop = false;
			Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
		}

		FString IssueKey;
		FString Status;
		FSimpleDelegate OnOpen;
		FOnBacklotIssueDropped OnIssueDropped;
		bool bAcceptingDrop = false;
		bool bCanDrag = true;
	};

	class SBacklotBoardDropTarget final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBacklotBoardDropTarget) {}
			SLATE_ARGUMENT(FString, Status)
			SLATE_EVENT(FOnBacklotIssueDropped, OnIssueDropped)
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Status = InArgs._Status;
			OnIssueDropped = InArgs._OnIssueDropped;
			ChildSlot
			[
				SNew(SBorder)
					.BorderImage_Lambda(
						[this]()
						{
							return Brush(
								bAcceptingDrop
									? TEXT("Backlot.Brush.BoardDrop")
									: TEXT("Backlot.Brush.Panel"));
						})
					.Padding(0.0f)
					[
						InArgs._Content.Widget
					]
			];
		}

		virtual int32 OnPaint(
			const FPaintArgs& Args,
			const FGeometry& AllottedGeometry,
			const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements,
			int32 LayerId,
			const FWidgetStyle& InWidgetStyle,
			bool bParentEnabled) const override
		{
			const int32 Result = SCompoundWidget::OnPaint(
				Args,
				AllottedGeometry,
				MyCullingRect,
				OutDrawElements,
				LayerId,
				InWidgetStyle,
				bParentEnabled);
			if (!bAcceptingDrop)
			{
				return Result;
			}
			const FVector2D Size = AllottedGeometry.GetLocalSize();
			const float Inset = 3.0f;
			const FLinearColor Outline =
				FExtendedAtlassianStyle::FromHex(TEXT("#3d7fc4"));
			auto DrawDashed = [
				&AllottedGeometry,
				&OutDrawElements,
				Result,
				Outline
			](const FVector2D& Start, const FVector2D& End)
			{
				const FVector2D Delta = End - Start;
				const float Length = Delta.Size();
				if (Length <= 0.0f)
				{
					return;
				}
				const FVector2D Direction = Delta / Length;
				for (float Offset = 0.0f; Offset < Length; Offset += 8.0f)
				{
					TArray<FVector2D> Segment;
					Segment.Add(Start + Direction * Offset);
					Segment.Add(
						Start + Direction * FMath::Min(Offset + 5.0f, Length));
					FSlateDrawElement::MakeLines(
						OutDrawElements,
						Result + 1,
						AllottedGeometry.ToPaintGeometry(),
						Segment,
						ESlateDrawEffect::None,
						Outline,
						true,
						1.0f);
				}
			};
			DrawDashed(
				FVector2D(Inset, Inset),
				FVector2D(Size.X - Inset, Inset));
			DrawDashed(
				FVector2D(Size.X - Inset, Inset),
				FVector2D(Size.X - Inset, Size.Y - Inset));
			DrawDashed(
				FVector2D(Size.X - Inset, Size.Y - Inset),
				FVector2D(Inset, Size.Y - Inset));
			DrawDashed(
				FVector2D(Inset, Size.Y - Inset),
				FVector2D(Inset, Inset));
			return Result + 1;
		}

		virtual void OnDragEnter(
			const FGeometry& MyGeometry,
			const FDragDropEvent& DragDropEvent) override
		{
			SCompoundWidget::OnDragEnter(MyGeometry, DragDropEvent);
			bAcceptingDrop =
				DragDropEvent.GetOperationAs<FBacklotIssueDragDropOperation>().IsValid();
			Invalidate(EInvalidateWidgetReason::Paint);
		}

		virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override
		{
			SCompoundWidget::OnDragLeave(DragDropEvent);
			bAcceptingDrop = false;
			Invalidate(EInvalidateWidgetReason::Paint);
		}

		virtual FReply OnDrop(
			const FGeometry&,
			const FDragDropEvent& DragDropEvent) override
		{
			bAcceptingDrop = false;
			const TSharedPtr<FBacklotIssueDragDropOperation> Operation =
				DragDropEvent.GetOperationAs<FBacklotIssueDragDropOperation>();
			if (!Operation.IsValid())
			{
				return FReply::Unhandled();
			}
			OnIssueDropped.ExecuteIfBound(
				Operation->IssueKey,
				Status,
				FString());
			return FReply::Handled();
		}

	private:
		FString Status;
		FOnBacklotIssueDropped OnIssueDropped;
		bool bAcceptingDrop = false;
	};

	/** Mouse-interactive 528 Ã— 288 annotation plane from the reference composer. */
	class SCaptureAnnotationSurface final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCaptureAnnotationSurface) {}
			SLATE_ARGUMENT(TArray<FExtendedAtlassianAnnotation>*, Annotations)
			SLATE_ARGUMENT(const FSlateBrush*, PreviewBrush)
			SLATE_ATTRIBUTE(FString, ActiveTool)
			SLATE_EVENT(FSimpleDelegate, OnChanged)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Annotations = InArgs._Annotations;
			PreviewBrush = InArgs._PreviewBrush;
			ActiveTool = InArgs._ActiveTool;
			OnChanged = InArgs._OnChanged;
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D(528.0f, 288.0f);
		}

		virtual int32 OnPaint(
			const FPaintArgs& Args,
			const FGeometry& AllottedGeometry,
			const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements,
			int32 LayerId,
			const FWidgetStyle& InWidgetStyle,
			bool bParentEnabled) const override
		{
			(void)Args;
			(void)bParentEnabled;
			const FVector2D Size = AllottedGeometry.GetLocalSize();
			static const FSlateColorBrush CaptureBase(
				FExtendedAtlassianStyle::FromHex(TEXT("#1a1c20")));
			const auto PaintTint =
				[&InWidgetStyle](const FSlateBrush& PaintBrush)
				{
					return PaintBrush.GetTint(InWidgetStyle)
						* InWidgetStyle.GetColorAndOpacityTint();
				};
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				&CaptureBase,
				ESlateDrawEffect::None,
				PaintTint(CaptureBase));
			const FLinearColor DarkBand =
				FExtendedAtlassianStyle::FromHex(TEXT("#171a1e"));
			constexpr float Period = 31.1127f;
			constexpr float StrokeWidth = 15.5563f;
			OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));
			for (float Offset = -Size.Y; Offset < Size.X + Size.Y;
				Offset += Period)
			{
				TArray<FVector2D> Segment;
				Segment.Reserve(2);
				Segment.Add(FVector2D(Offset, 0.0f));
				Segment.Add(FVector2D(Offset - Size.Y, Size.Y));
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId + 1,
					AllottedGeometry.ToPaintGeometry(),
					Segment,
					ESlateDrawEffect::None,
					DarkBand,
					true,
					StrokeWidth);
			}
			OutDrawElements.PopClip();
			if (PreviewBrush)
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId + 2,
					AllottedGeometry.ToPaintGeometry(),
					PreviewBrush,
					ESlateDrawEffect::None,
					PaintTint(*PreviewBrush));
			}

			if (!PreviewBrush)
			{
				FSlateDrawElement::MakeText(
					OutDrawElements,
					LayerId + 1,
					AllottedGeometry.ToPaintGeometry(
						FVector2D(250.0f, 16.0f),
						FSlateLayoutTransform(
							FVector2D(
								FMath::Max(0.0f, Size.X * 0.5f - 125.0f),
								FMath::Max(0.0f, Size.Y * 0.5f - 8.0f)))),
					Annotations && !Annotations->IsEmpty()
						? LOCTEXT("CapturedFrameHint", "CAPTURED FRAME")
						: LOCTEXT(
							"EmptyCapturedFrame",
							"CAPTURED FRAME · CLICK TO ANNOTATE"),
					Text(TEXT("Backlot.Mono.10")).Font,
					ESlateDrawEffect::None,
					FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")));
			}

			static const TCHAR* Colors[] = {
				TEXT("#e3a54a"),
				TEXT("#58a6ff"),
				TEXT("#57cc8a"),
				TEXT("#f0665f"),
				TEXT("#b6a9ff")
			};
			static const FSlateRoundedBoxBrush PinAmber(
				FExtendedAtlassianStyle::FromHex(TEXT("#e3a54a")), 11.0f);
			static const FSlateRoundedBoxBrush PinBlue(
				FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")), 11.0f);
			static const FSlateRoundedBoxBrush PinGreen(
				FExtendedAtlassianStyle::FromHex(TEXT("#57cc8a")), 11.0f);
			static const FSlateRoundedBoxBrush PinRed(
				FExtendedAtlassianStyle::FromHex(TEXT("#f0665f")), 11.0f);
			static const FSlateRoundedBoxBrush PinPurple(
				FExtendedAtlassianStyle::FromHex(TEXT("#b6a9ff")), 11.0f);
			static const FSlateRoundedBoxBrush* PinBrushes[] = {
				&PinAmber, &PinBlue, &PinGreen, &PinRed, &PinPurple
			};
			static const FSlateRoundedBoxBrush BoxBlue(
				FLinearColor::Transparent, 5.0f,
				FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")), 2.0f);
			static const FSlateRoundedBoxBrush BlurBlue(
				FLinearColor(FColor(190, 205, 225, 41)), 5.0f,
				FLinearColor(FColor(190, 205, 225, 82)), 1.0f);
			static const FSlateRoundedBoxBrush PinShadowOuter(
				FLinearColor(FColor(0, 0, 0, 28)), 14.0f);
			static const FSlateRoundedBoxBrush PinShadowInner(
				FLinearColor(FColor(0, 0, 0, 72)), 12.0f);
			static const FSlateRoundedBoxBrush RegionShadowOuter(
				FLinearColor(FColor(0, 0, 0, 28)), 8.0f);
			static const FSlateRoundedBoxBrush RegionShadowInner(
				FLinearColor(FColor(0, 0, 0, 72)), 6.0f);
			int32 PinNumber = 0;
			for (const FExtendedAtlassianAnnotation& Annotation : *Annotations)
			{
				const FVector2D Center(
					Annotation.NormalizedPosition.X * Size.X,
					Annotation.NormalizedPosition.Y * Size.Y);
				const int32 ColorIndex =
					FMath::Abs(Annotation.ColorIndex) % UE_ARRAY_COUNT(Colors);
				if (Annotation.Kind == EExtendedAtlassianAnnotationKind::Pin)
				{
					++PinNumber;
					FSlateDrawElement::MakeBox(
						OutDrawElements,
						LayerId + 2,
						AllottedGeometry.ToPaintGeometry(
							FVector2D(28.0f, 28.0f),
							FSlateLayoutTransform(Center - FVector2D(14.0f, 11.0f))),
						&PinShadowOuter,
						ESlateDrawEffect::None,
						PaintTint(PinShadowOuter));
					FSlateDrawElement::MakeBox(
						OutDrawElements,
						LayerId + 3,
						AllottedGeometry.ToPaintGeometry(
							FVector2D(24.0f, 24.0f),
							FSlateLayoutTransform(Center - FVector2D(12.0f, 10.0f))),
						&PinShadowInner,
						ESlateDrawEffect::None,
						PaintTint(PinShadowInner));
					FSlateDrawElement::MakeBox(
						OutDrawElements,
						LayerId + 4,
						AllottedGeometry.ToPaintGeometry(
							FVector2D(22.0f, 22.0f),
							FSlateLayoutTransform(Center - FVector2D(11.0f))),
						PinBrushes[ColorIndex],
						ESlateDrawEffect::None,
						PaintTint(*PinBrushes[ColorIndex]));
					FSlateDrawElement::MakeText(
						OutDrawElements,
						LayerId + 5,
						AllottedGeometry.ToPaintGeometry(
							FVector2D(20.0f, 14.0f),
							FSlateLayoutTransform(
								Center - FVector2D(10.0f, 7.0f))),
						FText::AsNumber(PinNumber),
						Text(TEXT("Backlot.Mono.9.Medium")).Font,
						ESlateDrawEffect::None,
						FExtendedAtlassianStyle::FromHex(TEXT("#0f1114")));
					continue;
				}

				const bool bBlur =
					Annotation.Kind == EExtendedAtlassianAnnotationKind::Blur;
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId + 2,
					AllottedGeometry.ToPaintGeometry(
						FVector2D(98.0f, 68.0f),
						FSlateLayoutTransform(
							Center - FVector2D(49.0f, 32.0f))),
					&RegionShadowOuter,
					ESlateDrawEffect::None,
					PaintTint(RegionShadowOuter));
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId + 3,
					AllottedGeometry.ToPaintGeometry(
						FVector2D(94.0f, 64.0f),
						FSlateLayoutTransform(
							Center - FVector2D(47.0f, 30.0f))),
					&RegionShadowInner,
					ESlateDrawEffect::None,
					PaintTint(RegionShadowInner));
				const FSlateBrush& RegionBrush =
					bBlur
						? static_cast<const FSlateBrush&>(BlurBlue)
						: static_cast<const FSlateBrush&>(BoxBlue);
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId + 4,
					AllottedGeometry.ToPaintGeometry(
						FVector2D(92.0f, 62.0f),
						FSlateLayoutTransform(
							Center - FVector2D(46.0f, 31.0f))),
					&RegionBrush,
					ESlateDrawEffect::None,
					PaintTint(RegionBrush));
			}
			return LayerId + 5;
		}

		virtual FReply OnMouseButtonDown(
			const FGeometry& MyGeometry,
			const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton
				|| !Annotations)
			{
				return FReply::Unhandled();
			}
			const FVector2D Size = MyGeometry.GetLocalSize();
			const FVector2D Local = MyGeometry.AbsoluteToLocal(
				MouseEvent.GetScreenSpacePosition());
			for (int32 Index = Annotations->Num() - 1; Index >= 0; --Index)
			{
				const FExtendedAtlassianAnnotation& Annotation =
					(*Annotations)[Index];
				const FVector2D Center(
					Annotation.NormalizedPosition.X * Size.X,
					Annotation.NormalizedPosition.Y * Size.Y);
				const FVector2D HalfSize =
					Annotation.Kind == EExtendedAtlassianAnnotationKind::Pin
						? FVector2D(11.0f)
						: FVector2D(46.0f, 31.0f);
				if (FMath::Abs(Local.X - Center.X) <= HalfSize.X
					&& FMath::Abs(Local.Y - Center.Y) <= HalfSize.Y)
				{
					Annotations->RemoveAt(Index);
					OnChanged.ExecuteIfBound();
					return FReply::Handled();
				}
			}

			FExtendedAtlassianAnnotation Annotation;
			Annotation.Id = FString::Printf(
				TEXT("capture-annotation-%d"),
				Annotations->Num() + 1);
			const FString Tool = ActiveTool.Get(TEXT("PIN"));
			Annotation.Kind =
				Tool == TEXT("BOX")
					? EExtendedAtlassianAnnotationKind::Box
					: Tool == TEXT("BLUR")
						? EExtendedAtlassianAnnotationKind::Blur
						: EExtendedAtlassianAnnotationKind::Pin;
			const FVector2D Clamped(
				FMath::Clamp(Local.X, 16.0f, FMath::Max(16.0f, Size.X - 16.0f)),
				FMath::Clamp(Local.Y, 16.0f, FMath::Max(16.0f, Size.Y - 16.0f)));
			Annotation.NormalizedPosition = FVector2D(
				Size.X > 0.0f ? Clamped.X / Size.X : 0.0f,
				Size.Y > 0.0f ? Clamped.Y / Size.Y : 0.0f);
			Annotation.NormalizedSize =
				Annotation.Kind == EExtendedAtlassianAnnotationKind::Pin
					? FVector2D::ZeroVector
					: FVector2D(92.0f / 528.0f, 62.0f / 288.0f);
			Annotation.ColorIndex = Annotations->Num() % 5;
			Annotations->Add(MoveTemp(Annotation));
			OnChanged.ExecuteIfBound();
			return FReply::Handled();
		}

		virtual FCursorReply OnCursorQuery(
			const FGeometry&,
			const FPointerEvent&) const override
		{
			return FCursorReply::Cursor(EMouseCursor::Crosshairs);
		}

	private:
		TArray<FExtendedAtlassianAnnotation>* Annotations = nullptr;
		const FSlateBrush* PreviewBrush = nullptr;
		TAttribute<FString> ActiveTool;
		FSimpleDelegate OnChanged;
	};
}

void SExtendedAtlassianWorkspace::Construct(const FArguments& InArgs)
{
	TSharedPtr<IExtendedAtlassianWorkspaceData> WorkspaceData =
		InArgs._WorkspaceData;
	if (!WorkspaceData.IsValid())
	{
		if (FParse::Param(
				FCommandLine::Get(),
				TEXT("ExtendedAtlassianFixture")))
		{
			WorkspaceData =
				StaticCastSharedRef<IExtendedAtlassianWorkspaceData>(
					MakeShared<FExtendedAtlassianFixtureWorkspaceData>());
		}
		else
		{
			WorkspaceData =
				StaticCastSharedRef<IExtendedAtlassianWorkspaceData>(
					MakeShared<FExtendedAtlassianLiveWorkspaceData>());
		}
	}
	HostServices = InArgs._HostServices.IsValid()
		? InArgs._HostServices
		: MakeShared<FExtendedAtlassianSystemWorkspaceHostServices>();
	bHighContrastEnabled = HostServices->ShouldUseHighContrast();
	bAnimationsEnabled =
		InArgs._AnimationsEnabled
		&& !HostServices->ShouldReduceMotion()
		&& !bHighContrastEnabled;
	Controller = MakeShared<FExtendedAtlassianWorkspaceController>(
		WorkspaceData.ToSharedRef(),
		InArgs._InteractionClock);
	if (Controller->IsFixtureProvider())
	{
		CaptureAnnotations = {
			{
				TEXT("fixture-capture-1"),
				EExtendedAtlassianAnnotationKind::Pin,
				FVector2D(143.0 / 528.0, 73.0 / 288.0),
				FVector2D::ZeroVector,
				0
			},
			{
				TEXT("fixture-capture-2"),
				EExtendedAtlassianAnnotationKind::Pin,
				FVector2D(329.0 / 528.0, 179.0 / 288.0),
				FVector2D::ZeroVector,
				1
			}
		};
	}
	Controller->Navigate(InArgs._StartRoute);
	ChangedHandle = Controller->OnChanged().AddSP(
		this,
		&SExtendedAtlassianWorkspace::HandleControllerChanged);
	if (const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient())
	{
		AuthChangedHandle = Client->OnAuthStateChanged().AddSP(
			this,
			&SExtendedAtlassianWorkspace::HandleAuthStateChanged);
	}

	SAssignNew(RootOverlay, SOverlay);
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(
			bHighContrastEnabled
				? ExtendedAtlassianWorkspacePrivate::Brush(
					TEXT("Backlot.Brush.HighContrastFrame"))
				: FStyleDefaults::GetNoBrush())
		.Padding(bHighContrastEnabled ? 2.0f : 0.0f)
		[
			RootOverlay.ToSharedRef()
		]
	];

	Rebuild();
	LastAuthConfigurationSignature =
		ExtendedAtlassianWorkspacePrivate::AuthConfigurationSignature();
	Controller->Refresh();
	LastSyncPollSeconds = HostServices->NowSeconds();
	StartWatchingDocuments();
	SettingsObjectChangedHandle =
		FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(
			this,
			&SExtendedAtlassianWorkspace::HandleSettingsObjectChanged);
	EnsureInteractionTimer();
	ScheduleBackgroundSync();
}

SExtendedAtlassianWorkspace::~SExtendedAtlassianWorkspace()
{
	if (const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient())
	{
		Client->OnAuthStateChanged().Remove(AuthChangedHandle);
	}
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(
		SettingsObjectChangedHandle);
	StopWatchingDocuments();
	if (CapturedViewportBrush.IsValid())
	{
		CapturedViewportBrush->ReleaseResource();
		CapturedViewportBrush.Reset();
	}
	if (Controller.IsValid())
	{
		Controller->OnChanged().Remove(ChangedHandle);
	}
}

void SExtendedAtlassianWorkspace::Navigate(EExtendedAtlassianWorkspaceRoute Route)
{
	if (Controller.IsValid())
	{
		if (bDocumentEditing && Route != EExtendedAtlassianWorkspaceRoute::Docs)
		{
			ResetDocumentEditState();
		}
		Controller->Navigate(Route);
	}
}

void SExtendedAtlassianWorkspace::Refresh()
{
	if (Controller.IsValid())
	{
		Controller->Refresh();
	}
}

FString SExtendedAtlassianWorkspace::ExportNormalizedStateForAutomation() const
{
	return Controller.IsValid()
		? Controller->ExportNormalizedState()
		: TEXT("controller=invalid");
}

FString SExtendedAtlassianWorkspace::ExportOverlayStateForAutomation() const
{
	return FString::Printf(
		TEXT("capture=%d|createCard=%d|pagePopover=%d|pinPopover=%d|")
		TEXT("menu=%d|confirm=%d|documentEdit=%d|issueEdit=%d"),
		bCaptureOpen ? 1 : 0,
		bCreateCardOpen ? 1 : 0,
		bPagePopoverOpen ? 1 : 0,
		bPinPopoverOpen ? 1 : 0,
		bMenuOpen ? 1 : 0,
		bConfirmOpen ? 1 : 0,
		bDocumentEditing ? 1 : 0,
		bIssueEditing ? 1 : 0);
}

float SExtendedAtlassianWorkspace::ExportOverlayAnimationProgressForAutomation()
	const
{
	return OverlayAnimationProgress();
}

FString SExtendedAtlassianWorkspace::ExportSchedulingStateForAutomation() const
{
	return FString::Printf(
		TEXT("interaction=%d|searchDebounce=%d|backgroundSync=%d"),
		bInteractionTimerRegistered ? 1 : 0,
		bSearchDebounceTimerRegistered ? 1 : 0,
		bBackgroundSyncTimerRegistered ? 1 : 0);
}

void SExtendedAtlassianWorkspace::SetGlobalSearchForAutomation(
	const FString& Value)
{
	OnSearchChanged(FText::FromString(Value));
}

void SExtendedAtlassianWorkspace::RevealDocumentAssetForAutomation(
	const FString& AssetName,
	const FString& PathOrMeta)
{
	OnDocumentAssetClicked(AssetName, PathOrMeta);
}

void SExtendedAtlassianWorkspace::HandleControllerChanged()
{
	if (bDocumentEditing
		&& (Controller->GetRoute() != EExtendedAtlassianWorkspaceRoute::Docs
			|| Controller->GetSelectedPageId() != EditingDocumentPageId))
	{
		// The reference intentionally abandons drafts during primary or page navigation.
		ResetDocumentEditState();
	}
	if (bDocumentPublishPending && !Controller->IsMutating())
	{
		bDocumentPublishPending = false;
		if (Controller->GetLastMutationError().IsSet())
		{
			bDocumentEditing = true;
		}
		else
		{
			const int32 PublishedVersion = SelectedPage()
				? SelectedPage()->Version
				: 1;
			ResetDocumentEditState();
			Controller->ShowToast(FText::Format(
				LOCTEXT(
					"DocumentPublishedToast",
					"Page published  ·  v{0}"),
				FText::AsNumber(PublishedVersion)));
		}
	}
	if (bCaptureMutationPending && !Controller->IsMutating())
	{
		if (Controller->GetLastMutationError().IsSet())
		{
			bCaptureMutationPending = false;
			bCaptureOpen = true;
			CaptureTitle = PendingCaptureSummary;
			CapturedViewportBrush =
				FExtendedAtlassianScreenshot::CreatePreviewBrush(
					CapturedViewportPng,
					FVector2D(528.0f, 288.0f));
		}
		else if (
			Controller->GetSnapshot().State == EExtendedAtlassianLoadState::Ready
			|| Controller->GetSnapshot().State == EExtendedAtlassianLoadState::Empty)
		{
			const FString ExpectedSummary =
				PendingCaptureSummary.TrimStartAndEnd().IsEmpty()
					? FString(TEXT("Untitled capture from the viewport"))
					: PendingCaptureSummary.TrimStartAndEnd();
			const FExtendedAtlassianIssue* Created =
				Controller->GetSnapshot().Issues.FindByPredicate(
					[&ExpectedSummary](const FExtendedAtlassianIssue& Issue)
					{
						return Issue.Summary == ExpectedSummary;
					});
			if (!Created && !Controller->GetSnapshot().Issues.IsEmpty())
			{
				Created = &Controller->GetSnapshot().Issues[0];
			}
			if (Created)
			{
				bCaptureMutationPending = false;
				CaptureAnnotations.Reset();
				Controller->SelectIssue(Created->Key);
				if (Controller->GetLastMutationWarning().IsSet())
				{
					Controller->ShowToast(FText::FromString(
						Controller->GetLastMutationWarning().Message));
				}
				else
				{
					Controller->ShowToast(FText::Format(
						PendingCaptureAnnotationCount == 1
							? LOCTEXT(
								"LiveCaptureCreatedOneToast",
								"Issue created with {0} annotation  {1}")
							: LOCTEXT(
								"LiveCaptureCreatedManyToast",
								"Issue created with {0} annotations  {1}"),
						FText::AsNumber(PendingCaptureAnnotationCount),
						FText::FromString(Created->Key)));
				}
			}
		}
	}
	if (bIssueEditing
		&& !bIssueEditMutationPending
		&& (Controller->GetRoute() != EExtendedAtlassianWorkspaceRoute::IssueDetail
			|| Controller->GetSelectedIssueKey() != EditingIssueKey))
	{
		bIssueEditing = false;
		EditingIssueKey.Reset();
		IssueDraftSummary.Reset();
		IssueDraftDescription.Reset();
		IssueEditConflictWarning = FText::GetEmpty();
	}
	if (bIssueEditing && !bIssueEditMutationPending)
	{
		if (const FExtendedAtlassianIssue* Issue = SelectedIssue();
			Issue
			&& IssueEditBaseUpdated != FDateTime::MinValue()
			&& Issue->Updated != FDateTime::MinValue()
			&& Issue->Updated != IssueEditBaseUpdated)
		{
			IssueEditConflictWarning = LOCTEXT(
				"IssueEditStaleWarning",
				"This issue changed in Jira while you were editing. Cancel and reopen Edit to use the latest version.");
		}
	}
	if (bIssueEditMutationPending && !Controller->IsMutating())
	{
		bIssueEditMutationPending = false;
		if (Controller->GetLastMutationError().IsSet())
		{
			bIssueEditing = true;
			IssueDraftSummary = PendingIssueDraftSummary;
			IssueDraftDescription = PendingIssueDraftDescription;
		}
		else
		{
			bIssueEditing = false;
			EditingIssueKey.Reset();
			IssueDraftSummary.Reset();
			IssueDraftDescription.Reset();
			IssueEditConflictWarning = FText::GetEmpty();
			Controller->ShowToast(LOCTEXT("IssueUpdatedToast", "Issue updated"));
		}
		PendingIssueDraftSummary.Reset();
		PendingIssueDraftDescription.Reset();
	}
	if (bIssueComposerMutationPending && !Controller->IsMutating())
	{
		bIssueComposerMutationPending = false;
		if (Controller->GetLastMutationError().IsSet())
		{
			NewIssueCommentDraft = PendingIssueCommentDraft;
			bIssueCommentAttachCapture = bPendingIssueCommentAttachCapture;
		}
		else
		{
			NewIssueCommentDraft.Reset();
			bIssueCommentAttachCapture = false;
			Controller->ShowToast(
				LOCTEXT("IssueCommentPostedToast", "Comment posted"));
		}
		PendingIssueCommentDraft.Reset();
		bPendingIssueCommentAttachCapture = false;
	}
	if (bCommentMutationPending && !Controller->IsMutating())
	{
		bCommentMutationPending = false;
		if (Controller->GetLastMutationError().IsSet())
		{
			if (bPendingCommentReply)
			{
				ReplyingCommentId = PendingCommentId;
				ReplyDraft = PendingCommentDraft;
			}
			else
			{
				EditingCommentId = PendingCommentId;
				CommentEditDraft = PendingCommentDraft;
			}
		}
		else
		{
			Controller->ShowToast(
				bPendingCommentReply
					? (bPendingCommentIssue
						? LOCTEXT("IssueReplyPostedToast", "Reply posted  ISSUE COMMENT")
						: LOCTEXT("PageReplyPostedToast", "Reply posted  PAGE COMMENT"))
					: LOCTEXT("CommentUpdatedToast", "Comment updated"));
		}
		PendingCommentId.Reset();
		PendingCommentDraft.Reset();
		PendingCommentScope.Reset();
	}
	EnsureInteractionTimer();
	Rebuild();
}

void SExtendedAtlassianWorkspace::Rebuild()
{
	if (!RootOverlay.IsValid())
	{
		return;
	}

	const uint8 OverlayMask =
		(bCreateCardOpen ? 1 : 0)
		| (bPagePopoverOpen ? 2 : 0)
		| (bPinPopoverOpen ? 4 : 0)
		| (bCaptureOpen ? 8 : 0)
		| (bMenuOpen ? 16 : 0)
		| (bConfirmOpen ? 32 : 0);
	if ((OverlayMask & ~LastOverlayMask) != 0)
	{
		OverlayAnimationStartedAt = HostServices->NowSeconds();
	}
	LastOverlayMask = OverlayMask;
	const FExtendedAtlassianToast& Toast = Controller->GetToast();
	if (Toast.IsSet() && Toast.Key != LastAnimatedToastKey)
	{
		LastAnimatedToastKey = Toast.Key;
		ToastAnimationStartedAt = HostServices->NowSeconds();
	}

	RootOverlay->ClearChildren();
	RootOverlay->AddSlot()
		.HAlign(HAlign_Fill)
		[
			BuildShell()
		];
	if (bCreateCardOpen)
	{
		RootOverlay->AddSlot()[BuildCreateCardPopover()];
	}
	if (bPagePopoverOpen)
	{
		RootOverlay->AddSlot()[BuildPagePopover()];
	}
	if (bPinPopoverOpen)
	{
		RootOverlay->AddSlot()[BuildPinPopover()];
	}
	if (bCaptureOpen)
	{
		RootOverlay->AddSlot()
		[
			BuildCaptureOverlay()
		];
	}
	if (Controller->GetToast().IsSet())
	{
		RootOverlay->AddSlot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(0.0f, 0.0f, 0.0f, 26.0f)
		[
			// `animation: bl-toast .2s ease both` — opacity plus a 10px rise, no scale.
			AnimatedPanel(BuildToast(), 0.20f, 10.0f, false)
		];
	}
	if (bMenuOpen)
	{
		RootOverlay->AddSlot()[BuildGenericMenu()];
	}
	if (bConfirmOpen)
	{
		RootOverlay->AddSlot()[BuildConfirmDialog()];
	}
	ExtendedAtlassianWorkspacePrivate::ApplyAutomationMetadata(
		RootOverlay.ToSharedRef());
}

float SExtendedAtlassianWorkspace::OverlayAnimationProgress() const
{
	if (!bAnimationsEnabled || !HostServices.IsValid())
	{
		return 1.0f;
	}
	return FMath::Clamp(
		static_cast<float>(
			(HostServices->NowSeconds() - OverlayAnimationStartedAt) / 0.18),
		0.0f,
		1.0f);
}

float SExtendedAtlassianWorkspace::ToastAnimationProgress() const
{
	if (!bAnimationsEnabled || !HostServices.IsValid())
	{
		return 1.0f;
	}
	return FMath::Clamp(
		static_cast<float>(
			(HostServices->NowSeconds() - ToastAnimationStartedAt) / 0.20),
		0.0f,
		1.0f);
}

float SExtendedAtlassianWorkspace::ClampedOverlayWidth(
	const TCHAR* PixelMetric,
	const TCHAR* ViewportPercentMetric) const
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const float AuthoredPixels = Metric(PixelMetric);
	const float Available = static_cast<float>(GetCachedGeometry().GetLocalSize().X);
	if (Available <= KINDA_SMALL_NUMBER)
	{
		return AuthoredPixels;
	}
	return FMath::Min(
		AuthoredPixels,
		Available * Metric(ViewportPercentMetric) / 100.0f);
}

float SExtendedAtlassianWorkspace::ClampedOverlayHeight(
	const TCHAR* ViewportPercentMetric) const
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const float Available = static_cast<float>(GetCachedGeometry().GetLocalSize().Y);
	if (Available <= KINDA_SMALL_NUMBER)
	{
		return Metric(TEXT("Backlot.Metric.Viewport.MinHeight"))
			* Metric(ViewportPercentMetric) / 100.0f;
	}
	return Available * Metric(ViewportPercentMetric) / 100.0f;
}

FOptionalSize SExtendedAtlassianWorkspace::CaptureWidth() const
{
	// `width: min(880px, 92vw)`
	return FOptionalSize(
		ClampedOverlayWidth(
			TEXT("Backlot.Metric.Capture.Width"),
			TEXT("Backlot.Metric.Capture.ViewportPercent")));
}

FOptionalSize SExtendedAtlassianWorkspace::CaptureMaxHeight() const
{
	// `max-height: 92vh`
	return FOptionalSize(
		ClampedOverlayHeight(TEXT("Backlot.Metric.Capture.MaxHeightPercent")));
}

FOptionalSize SExtendedAtlassianWorkspace::ConfirmWidth() const
{
	// `width: min(384px, 90vw)`
	return FOptionalSize(
		ClampedOverlayWidth(
			TEXT("Backlot.Metric.Confirm.Width"),
			TEXT("Backlot.Metric.Confirm.ViewportPercent")));
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::AnimatedPanel(
	const TSharedRef<SWidget>& Content,
	float DurationSeconds,
	float RisePixels,
	bool bScale)
{
	// bl-toast is the only keyframe that does not scale; it also uses the toast clock.
	const double StartedAt = bScale ? OverlayAnimationStartedAt : ToastAnimationStartedAt;
	const TSharedPtr<IExtendedAtlassianWorkspaceHostServices> Services = HostServices;
	SBacklotAnimatedPanel::FOnGetTimeSeconds TimeAccessor;
	if (Services.IsValid())
	{
		TimeAccessor = SBacklotAnimatedPanel::FOnGetTimeSeconds::CreateLambda(
			[Services]() { return Services->NowSeconds(); });
	}
	const bool bToast = RisePixels >= 9.5f;
	const float ShadowOffset = bToast ? 14.0f : (bScale ? 20.0f : 16.0f);
	const float ShadowBlur = bToast ? 34.0f : (bScale ? 46.0f : 34.0f);
	const float ShadowAlpha = bToast ? 0.50f : (bScale ? 0.58f : 0.55f);
	const TSharedRef<SWidget> ShadowedContent =
		SNew(SBacklotDropShadow)
		.OffsetY(ShadowOffset)
		.Blur(ShadowBlur)
		.ShadowColor(FLinearColor(0.0f, 0.0f, 0.0f, ShadowAlpha))
		.CornerRadius(bScale ? 12.0f : 7.0f)
		[
			Content
		];
	return SNew(SBacklotAnimatedPanel)
		.StartedAtSeconds(StartedAt)
		.DurationSeconds(DurationSeconds)
		.RisePixels(RisePixels)
		.StartScale(bScale ? 0.99f : 1.0f)
		.Animate(bAnimationsEnabled && Services.IsValid())
		.OnGetTimeSeconds(TimeAccessor)
		[
			ShadowedContent
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildShell()
{
	using namespace ExtendedAtlassianWorkspacePrivate;

	const TSharedRef<SVerticalBox> MainClient = SNew(SVerticalBox);
	MainClient->AddSlot()
	.AutoHeight()
	[
		BuildCommandHeader()
	];
	MainClient->AddSlot()
	.AutoHeight()
	[
		BuildWorkspaceStatusBanner()
	];

	const TSharedRef<SSplitter> MainBody = SNew(SSplitter)
		.Orientation(Orient_Horizontal);
	MainBody->AddSlot()
	.Value_Lambda([this]() { return 1.0f - RightRailFraction; })
	.MinSize(420.0f)
	.OnSlotResized_Lambda(
		[this](float NewValue)
		{
			RightRailFraction = FMath::Clamp(1.0f - NewValue, 0.12f, 0.50f);
		})
	[
		BuildMainView()
	];
	if (!Controller->IsCompact() && Controller->IsRailOpen())
	{
		MainBody->AddSlot()
		.Value_Lambda([this]() { return RightRailFraction; })
		.MinSize(
			ExtendedAtlassianWorkspacePrivate::Metric(
				TEXT("Backlot.Metric.RightRail.MinWidth")) + 1.0f)
		.OnSlotResized_Lambda(
			[this](float NewValue)
			{
				RightRailFraction = FMath::Clamp(NewValue, 0.12f, 0.50f);
			})
		[
			SNew(SBorder)
			.BorderImage(Brush(TEXT("Backlot.Brush.BorderSubtle")))
			.Padding(FMargin(1.0f, 0.0f, 0.0f, 0.0f))
			[
				BuildRightRail()
			]
		];
	}
	MainClient->AddSlot()
	.FillHeight(1.0f)
	[
		MainBody
	];

	const TSharedRef<SHorizontalBox> Dock = SNew(SHorizontalBox);
	Dock->AddSlot()
	.AutoWidth()
	[
		BuildNavigationRail()
	];

	if (Controller->IsCompact())
	{
		Dock->AddSlot()
		.FillWidth(1.0f)
		[
			MainClient
		];
	}
	else
	{
		const TSharedRef<SSplitter> WorkspaceSplitter = SNew(SSplitter)
			.Orientation(Orient_Horizontal);
		WorkspaceSplitter->AddSlot()
		.Value_Lambda([this]() { return ContextSidebarFraction; })
		.MinSize(
			ExtendedAtlassianWorkspacePrivate::Metric(
				TEXT("Backlot.Metric.Sidebar.MinWidth")) + 1.0f)
		.OnSlotResized_Lambda(
			[this](float NewValue)
			{
				ContextSidebarFraction = FMath::Clamp(NewValue, 0.10f, 0.45f);
			})
		[
			SNew(SBorder)
			.BorderImage(Brush(TEXT("Backlot.Brush.BorderSubtle")))
			.Padding(FMargin(0.0f, 0.0f, 1.0f, 0.0f))
			[
				BuildContextSidebar()
			]
		];
		WorkspaceSplitter->AddSlot()
		.Value_Lambda([this]() { return 1.0f - ContextSidebarFraction; })
		.MinSize(480.0f)
		.OnSlotResized_Lambda(
			[this](float NewValue)
			{
				ContextSidebarFraction = FMath::Clamp(1.0f - NewValue, 0.10f, 0.45f);
			})
		[
			MainClient
		];

		Dock->AddSlot()
		.FillWidth(1.0f)
		[
			WorkspaceSplitter
		];
	}

	const TSharedRef<SWidget> Shell = SNew(SBorder)
		.BorderImage(ExtendedAtlassianWorkspacePrivate::Brush(TEXT("Backlot.Brush.Root")))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildProjectStrip()
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				Dock
			]
		];
	return Shell;
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildProjectStrip()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FExtendedAtlassianWorkspaceSnapshot& Snapshot =
		Controller->GetSnapshot();
	FString ProjectLabel = Snapshot.ProjectFileLabel;
	FString VersionLabel = Snapshot.PluginVersionLabel;
	FString PlatformLabel = Snapshot.PlatformLabel;
	if (ProjectLabel.IsEmpty())
	{
		ProjectLabel = FString(FApp::GetProjectName()).ToUpper() + TEXT(".UPROJECT");
	}
	if (VersionLabel.IsEmpty())
	{
		if (const TSharedPtr<IPlugin> Plugin =
			IPluginManager::Get().FindPlugin(TEXT("UnrealExtendedAtlassian")))
		{
			VersionLabel = TEXT("PLUGIN v") + Plugin->GetDescriptor().VersionName;
		}
	}
	if (PlatformLabel.IsEmpty())
	{
		FString Rhi = FApp::GetGraphicsRHI().ToUpper();
		if (Rhi.IsEmpty())
		{
			Rhi = TEXT("RHI");
		}
		PlatformLabel = Rhi + TEXT(" · ") + FString(FPlatformProperties::PlatformName()).ToUpper();
	}
	return SNew(SBox)
		.HeightOverride(30.8f)
		[
			SNew(SBorder)
			.BorderImage(Brush(TEXT("Backlot.Brush.BorderSubtle")))
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 1.0f))
			[
				SNew(SBorder)
				.BorderImage(Brush(TEXT("Backlot.Brush.TopStrip")))
				.Padding(FMargin(8.0f, 0.0f))
				[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(ProjectLabel))
					.TextStyle(&Text(TEXT("Backlot.Mono.10.5.Medium")))
					.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#4d545e")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(1.0f, 0.0f)
				[
					SNew(SBorder)
					.BorderImage(Brush(TEXT("Backlot.Brush.Card")))
					.Padding(FMargin(15.0f, 0.0f))
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 7.0f, 0.0f)
						[
							SNew(SBox)
							.WidthOverride(6.0f)
							.HeightOverride(6.0f)
							.RenderTransform(FSlateRenderTransform(
								FQuat2D(FMath::DegreesToRadians(45.0f))))
							.RenderTransformPivot(FVector2D(0.5f, 0.5f))
							[
								SNew(SBorder)
								.BorderImage(Brush(TEXT("Backlot.Brush.BlueSolid")))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("BacklotName", "Backlot"))
							.TextStyle(&Text(TEXT("Backlot.Sans.11.5.Medium")))
						]
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(14.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(VersionLabel))
					.TextStyle(&Text(TEXT("Backlot.Mono.10.5")))
					.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#4d545e")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(14.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(PlatformLabel))
					.TextStyle(&Text(TEXT("Backlot.Mono.10.5")))
					.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#4d545e")))
				]
				]
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildNavigationRail()
{
	return SNew(SBox)
		.WidthOverride(59.0f)
		[
			SNew(SBorder)
			.BorderImage(ExtendedAtlassianWorkspacePrivate::Brush(TEXT("Backlot.Brush.BorderSubtle")))
			.Padding(FMargin(0.0f, 0.0f, 1.0f, 0.0f))
			[
				SNew(SBorder)
				.BorderImage(ExtendedAtlassianWorkspacePrivate::Brush(TEXT("Backlot.Brush.Navigation")))
				.Padding(FMargin(6.0f, 11.0f, 6.0f, 10.0f))
				[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SBox)
					.WidthOverride(31.6f)
					.HeightOverride(31.6f)
					[
						SNew(SBorder)
						.BorderImage(ExtendedAtlassianWorkspacePrivate::Brush(TEXT("Backlot.Brush.Blue")))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(9.0f)
							.HeightOverride(9.0f)
							.RenderTransform(FSlateRenderTransform(
								FQuat2D(FMath::DegreesToRadians(45.0f))))
							.RenderTransformPivot(FVector2D(0.5f, 0.5f))
							[
								SNew(SBorder)
								.BorderImage(
									ExtendedAtlassianWorkspacePrivate::Brush(
										TEXT("Backlot.Brush.BlueSolid")))
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)[NavButton(EExtendedAtlassianWorkspaceRoute::Docs, TEXT("Backlot.Icon.Docs"), TEXT("DOCS"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)[NavButton(EExtendedAtlassianWorkspaceRoute::Issues, TEXT("Backlot.Icon.Issues"), TEXT("ISSUES"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)[NavButton(EExtendedAtlassianWorkspaceRoute::Board, TEXT("Backlot.Icon.Board"), TEXT("BOARD"))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)[NavButton(EExtendedAtlassianWorkspaceRoute::Pins, TEXT("Backlot.Icon.Pins"), TEXT("PINS"))]
				+ SVerticalBox::Slot().AutoHeight()[NavButton(EExtendedAtlassianWorkspaceRoute::Inbox, TEXT("Backlot.Icon.Inbox"), TEXT("INBOX"), Controller->GetUnreadCount())]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 4.0f)
				[
					// `animation: bl-pulse 3s ease-in-out infinite` around the sync dot.
					SNew(SBacklotSyncPulse)
					.PulseColor(FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")))
					.MaxSpread(6.0f)
					.PeriodSeconds(3.0f)
					.CornerRadius(9.0f)
					.Animate(bAnimationsEnabled && HostServices.IsValid())
					.OnGetTimeSeconds(
						HostServices.IsValid()
							? SBacklotSyncPulse::FOnGetTimeSeconds::CreateLambda(
								[Services = HostServices]() { return Services->NowSeconds(); })
							: SBacklotSyncPulse::FOnGetTimeSeconds())
					[
					SNew(STextBlock)
					.Text_Lambda(
						[this]()
						{
							return bSyncRefreshDeferred
								? LOCTEXT("SyncDeferredDot", "·")
								: (Controller->GetSnapshot().bRefreshing
									? LOCTEXT("SyncRefreshingDot", "·")
									: LOCTEXT("SyncDot", "●"));
						})
					.TextStyle(&ExtendedAtlassianWorkspacePrivate::Text(TEXT("Backlot.Mono.10")))
					.ColorAndOpacity_Lambda(
						[this]()
						{
							return FExtendedAtlassianStyle::FromHex(
								bSyncRefreshDeferred
									? TEXT("#e3a54a")
									: (Controller->GetSnapshot().bStale
										? TEXT("#f0665f")
										: (Controller->GetSnapshot().bRefreshing
											? TEXT("#58a6ff")
											: TEXT("#57cc8a"))));
						})
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda(
						[this]()
						{
							return bSyncRefreshDeferred
								? LOCTEXT("SyncQueuedLabel", "QUEUE")
								: (Controller->GetSnapshot().bStale
									? LOCTEXT("SyncStaleLabel", "STALE")
									: (Controller->GetSnapshot().bRefreshing
										? LOCTEXT("SyncRefreshingLabel", "SYNC")
										: LOCTEXT("SyncLabel", "SYNC")));
						})
					.TextStyle(&ExtendedAtlassianWorkspacePrivate::Text(TEXT("Backlot.Mono.8")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(26.0f)
					.HeightOverride(26.0f)
					[
						SNew(SBorder)
						.BorderImage_Lambda(
							[this]()
							{
								const FExtendedAtlassianUser& User =
									Controller->GetSnapshot().CurrentUser;
								return ExtendedAtlassianWorkspacePrivate::AvatarBrush(
									&User,
									User.Initials);
							})
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.ToolTipText_Lambda(
							[this]()
							{
								const FExtendedAtlassianUser& User =
									Controller->GetSnapshot().CurrentUser;
								return User.DisplayName.IsEmpty()
									? LOCTEXT("UnverifiedUser", "Atlassian account not verified")
									: FText::FromString(User.DisplayName);
							})
						[
							SNew(STextBlock)
							.Text_Lambda(
								[this]()
								{
									const FString& Initials =
										Controller->GetSnapshot().CurrentUser.Initials;
									return FText::FromString(
										Initials.IsEmpty() ? TEXT("—") : Initials);
								})
							.TextStyle(&ExtendedAtlassianWorkspacePrivate::Text(TEXT("Backlot.Mono.10.Medium")))
							.ColorAndOpacity_Lambda(
								[this]()
								{
									return ExtendedAtlassianWorkspacePrivate::AvatarForeground(
										&Controller->GetSnapshot().CurrentUser);
								})
						]
					]
				]
				]
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::NavButton(
	EExtendedAtlassianWorkspaceRoute Route,
	const FName& IconName,
	const FString& Label,
	int32 Badge)
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const bool bActive =
		Controller->GetRoute() == Route
		|| (Route == EExtendedAtlassianWorkspaceRoute::Issues
			&& Controller->GetRoute() == EExtendedAtlassianWorkspaceRoute::IssueDetail);
	return SNew(SBacklotFocusRing)
		.Color(FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")))
		.OutlineWidth(Metric(TEXT("Backlot.Metric.Focus.OutlineWidth")))
		.OutlineOffset(Metric(TEXT("Backlot.Metric.Focus.OutlineOffset")))
		.CornerRadius(Metric(TEXT("Backlot.Metric.Focus.OutlineRadius")))
		[
			SNew(SBox)
			.WidthOverride(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.NavButton.Width")))
			.HeightOverride(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.NavButton.Height")))
			.Padding(FMargin(0.0f))
			[
				SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Nav")))
				.ContentPadding(0.0f)
				.Cursor(EMouseCursor::Hand)
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnNavigate, Route)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SBorder)
						.BorderImage(
							bActive
								? Brush(TEXT("Backlot.Brush.NavSelected"))
								: FStyleDefaults::GetNoBrush())
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Center)
							[
								SNew(SImage)
								.Image(Brush(IconName))
								.ColorAndOpacity(
									bActive
										? FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff"))
										: FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Center)
							.Padding(0.0f, 2.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(FText::FromString(Label))
								.TextStyle(&Text(TEXT("Backlot.Mono.8")))
								.ColorAndOpacity(
									bActive
										? FExtendedAtlassianStyle::FromHex(TEXT("#d7dce3"))
										: FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
							]
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Top)
					.Padding(0.0f, 3.0f, 3.0f, 0.0f)
					[
						SNew(STextBlock)
						.Visibility(Badge > 0 ? EVisibility::Visible : EVisibility::Collapsed)
						.Text(FText::AsNumber(Badge))
						.TextStyle(&Text(TEXT("Backlot.Mono.9")))
						.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#f0665f")))
					]
				]
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildCommandHeader()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const EExtendedAtlassianWorkspaceRoute Route = Controller->GetRoute();
	const bool bDocsRoute = Route == EExtendedAtlassianWorkspaceRoute::Docs;
	const bool bIssueDetailRoute =
		Route == EExtendedAtlassianWorkspaceRoute::IssueDetail;
	const FExtendedAtlassianWorkspaceSnapshot& Snapshot =
		Controller->GetSnapshot();
	const FExtendedAtlassianPage* HeaderPage = bDocsRoute ? SelectedPage() : nullptr;
	const bool bDocumentReadyForEdit =
		!bDocsRoute
		|| (HeaderPage
			&& !Snapshot.bRefreshing
			&& (Controller->IsFixtureProvider() || HeaderPage->Version > 0));
	FString Leaf = RouteLabel(Route);
	FString Root =
		Route == EExtendedAtlassianWorkspaceRoute::Docs
			? ConfluenceSpaceDisplayName(Snapshot)
		: Route == EExtendedAtlassianWorkspaceRoute::IssueDetail
			? FString(TEXT("Backlog"))
		: Route == EExtendedAtlassianWorkspaceRoute::Inbox
			? (Snapshot.CurrentUser.DisplayName.IsEmpty()
				? FString(TEXT("My work"))
				: Snapshot.CurrentUser.DisplayName)
			: ProjectDisplayName(Snapshot);
	FString CrumbTag;
	if (Route == EExtendedAtlassianWorkspaceRoute::Docs)
	{
		if (const FExtendedAtlassianPage* Page = SelectedPage()) { Leaf = Page->Title; }
	}
	else if (Route == EExtendedAtlassianWorkspaceRoute::IssueDetail)
	{
		Leaf = Controller->GetSelectedIssueKey();
		if (const FExtendedAtlassianIssue* Issue = SelectedIssue())
		{
			CrumbTag = Issue->PriorityName;
		}
	}
	if (bDocsRoute)
	{
		CrumbTag = TEXT("IN REVIEW");
	}
	else if (Route == EExtendedAtlassianWorkspaceRoute::Issues)
	{
		Leaf = TEXT("Backlog");
	}
	else if (Route == EExtendedAtlassianWorkspaceRoute::Board)
	{
		Leaf = SelectedSprintName(Snapshot) + TEXT(" board");
	}
	else if (Route == EExtendedAtlassianWorkspaceRoute::Pins)
	{
		Leaf = TEXT("Pinned threads");
	}
	else if (Route == EExtendedAtlassianWorkspaceRoute::Inbox)
	{
		Leaf = TEXT("My work");
	}

	return SNew(SBox)
		.HeightOverride(46.8f)
		[
			SNew(SBorder)
			.BorderImage(Brush(TEXT("Backlot.Brush.BorderSubtle")))
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 1.0f))
			[
				SNew(SBorder)
				.BorderImage(Brush(TEXT("Backlot.Brush.Sidebar")))
				.Padding(FMargin(16.0f, 0.0f))
				[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Root))
					.TextStyle(&Text(TEXT("Backlot.Sans.11")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(7.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CrumbSlash", "/"))
					.TextStyle(&Text(TEXT("Backlot.Mono.11")))
					.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#454c55")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Leaf))
					.TextStyle(&Text(TEXT("Backlot.Sans.12.Medium")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(CrumbTag.IsEmpty() ? 0.0f : 8.0f, 0.0f)
				[
					SNew(SBorder)
					.Visibility(
						CrumbTag.IsEmpty()
							? EVisibility::Collapsed
							: EVisibility::Visible)
					.BorderImage(Brush(TEXT("Backlot.Brush.DocumentDraftTag")))
					.Padding(FMargin(7.0f, 3.0f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(CrumbTag))
						.TextStyle(&Text(TEXT("Backlot.Mono.9")))
						.ColorAndOpacity(
							FExtendedAtlassianStyle::FromHex(TEXT("#e3a54a")))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(230.0f)
					.HeightOverride(30.0f)
					.Visibility(Controller->IsCompact() ? EVisibility::Collapsed : EVisibility::Visible)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SAssignNew(GlobalSearchBox, SEditableTextBox)
							.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("Backlot.Field.Search")))
							.HintText(LOCTEXT("SearchEverything", "Search everything"))
							.Text(FText::FromString(Controller->GetGlobalSearch()))
							.OnTextChanged(this, &SExtendedAtlassianWorkspace::OnSearchChanged)
						]
						+ SOverlay::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.Padding(9.0f, 0.0f)
						[
							SNew(SImage)
							.Visibility(EVisibility::HitTestInvisible)
							.Image(Brush(TEXT("Backlot.Icon.Search")))
							.ColorAndOpacity(
								FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(3.0f, 0.0f)
				[
					SNew(STextBlock)
					.Visibility(
						bDocsRoute && bDocumentEditing
							? EVisibility::Visible
							: EVisibility::Collapsed)
					.Text_Lambda(
						[this]()
						{
							return IsDocumentDraftDirty()
								? LOCTEXT("DocumentUnsaved", "UNSAVED CHANGES")
								: LOCTEXT("DocumentNoChanges", "NO CHANGES YET");
						})
					.TextStyle(&Text(TEXT("Backlot.Mono.9")))
					.ColorAndOpacity_Lambda(
						[this]()
						{
							return IsDocumentDraftDirty()
								? FExtendedAtlassianStyle::FromHex(TEXT("#e3a54a"))
								: FExtendedAtlassianStyle::FromHex(TEXT("#5c636d"));
						})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(3.0f, 0.0f)
				[
					SNew(SButton)
					.Visibility(
						bDocsRoute && bDocumentEditing
							? EVisibility::Visible
							: EVisibility::Collapsed)
					.IsEnabled(!bDocumentPublishPending)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
					.ContentPadding(FMargin(12.0f, 6.0f))
					.OnClicked(this, &SExtendedAtlassianWorkspace::OnCancelDocumentEdit)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CancelDocumentEdit", "Cancel"))
						.TextStyle(&Text(TEXT("Backlot.Sans.11")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(3.0f, 0.0f)
				[
					SNew(SButton)
					.Visibility(
						bDocsRoute && bDocumentEditing
							? EVisibility::Visible
							: EVisibility::Collapsed)
					.IsEnabled_Lambda(
						[this]()
						{
							const FExtendedAtlassianPage* Page = SelectedPage();
							return !bDocumentPublishPending
								&& Page
								&& Page->bCanRoundTrip
								&& Controller->CanExecuteMutation(
									EExtendedAtlassianWorkspaceMutation::UpdatePage);
						})
					.ToolTipText(MutationTooltip(
						Controller,
						EExtendedAtlassianWorkspaceMutation::UpdatePage))
					.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
					.ContentPadding(FMargin(13.0f, 6.0f))
					.OnClicked(this, &SExtendedAtlassianWorkspace::OnPublishDocumentEdit)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PublishDocumentEdit", "Publish"))
						.TextStyle(&Text(TEXT("Backlot.Sans.11")))
						.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#8fe0b3")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(94.0f)
					.HeightOverride(28.0f)
					.Visibility(
						(bDocsRoute && !bDocumentEditing)
								|| (bIssueDetailRoute && !bIssueEditing)
							? EVisibility::Visible
							: EVisibility::Collapsed)
					[
						SNew(SButton)
						.IsEnabled(
							bDocumentReadyForEdit
							&& Controller->CanExecuteMutation(
								bIssueDetailRoute
									? EExtendedAtlassianWorkspaceMutation::UpdateIssue
									: EExtendedAtlassianWorkspaceMutation::UpdatePage))
						.ToolTipText(MutationTooltip(
							Controller,
							bIssueDetailRoute
								? EExtendedAtlassianWorkspaceMutation::UpdateIssue
								: EExtendedAtlassianWorkspaceMutation::UpdatePage))
						.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
						.ContentPadding(FMargin(12.0f, 0.0f))
						.OnClicked_Lambda(
							[this, bIssueDetailRoute]()
							{
								return bIssueDetailRoute
									? OnStartIssueEdit()
									: OnStartDocumentEdit();
							})
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
								.Image(Brush(TEXT("Backlot.Icon.Edit")))
								.ColorAndOpacity(
									FExtendedAtlassianStyle::FromHex(TEXT("#a2a9b4")))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(7.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(bIssueDetailRoute
									? LOCTEXT("EditIssueHeader", "Edit issue")
									: LOCTEXT("EditDocument", "Edit page"))
								.TextStyle(&Text(TEXT("Backlot.Sans.11")))
							]
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(28.0f)
					.HeightOverride(28.0f)
					.Visibility(
						(bDocsRoute && !bDocumentEditing)
								|| (bIssueDetailRoute && !bIssueEditing)
							? EVisibility::Visible
							: EVisibility::Collapsed)
					[
						SNew(SButton)
						.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
						.ContentPadding(FMargin(0.0f))
						.ToolTipText(bIssueDetailRoute
							? LOCTEXT("IssueMoreTooltip", "Issue actions")
							: LOCTEXT("DocumentMoreTooltip", "Page actions"))
					.OnClicked_Lambda(
						[this, bIssueDetailRoute]()
						{
							if (bIssueDetailRoute)
							{
								OpenIssueActions();
							}
							else if (const FExtendedAtlassianPage* Page = SelectedPage())
							{
								OpenDocumentActions(Page->Id);
							}
							return FReply::Handled();
						})
						[
							SNew(SImage)
							.Image(Brush(TEXT("Backlot.Icon.More")))
							.ColorAndOpacity(
								FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(120.0f)
					.HeightOverride(28.0f)
					[
						SNew(SButton)
						.IsEnabled(Controller->CanExecuteMutation(
							EExtendedAtlassianWorkspaceMutation::CreateCaptureIssue))
					.ToolTipText(MutationTooltip(
						Controller,
						EExtendedAtlassianWorkspaceMutation::CreateCaptureIssue))
						.ButtonStyle(&Button(TEXT("Backlot.Button.Primary")))
					.ContentPadding(FMargin(13.0f, 0.0f))
					.OnClicked(this, &SExtendedAtlassianWorkspace::OnOpenCapture)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(Brush(TEXT("Backlot.Icon.Capture")))
							.ColorAndOpacity(
								FExtendedAtlassianStyle::FromHex(TEXT("#cfe0ff")))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(8.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Capture", "Capture"))
							.TextStyle(&Text(TEXT("Backlot.Sans.11")))
							.ColorAndOpacity(
								FExtendedAtlassianStyle::FromHex(TEXT("#cfe0ff")))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(7.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
									// The real binding on this platform, not the macOS glyph chord the HTML
									// reference shows. U+2318 has no glyph in any font this style chains, so
									// the pictorial form rendered as a box next to a mis-metriced shift arrow.
									.Text(LOCTEXT("CaptureShortcutHint", "CTRL+SHIFT+B"))
									.TextStyle(&Text(TEXT("Backlot.Mono.9")))
									.ColorAndOpacity(
										FExtendedAtlassianStyle::FromHex(TEXT("#7cbcff")))
							]
					]
				]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(111.0f)
					.HeightOverride(28.0f)
					[
						SNew(SButton)
						.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
						.ContentPadding(FMargin(10.0f, 0.0f))
						.OnClicked(this, &SExtendedAtlassianWorkspace::OnToggleCompact)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
								.Image(Brush(TEXT("Backlot.Icon.Dock")))
								.ColorAndOpacity(
									FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(7.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(
									Controller->IsCompact()
										? LOCTEXT("WideDock", "WIDE DOCK")
										: LOCTEXT("NarrowDock", "NARROW DOCK"))
								.TextStyle(&Text(TEXT("Backlot.Mono.9")))
							]
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(28.0f)
					.HeightOverride(28.0f)
					[
						SNew(SButton)
						.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
						.ContentPadding(FMargin(0.0f))
						.ToolTipText(LOCTEXT("ToggleRailTooltip", "Toggle context rail"))
						.OnClicked(this, &SExtendedAtlassianWorkspace::OnToggleRail)
						[
							SNew(SImage)
							.Image(Brush(TEXT("Backlot.Icon.Rail")))
							.ColorAndOpacity(
								FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
						]
					]
				]
			]
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildContextSidebar()
{
	switch (Controller->GetRoute())
	{
	case EExtendedAtlassianWorkspaceRoute::Docs: return BuildDocsSidebar();
	case EExtendedAtlassianWorkspaceRoute::Issues:
	case EExtendedAtlassianWorkspaceRoute::IssueDetail: return BuildIssuesSidebar();
	case EExtendedAtlassianWorkspaceRoute::Board: return BuildBoardSidebar();
	case EExtendedAtlassianWorkspaceRoute::Pins: return BuildPinsSidebar();
	case EExtendedAtlassianWorkspaceRoute::Inbox:
	default:
		return SNew(SBorder)
			.BorderImage(ExtendedAtlassianWorkspacePrivate::Brush(TEXT("Backlot.Brush.Sidebar")));
	}
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildDocsSidebar()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	TArray<TSharedRef<SWidget>> PageRows;
	const FExtendedAtlassianWorkspaceSnapshot& Snapshot = Controller->GetSnapshot();
	const FString SpaceName = ConfluenceSpaceDisplayName(Snapshot);
	int32 PageCount = 0;
	const FString EffectiveQuery = Controller->GetPageSearch().IsEmpty()
		? Controller->GetGlobalSearch()
		: Controller->GetPageSearch();
	TMap<FString, const FExtendedAtlassianDocumentTreeNode*> NodesById;
	TSet<FString> ExpandableNodeIds;
	for (const FExtendedAtlassianDocumentTreeNode& Node : Snapshot.DocumentTree)
	{
		NodesById.Add(Node.Id, &Node);
		if (!Node.ParentId.IsEmpty())
		{
			ExpandableNodeIds.Add(Node.ParentId);
		}
	}
	for (const FExtendedAtlassianDocumentTreeNode& Node : Snapshot.DocumentTree)
	{
		PageCount += Node.bSection ? 0 : 1;
		bool bAncestorsExpanded = true;
		if (!Node.ParentId.IsEmpty() && EffectiveQuery.IsEmpty())
		{
			FString ParentId = Node.ParentId;
			TSet<FString> VisitedAncestorIds;
			while (!ParentId.IsEmpty())
			{
				if (VisitedAncestorIds.Contains(ParentId))
				{
					bAncestorsExpanded = false;
					break;
				}
				VisitedAncestorIds.Add(ParentId);

				const FExtendedAtlassianDocumentTreeNode* const* ParentEntry =
					NodesById.Find(ParentId);
				if (!ParentEntry || !*ParentEntry)
				{
					break;
				}
				if (!(*ParentEntry)->bExpanded)
				{
					bAncestorsExpanded = false;
					break;
				}
				ParentId = (*ParentEntry)->ParentId;
			}
		}
		if (!bAncestorsExpanded)
		{
			continue;
		}

		bool bMatches = EffectiveQuery.IsEmpty()
			|| Node.Label.Contains(EffectiveQuery, ESearchCase::IgnoreCase);
		if (!bMatches && !Node.bSection)
		{
			if (const FExtendedAtlassianPage* Page = Snapshot.Pages.FindByPredicate(
				[&Node](const FExtendedAtlassianPage& Candidate)
				{
					return Candidate.Id == Node.Id;
				}))
			{
				bMatches = Page->Body.Contains(EffectiveQuery, ESearchCase::IgnoreCase);
			}
		}
		if (!bMatches)
		{
			continue;
		}

		const bool bSelected = !Node.bSection && Node.Id == Controller->GetSelectedPageId();
		const bool bExpandable = Node.bSection || ExpandableNodeIds.Contains(Node.Id);
		PageRows.Add(
			SNew(SBox)
			.HeightOverride(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.Tree.RowHeight")))
			[
				SNew(SBorder)
				.BorderImage(Brush(
					bSelected
						? TEXT("Backlot.Brush.CardSelected")
						: TEXT("Backlot.Brush.Panel")))
				.Padding(0.0f)
				[
					SNew(SButton)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
					.ContentPadding(FMargin(
						7.0f + Node.Depth * 14.0f,
						0.0f,
						5.0f,
						0.0f))
					.OnClicked_Lambda(
						[
							Controller = Controller,
							Id = Node.Id,
							bSection = Node.bSection,
							bExpandable
						]()
						{
							if (bSection)
							{
								Controller->ToggleDocumentNode(Id);
							}
							else
							{
								Controller->SelectPage(Id);
								if (bExpandable)
								{
									Controller->ToggleDocumentNode(Id);
								}
							}
							return FReply::Handled();
						})
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 5.0f, 0.0f)
						[
							SNew(SBox)
								.WidthOverride(8.0f)
								.HeightOverride(8.0f)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								[
									bExpandable
										? StaticCastSharedRef<SWidget>(
											SNew(SImage)
												.Image(Brush(
													Node.bExpanded
														? TEXT("Backlot.Icon.CaretDown")
														: TEXT("Backlot.Icon.CaretRight")))
												.ColorAndOpacity(
													bSelected
														? FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff"))
														: FExtendedAtlassianStyle::FromHex(TEXT("#4d545e"))))
										: StaticCastSharedRef<SWidget>(
											SNew(STextBlock)
												.Text(FText::FromString(TEXT("▪")))
												.TextStyle(&Text(TEXT("Backlot.Mono.9")))
												.ColorAndOpacity(
													bSelected
														? FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff"))
														: FExtendedAtlassianStyle::FromHex(TEXT("#4d545e"))))
								]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(FText::FromString(Node.Label))
								.TextStyle(&Text(
									Node.bSection
										? TEXT("Backlot.Sans.11.Medium")
										: TEXT("Backlot.Sans.11")))
								.ColorAndOpacity(
									bSelected
										? FExtendedAtlassianStyle::FromHex(TEXT("#e6e8ec"))
										: FExtendedAtlassianStyle::FromHex(TEXT("#a2a9b4")))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(4.0f, 0.0f)
						[
							SNew(STextBlock)
								.Visibility(
									Node.CommentBadge > 0
										? EVisibility::Visible
										: EVisibility::Collapsed)
								.Text(FText::AsNumber(Node.CommentBadge))
								.TextStyle(&Text(TEXT("Backlot.Mono.9")))
								.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#e3a54a")))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
								.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
								.ContentPadding(FMargin(5.0f, 0.0f))
								.OnClicked_Lambda(
									[this, Id = Node.Id]()
									{
										OpenDocumentActions(Id);
										return FReply::Handled();
									})
								[
									SNew(SImage)
										.Image(Brush(TEXT("Backlot.Icon.More")))
										.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
								]
						]
					]
				]
			]
		);
	}
	if (PageRows.IsEmpty())
	{
		PageRows.Add(
			SNew(SBox)
			.Padding(8.0f, 18.0f)
			[
				EmptyState(LOCTEXT("NoPagesFound", "No pages found"))
			]);
	}

	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Sidebar")))
		.Padding(FMargin(10.0f, 14.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SectionLabel(LOCTEXT("Space", "SPACE"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
						.Text(FText::Format(
							LOCTEXT("PageCount", "{0} pages"),
							FText::AsNumber(PageCount)))
						.TextStyle(&Text(TEXT("Backlot.Mono.9")))
						.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 10.0f, 0.0f, 12.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBorder)
						.BorderImage(Brush(TEXT("Backlot.Brush.CardSelected")))
						.Padding(FMargin(7.0f, 5.0f))
						[
							SNew(STextBlock)
							.Text(FText::FromString(
								Snapshot.ConfluenceSpaceKey.IsEmpty()
									? LabelInitials(SpaceName)
									: Snapshot.ConfluenceSpaceKey.Left(2).ToUpper()))
								.TextStyle(&Text(TEXT("Backlot.Mono.9")))
								.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#b6a9ff")))
						]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(8.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
							.Text(FText::FromString(SpaceName))
							.TextStyle(&Text(TEXT("Backlot.Sans.12.Medium")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
							.Text(FText::FromString(
								ProjectDisplayName(
									Controller->GetSnapshot())
								+ TEXT(" · shared")))
							.TextStyle(&Text(TEXT("Backlot.Sans.10")))
							.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 9.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SEditableTextBox)
					.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("Backlot.Field.Search")))
					.HintText(LOCTEXT("SearchPages", "Search pages"))
					.Text(FText::FromString(Controller->GetPageSearch()))
					.OnTextChanged(
						this,
						&SExtendedAtlassianWorkspace::OnPageSearchChanged)
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(9.0f, 0.0f)
				[
					SNew(SImage)
					.Visibility(EVisibility::HitTestInvisible)
					.Image(Brush(TEXT("Backlot.Icon.Search")))
					.ColorAndOpacity(
						FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBacklotVirtualizedWidgetList)
				.Widgets(PageRows)
				.ScrollBarStyle(
					&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(
						TEXT("Backlot.ScrollBar")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 9.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SButton)
					.IsEnabled(Controller->CanExecuteMutation(
						EExtendedAtlassianWorkspaceMutation::CreatePage))
					.ToolTipText(MutationTooltip(
						Controller,
						EExtendedAtlassianWorkspaceMutation::CreatePage))
					.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
					.ContentPadding(FMargin(8.0f, 6.0f))
					.OnClicked_Lambda(
						[this]()
						{
							OpenCreatePagePopover();
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NewPage", "+  New page"))
						.TextStyle(&Text(TEXT("Backlot.Sans.11")))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.IsEnabled(Controller->CanExecuteMutation(
						EExtendedAtlassianWorkspaceMutation::CreateSection))
					.ToolTipText(MutationTooltip(
						Controller,
						EExtendedAtlassianWorkspaceMutation::CreateSection))
					.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
					.ContentPadding(FMargin(8.0f, 6.0f))
					.OnClicked(this, &SExtendedAtlassianWorkspace::OnCreateSection)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NewSection", "+  Section"))
						.TextStyle(&Text(TEXT("Backlot.Sans.11")))
					]
				]
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildIssuesSidebar()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	TSharedRef<SVerticalBox> Views = SNew(SVerticalBox);
	for (const FExtendedAtlassianIssueView& View : Controller->GetSnapshot().IssueViews)
	{
		const bool bSelected = View.Id == Controller->GetSelectedIssueViewId();
		Views->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			SNew(SButton)
			.ButtonStyle(&Button(
				bSelected ? TEXT("Backlot.Button.Secondary") : TEXT("Backlot.Button.Clear")))
			.ContentPadding(FMargin(7.0f, 5.0f))
			.OnClicked_Lambda([Controller = Controller, Id = View.Id]()
			{
				Controller->SelectIssueView(Id);
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 7.0f, 0.0f)
				[
					SNew(SBox)
						.WidthOverride(5.0f)
						.HeightOverride(5.0f)
						[
							SNew(SImage)
							.Image(Brush(TEXT("Backlot.Brush.Dot")))
							.ColorAndOpacity(
								FExtendedAtlassianStyle::FromHex(*View.DotColor))
						]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::FromString(View.Label))
						.TextStyle(&Text(
							bSelected
								? TEXT("Backlot.Sans.11.Medium")
								: TEXT("Backlot.Sans.11")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::AsNumber(View.AuthoredCount))
						.TextStyle(&Text(TEXT("Backlot.Mono.9")))
						.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
				]
			]
		];
	}

	TSharedRef<SVerticalBox> Epics = SNew(SVerticalBox);
	for (const FExtendedAtlassianEpic& Epic : Controller->GetSnapshot().Epics)
	{
		const bool bSelected = Controller->GetEpicFilter() == Epic.Id;
		Epics->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
		[
			SNew(SButton)
				.ButtonStyle(&Button(
					bSelected ? TEXT("Backlot.Button.Secondary") : TEXT("Backlot.Button.Clear")))
				.ContentPadding(FMargin(6.0f, 5.0f))
				.OnClicked_Lambda([Controller = Controller, Id = Epic.Id]()
				{
					Controller->ToggleEpicFilter(Id);
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 7.0f, 0.0f)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("EpicMarker", "▌"))
								.TextStyle(&Text(TEXT("Backlot.Mono.11")))
								.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(*Epic.Color))
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(STextBlock)
								.Text(FText::FromString(Epic.Name))
								.TextStyle(&Text(TEXT("Backlot.Sans.11")))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
								.Text(FText::AsPercent(
									Epic.TotalIssues > 0
										? static_cast<double>(Epic.DoneIssues)
											/ Epic.TotalIssues
										: 0.0))
								.TextStyle(&Text(TEXT("Backlot.Mono.9")))
								.ColorAndOpacity(
									FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(13.0f, 5.0f, 0.0f, 0.0f)
					[
						SNew(SBox)
						.HeightOverride(3.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(FMath::Max(
								0.001,
								Epic.TotalIssues > 0
									? static_cast<double>(Epic.DoneIssues)
										/ Epic.TotalIssues
									: 0.0))
							[
								SNew(SBorder)
									.BorderImage(Brush(TEXT("Backlot.Brush.BlueSolid")))
									.BorderBackgroundColor(
										FExtendedAtlassianStyle::FromHex(*Epic.Color))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(FMath::Max(
								0.001,
								Epic.TotalIssues > 0
									? 1.0
										- static_cast<double>(Epic.DoneIssues)
											/ Epic.TotalIssues
									: 1.0))
							[
								SNew(SBorder)
									.BorderImage(Brush(TEXT("Backlot.Brush.Card")))
							]
						]
					]
				]
		];
	}
	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Sidebar")))
		.Padding(FMargin(13.0f, 15.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SectionLabel(LOCTEXT("Views", "VIEWS"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 18.0f)[Views]
			+ SVerticalBox::Slot().AutoHeight()[SectionLabel(LOCTEXT("Epics", "EPICS"))]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 8.0f)[Epics]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
						.Text_Lambda(
							[this]()
							{
								const FDateTime Synced =
									Controller->GetSnapshot().SyncedAt;
								const int64 Seconds = Synced == FDateTime::MinValue()
									? 0
									: FMath::Max<int64>(
										0,
										static_cast<int64>(
											(FDateTime::UtcNow() - Synced)
												.GetTotalSeconds()));
								return FText::Format(
									LOCTEXT("SyncedAge", "SYNCED {0}s AGO"),
									FText::AsNumber(Seconds));
							})
						.TextStyle(&Text(TEXT("Backlot.Mono.9")))
						.ColorAndOpacity(
							FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
						.Text_Lambda(
							[this]()
							{
								return bSyncRefreshDeferred
									? LOCTEXT("QueuedSync", "·  QUEUED")
									: LOCTEXT("LiveSync", "●  LIVE");
							})
						.TextStyle(&Text(TEXT("Backlot.Mono.9")))
						.ColorAndOpacity_Lambda(
							[this]()
							{
								return FExtendedAtlassianStyle::FromHex(
									bSyncRefreshDeferred
										? TEXT("#e3a54a")
										: TEXT("#57cc8a"));
							})
				]
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildBoardSidebar()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FExtendedAtlassianWorkspaceSnapshot& Snapshot = Controller->GetSnapshot();
	const FExtendedAtlassianSprintSummary& Summary = Snapshot.SprintSummary;
	const FExtendedAtlassianSprint* SelectedSprint =
		Snapshot.Sprints.FindByPredicate(
			[&Snapshot](const FExtendedAtlassianSprint& Sprint)
			{
				return Sprint.Id == Snapshot.SelectedSprintId;
			});
	TSharedRef<SVerticalBox> TeamRows = SNew(SVerticalBox);
	TArray<const FExtendedAtlassianTeamLoad*> OrderedTeamLoad;
	const FExtendedAtlassianTeamLoad* CurrentUserLoad =
		Snapshot.TeamLoad.FindByPredicate(
			[&Snapshot](const FExtendedAtlassianTeamLoad& Load)
			{
				return Load.User.AccountId == Snapshot.CurrentUser.AccountId;
			});
	if (CurrentUserLoad)
	{
		OrderedTeamLoad.Add(CurrentUserLoad);
	}
	for (const FExtendedAtlassianTeamLoad& Load : Snapshot.TeamLoad)
	{
		if (&Load != CurrentUserLoad)
		{
			OrderedTeamLoad.Add(&Load);
		}
	}
	for (const FExtendedAtlassianTeamLoad* LoadPtr : OrderedTeamLoad)
	{
		const FExtendedAtlassianTeamLoad& Load = *LoadPtr;
		const bool bSelected =
			Controller->GetAssigneeFilter() == Load.User.AccountId;
		const TCHAR* LoadBrush =
			Load.ThresholdColor == TEXT("#f0665f")
				? TEXT("Backlot.Brush.RedSolid")
				: (Load.ThresholdColor == TEXT("#e3a54a")
					? TEXT("Backlot.Brush.AmberSolid")
					: TEXT("Backlot.Brush.GreenSolid"));
		TeamRows->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 3.0f)
			[
				SNew(SBox)
				.HeightOverride(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.TeamLoadRow.Height")))
				[
					SNew(SBorder)
					.BorderImage(
						bSelected
							? Brush(TEXT("Backlot.Brush.CardSelected"))
							: FStyleDefaults::GetNoBrush())
					.Padding(FMargin(3.0f, 1.0f))
					[
						SNew(SButton)
						.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
						.ContentPadding(0.0f)
						.ToolTipText(FText::Format(
							LOCTEXT(
								"FilterBoardByPerson",
								"Show {0}'s {1} open issues ({2} story points)"),
							FText::FromString(Load.User.DisplayName),
							FText::AsNumber(Load.OpenIssueCount),
							FText::AsNumber(Load.OpenPoints)))
						.OnClicked_Lambda(
							[this, AccountId = Load.User.AccountId]()
							{
								Controller->SetAssigneeFilter(
									Controller->GetAssigneeFilter() == AccountId
										? TEXT("anyone")
										: AccountId);
								return FReply::Handled();
							})
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SBox)
									.WidthOverride(22.0f)
									.HeightOverride(22.0f)
									[
										SNew(SBorder)
										.BorderImage(AvatarBrush(&Load.User, Load.User.Initials))
										.Padding(0.0f)
										.HAlign(HAlign_Center)
										.VAlign(VAlign_Center)
										[
											SNew(STextBlock)
											.Text(FText::FromString(Load.User.Initials))
											.TextStyle(&Text(TEXT("Backlot.Mono.9")))
											.ColorAndOpacity(AvatarForeground(&Load.User))
										]
									]
								]
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.Padding(7.0f, 0.0f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(FText::FromString(Load.User.DisplayName))
									.TextStyle(&Text(TEXT("Backlot.Sans.11")))
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(FText::AsNumber(Load.OpenIssueCount))
									.TextStyle(&Text(TEXT("Backlot.Mono.10.Medium")))
								]
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBox)
								.HeightOverride(3.0f)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.FillWidth(FMath::Clamp(Load.Fraction, 0.01, 1.0))
									[
										SNew(SBorder).BorderImage(Brush(LoadBrush))
									]
									+ SHorizontalBox::Slot()
									.FillWidth(FMath::Clamp(1.0 - Load.Fraction, 0.0, 1.0))
									[
									SNew(SBorder).BorderImage(Brush(TEXT("Backlot.Brush.Card")))
								]
							]
						]
					]
				]
			]
		];
	}

	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Sidebar")))
		.Padding(FMargin(14.0f, 16.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(
							!SelectedSprint
								? TEXT("Sprint")
								: SelectedSprint->Name))
						.TextStyle(&Text(TEXT("Backlot.Sans.13.Medium")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
						.Text(FText::FromString(Summary.DaysLeft))
						.TextStyle(&Text(TEXT("Backlot.Mono.9")))
						.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#e3a54a")))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 13.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(Summary.DateRange + TEXT(" · ") + Summary.Goal))
					.TextStyle(&Text(TEXT("Backlot.Mono.9")))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
						.HeightOverride(5.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(Summary.DoneFraction)
							[
								SNew(SBorder).BorderImage(Brush(TEXT("Backlot.Brush.GreenSolid")))
							]
							+ SHorizontalBox::Slot().FillWidth(Summary.WipFraction)
							[
								SNew(SBorder).BorderImage(Brush(TEXT("Backlot.Brush.BlueSolid")))
							]
							+ SHorizontalBox::Slot().FillWidth(Summary.BlockedFraction)
							[
								SNew(SBorder).BorderImage(Brush(TEXT("Backlot.Brush.RedSolid")))
							]
							+ SHorizontalBox::Slot().FillWidth(
								FMath::Max(
									0.0,
									1.0
										- Summary.DoneFraction
										- Summary.WipFraction
										- Summary.BlockedFraction))
							[
								SNew(SBorder).BorderImage(Brush(TEXT("Backlot.Brush.Card")))
							]
						]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 7.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::Format(
							LOCTEXT("SprintNumbers", "{0} DONE       {1} WIP       {2} LEFT"),
							FText::AsNumber(Summary.Done),
							FText::AsNumber(Summary.Wip),
							FText::AsNumber(Summary.Left)))
						.TextStyle(&Text(TEXT("Backlot.Mono.10")))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 22.0f, 0.0f, 8.0f)[SectionLabel(LOCTEXT("TeamLoad", "TEAM LOAD"))]
			+ SVerticalBox::Slot().AutoHeight()[TeamRows]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildPinsSidebar()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	TSharedRef<SVerticalBox> Filters = SNew(SVerticalBox);
	const EExtendedAtlassianPinKind Kinds[] = {
		EExtendedAtlassianPinKind::Material,
		EExtendedAtlassianPinKind::Level,
		EExtendedAtlassianPinKind::Blueprint,
		EExtendedAtlassianPinKind::Page
	};
	for (EExtendedAtlassianPinKind Kind : Kinds)
	{
		const FString Key = PinKindKey(Kind);
		int32 Count = 0;
		for (const FExtendedAtlassianPin& Pin : Controller->GetSnapshot().Pins)
		{
			Count += Pin.Target.Kind == Kind ? 1 : 0;
		}
		const bool bSelected = PinKindFilter == Key;
		Filters->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 3.0f)
		[
			SNew(SBox)
			.HeightOverride(31.0f)
			[
				SNew(SBorder)
					.BorderImage(Brush(
						bSelected
							? TEXT("Backlot.Brush.PinThreadSelected")
							: TEXT("Backlot.Brush.PinThread")))
					.Padding(0.0f)
					[
						SNew(SButton)
							.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
							.ContentPadding(FMargin(8.0f, 0.0f))
							.OnClicked_Lambda(
								[this, Key]()
								{
									PinKindFilter = PinKindFilter == Key ? FString() : Key;
									Rebuild();
									return FReply::Handled();
								})
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 9.0f, 0.0f)
								[
									Kind == EExtendedAtlassianPinKind::Blueprint
										? StaticCastSharedRef<SWidget>(
											SNew(SBox)
												.WidthOverride(11.0f)
												.HeightOverride(11.0f)
												.HAlign(HAlign_Center)
												.VAlign(VAlign_Center)
												[
													SNew(SImage)
														.Image(Brush(TEXT("Backlot.Icon.Command")))
														.ColorAndOpacity(
															FExtendedAtlassianStyle::FromHex(PinKindColor(Kind)))
												])
										: StaticCastSharedRef<SWidget>(
											SNew(STextBlock)
												.Text(FText::FromString(PinKindGlyph(Kind)))
												.TextStyle(&Text(TEXT("Backlot.Mono.11")))
												.ColorAndOpacity(
													FExtendedAtlassianStyle::FromHex(PinKindColor(Kind))))
								]
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text(FText::FromString(PinKindLabel(Kind)))
										.TextStyle(&Text(TEXT("Backlot.Sans.11")))
										.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(
											bSelected ? TEXT("#e6e8ec") : TEXT("#a2a9b4")))
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text(FText::AsNumber(Count))
										.TextStyle(&Text(TEXT("Backlot.Mono.10")))
										.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
								]
							]
					]
			]
		];
	}
	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Sidebar")))
		.Padding(FMargin(14.0f, 16.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SectionLabel(LOCTEXT("PinnedOn", "PINNED ON"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)[Filters]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNullWidget::NullWidget
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 18.0f, 0.0f, 10.0f)
			[
				SNew(SBacklotDashedBorder)
				[
					SNew(SButton)
						.IsEnabled(Controller->CanExecuteMutation(
							EExtendedAtlassianWorkspaceMutation::CreatePin))
						.ToolTipText(MutationTooltip(
							Controller,
							EExtendedAtlassianWorkspaceMutation::CreatePin))
						.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
						.ContentPadding(FMargin(8.0f, 7.0f))
						.HAlign(HAlign_Left)
						.OnClicked_Lambda(
							[this]()
							{
								OpenPinPopover(false);
								return FReply::Handled();
							})
						[
							SNew(STextBlock)
								.Text(LOCTEXT("PinAsset", "+  Pin an asset"))
								.TextStyle(&Text(TEXT("Backlot.Sans.11")))
						]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
					.AutoWrapText(true)
					.Text(LOCTEXT("PinExplanation", "Pins live on the asset, not the level. Reopen the asset anywhere and its threads come with it."))
					.TextStyle(&Text(TEXT("Backlot.Sans.10")))
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildMainView()
{
	const FExtendedAtlassianWorkspaceSnapshot& Snapshot = Controller->GetSnapshot();
	if (Snapshot.State == EExtendedAtlassianLoadState::Idle
		|| Snapshot.State == EExtendedAtlassianLoadState::Loading)
	{
		return BuildWorkspaceState();
	}
	if (Snapshot.State == EExtendedAtlassianLoadState::Offline
		|| Snapshot.State == EExtendedAtlassianLoadState::Error
		|| Snapshot.State == EExtendedAtlassianLoadState::PermissionDenied)
	{
		return BuildWorkspaceState();
	}

	switch (Controller->GetRoute())
	{
	case EExtendedAtlassianWorkspaceRoute::Docs:
		return SNew(SBacklotDocsSurface)[BuildDocsMain()];
	case EExtendedAtlassianWorkspaceRoute::Issues:
		return SNew(SBacklotIssuesSurface)[BuildIssuesMain()];
	case EExtendedAtlassianWorkspaceRoute::IssueDetail:
		return SNew(SBacklotIssueDetailSurface)[BuildIssueDetailMainDynamic()];
	case EExtendedAtlassianWorkspaceRoute::Board:
		return SNew(SBacklotBoardSurface)[BuildBoardMain()];
	case EExtendedAtlassianWorkspaceRoute::Pins:
		return SNew(SBacklotPinsSurface)[BuildPinsMain()];
	case EExtendedAtlassianWorkspaceRoute::Inbox:
		return SNew(SBacklotInboxSurface)[BuildInboxMain()];
	default: return EmptyState(LOCTEXT("UnknownView", "NO VIEW"));
	}
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildWorkspaceState()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FExtendedAtlassianWorkspaceSnapshot& Snapshot = Controller->GetSnapshot();
	const FString Code = Snapshot.Error.Code;
	const bool bLoading =
		Snapshot.State == EExtendedAtlassianLoadState::Idle
		|| Snapshot.State == EExtendedAtlassianLoadState::Loading;
	const bool bOpenSettings =
		Code == TEXT("NotConfigured")
		|| Code == TEXT("Unauthorized")
		|| Code == TEXT("NotFound");

	FText Heading;
	FText Detail;
	FLinearColor Accent = FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff"));
	if (bLoading)
	{
		const TSharedPtr<FExtendedAtlassianClient> Client =
			FUnrealExtendedAtlassianModule::GetClient();
		const bool bConnecting =
			Client.IsValid()
			&& Client->HasCredentials()
			&& !Client->GetVerifiedUser().IsValid();
		Heading = bConnecting
			? LOCTEXT("ConnectingBacklot", "CONNECTING TO ATLASSIAN")
			: LOCTEXT("LoadingBacklot", "LOADING BACKLOT");
		Detail = LOCTEXT(
			"LoadingBacklotDetail",
			"Jira, Confluence, Pins, and Inbox are being reconciled.");
	}
	else if (Code == TEXT("NotConfigured"))
	{
		Heading = LOCTEXT("BacklotUnconfigured", "CONNECT ATLASSIAN");
		Detail = Snapshot.Error.Message.IsEmpty()
			? LOCTEXT(
				"BacklotUnconfiguredDetail",
				"Add your site URL, e-mail, and API token in Project Settings.")
			: FText::FromString(Snapshot.Error.Message);
		Accent = FExtendedAtlassianStyle::FromHex(TEXT("#e3a54a"));
	}
	else if (Code == TEXT("Unauthorized"))
	{
		Heading = LOCTEXT("BacklotUnauthorized", "CREDENTIALS REJECTED");
		Detail = FText::FromString(Snapshot.Error.Message);
		Accent = FExtendedAtlassianStyle::FromHex(TEXT("#f0665f"));
	}
	else if (Code == TEXT("Forbidden")
		|| Snapshot.State == EExtendedAtlassianLoadState::PermissionDenied)
	{
		Heading = LOCTEXT("BacklotForbidden", "PERMISSION REQUIRED");
		Detail = Snapshot.Error.Message.IsEmpty()
			? LOCTEXT(
				"BacklotForbiddenDetail",
				"Your Atlassian account cannot read this project or space.")
			: FText::FromString(Snapshot.Error.Message);
		Accent = FExtendedAtlassianStyle::FromHex(TEXT("#f0665f"));
	}
	else if (Code == TEXT("RateLimited"))
	{
		Heading = LOCTEXT("BacklotRateLimited", "ATLASSIAN IS THROTTLING REQUESTS");
		Detail = Snapshot.Error.Message.IsEmpty()
			? LOCTEXT(
				"BacklotRateLimitedDetail",
				"Wait for the Retry-After window, then try again.")
			: FText::FromString(Snapshot.Error.Message);
		Accent = FExtendedAtlassianStyle::FromHex(TEXT("#e3a54a"));
	}
	else if (Code == TEXT("Network")
		|| Snapshot.State == EExtendedAtlassianLoadState::Offline)
	{
		Heading = LOCTEXT("BacklotOffline", "YOU’RE OFFLINE");
		Detail = Snapshot.Error.Message.IsEmpty()
			? LOCTEXT(
				"BacklotOfflineDetail",
				"Reconnect to the network; local drafts and annotations are preserved.")
			: FText::FromString(Snapshot.Error.Message);
		Accent = FExtendedAtlassianStyle::FromHex(TEXT("#e3a54a"));
	}
	else if (Code == TEXT("NotFound"))
	{
		Heading = LOCTEXT("BacklotNotFound", "CONFIGURATION NOT FOUND");
		Detail = FText::FromString(Snapshot.Error.Message);
		Accent = FExtendedAtlassianStyle::FromHex(TEXT("#f0665f"));
	}
	else if (Code == TEXT("MalformedJson")
		|| Code == TEXT("Parse"))
	{
		Heading = LOCTEXT("BacklotMalformed", "RESPONSE COULD NOT BE READ");
		Detail = FText::FromString(Snapshot.Error.Message);
		Accent = FExtendedAtlassianStyle::FromHex(TEXT("#f0665f"));
	}
	else
	{
		Heading = Code == TEXT("ServerError")
			? LOCTEXT("BacklotServerError", "ATLASSIAN SERVER ERROR")
			: LOCTEXT("BacklotUnavailable", "BACKLOT IS UNAVAILABLE");
		Detail = Snapshot.Error.Message.IsEmpty()
			? LOCTEXT(
				"BacklotUnavailableDetail",
				"The workspace could not be loaded. Your local work has not been cleared.")
			: FText::FromString(Snapshot.Error.Message);
		Accent = FExtendedAtlassianStyle::FromHex(TEXT("#f0665f"));
	}

	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Workspace")))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
				.WidthOverride(420.0f)
				[
					SNew(SBorder)
						.BorderImage(Brush(TEXT("Backlot.Brush.BoardCard")))
						.Padding(FMargin(24.0f, 22.0f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock)
									.Text(bLoading
										? LOCTEXT("BacklotLoadingGlyph", "·")
										: LOCTEXT("BacklotStateGlyph", "◆"))
									.TextStyle(&Text(TEXT("Backlot.Mono.13")))
									.ColorAndOpacity(Accent)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Center)
							.Padding(0.0f, 10.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
									.Text(Heading)
									.TextStyle(&Text(TEXT("Backlot.Mono.10.Medium")))
									.ColorAndOpacity(Accent)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Center)
							.Padding(0.0f, 10.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
									.Text(Detail)
									.TextStyle(&Text(TEXT("Backlot.Sans.12")))
									.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#a2a9b4")))
									.Justification(ETextJustify::Center)
									.AutoWrapText(true)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Center)
							.Padding(0.0f, 16.0f, 0.0f, 0.0f)
							[
								SNew(SButton)
									.Visibility(bLoading
										? EVisibility::Collapsed
										: EVisibility::Visible)
									.ButtonStyle(&Button(TEXT("Backlot.Button.Primary")))
									.ContentPadding(FMargin(14.0f, 7.0f))
									.OnClicked_Lambda(
										[this, bOpenSettings]()
										{
											if (bOpenSettings)
											{
												if (ISettingsModule* SettingsModule =
													FModuleManager::LoadModulePtr<ISettingsModule>(
														TEXT("Settings")))
												{
													SettingsModule->ShowViewer(
														TEXT("Project"),
														TEXT("Extended Framework"),
														TEXT("ExtendedAtlassian"));
												}
												return FReply::Handled();
											}
											return OnRefresh();
										})
									[
										SNew(STextBlock)
											.Text(bOpenSettings
												? LOCTEXT("OpenBacklotSettings", "OPEN SETTINGS")
												: LOCTEXT("RetryBacklot", "RETRY"))
											.TextStyle(&Text(TEXT("Backlot.Sans.11")))
									]
							]
						]
				]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildWorkspaceStatusBanner()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FExtendedAtlassianWorkspaceSnapshot& Snapshot = Controller->GetSnapshot();
	const bool bVisible = Snapshot.bRefreshing || Snapshot.bStale;
	const bool bStale = Snapshot.bStale;
	const FText Detail =
		bStale && !Snapshot.Error.Message.IsEmpty()
			? FText::FromString(Snapshot.Error.Message)
			: (bStale
				? LOCTEXT(
					"BacklotStaleDetail",
					"Cached content is visible. Retry when the connection is available.")
				: LOCTEXT(
					"BacklotRefreshingDetail",
					"Cached content remains available while Backlot refreshes."));

	return SNew(SBorder)
		.Visibility(bVisible ? EVisibility::Visible : EVisibility::Collapsed)
		.BorderImage(Brush(
			bStale
				? TEXT("Backlot.Brush.IssueThreadAmber")
				: TEXT("Backlot.Brush.IssueThreadBlue")))
		.Padding(FMargin(12.0f, 6.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(bStale
						? LOCTEXT("BacklotStaleLabel", "STALE")
						: LOCTEXT("BacklotRefreshingLabel", "REFRESHING"))
					.TextStyle(&Text(TEXT("Backlot.Mono.9")))
					.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(
						bStale ? TEXT("#e3a54a") : TEXT("#58a6ff")))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(12.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text(Detail)
					.TextStyle(&Text(TEXT("Backlot.Sans.11")))
					.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#a2a9b4")))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
					.Visibility(bStale ? EVisibility::Visible : EVisibility::Collapsed)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
					.ContentPadding(FMargin(8.0f, 3.0f))
					.OnClicked(this, &SExtendedAtlassianWorkspace::OnRefresh)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("RetryStaleBacklot", "RETRY"))
							.TextStyle(&Text(TEXT("Backlot.Mono.9")))
							.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#e3a54a")))
					]
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildDocsMain()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FExtendedAtlassianPage* Page = SelectedPage();
	if (!Page)
	{
		return EmptyState(LOCTEXT("NoPages", "NO PAGES"));
	}
	FString SectionName = SelectedDocumentSection(
		Controller->GetSnapshot(),
		Page->Id);
	if (const FExtendedAtlassianDocumentTreeNode* PageNode =
		Controller->GetSnapshot().DocumentTree.FindByPredicate(
			[Page](const FExtendedAtlassianDocumentTreeNode& Node)
			{
				return Node.Id == Page->Id;
			}))
	{
		if (!PageNode->ParentId.IsEmpty())
		{
			if (const FExtendedAtlassianDocumentTreeNode* Section =
				Controller->GetSnapshot().DocumentTree.FindByPredicate(
					[PageNode](const FExtendedAtlassianDocumentTreeNode& Node)
					{
						return Node.Id == PageNode->ParentId;
					}))
			{
				SectionName = Section->Label;
			}
		}
	}
	TSharedRef<SHorizontalBox> Contributors = SNew(SHorizontalBox);
	// Only real contributors. This used to fall back to the first three people in the space
	// whenever the page carried none, which is always on the live path — so the header claimed
	// "3 contributors" and showed three arbitrary members who may never have touched the page.
	// Invented attribution is worse than none: when the data is missing it must look missing.
	const TArray<FExtendedAtlassianUser>& PageContributors = Page->Contributors;
	for (int32 Index = 0; Index < PageContributors.Num(); ++Index)
	{
		const FExtendedAtlassianUser& Contributor = PageContributors[Index];
		const FString Initials = Contributor.Initials;
		Contributors->AddSlot()
		.AutoWidth()
		.Padding(Index == 0 ? 0.0f : -7.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(22.0f)
			.HeightOverride(22.0f)
			[
				SNew(SBorder)
				.BorderImage(AvatarBrush(&Contributor, Initials))
				.Padding(0.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Initials))
					.TextStyle(&Text(TEXT("Backlot.Mono.9")))
					.ColorAndOpacity(AvatarForeground(&Contributor))
				]
			]
		];
	}

	TArray<const FExtendedAtlassianPage*> ChildPages;
	for (const FExtendedAtlassianPage& Candidate : Controller->GetSnapshot().Pages)
	{
		if (Candidate.ParentId == Page->Id)
		{
			ChildPages.Add(&Candidate);
		}
	}
	TSharedRef<SVerticalBox> RelatedContent = SNew(SVerticalBox);
	if (!ChildPages.IsEmpty())
	{
		TSharedRef<SWrapBox> RelatedGrid =
			SNew(SWrapBox)
			.UseAllottedSize(true)
			.InnerSlotPadding(FVector2D(12.0f, 12.0f));
		for (const FExtendedAtlassianPage* ChildPage : ChildPages)
		{
			const FText VersionText = ChildPage->Version > 0
				? FText::Format(
					LOCTEXT("RelatedPageVersion", "VERSION {0}"),
					FText::AsNumber(ChildPage->Version))
				: FText::GetEmpty();
			RelatedGrid->AddSlot()
			[
				SNew(SBox)
				.WidthOverride(286.0f)
				.MinDesiredHeight(126.0f)
				[
					SNew(SBorder)
					.BorderImage(Brush(TEXT("Backlot.Brush.Card")))
					.Padding(FMargin(16.0f, 14.0f))
					[
						SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SelectableText(
									LOCTEXT("RelatedChildPageType", "CHILD PAGE"),
									TEXT("Backlot.Mono.9"),
									false,
									1.0f,
									FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 7.0f, 0.0f, 9.0f)
							[
								SelectableText(
									FText::FromString(ChildPage->Title),
									TEXT("Backlot.Sans.13.Medium"),
									true)
							]
							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							.VAlign(VAlign_Bottom)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
								[
									SelectableText(
										VersionText,
										TEXT("Backlot.Mono.9"),
										false,
										1.0f,
										FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SButton)
									.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
									.ContentPadding(FMargin(5.0f, 2.0f))
									.OnClicked_Lambda(
										[this, PageId = ChildPage->Id]()
										{
											Controller->SelectPage(PageId);
											return FReply::Handled();
										})
									[
										SNew(STextBlock)
										.Text(LOCTEXT("OpenRelatedPage", "OPEN"))
										.TextStyle(&Text(TEXT("Backlot.Mono.9")))
										.ColorAndOpacity(
											FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")))
									]
								]
							]
					]
				]
			];
		}
		RelatedContent->AddSlot()
		.AutoHeight()
		[
			SelectableText(
				LOCTEXT("RelatedContentTitle", "RELATED CONTENT"),
				TEXT("Backlot.Mono.11"),
				false,
				1.0f,
				FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
		];
		RelatedContent->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 7.0f, 0.0f, 11.0f)
		[
			SelectableText(
				FText::Format(
					ChildPages.Num() == 1
						? LOCTEXT("OneRelatedChildPage", "{0} child page")
						: LOCTEXT("ManyRelatedChildPages", "{0} child pages"),
					FText::AsNumber(ChildPages.Num())),
				TEXT("Backlot.Sans.11"),
				false,
				1.0f,
				FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
		];
		RelatedContent->AddSlot()
		.AutoHeight()
		[
			RelatedGrid
		];
	}

	TSharedRef<SWidget> Document = SNullWidget::NullWidget;
	if (bDocumentEditing)
	{
		TSharedRef<SExtendedAtlassianDocumentEditor> Editor =
			SAssignNew(DocumentEditor, SExtendedAtlassianDocumentEditor)
			.OnMarkdownChanged(
				SExtendedAtlassianDocumentEditor::FOnMarkdownChanged::CreateSP(
					this,
					&SExtendedAtlassianWorkspace::OnDocumentMarkdownChanged));
		Editor->SetMarkdown(DocumentDraftMarkdown);
		const FString Blockers = FString::Join(Page->RoundTripBlockers, TEXT(", "));
		Editor->SetReadOnly(
			!Page->bCanRoundTrip,
			FText::Format(
				LOCTEXT(
					"DocumentRoundTripBlocked",
					"This page is read-only because publishing could discard unsupported Confluence content: {0}"),
				FText::FromString(
					Blockers.IsEmpty()
						? FString(TEXT("unsupported storage nodes"))
						: Blockers)));
		Document = Editor;
	}
	else
	{
		TSharedRef<SExtendedAtlassianDocumentView> Reader =
			SNew(SExtendedAtlassianDocumentView)
			// A reading width, not a font size: the 0.75 CSS-pixel-to-point factor this
			// style applies to type must not be applied here. Scaling it left the text
			// column a quarter narrower than the measure the layout was designed around.
			.MaxReadingWidth(920.0f)
			.OnTaskToggled(
				SExtendedAtlassianDocumentView::FOnTaskToggled::CreateSP(
					this,
					&SExtendedAtlassianWorkspace::OnDocumentTaskToggled))
			.OnIssueClicked(
				SExtendedAtlassianDocumentView::FOnIssueClicked::CreateSP(
					this,
					&SExtendedAtlassianWorkspace::OnDocumentIssueClicked))
			.OnAssetClicked(
				SExtendedAtlassianDocumentView::FOnAssetClicked::CreateSP(
					this,
					&SExtendedAtlassianWorkspace::OnDocumentAssetClicked));
		TMap<FString, FString> IssueStatuses;
		for (const FExtendedAtlassianIssue& Issue :
			Controller->GetSnapshot().Issues)
		{
			IssueStatuses.Add(Issue.Key, Issue.StatusName);
		}
		Reader->SetIssueStatuses(IssueStatuses);
		Reader->SetBlocks(Page->Blocks);
		Document = Reader;
		DocumentEditor.Reset();
	}

	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Workspace")))
		.Padding(0.0f)
		[
			SNew(SScrollBox)
			.ScrollBarStyle(
				&FExtendedAtlassianStyle::Get()
					.GetWidgetStyle<FScrollBarStyle>(
						TEXT("Backlot.ScrollBar")))
			+ SScrollBox::Slot()
			.Padding(
				Controller->IsCompact()
					? FMargin(20.0f, 22.0f, 20.0f, 60.0f)
					: FMargin(56.0f, 40.0f, 56.0f, 90.0f))
			[
			SNew(SBox)
			.WidthOverride_Lambda(
				[this]()
				{
					if (Controller->IsCompact())
					{
						return 462.0f;
					}
					const float WorkspaceWidth =
						GetCachedGeometry().GetLocalSize().X;
					const float SidebarWidth = FMath::Clamp(
						WorkspaceWidth * 0.17f,
						ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.Sidebar.MinWidth")),
						ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.Sidebar.MaxWidth"))) + 1.0f;
					const float RailWidth =
						Controller->IsRailOpen() ? ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.RightRail.Width")) + 1.0f : 0.0f;
					return FMath::Clamp(
						WorkspaceWidth
							- 59.0f
							- SidebarWidth
							- RailWidth
							- 112.0f,
						0.0f,
						ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.Document.MaxWidth")));
				})
			.HAlign(HAlign_Left)
			[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBorder)
					.BorderImage(Brush(
						bDocumentEditing
							? TEXT("Backlot.Brush.DocumentDraftTag")
							: TEXT("Backlot.Brush.DocumentLiveTag")))
					.Padding(FMargin(8.0f, 3.0f))
					[
						SelectableText(
							bDocumentEditing
								? LOCTEXT("DocumentDraftState", "DRAFT")
								: LOCTEXT("DocumentLiveState", "LIVE DOC"),
							TEXT("Backlot.Mono.9"),
							false,
							1.0f,
							bDocumentEditing
								? FExtendedAtlassianStyle::FromHex(TEXT("#e3a54a"))
								: FExtendedAtlassianStyle::FromHex(TEXT("#57cc8a")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(10.0f, 0.0f)
				[
					SelectableText(
						bDocumentEditing
							? FText::Format(
								LOCTEXT(
									"DocumentEditingVersion",
									"{0} · EDITING"),
								FText::FromString(SectionName.ToUpper()))
							: FText::Format(
								LOCTEXT(
									"DocumentLiveVersion",
									"{0} · v{1}"),
								FText::FromString(SectionName.ToUpper()),
								FText::AsNumber(Page->Version)),
						TEXT("Backlot.Mono.10"),
						false,
						1.0f,
						FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(10.0f, 0.0f)
				[
					SelectableText(
						bDocumentEditing
							? LOCTEXT(
								"DocumentNotPublished",
								"·  NOT PUBLISHED YET")
							: FText::Format(
								LOCTEXT(
									"DocumentEditedBy",
									"·  EDITED {0} BY {1}"),
								FText::FromString(Page->EditedAtLabel),
								FText::FromString(Page->EditedByLabel)),
						TEXT("Backlot.Mono.10"),
						false,
						1.0f,
						FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.7f, 0.0f, 13.2f)
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex(bDocumentEditing ? 1 : 0)
				+ SWidgetSwitcher::Slot()
				[
					SelectableText(
						FText::FromString(Page->Title),
						TEXT("Backlot.Sans.34.Semibold"),
						true,
						0.91f)
				]
				+ SWidgetSwitcher::Slot()
				[
					SNew(SEditableTextBox)
					.Style(
						&FExtendedAtlassianStyle::Get()
							.GetWidgetStyle<FEditableTextBoxStyle>(
								TEXT("Backlot.Field")))
					.Font(Text(TEXT("Backlot.Sans.34.Semibold")).Font)
					.Text(FText::FromString(DocumentDraftTitle))
					.HintText(LOCTEXT("DocumentTitleHint", "Page title"))
					.OnTextChanged(
						this,
						&SExtendedAtlassianWorkspace::OnDocumentTitleChanged)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(SBorder)
				.BorderImage(Brush(TEXT("Backlot.Brush.Card")))
				.Padding(FMargin(11.0f, 8.0f))
				.Visibility_Lambda(
					[this]()
					{
						return bDocumentEditing
							&& !DocumentExternalChangeWarning.IsEmpty()
								? EVisibility::Visible
								: EVisibility::Collapsed;
					})
				[
					SNew(STextBlock)
						.Text_Lambda(
							[this]()
							{
								return DocumentExternalChangeWarning;
							})
						.TextStyle(&Text(TEXT("Backlot.Sans.11")))
						.ColorAndOpacity(
							FExtendedAtlassianStyle::FromHex(TEXT("#e3a54a")))
						.AutoWrapText(true)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 20.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					Contributors
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(18.0f, 0.0f)
				[
					SNew(STextBlock)
					// Collapsed rather than showing "0 contributors": a count of nobody is noise,
					// and the row it sits in carries other page metadata that still reads fine.
					.Visibility(
						PageContributors.IsEmpty()
							? EVisibility::Collapsed
							: EVisibility::Visible)
					.Text(FText::Format(
						LOCTEXT(
							"DocumentContributorCount",
							"{0} contributors"),
						FText::AsNumber(PageContributors.Num())))
					.TextStyle(&Text(TEXT("Backlot.Sans.11")))
					.ColorAndOpacity(
						FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SSeparator)
					.Orientation(Orient_Vertical)
					.Thickness(1.0f)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(18.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(
						Page->OwnerAccountId.IsEmpty()
							? FString(TEXT("Owner · unassigned"))
							: Page->OwnerAccountId))
					.TextStyle(&Text(TEXT("Backlot.Sans.11")))
					.ColorAndOpacity(
						FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SSeparator)
					.Orientation(Orient_Vertical)
					.Thickness(1.0f)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(18.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(
						Page->MilestoneText.IsEmpty()
							? FString(TEXT("Reviewed each milestone"))
							: Page->MilestoneText))
					.TextStyle(&Text(TEXT("Backlot.Sans.11")))
					.ColorAndOpacity(
						FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 28.2f)
			[
				SNew(SSeparator)
				.Thickness(1.0f)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(
				bDocumentEditing
					? FMargin(-98.0f, 0.0f, 0.0f, 0.0f)
					: FMargin(0.0f))
			[
				Document
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 42.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.Visibility(
					!bDocumentEditing && !ChildPages.IsEmpty()
						? EVisibility::Visible
						: EVisibility::Collapsed)
				[
					RelatedContent
				]
			]
			]
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildIssuesMain()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	TArray<TSharedRef<SWidget>> RowWidgets;
	int32 VisibleCount = 0;
	const FExtendedAtlassianWorkspaceSnapshot& Snapshot = Controller->GetSnapshot();
	for (const FExtendedAtlassianIssue& Issue : Snapshot.Issues)
	{
		if (!IssueMatchesCurrentFilters(Issue))
		{
			continue;
		}
		++VisibleCount;
		const bool bSelected = Issue.Key == Controller->GetSelectedIssueKey();
		const bool bCompact = Controller->IsCompact();
		const FExtendedAtlassianTeamLoad* AssigneeLoad =
			Snapshot.TeamLoad.FindByPredicate(
				[&Issue](const FExtendedAtlassianTeamLoad& Load)
				{
					return Load.User.AccountId == Issue.AssigneeAccountId;
				});
		const FExtendedAtlassianUser* Assignee =
			AssigneeLoad ? &AssigneeLoad->User : nullptr;
		if (!Assignee)
		{
			Assignee = Snapshot.People.FindByPredicate(
				[&Issue](const FExtendedAtlassianUser& User)
				{
					return User.AccountId == Issue.AssigneeAccountId
						|| User.Initials == Issue.AssigneeAccountId;
				});
		}
		const FString AssigneeInitialsText = AssigneeInitials(Assignee, Issue);
		const FString TypeGlyph = Issue.IssueTypeName == TEXT("Bug")
			? TEXT("B")
			: (Issue.IssueTypeName == TEXT("Doc") ? TEXT("D") : TEXT("T"));
		const TCHAR* TypeBrush = Issue.IssueTypeName == TEXT("Bug")
			? TEXT("Backlot.Brush.Red")
			: (Issue.IssueTypeName == TEXT("Doc")
				? TEXT("Backlot.Brush.Purple")
				: TEXT("Backlot.Brush.Blue"));
		const TCHAR* TypeColor = Issue.IssueTypeName == TEXT("Bug")
			? TEXT("#f0665f")
			: (Issue.IssueTypeName == TEXT("Doc")
				? TEXT("#b6a9ff")
				: TEXT("#58a6ff"));
		const TSharedRef<SWidget> RowWidget =
			SNew(SBox)
			.HeightOverride(35.0f)
			[
				SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
				.ContentPadding(0.0f)
				.OnClicked_Lambda([Controller = Controller, Key = Issue.Key]()
				{
					Controller->SelectIssue(Key);
					return FReply::Handled();
				})
				[
					SNew(SBorder)
					.BorderImage(
						bSelected
							? Brush(TEXT("Backlot.Brush.RowSelected"))
							: FStyleDefaults::GetNoBrush())
					.Padding(FMargin(16.0f, 0.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(bCompact ? 56.0f : 84.0f)
							[
								SNew(STextBlock)
								.Text(FText::FromString(Issue.Key))
								.TextStyle(&Text(TEXT("Backlot.Mono.11")))
								.ColorAndOpacity(
									FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 16.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 9.0f, 0.0f)
							[
								SNew(SBox)
								.WidthOverride(13.0f)
								.HeightOverride(13.0f)
								[
									SNew(SBorder)
									.BorderImage(Brush(TypeBrush))
									.Padding(0.0f)
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(FText::FromString(TypeGlyph))
										.TextStyle(&Text(TEXT("Backlot.Mono.8")))
										.ColorAndOpacity(
											FExtendedAtlassianStyle::FromHex(TypeColor))
									]
								]
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(FText::FromString(Issue.Summary))
								.TextStyle(&Text(TEXT("Backlot.Sans.13")))
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(6.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SHorizontalBox)
								.Visibility(
									Issue.CommentCount > 0
										? EVisibility::Visible
										: EVisibility::Collapsed)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SImage)
									.Image(Brush(TEXT("Backlot.Icon.Comment")))
									.ColorAndOpacity(
										FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(3.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(FText::AsNumber(Issue.CommentCount))
									.TextStyle(&Text(TEXT("Backlot.Mono.10")))
									.ColorAndOpacity(
										FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
								]
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(bCompact ? 100.0f : 126.0f)
							[
								SNew(SButton)
								.IsEnabled(Controller->CanExecuteMutation(
									EExtendedAtlassianWorkspaceMutation::TransitionIssue))
								.ToolTipText(MutationTooltip(
									Controller,
									EExtendedAtlassianWorkspaceMutation::TransitionIssue))
								.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
								.ContentPadding(0.0f)
								.OnClicked_Lambda(
									[this, Key = Issue.Key]()
									{
										OpenStatusMenu(Key);
										return FReply::Handled();
									})
								[
									SNew(SBorder)
									.BorderImage(Brush(TEXT("Backlot.Brush.CardSelected")))
									.Padding(FMargin(8.0f, 4.0f))
									[
										SNew(SHorizontalBox)
											+ SHorizontalBox::Slot()
											.AutoWidth()
											.VAlign(VAlign_Center)
											.Padding(0.0f, 0.0f, 6.0f, 0.0f)
											[
												SNew(SBox)
												.WidthOverride(5.0f)
												.HeightOverride(5.0f)
												[
													SNew(SImage)
													.Image(Brush(TEXT("Backlot.Brush.Dot")))
													.ColorAndOpacity(
														StatusColor(
															Issue.StatusName,
															Issue.StatusCategoryKey))
												]
											]
											+ SHorizontalBox::Slot()
											.AutoWidth()
											.VAlign(VAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(
													Issue.StatusName))
												.TextStyle(&Text(
													TEXT(
														"Backlot.Sans.10.Medium")))
												.ColorAndOpacity(
													StatusColor(
														Issue.StatusName,
														Issue.StatusCategoryKey))
											]
											+ SHorizontalBox::Slot()
											.AutoWidth()
											.VAlign(VAlign_Center)
											.Padding(6.0f, 0.0f, 0.0f, 0.0f)
											[
												SNew(SImage)
												.Image(Brush(
													TEXT(
														"Backlot.Icon.CaretDown")))
												.ColorAndOpacity(
													StatusColor(
														Issue.StatusName,
														Issue.StatusCategoryKey))
											]
									]
								]
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(bCompact ? 0.0f : 150.0f)
							.Visibility(
								bCompact
									? EVisibility::Collapsed
									: EVisibility::Visible)
							[
								SNew(STextBlock)
								.Text(FText::FromString(
									TEXT("▌  ") + Issue.EpicName))
								.TextStyle(&Text(TEXT("Backlot.Sans.11")))
								.ColorAndOpacity(
									FExtendedAtlassianStyle::FromHex(
										Issue.EpicColor.IsEmpty()
											? TEXT("#8a919c")
											: *Issue.EpicColor))
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(54.0f)
							.Visibility(
								bCompact
									? EVisibility::Collapsed
									: EVisibility::Visible)
							[
								SNew(STextBlock)
								.Text(FText::AsNumber(Issue.Estimate))
								.TextStyle(&Text(TEXT("Backlot.Mono.11")))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(bCompact ? 26.0f : 72.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Right)
								[
									SNew(STextBlock)
									.Visibility(
										bCompact
											? EVisibility::Collapsed
											: EVisibility::Visible)
									.Text(FText::FromString(Issue.RelativeUpdated))
									.TextStyle(&Text(TEXT("Backlot.Mono.10")))
									.ColorAndOpacity(
										FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(9.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(SBox)
									.WidthOverride(22.0f)
									.HeightOverride(22.0f)
									[
										SNew(SBorder)
										.BorderImage(AvatarBrush(Assignee, AssigneeInitialsText))
										.Padding(0.0f)
										.HAlign(HAlign_Center)
										.VAlign(VAlign_Center)
										[
											SNew(STextBlock)
											.Text(FText::FromString(AssigneeInitialsText))
											.TextStyle(&Text(TEXT("Backlot.Mono.9")))
											.ColorAndOpacity(AvatarForeground(Assignee))
										]
									]
								]
							]
						]
					]
				]
			]
		;
		RowWidgets.Add(RowWidget);
	}
	if (VisibleCount == 0)
	{
		RowWidgets.Add(
			SNew(SBox)
			.Padding(FMargin(0.0f, 80.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT(
						"NoIssuesMatch",
						"NO ISSUES MATCH THIS FILTER"))
					.TextStyle(&Text(TEXT("Backlot.Mono.10")))
					.ColorAndOpacity(
						FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 11.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
					.ContentPadding(0.0f)
					.OnClicked_Lambda(
						[Controller = Controller]()
						{
							Controller->ResetIssueFilters();
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ClearIssueFilters", "Clear filters"))
						.TextStyle(&Text(TEXT("Backlot.Sans.11")))
						.ColorAndOpacity(
							FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")))
					]
				]
			]);
	}

	FString AssigneeLabel = TEXT("anyone");
	if (!Controller->GetAssigneeFilter().IsEmpty())
	{
		if (const FExtendedAtlassianUser* User =
			Controller->GetSnapshot().People.FindByPredicate(
				[this](const FExtendedAtlassianUser& Candidate)
				{
					return Candidate.AccountId == Controller->GetAssigneeFilter();
				}))
		{
			AssigneeLabel = User->DisplayName;
		}
	}
	const FString ProjectLabel =
		ProjectDisplayName(Controller->GetSnapshot());
	FString SprintLabel =
		SelectedSprintName(Controller->GetSnapshot());
	SprintLabel.RemoveFromStart(TEXT("Sprint "));

	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Workspace")))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(41.0f)
				[
					SNew(SBorder)
					.BorderImage(Brush(TEXT("Backlot.Brush.BorderSubtle")))
					.Padding(FMargin(0.0f, 0.0f, 0.0f, 1.0f))
					[
						SNew(SBorder)
						.BorderImage(Brush(TEXT("Backlot.Brush.Workspace")))
						.Padding(FMargin(16.0f, 0.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 8.0f, 0.0f)
							[
								SNew(SBox)
								.HeightOverride(27.0f)
								[
									SNew(SBorder)
									.BorderImage(Brush(TEXT("Backlot.Brush.FieldAlt")))
									.Padding(FMargin(10.0f, 0.0f))
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(FText::FromString(
											TEXT("Project   ") + ProjectLabel))
										.TextStyle(&Text(TEXT("Backlot.Sans.11")))
									]
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 8.0f, 0.0f)
							[
								SNew(SBox)
								.HeightOverride(27.0f)
								[
									SNew(SBorder)
									.BorderImage(Brush(TEXT("Backlot.Brush.FieldAlt")))
									.Padding(FMargin(10.0f, 0.0f))
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(FText::FromString(
											TEXT("Sprint   ") + SprintLabel))
										.TextStyle(&Text(TEXT("Backlot.Sans.11")))
									]
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 8.0f, 0.0f)
							[
								SNew(SBox)
								.HeightOverride(27.0f)
								[
									SNew(SButton)
									.ButtonStyle(&Button(
										TEXT("Backlot.Button.Secondary")))
									.ContentPadding(FMargin(10.0f, 0.0f))
									.OnClicked_Lambda(
										[Controller = Controller]()
										{
											Controller->CycleStatusFilter();
											return FReply::Handled();
										})
									[
										SNew(STextBlock)
										.Text(FText::FromString(
											TEXT("Status   ")
												+ Controller->GetStatusFilter()))
										.TextStyle(&Text(TEXT("Backlot.Sans.11")))
									]
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SBox)
								.HeightOverride(27.0f)
								[
									SNew(SButton)
									.ButtonStyle(&Button(
										TEXT("Backlot.Button.Secondary")))
									.ContentPadding(FMargin(10.0f, 0.0f))
									.OnClicked_Lambda(
										[Controller = Controller]()
										{
											Controller->CycleAssigneeFilter();
											return FReply::Handled();
										})
									[
										SNew(STextBlock)
										.Text(FText::FromString(
											TEXT("Assignee   ") + AssigneeLabel))
										.TextStyle(&Text(TEXT("Backlot.Sans.11")))
									]
								]
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 8.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(FText::Format(
									LOCTEXT("IssueCount", "{0} ISSUES"),
									FText::AsNumber(VisibleCount)))
								.TextStyle(&Text(TEXT("Backlot.Mono.10")))
								.ColorAndOpacity(
									FExtendedAtlassianStyle::FromHex(
										TEXT("#6f7783")))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SBox)
								.HeightOverride(27.0f)
								[
									SNew(SButton)
									.IsEnabled(Controller->CanExecuteMutation(
										EExtendedAtlassianWorkspaceMutation::
											CreateIssue))
									.ToolTipText(MutationTooltip(
										Controller,
										EExtendedAtlassianWorkspaceMutation::
											CreateIssue))
									.ButtonStyle(&Button(
										TEXT("Backlot.Button.Primary")))
									.ContentPadding(FMargin(11.0f, 0.0f))
									.OnClicked_Lambda(
										[this]()
										{
											OpenCreateCard(TEXT("Triage"));
											return FReply::Handled();
										})
									[
										SNew(STextBlock)
										.Text(LOCTEXT("NewIssue", "+  New issue"))
										.TextStyle(&Text(TEXT("Backlot.Sans.11")))
									]
								]
							]
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(29.0f)
				[
					SNew(SBorder)
					.BorderImage(Brush(TEXT("Backlot.Brush.FieldAlt")))
					.Padding(FMargin(16.0f, 0.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(Controller->IsCompact() ? 56.0f : 84.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("IssueColumnKey", "KEY"))
								.TextStyle(&Text(TEXT("Backlot.Mono.9")))
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("IssueColumnSummary", "SUMMARY"))
							.TextStyle(&Text(TEXT("Backlot.Mono.9")))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(Controller->IsCompact() ? 100.0f : 126.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("IssueColumnStatus", "STATUS"))
								.TextStyle(&Text(TEXT("Backlot.Mono.9")))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(150.0f)
							.Visibility(
								Controller->IsCompact()
									? EVisibility::Collapsed
									: EVisibility::Visible)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("IssueColumnEpic", "EPIC"))
								.TextStyle(&Text(TEXT("Backlot.Mono.9")))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(54.0f)
							.Visibility(
								Controller->IsCompact()
									? EVisibility::Collapsed
									: EVisibility::Visible)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("IssueColumnPoints", "PTS"))
								.TextStyle(&Text(TEXT("Backlot.Mono.9")))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(Controller->IsCompact() ? 26.0f : 72.0f)
							[
								SNew(STextBlock)
								.Text(Controller->IsCompact()
									? FText::GetEmpty()
									: LOCTEXT("IssueColumnUpdated", "UPDATED"))
								.TextStyle(&Text(TEXT("Backlot.Mono.9")))
							]
						]
					]
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SBacklotVirtualizedWidgetList)
				.Widgets(RowWidgets)
				.ScrollBarStyle(
					&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(
						TEXT("Backlot.ScrollBar")))
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildIssueDescription(
	const FExtendedAtlassianIssue& Issue)
{
	using namespace ExtendedAtlassianWorkspacePrivate;

	TArray<FExtendedAtlassianDocBlock> Blocks = Issue.DescriptionBlocks;
	if (Blocks.IsEmpty() && !Issue.Description.IsEmpty())
	{
		// Fixtures and older Jira sites can still provide plain text. They enter the exact same
		// document renderer after this adapter rather than creating a second issue-only text path.
		Blocks = FExtendedAtlassianMarkdown::ToBlocks(Issue.Description);
	}

	if (Blocks.IsEmpty())
	{
		return SelectableText(
			LOCTEXT("IssueNoDescription", "No description yet."),
			TEXT("Backlot.Sans.14"),
			true,
			1.3f,
			FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")));
	}

	TSharedRef<SExtendedAtlassianDocumentView> Reader =
		SNew(SExtendedAtlassianDocumentView)
		.MaxReadingWidth(672.0f)
		.OnIssueClicked(
			SExtendedAtlassianDocumentView::FOnIssueClicked::CreateSP(
				this,
				&SExtendedAtlassianWorkspace::OnDocumentIssueClicked));
	TMap<FString, FString> IssueStatuses;
	for (const FExtendedAtlassianIssue& SnapshotIssue :
		Controller->GetSnapshot().Issues)
	{
		IssueStatuses.Add(SnapshotIssue.Key, SnapshotIssue.StatusName);
	}
	Reader->SetIssueStatuses(IssueStatuses);
	Reader->SetBlocks(Blocks);
	return Reader;
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildIssueDetailMain()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FExtendedAtlassianIssue* Issue = SelectedIssue();
	if (!Issue) { return EmptyState(LOCTEXT("NoIssue", "NO ISSUE")); }
	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Workspace")))
		.Padding(FMargin(38.0f, 26.0f))
		[
			SNew(SScrollBox)
			.ScrollBarStyle(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(TEXT("Backlot.ScrollBar")))
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SButton).ButtonStyle(&Button(TEXT("Backlot.Button.Clear"))).ContentPadding(0.0f).OnClicked(this, &SExtendedAtlassianWorkspace::OnNavigate, EExtendedAtlassianWorkspaceRoute::Issues)
					[
						SNew(STextBlock).Text(LOCTEXT("BacklogBack", "←  BACKLOG")).TextStyle(&Text(TEXT("Backlot.Mono.10")))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 18.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Issue->Key + TEXT("     ") + Issue->IssueTypeName + TEXT("     ") + Issue->PriorityName)).TextStyle(&Text(TEXT("Backlot.Mono.11"))).ColorAndOpacity(StatusColor(Issue->StatusName, Issue->StatusCategoryKey))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SelectableText(
						FText::FromString(Issue->Summary),
						TEXT("Backlot.Sans.27.Semibold"),
						true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 28.0f, 0.0f, 8.0f)[SectionLabel(LOCTEXT("Description", "DESCRIPTION"))]
				+ SVerticalBox::Slot().AutoHeight()
				[
					BuildIssueDescription(*Issue)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 28.0f, 0.0f, 8.0f)[SectionLabel(LOCTEXT("Threads", "THREADS"))]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("LegacyThreadsUnavailable", "Threads are provided by the connected workspace.")).TextStyle(&Text(TEXT("Backlot.Sans.12")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 28.0f, 0.0f, 8.0f)[SectionLabel(LOCTEXT("Activity", "ACTIVITY"))]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("LegacyActivityUnavailable", "Activity is provided by the connected workspace.")).TextStyle(&Text(TEXT("Backlot.Sans.11"))).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 28.0f, 0.0f, 8.0f)[SectionLabel(LOCTEXT("Comments", "COMMENTS"))]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SEditableTextBox).Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("Backlot.Field"))).HintText(LOCTEXT("CommentHint", "Write a comment…"))
				]
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildIssueDetailMainDynamic()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FExtendedAtlassianIssue* Issue = SelectedIssue();
	if (!Issue)
	{
		return EmptyState(LOCTEXT("NoIssueDynamic", "NO ISSUE"));
	}
	const FString Scope = TEXT("issue:") + Issue->Key;
	const FExtendedAtlassianCommentCollection* Collection =
		Controller->GetSnapshot().CommentCollections.FindByPredicate(
			[&Scope](const FExtendedAtlassianCommentCollection& Candidate)
			{
				return Candidate.TargetId == Scope;
			});

	TSharedRef<SVerticalBox> Threads = SNew(SVerticalBox);
	int32 ThreadCount = 0;
	if (SelectedIssueThreadId.IsEmpty())
	{
		TArray<const FExtendedAtlassianIssueThread*> MatchingThreads;
		for (const FExtendedAtlassianIssueThread& Thread :
			Controller->GetSnapshot().IssueThreads)
		{
			if (Thread.IssueKey == Issue->Key)
			{
				MatchingThreads.Add(&Thread);
			}
		}
		if (!MatchingThreads.IsEmpty())
		{
			SelectedIssueThreadId =
				MatchingThreads[
					Controller->IsFixtureProvider() && MatchingThreads.Num() > 1
						? 1
						: 0]->Id;
		}
	}
	for (const FExtendedAtlassianIssueThread& Thread :
		Controller->GetSnapshot().IssueThreads)
	{
		if (Thread.IssueKey != Issue->Key)
		{
			continue;
		}
		++ThreadCount;
		const FExtendedAtlassianUser* Author =
			Controller->GetSnapshot().People.FindByPredicate(
				[&Thread](const FExtendedAtlassianUser& Candidate)
				{
					return Candidate.AccountId == Thread.AuthorAccountId
						|| Candidate.Initials == Thread.AuthorAccountId;
				});
		const FString Initials = Author
			? Author->Initials
			: Thread.AuthorAccountId.Left(2).ToUpper();
		const FString AuthorName = !Thread.AuthorDisplayName.IsEmpty()
			? Thread.AuthorDisplayName
			: (Author ? Author->DisplayName : Initials);
		const bool bSelected = SelectedIssueThreadId == Thread.Id;
		const TCHAR* SelectedBrush = ThreadCount == 1
			? TEXT("Backlot.Brush.IssueThreadAmber")
			: (ThreadCount == 2
				? TEXT("Backlot.Brush.IssueThreadBlue")
				: TEXT("Backlot.Brush.IssueThreadGreen"));
		Threads->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 9.0f)
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
				.ContentPadding(0.0f)
				.OnClicked_Lambda(
					[this, ThreadId = Thread.Id]()
					{
						SelectedIssueThreadId = ThreadId;
						Rebuild();
						return FReply::Handled();
					})
			[
				SNew(SBorder)
				.BorderImage(Brush(
					bSelected
						? SelectedBrush
						: TEXT("Backlot.Brush.IssueThread")))
				.Padding(FMargin(13.0f, 11.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 11.0f, 0.0f)
					[
						SNew(SBox)
							.WidthOverride(19.0f)
							.HeightOverride(19.0f)
						[
							SNew(SBorder)
								.BorderImage(AvatarBrush(Author, Initials))
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Text(FText::FromString(Initials))
									.TextStyle(&Text(TEXT("Backlot.Mono.9.Medium")))
									.ColorAndOpacity(AvatarForeground(Author))
							]
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(STextBlock)
									.Text(FText::FromString(AuthorName))
									.TextStyle(&Text(TEXT("Backlot.Sans.12.Medium")))
									.ColorAndOpacity(
										FExtendedAtlassianStyle::FromHex(
											TEXT("#d7dce3")))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(8.0f, 0.0f)
							[
								SNew(STextBlock)
									.Text(FText::FromString(Thread.RelativeTime))
									.TextStyle(&Text(TEXT("Backlot.Mono.9")))
									.ColorAndOpacity(
										FExtendedAtlassianStyle::FromHex(
											TEXT("#5c636d")))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(STextBlock)
									.Text(FText::FromString(Thread.Label))
									.TextStyle(&Text(TEXT("Backlot.Mono.10")))
									.ColorAndOpacity(
										FExtendedAtlassianStyle::FromHex(
											Thread.AccentColor.IsEmpty()
												? TEXT("#58a6ff")
												: *Thread.AccentColor))
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
								.Text(FText::FromString(Thread.Body))
								.TextStyle(&Text(TEXT("Backlot.Sans.12")))
								.ColorAndOpacity(
									FExtendedAtlassianStyle::FromHex(
										TEXT("#a2a9b4")))
								.AutoWrapText(true)
						]
					]
				]
			]
		];
	}
	if (ThreadCount == 0)
	{
		Threads->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
				.Text(LOCTEXT(
					"NoIssueThreads",
					"No viewport threads on this issue. Pins linked to this key appear here."))
				.TextStyle(&Text(TEXT("Backlot.Sans.11")))
				.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
		];
	}

	TSharedRef<SVerticalBox> ActivityRows = SNew(SVerticalBox);
	int32 ActivityCount = 0;
	for (const FExtendedAtlassianActivity& Activity :
		Controller->GetSnapshot().Activity)
	{
		if (Activity.IssueKey != Issue->Key)
		{
			continue;
		}
		++ActivityCount;
		const FExtendedAtlassianUser* Actor =
			Controller->GetSnapshot().People.FindByPredicate(
				[&Activity](const FExtendedAtlassianUser& Candidate)
				{
					return Candidate.AccountId == Activity.ActorAccountId
						|| Candidate.Initials == Activity.ActorAccountId;
				});
		const FString Initials = Actor
			? Actor->Initials
			: Activity.ActorAccountId.Left(2).ToUpper();
		ActivityRows->AddSlot()
		.AutoHeight()
		[
			SNew(SBorder)
				.BorderImage(Brush(TEXT("Backlot.Brush.Transparent")))
				.Padding(FMargin(0.0f, 10.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SBox)
						.WidthOverride(22.0f)
						.HeightOverride(22.0f)
					[
						SNew(SBorder)
							.BorderImage(AvatarBrush(Actor, Initials))
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(FText::FromString(Initials))
								.TextStyle(&Text(TEXT("Backlot.Mono.9.Medium")))
								.ColorAndOpacity(AvatarForeground(Actor))
						]
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(Activity.Detail))
						.TextStyle(&Text(TEXT("Backlot.Sans.12")))
						.ColorAndOpacity(
							FExtendedAtlassianStyle::FromHex(TEXT("#a2a9b4")))
						.AutoWrapText(true)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(12.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(
							!Activity.RelativeTime.IsEmpty()
								? Activity.RelativeTime
								: Activity.Verb))
						.TextStyle(&Text(TEXT("Backlot.Mono.10")))
						.ColorAndOpacity(
							FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
				]
			]
		];
		ActivityRows->AddSlot()
		.AutoHeight()
		[
			SNew(SSeparator)
				.SeparatorImage(Brush(TEXT("Backlot.Brush.BorderSubtle")))
				.Thickness(1.0f)
		];
	}
	if (ActivityCount == 0)
	{
		ActivityRows->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
				.Text(LOCTEXT(
					"NoIssueActivity",
					"Nothing recorded yet. Changes you make show up here."))
				.TextStyle(&Text(TEXT("Backlot.Sans.11")))
				.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
		];
	}

	TSharedRef<SVerticalBox> CommentRows = SNew(SVerticalBox);
	if (Collection && !Collection->Comments.IsEmpty())
	{
		for (const FExtendedAtlassianComment& Comment : Collection->Comments)
		{
			CommentRows->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				BuildCommentCard(Scope, Comment, true)
			];
		}
	}
	else
	{
		CommentRows->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
				.Text(LOCTEXT(
					"NoIssueComments",
					"No comments yet. Start the thread below."))
				.TextStyle(&Text(TEXT("Backlot.Sans.11")))
				.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
		];
	}

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	Content->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
				.ContentPadding(0.0f)
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnNavigate,
					EExtendedAtlassianWorkspaceRoute::Issues)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("BacklogBackDynamic", "←  BACKLOG"))
						.TextStyle(&Text(TEXT("Backlot.Mono.10")))
				]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(11.0f, 0.0f)
		[
			SNew(SBox)
				.WidthOverride(1.0f)
				.HeightOverride(13.0f)
			[
				SNew(SImage)
					.Image(Brush(TEXT("Backlot.Brush.BorderSubtle")))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
				.Text(FText::FromString(Issue->Key))
				.TextStyle(&Text(TEXT("Backlot.Mono.11")))
				.ColorAndOpacity(
					FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(11.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
				.BorderImage(Brush(
					Issue->IssueTypeName == TEXT("Bug")
						? TEXT("Backlot.Brush.Red")
						: (Issue->IssueTypeName == TEXT("Doc")
							? TEXT("Backlot.Brush.Purple")
							: TEXT("Backlot.Brush.Blue"))))
				.Padding(FMargin(8.0f, 3.0f))
			[
				SNew(STextBlock)
					.Text(FText::FromString(Issue->IssueTypeName.ToUpper()))
					.TextStyle(&Text(TEXT("Backlot.Mono.9.Medium")))
					.ColorAndOpacity(
						FExtendedAtlassianStyle::FromHex(
							Issue->IssueTypeName == TEXT("Bug")
								? TEXT("#f0a9a4")
								: (Issue->IssueTypeName == TEXT("Doc")
									? TEXT("#d8d0ff")
									: TEXT("#cfe0ff"))))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(7.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
				.BorderImage(Brush(TEXT("Backlot.Brush.Red")))
				.Padding(FMargin(8.0f, 3.0f))
			[
				SNew(STextBlock)
					.Text(FText::FromString(Issue->PriorityName.ToUpper()))
					.TextStyle(&Text(TEXT("Backlot.Mono.9.Medium")))
					.ColorAndOpacity(
						FExtendedAtlassianStyle::FromHex(TEXT("#f0665f")))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
				.Visibility(EVisibility::Collapsed)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
				.ContentPadding(FMargin(8.0f, 6.0f))
				.OnClicked_Lambda(
					[this]()
					{
						OpenIssueActions();
						return FReply::Handled();
					})
				[
					SNew(SImage)
						.Image(Brush(TEXT("Backlot.Icon.More")))
						.ColorAndOpacity(
							FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
				]
		]
	];
	if (bIssueEditing)
	{
		Content->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 15.0f, 0.0f, 0.0f)
		[
			SNew(SEditableTextBox)
				.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
					TEXT("Backlot.Field")))
				.Text(FText::FromString(IssueDraftSummary))
				.OnTextChanged_Lambda(
					[this](const FText& Value)
					{
						IssueDraftSummary = Value.ToString();
					})
		];
		Content->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text(!IssueEditConflictWarning.IsEmpty()
					? IssueEditConflictWarning
					: LOCTEXT(
						"IssueEditingHint",
						"EDITING · SAVE TO PUBLISH, CANCEL TO DISCARD"))
				.TextStyle(&Text(TEXT("Backlot.Mono.10")))
				.ColorAndOpacity(
					FExtendedAtlassianStyle::FromHex(
						!IssueEditConflictWarning.IsEmpty()
							? TEXT("#f0665f")
							: TEXT("#4d545e")))
				.AutoWrapText(true)
		];
		Content->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 12.0f, 0.0f, 0.0f)
		[
			SNew(SMultiLineEditableTextBox)
				.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
					TEXT("Backlot.Field")))
				.Text(FText::FromString(IssueDraftDescription))
				.HintText(LOCTEXT(
					"IssueDescriptionEditHint",
					"Describe the problem, the repro, and what you expect instead…"))
				.OnTextChanged_Lambda(
					[this](const FText& Value)
					{
						IssueDraftDescription = Value.ToString();
					})
		];
		Content->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 10.0f, 0.0f, 0.0f)
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
					.ContentPadding(FMargin(12.0f, 6.0f))
					.OnClicked(this, &SExtendedAtlassianWorkspace::OnCancelIssueEdit)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("CancelIssueEdit", "Cancel"))
							.TextStyle(&Text(TEXT("Backlot.Sans.11")))
					]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(7.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
					.IsEnabled(Controller->CanExecuteMutation(
						EExtendedAtlassianWorkspaceMutation::UpdateIssue))
					.ToolTipText(MutationTooltip(
						Controller,
						EExtendedAtlassianWorkspaceMutation::UpdateIssue))
					.ButtonStyle(&Button(TEXT("Backlot.Button.Primary")))
					.ContentPadding(FMargin(13.0f, 6.0f))
					.OnClicked(this, &SExtendedAtlassianWorkspace::OnSaveIssueEdit)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("SaveIssueEdit", "Save"))
							.TextStyle(&Text(TEXT("Backlot.Sans.11.Medium")))
					]
			]
		];
	}
	else
	{
		Content->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 15.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SelectableText(
					FText::FromString(Issue->Summary),
					TEXT("Backlot.Sans.27.Semibold"),
					true)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(12.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
					.Visibility(EVisibility::Collapsed)
					.IsEnabled(Controller->CanExecuteMutation(
						EExtendedAtlassianWorkspaceMutation::UpdateIssue))
					.ToolTipText(MutationTooltip(
						Controller,
						EExtendedAtlassianWorkspaceMutation::UpdateIssue))
					.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
					.ContentPadding(FMargin(12.0f, 6.0f))
					.OnClicked(this, &SExtendedAtlassianWorkspace::OnStartIssueEdit)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("EditIssue", "Edit"))
							.TextStyle(&Text(TEXT("Backlot.Sans.11.Medium")))
					]
				]
		];
		Content->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT(
					"IssueReadOnlyHint",
					"READ ONLY · HIT EDIT TO CHANGE THE SUMMARY OR DESCRIPTION"))
				.TextStyle(&Text(TEXT("Backlot.Mono.10")))
				.ColorAndOpacity(
					FExtendedAtlassianStyle::FromHex(TEXT("#4d545e")))
		];
		Content->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 28.0f, 0.0f, 8.0f)
		[
			SectionLabel(LOCTEXT("DescriptionDynamic", "DESCRIPTION"))
		];
		Content->AddSlot()
		.AutoHeight()
		[
			SNew(SBox)
			.WidthOverride(672.0f)
			[
				BuildIssueDescription(*Issue)
			]
		];
	}
	Content->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 28.0f, 0.0f, 8.0f)
	[
		SectionLabel(LOCTEXT("ThreadsDynamic", "THREADS"))
	];
	Content->AddSlot()
	.AutoHeight()
	[
		SNew(SBox)
		.WidthOverride(672.0f)
		[
			Threads
		]
	];
	Content->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 28.0f, 0.0f, 8.0f)
	[
		SectionLabel(LOCTEXT("ActivityDynamic", "ACTIVITY"))
	];
	Content->AddSlot()
	.AutoHeight()
	[
		SNew(SBox)
		.WidthOverride(672.0f)
		[
			ActivityRows
		]
	];
	Content->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 28.0f, 0.0f, 8.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SectionLabel(LOCTEXT("CommentsDynamic", "COMMENTS"))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(10.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text(Collection && !Collection->Comments.IsEmpty()
					? FText::Format(
						Collection->Comments.Num() == 1
							? LOCTEXT("IssueOneCommentMeta", "{0} COMMENT")
							: LOCTEXT("IssueManyCommentsMeta", "{0} COMMENTS"),
						FText::AsNumber(Collection->Comments.Num()))
					: LOCTEXT("IssueNoCommentMeta", "NONE YET"))
				.TextStyle(&Text(TEXT("Backlot.Mono.10")))
				.ColorAndOpacity(
					FExtendedAtlassianStyle::FromHex(TEXT("#4d545e")))
		]
	];
	Content->AddSlot()
	.AutoHeight()
	[
		SNew(SBox)
		.WidthOverride(672.0f)
		[
			CommentRows
		]
	];
	Content->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 5.0f, 0.0f, 0.0f)
	[
		SNew(SBox)
		.WidthOverride(672.0f)
		[
			SNew(SBorder)
				.BorderImage(Brush(TEXT("Backlot.Brush.Card")))
				.Padding(10.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SMultiLineEditableTextBox)
						.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
							TEXT("Backlot.Field")))
						.IsEnabled(Controller->CanExecuteMutation(
							EExtendedAtlassianWorkspaceMutation::CreateIssueComment))
						.ToolTipText(MutationTooltip(
							Controller,
							EExtendedAtlassianWorkspaceMutation::CreateIssueComment))
						.HintText(LOCTEXT(
							"IssueCommentHintDynamic",
							"Add a comment… type @ to notify, # to link an issue"))
						.Text(FText::FromString(NewIssueCommentDraft))
						.OnTextChanged_Lambda(
							[this](const FText& Value)
							{
								NewIssueCommentDraft = Value.ToString();
							})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SButton)
							.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
							.ContentPadding(0.0f)
							.OnClicked_Lambda(
								[this]()
								{
									bIssueCommentAttachCapture = !bIssueCommentAttachCapture;
									Rebuild();
									return FReply::Handled();
								})
							[
								SNew(STextBlock)
							.Text(bIssueCommentAttachCapture
										? LOCTEXT("CaptureAttached", "CAPTURE ATTACHED ✓")
										: LOCTEXT("AttachCapture", "ATTACH CAPTURE"))
									.TextStyle(&Text(TEXT("Backlot.Mono.10")))
									.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(
										bIssueCommentAttachCapture
											? TEXT("#57cc8a")
											: TEXT("#6f7783")))
							]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
							.IsEnabled(Controller->CanExecuteMutation(
								EExtendedAtlassianWorkspaceMutation::CreateIssueComment))
							.ToolTipText(MutationTooltip(
								Controller,
								EExtendedAtlassianWorkspaceMutation::CreateIssueComment))
							.ButtonStyle(&Button(TEXT("Backlot.Button.Primary")))
							.ContentPadding(FMargin(13.0f, 6.0f))
							.OnClicked(this, &SExtendedAtlassianWorkspace::OnPostIssueComment)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("PostIssueComment", "Comment"))
									.TextStyle(&Text(TEXT("Backlot.Sans.11.Medium")))
							]
					]
				]
			]
		]
	];

	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Workspace")))
		.Padding(
			Controller->IsCompact()
				? FMargin(18.0f, 20.0f, 18.0f, 60.0f)
				: FMargin(44.0f, 28.0f, 44.0f, 80.0f))
		[
			SNew(SScrollBox)
				.ScrollBarStyle(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(
				TEXT("Backlot.ScrollBar")))
				+ SScrollBox::Slot()
				.HAlign(HAlign_Left)
				[
					SNew(SBox)
						.MaxDesiredWidth(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.IssueDetail.MaxWidth")))
						[
							Content
						]
				]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildBoardMain()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	TSharedRef<SHorizontalBox> Columns = SNew(SHorizontalBox);
	const FString Search = Controller->GetGlobalSearch().TrimStartAndEnd();
	const FExtendedAtlassianWorkspaceSnapshot& Snapshot = Controller->GetSnapshot();
	const FString AssigneeFilter = Controller->GetAssigneeFilter();
	const bool bFilterByAssignee =
		!AssigneeFilter.IsEmpty()
		&& !AssigneeFilter.Equals(TEXT("anyone"), ESearchCase::IgnoreCase);
	for (const FExtendedAtlassianBoardColumn& Column : Snapshot.BoardColumns)
	{
		TSharedRef<SVerticalBox> Cards = SNew(SVerticalBox);
		int32 Count = 0;
		for (const FExtendedAtlassianIssue& Issue : Snapshot.Issues)
		{
			if (!Column.StatusNames.ContainsByPredicate(
					[&Issue](const FString& StatusName)
					{
						return StatusName.Equals(
							Issue.StatusName,
							ESearchCase::IgnoreCase);
					})
				|| !ContainsSearch(
					Issue.Key + TEXT(" ") + Issue.Summary + TEXT(" ")
						+ Issue.EpicName,
					Search)
				|| (bFilterByAssignee
					&& Issue.AssigneeAccountId != AssigneeFilter))
			{
				continue;
			}
			++Count;
			const FExtendedAtlassianTeamLoad* AssigneeLoad =
				Snapshot.TeamLoad.FindByPredicate(
					[&Issue](const FExtendedAtlassianTeamLoad& Load)
					{
						return Load.User.AccountId == Issue.AssigneeAccountId;
					});
			const FExtendedAtlassianUser* Assignee =
				AssigneeLoad ? &AssigneeLoad->User : nullptr;
			if (!Assignee)
			{
				Assignee = Snapshot.People.FindByPredicate(
					[&Issue](const FExtendedAtlassianUser& User)
					{
						return User.AccountId == Issue.AssigneeAccountId
							|| User.Initials == Issue.AssigneeAccountId;
					});
			}
			const FString AssigneeInitials =
				// Qualified: the local shadows the helper inside its own initialiser.
				// The old fallback took the first two characters of an account id,
				// so an unresolved assignee showed digits from an opaque string.
				ExtendedAtlassianWorkspacePrivate::AssigneeInitials(Assignee, Issue);
			const FString TypeGlyph = Issue.IssueTypeName == TEXT("Bug")
				? TEXT("B")
				: (Issue.IssueTypeName == TEXT("Doc") ? TEXT("D") : TEXT("T"));
			const TCHAR* TypeBrush = Issue.IssueTypeName == TEXT("Bug")
				? TEXT("Backlot.Brush.Red")
				: (Issue.IssueTypeName == TEXT("Doc")
					? TEXT("Backlot.Brush.Purple")
					: TEXT("Backlot.Brush.Blue"));
			const TCHAR* TypeColor = Issue.IssueTypeName == TEXT("Bug")
				? TEXT("#f0665f")
				: (Issue.IssueTypeName == TEXT("Doc")
					? TEXT("#b6a9ff")
					: TEXT("#58a6ff"));
			Cards->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 9.0f)
			[
				SNew(SBacklotBoardCard)
				.IssueKey(Issue.Key)
				.Status(Column.DisplayName)
				.CanDrag(Controller->CanExecuteMutation(
					EExtendedAtlassianWorkspaceMutation::MoveIssue))
				.DragTooltip(MutationTooltip(
					Controller,
					EExtendedAtlassianWorkspaceMutation::MoveIssue))
				.OnIssueDropped(
					this,
					&SExtendedAtlassianWorkspace::DropBoardIssue)
				.OnOpen_Lambda(
					[Controller = Controller, Key = Issue.Key]()
					{
						Controller->OpenIssue(Key);
					})
				[
					SNew(SBorder)
					.BorderImage(Brush(TEXT("Backlot.Brush.BoardCard")))
					.Padding(FMargin(12.0f, 14.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SBox)
									.WidthOverride(13.0f)
									.HeightOverride(13.0f)
									[
										SNew(SBorder)
											.BorderImage(Brush(TypeBrush))
											.Padding(0.0f)
											.HAlign(HAlign_Center)
											.VAlign(VAlign_Center)
											[
												SNew(STextBlock)
													.Text(FText::FromString(TypeGlyph))
													.TextStyle(&Text(TEXT("Backlot.Mono.8")))
													.ColorAndOpacity(
														FExtendedAtlassianStyle::FromHex(
															TypeColor))
											]
									]
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(8.0f, 0.0f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Text(FText::FromString(Issue.Key))
									.TextStyle(&Text(TEXT("Backlot.Mono.10")))
									.ColorAndOpacity(
										FExtendedAtlassianStyle::FromHex(
											TEXT("#6f7783")))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SButton)
									.IsEnabled(Controller->CanExecuteMutation(
										EExtendedAtlassianWorkspaceMutation::UpdateIssue))
									.ToolTipText(MutationTooltip(
										Controller,
										EExtendedAtlassianWorkspaceMutation::UpdateIssue))
									.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
									.ContentPadding(FMargin(2.0f))
									.OnClicked_Lambda(
										[this, Key = Issue.Key]()
										{
											OpenCardEdit(Key);
											return FReply::Handled();
										})
									[
										SNew(SImage)
											.Image(Brush(TEXT("Backlot.Icon.Edit")))
											.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
									]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(7.0f, 0.0f, 0.0f, 0.0f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Text(FText::FromString(TEXT("▌")))
									.TextStyle(&Text(TEXT("Backlot.Mono.10")))
									.ColorAndOpacity(
										FExtendedAtlassianStyle::FromHex(
											Issue.EpicColor.IsEmpty()
												? TEXT("#6f7783")
												: *Issue.EpicColor))
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 11.0f)
						[
							SNew(STextBlock).Text(FText::FromString(Issue.Summary)).TextStyle(&Text(TEXT("Backlot.Sans.13"))).AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SBox)
									.WidthOverride(22.0f)
									.HeightOverride(22.0f)
									[
										SNew(SBorder)
											.BorderImage(AvatarBrush(Assignee, AssigneeInitials))
											.Padding(0.0f)
											.HAlign(HAlign_Center)
											.VAlign(VAlign_Center)
											[
												SNew(STextBlock)
													.Text(FText::FromString(AssigneeInitials))
													.TextStyle(&Text(TEXT("Backlot.Mono.9")))
													.ColorAndOpacity(AvatarForeground(Assignee))
											]
									]
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(9.0f, 0.0f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Text(FText::Format(
										LOCTEXT("BoardCardPoints", "{0} PTS"),
										FText::AsNumber(Issue.Estimate)))
									.TextStyle(&Text(TEXT("Backlot.Mono.10")))
									.ColorAndOpacity(
										FExtendedAtlassianStyle::FromHex(
											TEXT("#6f7783")))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SHorizontalBox)
									.Visibility(
										Issue.CommentCount > 0
											? EVisibility::Visible
											: EVisibility::Collapsed)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(SImage)
										.Image(Brush(TEXT("Backlot.Icon.Comment")))
										.ColorAndOpacity(
											FExtendedAtlassianStyle::FromHex(
												TEXT("#6f7783")))
									]
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									.Padding(3.0f, 0.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock)
										.Text(FText::AsNumber(Issue.CommentCount))
										.TextStyle(&Text(TEXT("Backlot.Mono.10")))
										.ColorAndOpacity(
											FExtendedAtlassianStyle::FromHex(
												TEXT("#6f7783")))
									]
							]
						]
					]
				]
			];
		}
		Cards->AddSlot().AutoHeight()
		[
			SNew(SBox)
				.HeightOverride(36.0f)
				[
					SNew(SBacklotDashedBorder)
					[
						SNew(SButton)
							.IsEnabled(Controller->CanExecuteMutation(
								EExtendedAtlassianWorkspaceMutation::CreateIssue))
							.ToolTipText(MutationTooltip(
								Controller,
								EExtendedAtlassianWorkspaceMutation::CreateIssue))
							.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
							.ContentPadding(0.0f)
							.OnClicked_Lambda(
								[this, Status = Column.DisplayName]()
								{
									OpenCreateCard(Status);
									return FReply::Handled();
								})
						[
							SNew(STextBlock)
								.Text(LOCTEXT("AddCard", "+     Add"))
								.TextStyle(&Text(TEXT("Backlot.Sans.11")))
						]
					]
				]
		];
		const bool bWipColumn = Column.DisplayName == TEXT("In progress");
		const int32 WipLimit =
			bWipColumn ? FMath::Max(1, Column.WipLimit) : 0;
		const FString ColumnAccent =
			!Column.AccentColor.IsEmpty()
				? Column.AccentColor
				: (Column.DisplayName == TEXT("Done")
					? TEXT("#57cc8a")
					: (Column.DisplayName == TEXT("In review")
						? TEXT("#b6a9ff")
						: (Column.DisplayName == TEXT("In progress")
							? TEXT("#58a6ff")
							: TEXT("#a2a9b4"))));
		Columns->AddSlot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 14.0f, 0.0f)
		[
			SNew(SBacklotBoardDropTarget)
			.Status(Column.DisplayName)
			.OnIssueDropped(this, &SExtendedAtlassianWorkspace::DropBoardIssue)
			[
				SNew(SBox)
				.WidthOverride(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.BoardColumn.Width")))
				[
					SNew(SBorder)
					.BorderImage(Brush(TEXT("Backlot.Brush.BoardColumn")))
					.Padding(0.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBox)
								.HeightOverride(40.0f)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(13.0f, 0.0f, 9.0f, 0.0f)
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
											.Text(FText::FromString(TEXT("●")))
											.TextStyle(&Text(TEXT("Backlot.Mono.10")))
											.ColorAndOpacity(
												FExtendedAtlassianStyle::FromHex(
													*ColumnAccent))
									]
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
											.Text(FText::FromString(Column.DisplayName))
											.TextStyle(&Text(TEXT("Backlot.Sans.11.Medium")))
									]
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(9.0f, 0.0f)
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
											.Text(FText::AsNumber(Count))
											.TextStyle(&Text(TEXT("Backlot.Mono.10")))
											.ColorAndOpacity(
												FExtendedAtlassianStyle::FromHex(
													TEXT("#6f7783")))
									]
									+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									[
										SNullWidget::NullWidget
									]
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(0.0f, 0.0f, 13.0f, 0.0f)
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
											.Visibility(
												bWipColumn
													? EVisibility::Visible
													: EVisibility::Collapsed)
											.Text(FText::Format(
												LOCTEXT(
													"BoardColumnWip",
													"WIP {0} / {1}"),
												FText::AsNumber(Count),
												FText::AsNumber(WipLimit)))
											.TextStyle(&Text(TEXT("Backlot.Mono.9")))
											.ColorAndOpacity(
												FExtendedAtlassianStyle::FromHex(
													Count > WipLimit
														? TEXT("#f0665f")
														: TEXT("#6f7783")))
									]
								]
						]
						+ SVerticalBox::Slot().FillHeight(1.0f)
						.Padding(10.0f)
						[
							SNew(SScrollBox)
							.ScrollBarStyle(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(TEXT("Backlot.ScrollBar")))
							+ SScrollBox::Slot()[Cards]
						]
					]
				]
			]
		];
	}
	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Workspace")))
		.Padding(FMargin(16.0f))
		[
			SNew(SScrollBox)
			.Orientation(Orient_Horizontal)
			.ScrollBarStyle(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(TEXT("Backlot.ScrollBar")))
			+ SScrollBox::Slot()[Columns]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildPinsMain()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	TArray<TSharedRef<SWidget>> PinCards;
	int32 VisibleCount = 0;
	const FString Search = Controller->GetGlobalSearch().TrimStartAndEnd();
	for (const FExtendedAtlassianPin& Pin : Controller->GetSnapshot().Pins)
	{
		FString SearchText = Pin.DisplayName + TEXT(" ") + PinKindKey(Pin.Target.Kind);
		for (const FExtendedAtlassianPinThread& Thread : Pin.Threads)
		{
			SearchText += TEXT(" ") + Thread.Body + TEXT(" ") + Thread.AuthorDisplayName;
		}
		if ((!PinKindFilter.IsEmpty() && PinKindFilter != PinKindKey(Pin.Target.Kind))
			|| !ContainsSearch(SearchText, Search))
		{
			continue;
		}
		++VisibleCount;
		int32 OpenCount = 0;
		for (const FExtendedAtlassianPinThread& Thread : Pin.Threads)
		{
			OpenCount += Thread.bResolved ? 0 : 1;
		}
		TSharedRef<SVerticalBox> ThreadRows = SNew(SVerticalBox);
		for (int32 ThreadIndex = 0; ThreadIndex < Pin.Threads.Num(); ++ThreadIndex)
		{
			const FExtendedAtlassianPinThread& Thread = Pin.Threads[ThreadIndex];
			const bool bSelected =
				Pin.Id == Controller->GetSelectedPinId()
				&& (SelectedPinThreadId == Thread.Id
					|| (SelectedPinThreadId.IsEmpty() && ThreadIndex == 0));
			ThreadRows->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SBorder)
					.BorderImage(Brush(
						bSelected
							? TEXT("Backlot.Brush.CardSelected")
							: TEXT("Backlot.Brush.Panel")))
					.Padding(0.0f)
					[
						SNew(SButton)
							.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
							.ContentPadding(FMargin(9.0f, 8.0f))
							.OnClicked_Lambda(
								[this, PinId = Pin.Id, ThreadId = Thread.Id]()
								{
									SelectPinThread(PinId, ThreadId);
									return FReply::Handled();
								})
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SBox)
										.WidthOverride(20.0f)
										.HeightOverride(20.0f)
										[
											SNew(SBorder)
												.BorderImage(AvatarBrush(
													Thread.AuthorAccountId))
												.Padding(0.0f)
												.HAlign(HAlign_Center)
												.VAlign(VAlign_Center)
												[
													SNew(STextBlock)
														.Text(FText::FromString(
															Thread.AuthorAccountId.IsEmpty()
																? TEXT("AK")
																: Thread.AuthorAccountId))
														.TextStyle(&Text(TEXT("Backlot.Mono.9")))
												]
										]
								]
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(STextBlock)
											.Text(FText::FromString(Thread.Body))
											.TextStyle(&Text(TEXT("Backlot.Sans.12")))
											.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#b9c0ca")))
											.AutoWrapText(true)
											.LineHeightPercentage(1.1f)
									]
									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(0.0f, 4.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock)
											.Text(FText::FromString(
												Thread.RelativeTime
													+ TEXT("     ")
													+ (!Thread.LinkedLabel.IsEmpty()
														? Thread.LinkedLabel
														: (Thread.bResolved ? TEXT("RESOLVED") : TEXT("OPEN")))))
											.TextStyle(&Text(TEXT("Backlot.Mono.9")))
											.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(
												!Thread.LinkedLabel.IsEmpty()
													? TEXT("#58a6ff")
													: (Thread.bResolved ? TEXT("#57cc8a") : TEXT("#e3a54a"))))
									]
								]
							]
					]
			];
		}
		PinCards.Add(
			SNew(SBacklotHoverBrightness)
			[
			SNew(SBox)
			.WidthOverride_Lambda(
				[this]()
				{
					if (Controller->IsCompact())
					{
						return 462.0f;
					}
					const float WorkspaceWidth =
						GetCachedGeometry().GetLocalSize().X;
					const float SidebarWidth = FMath::Clamp(
						WorkspaceWidth * 0.17f,
						208.0f,
						262.0f) + 1.0f;
					const float RailWidth =
						Controller->IsRailOpen() ? 337.0f : 0.0f;
					const float MainContentWidth =
						WorkspaceWidth
							- 59.0f
							- SidebarWidth
							- RailWidth
							- 40.0f;
					return FMath::Max(
						286.0f,
						(MainContentWidth - 16.0f) * 0.5f);
				})
			.HeightOverride_Lambda(
				[this]() -> FOptionalSize
				{
					return Controller->IsCompact()
						? FOptionalSize()
						: FOptionalSize(164.0f);
				})
			[
				SNew(SBorder)
					.BorderImage(Brush(TEXT("Backlot.Brush.PinCard")))
					.Padding(0.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBox)
								.HeightOverride(38.0f)
								[
									SNew(SBorder)
										.BorderImage(Brush(TEXT("Backlot.Brush.Sidebar")))
										.Padding(FMargin(13.0f, 0.0f))
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot()
											.AutoWidth()
											.VAlign(VAlign_Center)
											.Padding(0.0f, 0.0f, 9.0f, 0.0f)
											[
												SNew(STextBlock)
													.Text(FText::FromString(PinKindKey(Pin.Target.Kind)))
													.TextStyle(&Text(TEXT("Backlot.Mono.10")))
													.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(PinKindColor(Pin.Target.Kind)))
											]
											+ SHorizontalBox::Slot()
											.FillWidth(1.0f)
											.VAlign(VAlign_Center)
											[
												SNew(SButton)
													.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
													.ContentPadding(0.0f)
													.OnClicked_Lambda(
														[this, PinId = Pin.Id]()
														{
															Controller->SelectPin(PinId);
															SelectedPinThreadId.Reset();
															return FReply::Handled();
														})
													[
														SNew(STextBlock)
															.Text(FText::FromString(Pin.DisplayName))
															.TextStyle(&Text(TEXT("Backlot.Mono.12")))
															.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
													]
											]
											+ SHorizontalBox::Slot()
											.AutoWidth()
											.VAlign(VAlign_Center)
											.Padding(8.0f, 0.0f)
											[
												SNew(STextBlock)
													.Text(FText::Format(
														LOCTEXT("PinOpenCount", "{0} OPEN"),
														FText::AsNumber(OpenCount)))
													.TextStyle(&Text(TEXT("Backlot.Mono.10")))
													.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
											]
											+ SHorizontalBox::Slot()
											.AutoWidth()
											.VAlign(VAlign_Center)
											[
												SNew(SButton)
													.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
													.ContentPadding(FMargin(3.0f))
													.OnClicked_Lambda(
														[this, PinId = Pin.Id]()
														{
															OpenPinActions(PinId);
															return FReply::Handled();
														})
													[
														SNew(SImage)
															.Image(Brush(TEXT("Backlot.Icon.More")))
															.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
													]
											]
										]
								]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(12.0f, 10.0f, 12.0f, 2.0f)
						[
							Pin.Threads.IsEmpty()
								? StaticCastSharedRef<SWidget>(
									SNew(STextBlock)
										.Text(LOCTEXT("NoPinThreads", "No replies yet"))
										.TextStyle(&Text(TEXT("Backlot.Sans.11")))
										.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d"))))
								: StaticCastSharedRef<SWidget>(ThreadRows)
						]
					]
			]
			]
		);
	}
	TSharedRef<SWrapBox> Cards =
		SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(FVector2D(16.0f, 16.0f));
	for (const TSharedRef<SWidget>& PinCard : PinCards)
	{
		Cards->AddSlot()[PinCard];
	}
	const bool bVirtualizePins = PinCards.Num() > 40;
	TSharedRef<SWidget> Content = VisibleCount == 0
		? StaticCastSharedRef<SWidget>(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 64.0f, 0.0f, 11.0f)
			[
				SNew(STextBlock)
					.Text(LOCTEXT("NoPinsMatch", "NO PINNED THREADS MATCH"))
					.TextStyle(&Text(TEXT("Backlot.Mono.10")))
					.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(SButton)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
					.ContentPadding(4.0f)
					.OnClicked_Lambda(
						[this]()
						{
							PinKindFilter.Reset();
							Controller->SetGlobalSearch(FString());
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
							.Text(LOCTEXT("ClearPinFilters", "Clear filters"))
							.TextStyle(&Text(TEXT("Backlot.Sans.11")))
							.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")))
					]
			])
		: (bVirtualizePins
			? StaticCastSharedRef<SWidget>(
				SNew(SBacklotVirtualizedWidgetTileList)
				.Widgets(PinCards)
				.ItemWidth(310.0f)
				.ItemHeight(360.0f)
				.ScrollBarStyle(
					&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(
						TEXT("Backlot.ScrollBar"))))
			: StaticCastSharedRef<SWidget>(Cards));
	const TSharedRef<SWidget> ScrollContent = bVirtualizePins
		? Content
		: StaticCastSharedRef<SWidget>(
			SNew(SScrollBox)
			.ScrollBarStyle(
				&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(
					TEXT("Backlot.ScrollBar")))
			.ScrollBarVisibility(
				Controller->IsCompact()
					? EVisibility::Collapsed
					: EVisibility::Visible)
			+ SScrollBox::Slot()[Content]);
	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Workspace")))
		.Padding(FMargin(20.0f, 18.0f, 20.0f, 40.0f))
		[
			ScrollContent
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildInboxMain()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
	const FString TabNames[] = {
		TEXT("All"),
		TEXT("Mentions"),
		TEXT("Reviews"),
		TEXT("Pins")
	};
	for (const FString& Tab : TabNames)
	{
		int32 Count = 0;
		for (const FExtendedAtlassianNotification& Notification :
			Controller->GetSnapshot().Notifications)
		{
			Count += !Notification.bArchived
				&& InboxTabMatches(Tab, Notification.Kind) ? 1 : 0;
		}
		const bool bSelected = Controller->GetInboxTab() == Tab;
		Tabs->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 7.0f, 0.0f)
		[
			SNew(SBorder)
				.BorderImage(Brush(
					bSelected
						? TEXT("Backlot.Brush.CardSelected")
						: TEXT("Backlot.Brush.Card")))
				.Padding(0.0f)
				[
					SNew(SButton)
						.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
						.ContentPadding(FMargin(11.0f, 6.0f))
						.OnClicked_Lambda(
							[this, Tab]()
							{
								SetInboxTabAndSelect(Tab);
								return FReply::Handled();
							})
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 7.0f, 0.0f)
							[
								SNew(STextBlock)
									.Text(FText::FromString(Tab))
									.TextStyle(&Text(TEXT("Backlot.Sans.11.Medium")))
									.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(
										bSelected ? TEXT("#e6e8ec") : TEXT("#8a919c")))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(STextBlock)
									.Text(FText::AsNumber(Count))
									.TextStyle(&Text(TEXT("Backlot.Mono.10")))
									.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
							]
						]
				]
		];
	}

	TArray<TSharedRef<SWidget>> Rows;
	int32 VisibleCount = 0;
	const FString Search = Controller->GetGlobalSearch().TrimStartAndEnd();
	for (const FExtendedAtlassianNotification& Notification : Controller->GetSnapshot().Notifications)
	{
		const FString SearchText =
			Notification.ActorDisplayName
			+ TEXT(" ") + Notification.Action
			+ TEXT(" ") + Notification.Target
			+ TEXT(" ") + Notification.Quote
			+ TEXT(" ") + NotificationKindLabel(Notification.Kind);
		if (Notification.bArchived
			|| !InboxTabMatches(Controller->GetInboxTab(), Notification.Kind)
			|| !ContainsSearch(SearchText, Search))
		{
			continue;
		}
		++VisibleCount;
		const bool bSelected = Notification.Id == Controller->GetSelectedNotificationId();
		Rows.Add(
			SNew(SBox)
			.MinDesiredHeight(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.InboxRow.MinHeight")))
			[
				SNew(SBorder)
				.BorderImage(bSelected ? Brush(TEXT("Backlot.Brush.CardSelected")) : FStyleDefaults::GetNoBrush())
				.Padding(FMargin(10.0f, 0.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SButton)
							.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
							.ContentPadding(FMargin(6.0f, 11.0f))
							.OnClicked_Lambda(
								[this, Id = Notification.Id]()
								{
									SelectInboxNotification(Id);
									return FReply::Handled();
								})
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 12.0f, 0.0f)
								[
									SNew(STextBlock)
										.Text(Notification.bRead
											? LOCTEXT("InboxReadDot", "○")
											: LOCTEXT("InboxUnreadDot", "●"))
										.TextStyle(&Text(TEXT("Backlot.Mono.10")))
										.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(
											Notification.bRead ? TEXT("#24282e") : TEXT("#58a6ff")))
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 12.0f, 0.0f)
								[
									SNew(SBox)
										.WidthOverride(26.0f)
										.HeightOverride(26.0f)
										[
											SNew(SBorder)
												.BorderImage(AvatarBrush(
													Notification.ActorAccountId))
												.Padding(0.0f)
												.HAlign(HAlign_Center)
												.VAlign(VAlign_Center)
												[
													SNew(STextBlock)
														.Text(FText::FromString(Notification.ActorAccountId))
														.TextStyle(&Text(TEXT("Backlot.Mono.9")))
												]
										]
								]
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(STextBlock)
											.Text(FText::FromString(
												Notification.ActorDisplayName
													+ TEXT(" ") + Notification.Action
													+ TEXT(" ") + Notification.Target))
											.TextStyle(&Text(TEXT("Backlot.Sans.12")))
											.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
									]
									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(0.0f, 2.0f, 0.0f, 0.0f)
									[
										SNew(STextBlock)
											.Text(FText::FromString(Notification.Quote))
											.TextStyle(&Text(TEXT("Backlot.Sans.11")))
											.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
											.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
									]
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(12.0f, 0.0f)
								[
									SNew(SBorder)
										.BorderImage(Brush(TEXT("Backlot.Brush.Card")))
										.Padding(FMargin(8.0f, 3.0f))
										[
											SNew(STextBlock)
												.Text(FText::FromString(NotificationKindLabel(Notification.Kind)))
												.TextStyle(&Text(TEXT("Backlot.Mono.9.Medium")))
												.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(
													NotificationKindColor(Notification.Kind)))
										]
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SBox)
										.WidthOverride(34.0f)
										[
											SNew(STextBlock)
												.Text(FText::FromString(Notification.RelativeTime))
												.Justification(ETextJustify::Right)
												.TextStyle(&Text(TEXT("Backlot.Mono.10")))
												.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
										]
								]
							]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
							.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
							.ContentPadding(FMargin(5.0f))
							.OnClicked_Lambda(
								[this, Id = Notification.Id]()
								{
									DismissInboxNotification(Id);
									return FReply::Handled();
								})
							[
								SNew(STextBlock)
									.Text(LOCTEXT("DismissInboxRow", "✕"))
									.TextStyle(&Text(TEXT("Backlot.Mono.10")))
									.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#4d545e")))
							]
					]
				]
			]
		);
	}
	if (VisibleCount == 0)
	{
		Rows.Add(
			SNew(SBox)
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 64.0f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("InboxEmpty", "NOTHING HERE — YOU ARE ALL CAUGHT UP"))
				.TextStyle(&Text(TEXT("Backlot.Mono.10")))
				.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
			]);
	}
	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Workspace")))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
					.HeightOverride(40.0f)
					[
						SNew(SBorder)
							.BorderImage(Brush(TEXT("Backlot.Brush.Panel")))
							.Padding(FMargin(16.0f, 0.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									Tabs
								]
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								[
									SNullWidget::NullWidget
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 14.0f, 0.0f)
								[
									SNew(SButton)
										.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
										.ContentPadding(2.0f)
										.OnClicked_Lambda(
											[this]()
											{
												MarkAllInboxRead();
												return FReply::Handled();
											})
										[
											SNew(STextBlock)
												.Text(LOCTEXT("MarkAllInboxRead", "MARK ALL READ"))
												.TextStyle(&Text(TEXT("Backlot.Mono.10")))
												.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
										]
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SButton)
										.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
										.ContentPadding(2.0f)
										.OnClicked_Lambda(
											[this]()
											{
												ArchiveReadInbox();
												return FReply::Handled();
											})
										[
											SNew(STextBlock)
												.Text(LOCTEXT("ArchiveInboxRead", "ARCHIVE READ"))
												.TextStyle(&Text(TEXT("Backlot.Mono.10")))
												.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
										]
								]
							]
					]
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SBacklotVirtualizedWidgetList)
				.Widgets(Rows)
				.ScrollBarStyle(
					&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(
						TEXT("Backlot.ScrollBar")))
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildRightRail()
{
	switch (Controller->GetRoute())
	{
	case EExtendedAtlassianWorkspaceRoute::Docs: return BuildDocsRailDynamic();
	case EExtendedAtlassianWorkspaceRoute::Issues:
	case EExtendedAtlassianWorkspaceRoute::IssueDetail: return BuildIssueRail();
	case EExtendedAtlassianWorkspaceRoute::Board: return BuildBoardRail();
	case EExtendedAtlassianWorkspaceRoute::Pins: return BuildPinsRail();
	case EExtendedAtlassianWorkspaceRoute::Inbox: return BuildInboxRail();
	default: return EmptyState(LOCTEXT("EmptyRail", "NO CONTEXT"));
	}
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildDocsRail()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	return SNew(SBorder)
	.BorderImage(Brush(TEXT("Backlot.Brush.Sidebar")))
	.Padding(FMargin(14.0f, 16.0f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[SectionLabel(LOCTEXT("CommentsRail", "COMMENTS                 2 OPEN · 1 RESOLVED"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
		[
			SNew(SBorder).BorderImage(Brush(TEXT("Backlot.Brush.Card"))).Padding(11.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("LegacyDocCommentOne", "Comments are provided by the connected workspace.")).TextStyle(&Text(TEXT("Backlot.Sans.11"))).AutoWrapText(true)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f)
		[
			SNew(SBorder).BorderImage(Brush(TEXT("Backlot.Brush.Card"))).Padding(11.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("LegacyDocCommentTwo", "Select a page to load its comment thread.")).TextStyle(&Text(TEXT("Backlot.Sans.11"))).AutoWrapText(true)
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		+ SVerticalBox::Slot().AutoHeight()[SectionLabel(LOCTEXT("LinkedWork", "LINKED WORK"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("LegacyLinkedRows", "Linked work is derived from page content.")).TextStyle(&Text(TEXT("Backlot.Sans.11")))
		]
	];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildCommentCard(
	const FString& Scope,
	const FExtendedAtlassianComment& Comment,
	bool bIssueComment)
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FExtendedAtlassianUser* User =
		Controller->GetSnapshot().People.FindByPredicate(
			[&Comment](const FExtendedAtlassianUser& Candidate)
			{
				return Candidate.AccountId == Comment.AuthorAccountId
					|| Candidate.Initials == Comment.AuthorAccountId;
			});
	const FString Initials = User
		? User->Initials
		: (!Comment.AuthorAccountId.IsEmpty()
			? Comment.AuthorAccountId.Left(2).ToUpper()
			: TEXT("AK"));
	const FString DisplayName = !Comment.AuthorDisplayName.IsEmpty()
		? Comment.AuthorDisplayName
		: (User ? User->DisplayName : Initials);
	const bool bEditing = EditingCommentId == Comment.Id;
	const bool bReplying = ReplyingCommentId == Comment.Id;
	const bool bRepliesOpen = ExpandedCommentReplies.Contains(Comment.Id);
	const FLinearColor Accent = FExtendedAtlassianStyle::FromHex(
		Comment.bResolved
			? TEXT("#57cc8a")
			: (Comment.AccentColor.IsEmpty() ? TEXT("#58a6ff") : *Comment.AccentColor));

	auto ActionButton = [](const FText& Label, TFunction<FReply()> Action)
	{
		return SNew(SButton)
			.ButtonStyle(&ExtendedAtlassianWorkspacePrivate::Button(TEXT("Backlot.Button.Clear")))
			.ContentPadding(FMargin(2.0f, 1.0f))
			.OnClicked_Lambda([Action = MoveTemp(Action)]() { return Action(); })
			[
				SNew(STextBlock)
					.Text(Label)
					.TextStyle(&ExtendedAtlassianWorkspacePrivate::Text(TEXT("Backlot.Mono.10")))
					.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
			];
	};

	TSharedRef<SVerticalBox> Card = SNew(SVerticalBox);
	if (!Comment.Quote.IsEmpty())
	{
		Card->AddSlot()
		.AutoHeight()
		.Padding(11.0f, 9.0f, 11.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox)
					.WidthOverride(2.0f)
					[
						SNew(SImage)
							.Image(Brush(TEXT("Backlot.Brush.BlueSolid")))
							.ColorAndOpacity(Accent)
					]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(Comment.Quote))
					.TextStyle(&Text(TEXT("Backlot.Sans.11")))
					.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
					.AutoWrapText(true)
			]
		];
	}

	TSharedRef<SVerticalBox> Message = SNew(SVerticalBox);
	Message->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
				.Text(FText::FromString(DisplayName))
				.TextStyle(&Text(TEXT("Backlot.Sans.11.Medium")))
				.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#d7dce3")))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(7.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text(FText::FromString(Comment.RelativeTime))
				.TextStyle(&Text(TEXT("Backlot.Mono.9")))
				.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
		]
	];
	if (bEditing)
	{
		Message->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 5.0f, 0.0f, 0.0f)
		[
			SNew(SMultiLineEditableTextBox)
				.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
					TEXT("Backlot.Field")))
				.Text(FText::FromString(CommentEditDraft))
				.OnTextChanged_Lambda(
					[this](const FText& Value)
					{
						CommentEditDraft = Value.ToString();
					})
		];
		Message->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 7.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Primary")))
					.ContentPadding(FMargin(10.0f, 5.0f))
					.OnClicked_Lambda(
						[this, Id = Comment.Id, bIssueComment]()
						{
							SaveCommentEdit(Id, bIssueComment);
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
							.Text(LOCTEXT("SaveCommentEdit", "Save"))
							.TextStyle(&Text(TEXT("Backlot.Sans.10.Medium")))
					]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(7.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
					.ContentPadding(FMargin(10.0f, 5.0f))
					.OnClicked_Lambda(
						[this]()
						{
							EditingCommentId.Reset();
							CommentEditDraft.Reset();
							Rebuild();
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
							.Text(LOCTEXT("CancelCommentEdit", "Cancel"))
							.TextStyle(&Text(TEXT("Backlot.Sans.10")))
					]
			]
		];
	}
	else
	{
		Message->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 3.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text(FText::FromString(Comment.Body))
				.TextStyle(&Text(TEXT("Backlot.Sans.12")))
				.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#a2a9b4")))
				.AutoWrapText(true)
		];
	}

	Card->AddSlot()
	.AutoHeight()
	.Padding(11.0f, 9.0f, 11.0f, 0.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 9.0f, 0.0f)
		[
			SNew(SBox)
				.WidthOverride(21.0f)
				.HeightOverride(21.0f)
				[
					SNew(SBorder)
						.BorderImage(Brush(TEXT("Backlot.Brush.CardSelected")))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(FText::FromString(Initials))
								.TextStyle(&Text(TEXT("Backlot.Mono.9.Medium")))
								.ColorAndOpacity(Accent)
						]
				]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			Message
		]
	];

	Card->AddSlot()
	.AutoHeight()
	.Padding(11.0f, 9.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			ActionButton(
				LOCTEXT("ReplyComment", "REPLY"),
				[this, Id = Comment.Id]()
				{
					ToggleCommentReply(Id);
					return FReply::Handled();
				})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(10.0f, 0.0f, 0.0f, 0.0f)
		[
			ActionButton(
				Comment.bResolved
					? LOCTEXT("CommentResolved", "RESOLVED")
					: LOCTEXT("ResolveComment", "RESOLVE"),
				[this, Comment, bIssueComment]()
				{
					ToggleCommentResolved(Comment, bIssueComment);
					return FReply::Handled();
				})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(10.0f, 0.0f, 0.0f, 0.0f)
		[
			ActionButton(
				LOCTEXT("EditComment", "EDIT"),
				[this, Comment]()
				{
					BeginCommentEdit(Comment);
					return FReply::Handled();
				})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(10.0f, 0.0f, 0.0f, 0.0f)
		[
			ActionButton(
				LOCTEXT("DeleteComment", "DELETE"),
				[this, Id = Comment.Id, bIssueComment]()
				{
					ConfirmDeleteComment(Id, bIssueComment);
					return FReply::Handled();
				})
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			ActionButton(
				Comment.Replies.IsEmpty()
					? FText::GetEmpty()
					: FText::Format(
						Comment.Replies.Num() == 1
							? LOCTEXT("OneReply", "{0} REPLY")
							: LOCTEXT("ManyReplies", "{0} REPLIES"),
						FText::AsNumber(Comment.Replies.Num())),
				[this, Id = Comment.Id]()
				{
					ToggleCommentReplies(Id);
					return FReply::Handled();
				})
		]
	];

	if (bRepliesOpen && !Comment.Replies.IsEmpty())
	{
		TSharedRef<SVerticalBox> Replies = SNew(SVerticalBox);
		for (const FExtendedAtlassianComment& Reply : Comment.Replies)
		{
			const FExtendedAtlassianUser* ReplyUser =
				Controller->GetSnapshot().People.FindByPredicate(
					[&Reply](const FExtendedAtlassianUser& Candidate)
					{
						return Candidate.AccountId == Reply.AuthorAccountId
							|| Candidate.Initials == Reply.AuthorAccountId;
					});
			const FString ReplyInitials = ReplyUser
				? ReplyUser->Initials
				: (!Reply.AuthorAccountId.IsEmpty()
					? Reply.AuthorAccountId.Left(2).ToUpper()
					: TEXT("AK"));
			const FString ReplyName = !Reply.AuthorDisplayName.IsEmpty()
				? Reply.AuthorDisplayName
				: (ReplyUser ? ReplyUser->DisplayName : ReplyInitials);
			Replies->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(18.0f)
					.HeightOverride(18.0f)
					[
						SNew(SBorder)
						.BorderImage(AvatarBrush(ReplyUser, ReplyInitials))
						.Padding(0.0f)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(FText::FromString(ReplyInitials))
								.TextStyle(&Text(TEXT("Backlot.Mono.8")))
								.ColorAndOpacity(AvatarForeground(ReplyUser))
						]
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
								.Text(FText::FromString(ReplyName))
								.TextStyle(&Text(TEXT("Backlot.Sans.10.Medium")))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(7.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
								.Text(FText::FromString(Reply.RelativeTime))
								.TextStyle(&Text(TEXT("Backlot.Mono.9")))
								.ColorAndOpacity(
									FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
							.Text(FText::FromString(Reply.Body))
							.TextStyle(&Text(TEXT("Backlot.Sans.11")))
							.ColorAndOpacity(
								FExtendedAtlassianStyle::FromHex(TEXT("#9aa2ad")))
							.AutoWrapText(true)
					]
				]
			];
		}
		Card->AddSlot()
		.AutoHeight()
		.Padding(30.0f, 4.0f, 11.0f, 10.0f)
		[
			Replies
		];
	}

	if (bReplying)
	{
		Card->AddSlot()
		.AutoHeight()
		.Padding(11.0f, 9.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
					.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
						TEXT("Backlot.Field")))
					.HintText(LOCTEXT("CommentReplyHint", "Write a reply…"))
					.Text(FText::FromString(ReplyDraft))
					.OnTextChanged_Lambda(
						[this](const FText& Value)
						{
							ReplyDraft = Value.ToString();
						})
					.OnTextCommitted_Lambda(
						[this, Scope, Id = Comment.Id, bIssueComment](
							const FText&,
							ETextCommit::Type CommitType)
						{
							if (CommitType == ETextCommit::OnEnter)
							{
								SendCommentReply(Scope, Id, bIssueComment);
							}
						})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(7.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Primary")))
					.ContentPadding(FMargin(11.0f, 5.0f))
					.OnClicked_Lambda(
						[this, Scope, Id = Comment.Id, bIssueComment]()
						{
							SendCommentReply(Scope, Id, bIssueComment);
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
							.Text(LOCTEXT("SendCommentReply", "Send"))
							.TextStyle(&Text(TEXT("Backlot.Sans.11.Medium")))
					]
			]
		];
	}

	return SNew(SBorder)
		.BorderImage(Brush(
			Comment.bResolved
				? TEXT("Backlot.Brush.CommentResolved")
				: (Comment.AccentColor == TEXT("#e3a54a")
					? TEXT("Backlot.Brush.CommentAmber")
					: TEXT("Backlot.Brush.CommentOpen"))))
		.Padding(0.0f)
		[
			Card
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildDocsRailDynamic()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FString Scope = TEXT("page:") + Controller->GetSelectedPageId();
	const FExtendedAtlassianCommentCollection* Collection =
		Controller->GetSnapshot().CommentCollections.FindByPredicate(
			[&Scope](const FExtendedAtlassianCommentCollection& Candidate)
			{
				return Candidate.TargetId == Scope;
			});
	const TArray<FExtendedAtlassianComment>* Comments =
		Collection ? &Collection->Comments : nullptr;
	int32 OpenCount = 0;
	if (Comments)
	{
		for (const FExtendedAtlassianComment& Comment : *Comments)
		{
			OpenCount += Comment.bResolved ? 0 : 1;
		}
	}
	const int32 TotalCount = Comments ? Comments->Num() : 0;

	TArray<TSharedRef<SWidget>> CommentRows;
	if (Comments && !Comments->IsEmpty())
	{
		for (const FExtendedAtlassianComment& Comment : *Comments)
		{
			CommentRows.Add(
				SNew(SBox)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
				[
					BuildCommentCard(Scope, Comment, false)
				]);
		}
	}
	else
	{
		CommentRows.Add(
			SNew(SBox)
			.Padding(FMargin(4.0f, 20.0f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoDocComments", "No comments on this page yet."))
				.TextStyle(&Text(TEXT("Backlot.Sans.11")))
				.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
				.AutoWrapText(true)
			]);
	}

	TSharedRef<SVerticalBox> LinkedRows = SNew(SVerticalBox);
	TArray<FString> LinkedKeys;
	if (const FExtendedAtlassianPage* Page = SelectedPage())
	{
		LinkedKeys = Page->LinkedIssueKeys;
		const FString Searchable = Page->Markdown.IsEmpty()
			? Page->Body
			: Page->Markdown;
		FRegexMatcher Matcher(
			FRegexPattern(TEXT("\\b[A-Z][A-Z0-9]+-[0-9]+\\b")),
			Searchable);
		while (Matcher.FindNext())
		{
			LinkedKeys.AddUnique(Matcher.GetCaptureGroup(0));
		}
	}
	for (const FString& Key : LinkedKeys)
	{
		const FExtendedAtlassianIssue* Issue =
			Controller->GetSnapshot().Issues.FindByPredicate(
				[&Key](const FExtendedAtlassianIssue& Candidate)
				{
					return Candidate.Key == Key;
				});
		if (!Issue)
		{
			continue;
		}
		LinkedRows->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 7.0f)
		[
			SNew(SBox)
			.HeightOverride(30.0f)
			[
				SNew(SButton)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
					.ContentPadding(FMargin(9.0f, 0.0f))
					.OnClicked_Lambda(
						[Controller = Controller, Key]()
						{
							Controller->OpenIssue(Key);
							return FReply::Handled();
						})
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 9.0f, 0.0f)
						[
							SNew(STextBlock)
								.Text(FText::FromString(Key))
								.TextStyle(&Text(TEXT("Backlot.Mono.11")))
								.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(FText::FromString(Issue->Summary))
								.TextStyle(&Text(TEXT("Backlot.Sans.11")))
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("LinkedWorkStatusDot", "●"))
								.TextStyle(&Text(TEXT("Backlot.Mono.8")))
								.ColorAndOpacity(StatusColor(Issue->StatusName, Issue->StatusCategoryKey))
						]
					]
			]
		];
	}

	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Sidebar")))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(38.0f)
				.Padding(FMargin(14.0f, 0.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 9.0f, 0.0f)
					[
						SectionLabel(LOCTEXT("CommentsRailDynamic", "COMMENTS"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(FText::Format(
								LOCTEXT("DocCommentMeta", "{0} OPEN · {1} RESOLVED"),
								FText::AsNumber(OpenCount),
								FText::AsNumber(TotalCount - OpenCount)))
							.TextStyle(&Text(TEXT("Backlot.Mono.10")))
							.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#4d545e")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBox)
					.Padding(12.0f)
					[
						SNew(SBacklotVirtualizedWidgetList)
						.Widgets(CommentRows)
						.ScrollBarStyle(
							&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(
								TEXT("Backlot.ScrollBar")))
					]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
					.BorderImage(Brush(TEXT("Backlot.Brush.Sidebar")))
					.Padding(FMargin(14.0f, 12.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 11.0f)
						[
							SectionLabel(LOCTEXT("LinkedWorkDynamic", "LINKED WORK"))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							LinkedRows
						]
					]
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildIssueRail()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FExtendedAtlassianIssue* Issue = SelectedIssue();
	if (!Issue) { return EmptyState(LOCTEXT("NoIssueRail", "NO ISSUE")); }
	const bool bPreview =
		Controller->GetRoute() == EExtendedAtlassianWorkspaceRoute::Issues;
	TSharedRef<SVerticalBox> Fields = SNew(SVerticalBox);
	auto AddField = [
		this,
		&Fields,
		bPreview
	](
		const FText& Label,
		const FText& Value,
		const FString& FieldName = FString())
	{
		const bool bEditable = !bPreview && !FieldName.IsEmpty();
		TSharedRef<SWidget> ValueWidget = bEditable
			? StaticCastSharedRef<SWidget>(
				SNew(SButton)
					.ButtonStyle(&ExtendedAtlassianWorkspacePrivate::Button(
						TEXT("Backlot.Button.Clear")))
					.ContentPadding(0.0f)
					.HAlign(HAlign_Left)
					.OnClicked_Lambda(
						[this, FieldName]()
						{
							OpenIssueFieldMenu(FieldName);
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
							.Text(Value)
							.TextStyle(&ExtendedAtlassianWorkspacePrivate::Text(
								TEXT("Backlot.Sans.12")))
							.ColorAndOpacity(
								FExtendedAtlassianStyle::FromHex(TEXT("#d7dce3")))
							.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					])
			: StaticCastSharedRef<SWidget>(
				SNew(STextBlock)
					.Text(Value)
					.TextStyle(&ExtendedAtlassianWorkspacePrivate::Text(
						TEXT("Backlot.Sans.12")))
					.ColorAndOpacity(
						FExtendedAtlassianStyle::FromHex(TEXT("#b9c0ca")))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis));
		Fields->AddSlot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(bPreview ? 38.63f : 41.0f)
			[
				SNew(SBorder)
					.BorderImage(ExtendedAtlassianWorkspacePrivate::Brush(
						TEXT("Backlot.Brush.Transparent")))
					.Padding(FMargin(0.0f, 4.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
							.WidthOverride(bPreview ? 96.0f : 92.0f)
						[
							SNew(STextBlock)
								.Text(Label)
								.TextStyle(&ExtendedAtlassianWorkspacePrivate::Text(
									TEXT("Backlot.Mono.10")))
								.ColorAndOpacity(
									FExtendedAtlassianStyle::FromHex(
										TEXT("#6f7783")))
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						ValueWidget
					]
				]
			]
		];
		Fields->AddSlot()
		.AutoHeight()
		[
			SNew(SSeparator)
				.SeparatorImage(ExtendedAtlassianWorkspacePrivate::Brush(
					TEXT("Backlot.Brush.BorderSubtle")))
				.Thickness(1.0f)
		];
	};
	AddField(
		LOCTEXT("PreviewStatus", "STATUS"),
		FText::FromString(Issue->StatusName),
		TEXT("status"));
	AddField(
		LOCTEXT("PreviewAssignee", "ASSIGNEE"),
		FText::FromString(
			Issue->AssigneeDisplayName.IsEmpty()
				? FString(TEXT("Unassigned"))
				: Issue->AssigneeDisplayName),
		TEXT("assignee"));
	AddField(
		LOCTEXT("PreviewEpic", "EPIC"),
		FText::FromString(
			Issue->EpicName.IsEmpty()
				? FString(TEXT("None"))
				: Issue->EpicName),
		TEXT("epic"));
	AddField(
		LOCTEXT("PreviewPriority", "PRIORITY"),
		FText::FromString(Issue->PriorityName),
		TEXT("priority"));
	AddField(
		LOCTEXT("PreviewPoints", "POINTS"),
		FText::AsNumber(Issue->Estimate),
		TEXT("points"));
	AddField(
		LOCTEXT("PreviewSprint", "SPRINT"),
		FText::FromString(
			Issue->SprintName.IsEmpty()
				? SelectedSprintName(Controller->GetSnapshot())
				: Issue->SprintName));
	if (!bPreview)
	{
		AddField(
			LOCTEXT("PreviewReporter", "REPORTER"),
			FText::FromString(
				Issue->ReporterDisplayName.IsEmpty()
					? Controller->GetSnapshot().CurrentUser.DisplayName
					: Issue->ReporterDisplayName));
		AddField(
			LOCTEXT("DetailLabels", "LABELS"),
			FText::FromString(
				Issue->Labels.IsEmpty()
					? FString(TEXT("None"))
					: FString::Join(Issue->Labels, TEXT(", "))));
	}

	if (bPreview)
	{
		return SNew(SBorder)
			.BorderImage(Brush(TEXT("Backlot.Brush.Sidebar")))
			.Padding(0.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(39.0f)
					[
						SNew(SBorder)
						.BorderImage(Brush(TEXT("Backlot.Brush.BorderSubtle")))
						.Padding(FMargin(0.0f, 0.0f, 0.0f, 1.0f))
						[
							SNew(SBorder)
							.BorderImage(Brush(TEXT("Backlot.Brush.Sidebar")))
							.Padding(FMargin(14.0f, 0.0f))
							[
								SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SectionLabel(
											LOCTEXT("Preview", "PREVIEW"))
									]
									+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(FText::FromString(Issue->Key))
										.TextStyle(&Text(
											TEXT("Backlot.Mono.10")))
										.ColorAndOpacity(
											FExtendedAtlassianStyle::FromHex(
												TEXT("#8a919c")))
									]
							]
						]
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SScrollBox)
					.ScrollBarStyle(
						&FExtendedAtlassianStyle::Get()
							.GetWidgetStyle<FScrollBarStyle>(
								TEXT("Backlot.ScrollBar")))
					+ SScrollBox::Slot()
					.Padding(14.0f)
					[
						SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 0.0f, 0.0f, 14.0f)
							[
								SNew(STextBlock)
								.Text(FText::FromString(Issue->Summary))
								.TextStyle(&Text(
									TEXT("Backlot.Sans.15.Medium")))
								.LineHeightPercentage(1.0875f)
								.AutoWrapText(true)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								Fields
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 16.0f, 0.0f, 0.0f)
							[
								SNew(SBox)
								.HeightOverride(32.0f)
								[
									SNew(SButton)
									.ButtonStyle(&Button(
										TEXT("Backlot.Button.Primary")))
									.ContentPadding(0.0f)
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									.OnClicked(
										this,
										&SExtendedAtlassianWorkspace::
											OnOpenSelectedIssue)
									[
										SNew(STextBlock)
										.Text(LOCTEXT(
											"OpenFullIssue",
											"Open full issue"))
										.TextStyle(&Text(
											TEXT("Backlot.Sans.12.Medium")))
									]
								]
							]
					]
				]
			];
	}

	TSharedRef<SVerticalBox> MentionedPages = SNew(SVerticalBox);
	if (!bPreview)
	{
		TArray<const FExtendedAtlassianPage*> Mentioned;
		for (const FExtendedAtlassianPage& Page :
			Controller->GetSnapshot().Pages)
		{
			if (!Page.Title.IsEmpty()
				&& Issue->Description.Contains(
					Page.Title,
					ESearchCase::IgnoreCase))
			{
				Mentioned.Add(&Page);
			}
		}
		for (const FExtendedAtlassianPage* MentionedPage : Mentioned)
		{
			const FString PageTitle = MentionedPage->Title;
			const FString PageId = MentionedPage->Id;
			MentionedPages->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 7.0f)
			[
				SNew(SButton)
					.ButtonStyle(&ExtendedAtlassianWorkspacePrivate::Button(
						TEXT("Backlot.Button.Secondary")))
					.ContentPadding(FMargin(10.0f, 8.0f))
					.HAlign(HAlign_Left)
					.OnClicked_Lambda(
						[this, PageId]()
						{
							Controller->SelectPage(PageId);
							Controller->Navigate(
								EExtendedAtlassianWorkspaceRoute::Docs);
							return FReply::Handled();
						})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 9.0f, 0.0f)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("MentionedPageGlyph", "▪"))
							.TextStyle(&ExtendedAtlassianWorkspacePrivate::Text(
								TEXT("Backlot.Mono.10")))
							.ColorAndOpacity(
								FExtendedAtlassianStyle::FromHex(
									TEXT("#b6a9ff")))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
							.Text(FText::FromString(PageTitle))
						.TextStyle(&ExtendedAtlassianWorkspacePrivate::Text(
							TEXT("Backlot.Sans.12")))
						.ColorAndOpacity(
							FExtendedAtlassianStyle::FromHex(TEXT("#b9c0ca")))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					]
				]
			];
		}
	}

	return SNew(SBorder).BorderImage(Brush(TEXT("Backlot.Brush.Sidebar"))).Padding(FMargin(14.0f, 16.0f))
	[
		SNew(SScrollBox)
		.ScrollBarStyle(
			&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(
				TEXT("Backlot.ScrollBar")))
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SectionLabel(
					bPreview
						? LOCTEXT("Preview", "PREVIEW")
						: LOCTEXT("Properties", "PROPERTIES"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				Fields
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 18.0f, 0.0f, 11.0f)
			[
				SNew(SBox)
					.Visibility(
						bPreview
							? EVisibility::Collapsed
							: EVisibility::Visible)
				[
					SectionLabel(
						LOCTEXT("MentionedPages", "MENTIONED PAGES"))
				]
			]
			+ SVerticalBox::Slot().AutoHeight()[MentionedPages]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 16.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
					.Visibility(
						bPreview
							? EVisibility::Visible
							: EVisibility::Collapsed)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Primary")))
					.ContentPadding(FMargin(8.0f, 8.0f))
					.OnClicked(
						this,
						&SExtendedAtlassianWorkspace::OnOpenSelectedIssue)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("OpenFullIssue", "Open full issue"))
						.TextStyle(&Text(TEXT("Backlot.Sans.12.Medium")))
				]
			]
		]
	];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildBoardRail()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	for (const FExtendedAtlassianIssue& Issue : Controller->GetSnapshot().Issues)
	{
		if (Issue.StatusName != TEXT("Blocked")) { continue; }
		Rows->AddSlot().AutoHeight().Padding(0.0f, 6.0f)
		[
			SNew(SButton).ButtonStyle(&Button(TEXT("Backlot.Button.Clear"))).ContentPadding(0.0f).OnClicked_Lambda([Controller = Controller, Key = Issue.Key]()
			{
				Controller->OpenIssue(Key);
				return FReply::Handled();
			})
			[
				SNew(SBorder)
				.BorderImage(Brush(TEXT("Backlot.Brush.IssueBlocked")))
				.Padding(FMargin(10.0f, 9.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 10.0f, 0.0f)
						[
							SNew(STextBlock)
								.Text(FText::FromString(Issue.Key))
								.TextStyle(&Text(TEXT("Backlot.Mono.10.Medium")))
								.ColorAndOpacity(
									FExtendedAtlassianStyle::FromHex(
										TEXT("#f0665f")))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 7.0f, 0.0f)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("BlockedCardState", "BLOCKED"))
								.TextStyle(&Text(TEXT("Backlot.Mono.9")))
								.ColorAndOpacity(
									FExtendedAtlassianStyle::FromHex(
										TEXT("#f0a9a4")))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
								.Text(FText::FromString(
									Issue.RelativeBlocked.IsEmpty()
										? Issue.RelativeUpdated
										: Issue.RelativeBlocked))
								.TextStyle(&Text(TEXT("Backlot.Mono.9")))
								.ColorAndOpacity(
									FExtendedAtlassianStyle::FromHex(
										TEXT("#6f7783")))
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 6.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
							.Text(FText::FromString(Issue.Summary))
							.TextStyle(&Text(TEXT("Backlot.Sans.11")))
							.AutoWrapText(true)
					]
				]
			]
		];
	}
	return SNew(SBorder)
	.BorderImage(Brush(TEXT("Backlot.Brush.Sidebar")))
	.Padding(FMargin(14.0f, 16.0f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[SectionLabel(LOCTEXT("BlockedRail", "BLOCKED"))]
		+ SVerticalBox::Slot().FillHeight(1.0f)[Rows]
	];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildPinsRail()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FExtendedAtlassianPin* Pin = SelectedPin();
	if (!Pin) { return EmptyState(LOCTEXT("NoPinRail", "NO PIN")); }
	const FExtendedAtlassianPinThread* SelectedThread = nullptr;
	if (!SelectedPinThreadId.IsEmpty())
	{
		SelectedThread = Pin->Threads.FindByPredicate(
			[this](const FExtendedAtlassianPinThread& Thread)
			{
				return Thread.Id == SelectedPinThreadId;
			});
	}
	if (!SelectedThread && !Pin->Threads.IsEmpty())
	{
		SelectedThread = &Pin->Threads[0];
	}
	int32 OpenCount = 0;
	TSharedRef<SVerticalBox> Threads = SNew(SVerticalBox);
	for (const FExtendedAtlassianPinThread& Thread : Pin->Threads)
	{
		OpenCount += Thread.bResolved ? 0 : 1;
		const bool bEditing = EditingPinMessageId == Thread.Id;
		TSharedRef<SVerticalBox> Message = SNew(SVerticalBox);
		Message->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
				.Text(FText::FromString(
					Thread.AuthorDisplayName + TEXT(" · ") + Thread.RelativeTime))
				.TextStyle(&Text(TEXT("Backlot.Sans.11.Medium")))
		];
		if (bEditing)
		{
			Message->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 5.0f, 0.0f, 7.0f)
			[
				SNew(SMultiLineEditableTextBox)
					.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
						TEXT("Backlot.Field")))
					.Text(FText::FromString(PinMessageEditDraft))
					.OnTextChanged_Lambda(
						[this](const FText& Value)
						{
							PinMessageEditDraft = Value.ToString();
						})
			];
			Message->AddSlot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 7.0f, 0.0f)
				[
					SNew(SButton)
						.ButtonStyle(&Button(TEXT("Backlot.Button.Primary")))
						.ContentPadding(FMargin(10.0f, 5.0f))
						.OnClicked_Lambda(
							[this, Id = Thread.Id]()
							{
								SavePinMessageEdit(Id);
								return FReply::Handled();
							})
						[
							SNew(STextBlock)
								.Text(LOCTEXT("SavePinMessage", "Save"))
								.TextStyle(&Text(TEXT("Backlot.Sans.10.Medium")))
						]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
						.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
						.ContentPadding(FMargin(10.0f, 5.0f))
						.OnClicked_Lambda(
							[this]()
							{
								CancelPinMessageEdit();
								return FReply::Handled();
							})
						[
							SNew(STextBlock)
								.Text(LOCTEXT("CancelPinMessage", "Cancel"))
								.TextStyle(&Text(TEXT("Backlot.Sans.10")))
						]
				]
			];
		}
		else
		{
			Message->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 3.0f, 0.0f, 7.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(Thread.Body))
					.TextStyle(&Text(TEXT("Backlot.Sans.12")))
					.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#a2a9b4")))
					.AutoWrapText(true)
			];
			Message->AddSlot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SButton)
						.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
						.ContentPadding(1.0f)
						.OnClicked_Lambda(
							[this, Thread]()
							{
								BeginPinMessageEdit(Thread);
								return FReply::Handled();
							})
						[
							SNew(STextBlock)
								.Text(LOCTEXT("EditPinMessage", "EDIT"))
								.TextStyle(&Text(TEXT("Backlot.Mono.10")))
								.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
						]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
						.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
						.ContentPadding(1.0f)
						.OnClicked_Lambda(
							[this, PinId = Pin->Id, ThreadId = Thread.Id]()
							{
								ConfirmDeletePinMessage(PinId, ThreadId);
								return FReply::Handled();
							})
						[
							SNew(STextBlock)
								.Text(LOCTEXT("DeletePinMessage", "DELETE"))
								.TextStyle(&Text(TEXT("Backlot.Mono.10")))
								.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
						]
				]
			];
		}
		Threads->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 16.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 10.0f, 0.0f)
			.VAlign(VAlign_Top)
			[
				SNew(SBox)
					.WidthOverride(22.0f)
					.HeightOverride(22.0f)
					[
						SNew(SBorder)
							.BorderImage(AvatarBrush(
								Thread.AuthorAccountId))
							.Padding(0.0f)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Text(FText::FromString(
										Thread.AuthorAccountId.IsEmpty()
											? TEXT("AK")
											: Thread.AuthorAccountId))
									.TextStyle(&Text(TEXT("Backlot.Mono.8")))
							]
					]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				Message
			]
		];
	}
	return SNew(SBorder).BorderImage(Brush(TEXT("Backlot.Brush.Sidebar"))).Padding(FMargin(14.0f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SectionLabel(LOCTEXT("ThreadRail", "THREAD"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
					.Text(SelectedThread && SelectedThread->bResolved
						? LOCTEXT("SelectedThreadResolved", "RESOLVED")
						: LOCTEXT("SelectedThreadOpen", "OPEN"))
					.TextStyle(&Text(TEXT("Backlot.Mono.10")))
					.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(
						SelectedThread && SelectedThread->bResolved
							? TEXT("#57cc8a")
							: TEXT("#e3a54a")))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 25.0f, 0.0f, 9.0f)
		[
			SNew(SBorder)
			.BorderImage(Brush(TEXT("Backlot.Brush.Card")))
			.Padding(FMargin(10.0f, 9.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 9.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(20.0f)
					.HeightOverride(20.0f)
					[
						SNew(SBorder)
						.BorderImage(Brush(TEXT("Backlot.Brush.Purple")))
						.Padding(0.0f)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::AsNumber(OpenCount))
							.TextStyle(&Text(TEXT("Backlot.Mono.9.Medium")))
							.ColorAndOpacity(
								FExtendedAtlassianStyle::FromHex(
									TEXT("#cfe0ff")))
						]
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SButton)
						.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
						.ContentPadding(0.0f)
						.HAlign(HAlign_Left)
						.ToolTipText(LOCTEXT(
							"RevealPinTargetTooltip",
							"Reveal this target"))
						.OnClicked_Lambda(
							[this, PinValue = *Pin]()
							{
								RevealPinTarget(PinValue);
								return FReply::Handled();
							})
						[
							SNew(STextBlock)
								.Text(FText::FromString(Pin->DisplayName))
								.TextStyle(&Text(TEXT("Backlot.Mono.12")))
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						]
				]
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SScrollBox)
				.ScrollBarStyle(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(TEXT("Backlot.ScrollBar")))
				+ SScrollBox::Slot()
				.Padding(0.0f, 7.0f, 0.0f, 0.0f)
				[
					Threads
				]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 9.0f, 0.0f, 0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(68.0f)
				[
					SNew(SMultiLineEditableTextBox)
						.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
							TEXT("Backlot.Field")))
						.HintText(LOCTEXT("ReplyPin", "Reply to this pin…"))
						.Text(FText::FromString(PinReplyDraft))
						.OnTextChanged_Lambda(
							[this](const FText& Value)
							{
								PinReplyDraft = Value.ToString();
							})
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
						.IsEnabled(SelectedThread != nullptr)
						.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
						.ContentPadding(2.0f)
						.OnClicked_Lambda(
							[this, Thread = SelectedThread]()
							{
								if (Thread)
								{
									TogglePinThreadResolved(*Thread);
								}
								return FReply::Handled();
							})
						[
							SNew(STextBlock)
								.Text(SelectedThread && SelectedThread->bResolved
									? LOCTEXT("ReopenPinThread", "REOPEN")
									: LOCTEXT("ResolvePinThread", "RESOLVE"))
								.TextStyle(&Text(TEXT("Backlot.Mono.10")))
								.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(
									SelectedThread && SelectedThread->bResolved
										? TEXT("#57cc8a")
										: TEXT("#6f7783")))
						]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("PinWatchers", "3 WATCHING"))
						.TextStyle(&Text(TEXT("Backlot.Mono.9")))
						.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
						.ButtonStyle(&Button(TEXT("Backlot.Button.Primary")))
						.ContentPadding(FMargin(12.0f, 6.0f))
						.OnClicked(this, &SExtendedAtlassianWorkspace::OnPostPinReply)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("SendPinReply", "Send"))
								.TextStyle(&Text(TEXT("Backlot.Sans.11.Medium")))
						]
				]
			]
		]
	];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildInboxRail()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FExtendedAtlassianNotification* Notification = SelectedNotification();
	if (!Notification) { return EmptyState(LOCTEXT("NoNotification", "NO NOTIFICATION")); }
	return SNew(SBorder).BorderImage(Brush(TEXT("Backlot.Brush.Sidebar"))).Padding(FMargin(14.0f, 16.0f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SectionLabel(LOCTEXT("NotificationRail", "NOTIFICATION"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
					.Text(FText::FromString(NotificationKindLabel(Notification->Kind)))
					.TextStyle(&Text(TEXT("Backlot.Mono.10")))
					.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(
						NotificationKindColor(Notification->Kind)))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 22.0f, 0.0f, 18.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 10.0f, 0.0f)
			.VAlign(VAlign_Top)
			[
				SNew(SBox)
				.WidthOverride(26.0f)
				.HeightOverride(26.0f)
				[
					SNew(SBorder)
						.BorderImage(AvatarBrush(
							Notification->ActorAccountId))
						.Padding(0.0f)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(FText::FromString(
									Notification->ActorAccountId))
								.TextStyle(&Text(TEXT("Backlot.Mono.9")))
						]
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
						.Text(FText::FromString(
							Notification->ActorDisplayName))
						.TextStyle(&Text(TEXT("Backlot.Sans.12.Medium")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(
							Notification->RelativeTime
								+ TEXT(" AGO  ·  ")
								+ Notification->Source))
						.TextStyle(&Text(TEXT("Backlot.Mono.9")))
						.ColorAndOpacity(
							FExtendedAtlassianStyle::FromHex(
								TEXT("#6f7783")))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(88.0f)
			[
				SNew(SBorder)
				.BorderImage(Brush(TEXT("Backlot.Brush.Card")))
				.Padding(FMargin(12.0f, 13.0f))
				[
					SNew(STextBlock)
						.Text(FText::FromString(Notification->Quote))
						.TextStyle(&Text(TEXT("Backlot.Sans.13")))
						.LineHeightPercentage(1.2f)
						.AutoWrapText(true)
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 18.0f, 0.0f, 8.0f)
		[
			SectionLabel(LOCTEXT("NotificationOn", "ON"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
				.ContentPadding(FMargin(11.0f, 9.0f))
				.HAlign(HAlign_Left)
				.OnClicked_Lambda(
					[this, NotificationValue = *Notification]()
					{
						OpenInboxNotificationTarget(NotificationValue);
						return FReply::Handled();
					})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 9.0f, 0.0f)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("NotificationTargetGlyph", "▪"))
							.TextStyle(&Text(TEXT("Backlot.Mono.10")))
							.ColorAndOpacity(
								FExtendedAtlassianStyle::FromHex(
									NotificationKindColor(
										Notification->Kind)))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
							.Text(FText::FromString(Notification->Target))
							.TextStyle(&Text(TEXT("Backlot.Sans.12")))
							.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					]
				]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Primary")))
				.ContentPadding(FMargin(8.0f, 8.0f))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda(
					[this, NotificationValue = *Notification]()
					{
						OpenInboxNotificationTarget(NotificationValue);
						return FReply::Handled();
					})
			[
				SNew(STextBlock).Text(LOCTEXT("OpenBacklot", "Open in Backlot")).TextStyle(&Text(TEXT("Backlot.Sans.12.Medium")))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
					.ContentPadding(FMargin(8.0f, 7.0f))
					.HAlign(HAlign_Center)
					.OnClicked_Lambda(
						[this, Id = Notification->Id]()
						{
							MarkInboxNotificationRead(Id, true);
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
							.Text(LOCTEXT("MarkInboxRead", "Mark read"))
							.TextStyle(&Text(TEXT("Backlot.Sans.11")))
					]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
					.ContentPadding(FMargin(8.0f, 7.0f))
					.HAlign(HAlign_Center)
					.OnClicked_Lambda(
						[this, Id = Notification->Id]()
						{
							MuteInboxThread(Id);
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
							.Text(LOCTEXT("MuteInboxThread", "Mute thread"))
							.TextStyle(&Text(TEXT("Backlot.Sans.11")))
					]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 9.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT(
					"QueueCopy",
					"Backlot never interrupts a build. Everything queues here until you open the tab."))
				.TextStyle(&Text(TEXT("Backlot.Sans.10")))
				.ColorAndOpacity(
					FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
				.AutoWrapText(true)
		]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildCreateCardPopover()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const float Height = bCreateCardEdit ? 434.0f : 366.0f;
	TSharedRef<SVerticalBox> Contents = SNew(SVerticalBox);
	Contents->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 11.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("CreateCardDot", "●"))
				.TextStyle(&Text(TEXT("Backlot.Mono.8")))
				.ColorAndOpacity(StatusColor(CreateCardStatus))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
				.Text(bCreateCardEdit
					? LOCTEXT("EditCardHeading", "Edit card")
					: LOCTEXT("NewCardHeading", "New card"))
				.TextStyle(&Text(TEXT("Backlot.Sans.11.Medium")))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
				.ContentPadding(3.0f)
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnDismissCreateCard)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("CloseCreateCard", "×"))
						.TextStyle(&Text(TEXT("Backlot.Mono.12")))
						.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
				]
		]
	];
	Contents->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 12.0f)
	[
		SNew(SEditableTextBox)
			.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
				TEXT("Backlot.Field")))
			.HintText(LOCTEXT("CreateCardSummaryHint", "What needs doing?"))
			.Text(FText::FromString(CreateCardSummary))
			.OnTextChanged_Lambda(
				[this](const FText& Value)
				{
					CreateCardSummary = Value.ToString();
				})
			.OnTextCommitted_Lambda(
				[this](const FText&, ETextCommit::Type CommitType)
				{
					if (CommitType == ETextCommit::OnEnter)
					{
						OnSubmitCreateCard();
					}
				})
	];

	Contents->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
	[
		SectionLabel(LOCTEXT("CreateCardType", "TYPE"))
	];
	TSharedRef<SHorizontalBox> TypeChoices = SNew(SHorizontalBox);
	for (const FExtendedAtlassianIssueType& Type : Controller->GetSnapshot().IssueTypes)
	{
		const bool bSelected = CreateCardType.Equals(Type.Name, ESearchCase::IgnoreCase);
		TypeChoices->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 0.0f, 5.0f, 0.0f)
		[
			SNew(SButton)
				.ButtonStyle(&Button(
					bSelected ? TEXT("Backlot.Button.Primary") : TEXT("Backlot.Button.Secondary")))
				.ContentPadding(FMargin(4.0f, 6.0f))
				.OnClicked_Lambda(
					[this, Name = Type.Name]()
					{
						CreateCardType = Name;
						Rebuild();
						return FReply::Handled();
					})
				[
					SNew(STextBlock)
						.Text(FText::FromString(Type.Name))
						.TextStyle(&Text(TEXT("Backlot.Sans.11.Medium")))
				]
		];
	}
	Contents->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 11.0f)[TypeChoices];

	Contents->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
	[
		SectionLabel(LOCTEXT("CreateCardPriority", "PRIORITY"))
	];
	TSharedRef<SHorizontalBox> PriorityChoices = SNew(SHorizontalBox);
	for (const FExtendedAtlassianPriority& Priority : Controller->GetSnapshot().Priorities)
	{
		const bool bSelected = CreateCardPriority.Equals(Priority.Name, ESearchCase::IgnoreCase);
		PriorityChoices->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 0.0f, 5.0f, 0.0f)
		[
			SNew(SButton)
				.ButtonStyle(&Button(
					bSelected ? TEXT("Backlot.Button.Primary") : TEXT("Backlot.Button.Secondary")))
				.ContentPadding(FMargin(3.0f, 6.0f))
				.OnClicked_Lambda(
					[this, Name = Priority.Name]()
					{
						CreateCardPriority = Name;
						Rebuild();
						return FReply::Handled();
					})
				[
					SNew(STextBlock)
						.Text(FText::FromString(Priority.Name.ToUpper()))
						.TextStyle(&Text(TEXT("Backlot.Sans.10.Medium")))
				]
		];
	}
	Contents->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 11.0f)[PriorityChoices];

	Contents->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
	[
		SectionLabel(LOCTEXT("CreateCardAssignee", "ASSIGNEE"))
	];
	TSharedRef<SHorizontalBox> AssigneeChoices = SNew(SHorizontalBox);
	for (const FExtendedAtlassianUser& User : Controller->GetSnapshot().People)
	{
		const bool bSelected = CreateCardAssignee == User.AccountId;
		const FExtendedAtlassianTeamLoad* AvatarLoad =
			Controller->GetSnapshot().TeamLoad.FindByPredicate(
				[&User](const FExtendedAtlassianTeamLoad& Load)
				{
					return Load.User.AccountId == User.AccountId;
				});
		const FExtendedAtlassianUser* AvatarUser =
			AvatarLoad ? &AvatarLoad->User : &User;
		const FString UserInitials = !AvatarUser->Initials.IsEmpty()
			? AvatarUser->Initials
			: LabelInitials(User.DisplayName);
		AssigneeChoices->AddSlot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(SBox)
				.WidthOverride(26.0f)
				.HeightOverride(26.0f)
				[
					SNew(SButton)
						.ButtonStyle(&Button(
							bSelected
								? TEXT("Backlot.Button.Primary")
								: TEXT("Backlot.Button.Secondary")))
						.ContentPadding(0.0f)
						.ToolTipText(FText::FromString(User.DisplayName))
						.OnClicked_Lambda(
							[this, AccountId = User.AccountId]()
							{
								CreateCardAssignee = AccountId;
								Rebuild();
								return FReply::Handled();
						})
					[
						SNew(SBorder)
						.BorderImage(AvatarBrush(AvatarUser, UserInitials))
						.Padding(0.0f)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(UserInitials))
							.TextStyle(&Text(TEXT("Backlot.Mono.9.Medium")))
							.ColorAndOpacity(AvatarForeground(AvatarUser))
						]
					]
			]
		];
	}
	Contents->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 11.0f)
	[
		SNew(SBox)
		.HeightOverride(36.0f)
		[
			SNew(SScrollBox)
			.Orientation(Orient_Horizontal)
			.ScrollBarStyle(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(
				TEXT("Backlot.ScrollBar")))
			+ SScrollBox::Slot()
			[
				AssigneeChoices
			]
		]
	];

	Contents->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
	[
		SectionLabel(LOCTEXT("CreateCardEpic", "EPIC"))
	];
	Contents->AddSlot()
	.AutoHeight()
	[
		SNew(SComboButton)
			.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
			.ContentPadding(FMargin(10.0f, 7.0f))
			.HasDownArrow(false)
			.OnGetMenuContent(
				this,
				&SExtendedAtlassianWorkspace::BuildCreateCardEpicPicker)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
						.Text(
							CreateCardEpic.IsEmpty()
								? LOCTEXT("CreateCardNoEpic", "No epic")
								: FText::FromString(CreateCardEpic))
						.TextStyle(&Text(TEXT("Backlot.Sans.12")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SImage)
						.Image(Brush(TEXT("Backlot.Icon.CaretDown")))
						.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
				]
			]
	];
	if (bCreateCardEdit)
	{
		Contents->AddSlot().AutoHeight().Padding(0.0f, 11.0f, 0.0f, 6.0f)
		[
			SectionLabel(LOCTEXT("CreateCardStatus", "STATUS"))
		];
		Contents->AddSlot()
		.AutoHeight()
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
				.ContentPadding(FMargin(10.0f, 7.0f))
				.OnClicked_Lambda(
					[this]()
					{
						OpenStatusMenu(CreateCardTargetKey);
						return FReply::Handled();
					})
				[
					SNew(STextBlock)
						.Text(FText::FromString(CreateCardStatus))
						.TextStyle(&Text(TEXT("Backlot.Sans.12")))
						.ColorAndOpacity(StatusColor(CreateCardStatus))
				]
		];
	}

	Contents->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 13.0f, 0.0f, 0.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
				.Visibility(bCreateCardEdit ? EVisibility::Visible : EVisibility::Collapsed)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Danger")))
				.ContentPadding(FMargin(11.0f, 7.0f))
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnDeleteCreateCard)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("DeleteCreateCard", "Delete"))
						.TextStyle(&Text(TEXT("Backlot.Sans.11")))
				]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(7.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
				.ContentPadding(FMargin(13.0f, 7.0f))
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnDismissCreateCard)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("CancelCreateCard", "Cancel"))
						.TextStyle(&Text(TEXT("Backlot.Sans.11")))
				]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(7.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Primary")))
				.ContentPadding(FMargin(10.0f, 7.0f))
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnSubmitCreateCard)
				[
					SNew(STextBlock)
						.Text(bCreateCardEdit
							? LOCTEXT("SaveCreateCard", "Save card")
							: LOCTEXT("AddCreateCard", "Add card"))
						.TextStyle(&Text(TEXT("Backlot.Sans.12.Medium")))
				]
		]
	];

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Invisible")))
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnDismissCreateCard)
		]
		+ SOverlay::Slot()
		[
			SNew(SConstraintCanvas)
			+ SConstraintCanvas::Slot()
			.Offset(FMargin(
				CreateCardPosition.X,
				CreateCardPosition.Y,
				ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.CreateCard.Width")),
				Height))
			.Anchors(FAnchors(0.0f, 0.0f))
			// PopupPosition returns a top-left already clamped inside the visible bounds. A canvas
			// slot's alignment is the widget's pivot and defaults to centred, which would re-centre
			// the panel on that point and push half of it back out past the edge it was clamped to.
			.Alignment(FVector2D::ZeroVector)
			[
				AnimatedPanel(
					SNew(SBorder)
						.BorderImage(Brush(TEXT("Backlot.Brush.Modal")))
						.Padding(13.0f)
						[
							Contents
						],
					0.14f,
					6.0f,
					true)
			]
		];
}

void SExtendedAtlassianWorkspace::HandleAuthStateChanged()
{
	if (!Controller.IsValid() || Controller->IsFixtureProvider())
	{
		return;
	}
	LastAuthConfigurationSignature =
		ExtendedAtlassianWorkspacePrivate::AuthConfigurationSignature();
	LastSyncPollSeconds = HostServices.IsValid()
		? HostServices->NowSeconds()
		: 0.0;
	bSyncRefreshDeferred = false;
	Controller->Refresh();
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildCreateCardEpicPicker()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	CreateCardEpicSearch.Reset();
	CreateCardEpicRows = SNew(SVerticalBox);
	RebuildCreateCardEpicRows();

	return SNew(SBox)
		.WidthOverride(300.0f)
		.MaxDesiredHeight(278.0f)
		[
			SNew(SBorder)
			.BorderImage(Brush(TEXT("Backlot.Brush.Modal")))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 7.0f)
				[
					SNew(SEditableTextBox)
					.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
						TEXT("Backlot.Field")))
					.HintText(LOCTEXT("CreateCardEpicSearchHint", "Search epics..."))
					.Text(FText::FromString(CreateCardEpicSearch))
					.SelectAllTextWhenFocused(true)
					.OnTextChanged_Lambda(
						[this](const FText& Value)
						{
							CreateCardEpicSearch = Value.ToString();
							RebuildCreateCardEpicRows();
						})
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SBox)
					.MaxDesiredHeight(220.0f)
					[
						SNew(SScrollBox)
						.ScrollBarStyle(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(
							TEXT("Backlot.ScrollBar")))
						+ SScrollBox::Slot()
						[
							CreateCardEpicRows.ToSharedRef()
						]
					]
				]
			]
		];
}

void SExtendedAtlassianWorkspace::RebuildCreateCardEpicRows()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	if (!CreateCardEpicRows.IsValid())
	{
		return;
	}

	CreateCardEpicRows->ClearChildren();
	const FString Search = CreateCardEpicSearch.TrimStartAndEnd();
	auto AddEpicRow =
		[this](
			const FText& Label,
			const FString& EpicName,
			const FString& Color,
			const FString& Glyph)
		{
			const bool bSelected = CreateCardEpic == EpicName;
			CreateCardEpicRows->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SButton)
				.ButtonStyle(&Button(
					bSelected
						? TEXT("Backlot.Button.Primary")
						: TEXT("Backlot.Button.Secondary")))
				.ContentPadding(FMargin(9.0f, 7.0f))
				.HAlign(HAlign_Left)
				.OnClicked_Lambda(
					[this, EpicName]()
					{
						CreateCardEpic = EpicName;
						CreateCardEpicSearch.Reset();
						CreateCardEpicRows.Reset();
						FSlateApplication::Get().DismissAllMenus();
						Rebuild();
						return FReply::Handled();
					})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Glyph))
						.TextStyle(&Text(TEXT("Backlot.Mono.10")))
						.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(
							Color.IsEmpty() ? TEXT("#6f7783") : *Color))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(Label)
						.TextStyle(&Text(TEXT("Backlot.Sans.11")))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					]
				]
			];
		};

	AddEpicRow(
		LOCTEXT("CreateCardEpicNone", "No epic"),
		FString(),
		TEXT("#6f7783"),
		TEXT("—"));

	int32 MatchCount = 0;
	for (const FExtendedAtlassianEpic& Epic : Controller->GetSnapshot().Epics)
	{
		if (!Search.IsEmpty()
			&& !Epic.Name.Contains(Search, ESearchCase::IgnoreCase)
			&& !Epic.Id.Contains(Search, ESearchCase::IgnoreCase))
		{
			continue;
		}
		++MatchCount;
		AddEpicRow(
			FText::FromString(Epic.Name),
			Epic.Name,
			Epic.Color,
			TEXT("●"));
	}
	if (MatchCount == 0)
	{
		CreateCardEpicRows->AddSlot()
		.AutoHeight()
		.Padding(9.0f, 8.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CreateCardNoEpicMatches", "No matching epics"))
			.TextStyle(&Text(TEXT("Backlot.Sans.11")))
			.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
		];
	}
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildPagePopover()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const float Height = bPagePopoverRename ? 151.0f : 210.0f;
	FString ParentLabel = TEXT("Top level");
	if (!PagePopoverParentId.IsEmpty())
	{
		if (const FExtendedAtlassianDocumentTreeNode* Parent =
			Controller->GetSnapshot().DocumentTree.FindByPredicate(
				[this](const FExtendedAtlassianDocumentTreeNode& Node)
				{
					return Node.Id == PagePopoverParentId && Node.bSection;
				}))
		{
			ParentLabel = Parent->Label;
		}
	}

	TSharedRef<SVerticalBox> Contents = SNew(SVerticalBox);
	Contents->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 11.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(5.0f)
			.HeightOverride(5.0f)
			[
				SNew(SImage)
				.Image(Brush(TEXT("Backlot.Brush.BlueSolid")))
				.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#b6a9ff")))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
				.Text(bPagePopoverRename
					? LOCTEXT("RenamePagePopover", "Rename")
					: LOCTEXT("NewPagePopover", "New page"))
				.TextStyle(&Text(TEXT("Backlot.Sans.11.Medium")))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
				.ContentPadding(FMargin(3.0f))
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnDismissPagePopover)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("ClosePagePopover", "×"))
						.TextStyle(&Text(TEXT("Backlot.Mono.12")))
						.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
				]
		]
	];
	Contents->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, bPagePopoverRename ? 12.0f : 10.0f)
	[
		SAssignNew(PageTitleBox, SEditableTextBox)
			.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
				TEXT("Backlot.Field")))
			.HintText(LOCTEXT("PageTitleHint", "Page title"))
			.Text(FText::FromString(PagePopoverTitle))
			.OnTextChanged(this, &SExtendedAtlassianWorkspace::OnPagePopoverTitleChanged)
			.OnTextCommitted_Lambda(
				[this](const FText&, ETextCommit::Type CommitType)
				{
					if (CommitType == ETextCommit::OnEnter)
					{
						OnSubmitPagePopover();
					}
				})
	];
	if (!bPagePopoverRename)
	{
		Contents->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SectionLabel(LOCTEXT("PageUnder", "UNDER"))
		];
		Contents->AddSlot()
		.AutoHeight()
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
				.ContentPadding(FMargin(10.0f, 7.0f))
				.OnClicked_Lambda(
					[this]()
					{
						TArray<FWorkspaceMenuItem> Items;
						Items.Add({
							LOCTEXT("TopLevel", "Top level"),
							FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")),
							PagePopoverParentId.IsEmpty() ? TEXT("●") : FString(),
							[this]() { PagePopoverParentId.Reset(); Rebuild(); }
						});
						for (const FExtendedAtlassianDocumentTreeNode& Node :
							Controller->GetSnapshot().DocumentTree)
						{
							if (!Node.bSection)
							{
								continue;
							}
							Items.Add({
								FText::FromString(Node.Label),
								FExtendedAtlassianStyle::FromHex(TEXT("#b6a9ff")),
								PagePopoverParentId == Node.Id ? TEXT("●") : FString(),
								[this, Id = Node.Id]() { PagePopoverParentId = Id; Rebuild(); }
							});
						}
						OpenMenu(LOCTEXT("PlaceUnder", "PLACE UNDER"), MoveTemp(Items));
						return FReply::Handled();
					})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
							.Text(FText::FromString(ParentLabel))
							.TextStyle(&Text(TEXT("Backlot.Sans.12")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SImage)
							.Image(Brush(TEXT("Backlot.Icon.CaretDown")))
							.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
					]
				]
		];
	}
	Contents->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 13.0f, 0.0f, 0.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
				.ContentPadding(FMargin(13.0f, 7.0f))
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnDismissPagePopover)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("CancelPagePopover", "Cancel"))
						.TextStyle(&Text(TEXT("Backlot.Sans.11")))
				]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(7.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Primary")))
				.ContentPadding(FMargin(10.0f, 7.0f))
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnSubmitPagePopover)
				[
					SNew(STextBlock)
						.Text(bPagePopoverRename
							? LOCTEXT("RenamePageSubmit", "Rename")
							: LOCTEXT("CreatePageSubmit", "Create page"))
						.TextStyle(&Text(TEXT("Backlot.Sans.12.Medium")))
				]
		]
	];

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Invisible")))
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnDismissPagePopover)
		]
		+ SOverlay::Slot()
		[
			SNew(SConstraintCanvas)
			+ SConstraintCanvas::Slot()
			.Offset(FMargin(
				PagePopoverPosition.X,
				PagePopoverPosition.Y,
				ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.PagePopover.Width")),
				Height))
			.Anchors(FAnchors(0.0f, 0.0f))
			.Alignment(FVector2D::ZeroVector)
			[
				AnimatedPanel(
					SNew(SBorder)
						.BorderImage(Brush(TEXT("Backlot.Brush.Modal")))
						.Padding(13.0f)
						[
							Contents
						],
					0.14f,
					6.0f,
					true)
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildPinPopover()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const float Height = bPinPopoverRename ? 150.0f : 210.0f;
	TSharedRef<SUniformGridPanel> Kinds = SNew(SUniformGridPanel).SlotPadding(FMargin(2.5f));
	if (!bPinPopoverRename)
	{
		const EExtendedAtlassianPinKind Values[] = {
			EExtendedAtlassianPinKind::Material,
			EExtendedAtlassianPinKind::Level,
			EExtendedAtlassianPinKind::Blueprint,
			EExtendedAtlassianPinKind::Page
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Values); ++Index)
		{
			const EExtendedAtlassianPinKind Kind = Values[Index];
			const bool bSelected = Kind == PinPopoverKind;
			Kinds->AddSlot(Index, 0)
			[
				SNew(SButton)
					.ButtonStyle(&Button(
						bSelected
							? TEXT("Backlot.Button.Secondary")
							: TEXT("Backlot.Button.Clear")))
					.ContentPadding(FMargin(5.0f, 7.0f))
					.OnClicked_Lambda(
						[this, Kind]()
						{
							ResolvePinPopoverTarget(Kind);
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
							.Text(FText::FromString(PinKindKey(Kind)))
							.Justification(ETextJustify::Center)
							.TextStyle(&Text(TEXT("Backlot.Mono.9.Medium")))
							.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(
								bSelected ? PinKindColor(Kind) : TEXT("#8a919c")))
					]
			];
		}
	}

	TSharedRef<SVerticalBox> Contents = SNew(SVerticalBox);
	Contents->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 11.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
				.Text(bPinPopoverRename
					? LOCTEXT("RenamePinPopover", "Rename pin")
					: LOCTEXT("NewPinPopover", "Pin an asset"))
				.TextStyle(&Text(TEXT("Backlot.Sans.11.Medium")))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
				.ContentPadding(3.0f)
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnDismissPinPopover)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("ClosePinPopover", "✕"))
						.TextStyle(&Text(TEXT("Backlot.Mono.12")))
						.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
				]
		]
	];
	Contents->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, bPinPopoverRename ? 0.0f : 12.0f)
	[
		SNew(SEditableTextBox)
			.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
				TEXT("Backlot.Field")))
			.HintText(PinPopoverTargetError.IsEmpty()
				? LOCTEXT("PinAssetNameHint", "Asset name")
				: PinPopoverTargetError)
			.Text(FText::FromString(PinPopoverName))
			.ToolTipText(PinPopoverTargetError)
			.OnTextChanged(this, &SExtendedAtlassianWorkspace::OnPinPopoverNameChanged)
			.OnTextCommitted_Lambda(
				[this](const FText&, ETextCommit::Type CommitType)
				{
					if (CommitType == ETextCommit::OnEnter)
					{
						OnSubmitPinPopover();
					}
				})
	];
	if (!bPinPopoverRename)
	{
		Contents->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SectionLabel(LOCTEXT("PinKindHeading", "KIND"))
		];
		Contents->AddSlot()
		.AutoHeight()
		[
			Kinds
		];
	}
	Contents->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 13.0f, 0.0f, 0.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
				.ContentPadding(FMargin(13.0f, 7.0f))
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnDismissPinPopover)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("CancelPinPopover", "Cancel"))
						.TextStyle(&Text(TEXT("Backlot.Sans.11")))
				]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(7.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Primary")))
				.ContentPadding(FMargin(10.0f, 7.0f))
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnSubmitPinPopover)
				[
					SNew(STextBlock)
						.Text(bPinPopoverRename
							? LOCTEXT("RenamePinSubmit", "Rename")
							: LOCTEXT("CreatePinSubmit", "Pin it"))
						.TextStyle(&Text(TEXT("Backlot.Sans.12.Medium")))
				]
		]
	];

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Invisible")))
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnDismissPinPopover)
		]
		+ SOverlay::Slot()
		[
			SNew(SConstraintCanvas)
			+ SConstraintCanvas::Slot()
			.Offset(FMargin(
				PinPopoverPosition.X,
				PinPopoverPosition.Y,
				ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.PinPopover.Width")),
				Height))
			.Anchors(FAnchors(0.0f, 0.0f))
			.Alignment(FVector2D::ZeroVector)
			[
				AnimatedPanel(
					SNew(SBorder)
						.BorderImage(Brush(TEXT("Backlot.Brush.Modal")))
						.Padding(13.0f)
						[
							Contents
						],
					0.14f,
					6.0f,
					true)
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildGenericMenu()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const float Height = 35.0f + MenuItems.Num() * 30.0f + 10.0f;
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	Rows->AddSlot()
	.AutoHeight()
	.Padding(9.0f, 7.0f, 9.0f, 8.0f)
	[
		SNew(STextBlock)
			.Text(MenuTitle)
			.TextStyle(&Text(TEXT("Backlot.Mono.9.Medium")))
			.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
	];
	for (int32 Index = 0; Index < MenuItems.Num(); ++Index)
	{
		const FWorkspaceMenuItem Item = MenuItems[Index];
		Rows->AddSlot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.GenericMenu.ItemHeight")))
			[
				SNew(SBorder)
					.BorderImage(Brush(
						Index == MenuSelectedIndex
							? TEXT("Backlot.Brush.CardSelected")
							: TEXT("Backlot.Brush.Panel")))
					.Padding(0.0f)
					[
						SNew(SButton)
							.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
							.ContentPadding(FMargin(9.0f, 0.0f))
							.OnClicked_Lambda(
								[this, Index]()
								{
									const TFunction<void()> Action = MenuItems[Index].Action;
									CloseMenu(false);
									if (Action)
									{
										Action();
									}
									return FReply::Handled();
								})
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 9.0f, 0.0f)
								[
									SNew(SImage)
										.Image(Brush(TEXT("Backlot.Brush.BlueSolid")))
										.ColorAndOpacity(Item.Color)
								]
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text(Item.Label)
										.TextStyle(&Text(TEXT("Backlot.Sans.12")))
										.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#d7dce3")))
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text(FText::FromString(Item.Mark))
										.TextStyle(&Text(TEXT("Backlot.Mono.10")))
										.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")))
								]
							]
					]
			]
		];
	}

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Invisible")))
				.OnClicked(this, &SExtendedAtlassianWorkspace::OnDismissMenu)
		]
		+ SOverlay::Slot()
		[
			SNew(SConstraintCanvas)
			+ SConstraintCanvas::Slot()
			.Offset(FMargin(MenuPosition.X, MenuPosition.Y, MenuWidth, Height))
			.Anchors(FAnchors(0.0f, 0.0f))
			.Alignment(FVector2D::ZeroVector)
			[
				AnimatedPanel(
					SNew(SBorder)
						.BorderImage(Brush(TEXT("Backlot.Brush.Modal")))
						.Padding(5.0f)
						[
							Rows
						],
					0.12f,
					6.0f,
					true)
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildConfirmDialog()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Scrim")))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(0.0f)
		[
			SNew(SBox)
			.WidthOverride(TAttribute<FOptionalSize>::CreateSP(
			this,
			&SExtendedAtlassianWorkspace::ConfirmWidth))
			[
				SNew(SBorder)
					.BorderImage(Brush(TEXT("Backlot.Brush.Modal")))
					.Padding(18.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 8.0f)
						[
							SNew(STextBlock)
								.Text(ConfirmTitle)
								.TextStyle(&Text(TEXT("Backlot.Sans.13.Medium")))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 17.0f)
						[
							SNew(STextBlock)
								.Text(ConfirmBody)
								.TextStyle(&Text(TEXT("Backlot.Sans.12")))
								.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#a2a9b4")))
								.AutoWrapText(true)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Right)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SButton)
									.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
									.ContentPadding(FMargin(14.0f, 7.0f))
									.OnClicked(this, &SExtendedAtlassianWorkspace::OnDismissConfirm)
									[
										SNew(STextBlock)
											.Text(LOCTEXT("CancelConfirm", "Cancel"))
											.TextStyle(&Text(TEXT("Backlot.Sans.12")))
									]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(8.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SButton)
									.ButtonStyle(&Button(TEXT("Backlot.Button.Danger")))
									.ContentPadding(FMargin(14.0f, 7.0f))
									.OnClicked(this, &SExtendedAtlassianWorkspace::OnAcceptConfirm)
									[
										SNew(STextBlock)
											.Text(ConfirmAcceptLabel)
											.TextStyle(&Text(TEXT("Backlot.Sans.12.Medium")))
									]
							]
						]
					]
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildCaptureOverlay()
{
	using namespace ExtendedAtlassianWorkspacePrivate;

	const auto CaptureChoice = [this](
		const FString& Value,
		const FString& Selected,
		TFunction<void()> Select) -> TSharedRef<SWidget>
	{
		const bool bSelected = Value == Selected;
		return SNew(SBox)
			.HeightOverride(29.0f)
			[
				SNew(SButton)
					.ButtonStyle(&Button(
						bSelected
							? TEXT("Backlot.Button.CaptureTypeSelected")
							: TEXT("Backlot.Button.CaptureChoice")))
					.ContentPadding(FMargin(8.0f, 0.0f))
					.OnClicked_Lambda(
						[this, Select = MoveTemp(Select)]()
						{
							Select();
							Rebuild();
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
							.Text(FText::FromString(Value))
							.TextStyle(&Text(TEXT("Backlot.Sans.11")))
							.ColorAndOpacity(
								bSelected
									? FExtendedAtlassianStyle::FromHex(TEXT("#cfe0ff"))
									: FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
					]
			];
	};
	const auto CapturePriorityChoice = [this](
		const FString& Value,
		const FString& Selected,
		TFunction<void()> Select) -> TSharedRef<SWidget>
	{
		const bool bSelected = Value == Selected;
		return SNew(SBox)
			.HeightOverride(29.0f)
			[
				SNew(SButton)
					.ButtonStyle(&Button(
						bSelected
							? TEXT("Backlot.Button.CapturePrioritySelected")
							: TEXT("Backlot.Button.CaptureChoice")))
					.ContentPadding(FMargin(5.0f, 0.0f))
					.OnClicked_Lambda(
						[this, Select = MoveTemp(Select)]()
						{
							Select();
							Rebuild();
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
							.Text(FText::FromString(Value))
							.TextStyle(&Text(TEXT("Backlot.Sans.11")))
							.ColorAndOpacity(
								bSelected
									? FExtendedAtlassianStyle::FromHex(TEXT("#f0a9a4"))
									: FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
					]
			];
	};
	const auto CaptureToolChoice = [this](
		const FString& Value,
		const FString& Selected,
		TFunction<void()> Select) -> TSharedRef<SWidget>
	{
		const bool bSelected = Value == Selected;
		return SNew(SBox)
			.HeightOverride(24.0f)
			[
				SNew(SButton)
					.ButtonStyle(&Button(
						bSelected
							? TEXT("Backlot.Button.CaptureToolSelected")
							: TEXT("Backlot.Button.CaptureTool")))
					.ContentPadding(FMargin(10.0f, 0.0f))
					.OnClicked_Lambda(
						[this, Select = MoveTemp(Select)]()
						{
							Select();
							Rebuild();
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
							.Text(FText::FromString(Value))
							.TextStyle(&Text(TEXT("Backlot.Mono.10")))
							.ColorAndOpacity(
								bSelected
									? FExtendedAtlassianStyle::FromHex(TEXT("#d7dce3"))
									: FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
					]
			];
	};
	const FText AnnotationCountText = FText::Format(
		CaptureAnnotations.Num() == 1
			? LOCTEXT("OneAnnotationPlaced", "{0} ANNOTATION PLACED")
			: LOCTEXT("ManyAnnotationsPlaced", "{0} ANNOTATIONS PLACED"),
		FText::AsNumber(CaptureAnnotations.Num()));
	const FText CaptureToolHint =
		CaptureTool == TEXT("BOX")
			? LOCTEXT("CaptureBoxHint", "CLICK THE FRAME TO DROP A BOX")
			: CaptureTool == TEXT("BLUR")
				? LOCTEXT("CaptureBlurHint", "CLICK THE FRAME TO MASK AN AREA")
				: LOCTEXT(
					"CapturePinHint",
					"CLICK THE FRAME TO PIN · CLICK A MARK TO REMOVE");
	FLinearColor CaptureEpicColor =
		FExtendedAtlassianStyle::FromHex(TEXT("#b6a9ff"));
	for (const FExtendedAtlassianEpic& Epic : Controller->GetSnapshot().Epics)
	{
		if (Epic.Name == CaptureEpic && !Epic.Color.IsEmpty())
		{
			CaptureEpicColor = FExtendedAtlassianStyle::FromHex(*Epic.Color);
			break;
		}
	}

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
				.BorderImage(Brush(TEXT("Backlot.Brush.Scrim")))
				.Padding(0.0f)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
				.WidthOverride(TAttribute<FOptionalSize>::CreateSP(
					this,
					&SExtendedAtlassianWorkspace::CaptureWidth))
				.MaxDesiredHeight(TAttribute<FOptionalSize>::CreateSP(
					this,
					&SExtendedAtlassianWorkspace::CaptureMaxHeight))
				[
					SNew(SBorder)
						.BorderImage(Brush(TEXT("Backlot.Brush.Modal")))
						.Padding(0.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBox)
									.HeightOverride(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.CommandHeader.Height")))
									[
										SNew(SBorder)
											.BorderImage(Brush(TEXT("Backlot.Brush.Panel")))
											.Padding(FMargin(16.0f, 0.0f))
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot()
												.AutoWidth()
												.VAlign(VAlign_Center)
												.Padding(0.0f, 0.0f, 11.0f, 0.0f)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("CaptureDiamond", "◆"))
														.TextStyle(&Text(TEXT("Backlot.Mono.10.Medium")))
														.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")))
												]
												+ SHorizontalBox::Slot()
												.AutoWidth()
												.VAlign(VAlign_Center)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("CaptureTitle", "New issue from capture"))
														.TextStyle(&Text(TEXT("Backlot.Sans.13.Medium")))
												]
												+ SHorizontalBox::Slot()
												.AutoWidth()
												.VAlign(VAlign_Center)
												.Padding(11.0f, 0.0f, 0.0f, 0.0f)
												[
													SNew(STextBlock)
														.Text(LOCTEXT("CaptureShortcut", "CTRL+SHIFT+B"))
														.TextStyle(&Text(TEXT("Backlot.Mono.10")))
														.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
												]
												+ SHorizontalBox::Slot()
												.FillWidth(1.0f)
												[
													SNullWidget::NullWidget
												]
												+ SHorizontalBox::Slot()
												.AutoWidth()
												.VAlign(VAlign_Center)
												[
													SNew(SButton)
														.ButtonStyle(&Button(TEXT("Backlot.Button.Ghost")))
														.ContentPadding(FMargin(6.0f, 4.0f))
														.OnClicked(this, &SExtendedAtlassianWorkspace::OnCancelCapture)
														[
															SNew(STextBlock)
																.Text(LOCTEXT("CloseCapture", "✕"))
																.TextStyle(&Text(TEXT("Backlot.Mono.13")))
																.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")))
														]
												]
											]
									]
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(16.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(SBox)
											.WidthOverride(528.0f)
											.HeightOverride(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.Capture.FrameHeight")))
											[
												SNew(SOverlay)
												+ SOverlay::Slot()
												[
													SNew(SBorder)
														.BorderImage(Brush(TEXT("Backlot.Brush.CaptureFrame")))
														.Padding(1.0f)
														[
															SNew(SBacklotDiagonalPattern)
																.ColorA(FExtendedAtlassianStyle::FromHex(TEXT("#22262c")))
																.ColorB(FExtendedAtlassianStyle::FromHex(TEXT("#1c2025")))
																.StripeWidth(9.0f)
																.HAlign(HAlign_Center)
																.VAlign(VAlign_Center)
																[
																	SNew(STextBlock)
																		.Text(FText::Format(
																			LOCTEXT(
																				"ViewportCaptured",
																				"VIEWPORT CAPTURE\n{0} × {1}"),
																			FText::AsNumber(CapturedViewportSize.X),
																			FText::AsNumber(CapturedViewportSize.Y)))
																		.Justification(ETextJustify::Center)
																		.TextStyle(&Text(TEXT("Backlot.Mono.10")))
																		.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#4d545e")))
																]
														]
												]
												+ SOverlay::Slot()
												[
													SNew(SCaptureAnnotationSurface)
														.Annotations(&CaptureAnnotations)
														.PreviewBrush(
															Controller->IsFixtureProvider()
																? nullptr
																: CapturedViewportBrush.Get())
														.ActiveTool_Lambda(
															[this]()
															{
																return CaptureTool;
															})
														.OnChanged_Lambda(
															[this]()
															{
																Rebuild();
															})
												]
												+ SOverlay::Slot()
												.HAlign(HAlign_Left)
												.VAlign(VAlign_Bottom)
												.Padding(10.0f)
												[
													SNew(SHorizontalBox)
													+ SHorizontalBox::Slot()
													.AutoWidth()
													[
														CaptureToolChoice(
															TEXT("PIN"),
															CaptureTool,
															[this]() { CaptureTool = TEXT("PIN"); })
													]
													+ SHorizontalBox::Slot()
													.AutoWidth()
													.Padding(6.0f, 0.0f)
													[
														CaptureToolChoice(
															TEXT("BOX"),
															CaptureTool,
															[this]() { CaptureTool = TEXT("BOX"); })
													]
													+ SHorizontalBox::Slot()
													.AutoWidth()
													[
														CaptureToolChoice(
															TEXT("BLUR"),
															CaptureTool,
															[this]() { CaptureTool = TEXT("BLUR"); })
													]
												]
											]
									]
									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(0.0f, 12.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										[
											SNew(STextBlock)
												.Text(AnnotationCountText)
												.TextStyle(&Text(TEXT("Backlot.Mono.10")))
												.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
										]
										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										.Padding(10.0f, 0.0f)
										[
											SNew(STextBlock)
												.Text(LOCTEXT("CaptureHintSeparator", "·"))
												.TextStyle(&Text(TEXT("Backlot.Mono.10")))
												.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
										]
										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										[
											SNew(STextBlock)
												.Text(CaptureToolHint)
												.TextStyle(&Text(TEXT("Backlot.Mono.10")))
												.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
										]
									]
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(16.0f, 0.0f, 17.0f, 0.0f)
								[
									SNew(SSeparator)
										.Orientation(Orient_Vertical)
										.Thickness(1.0f)
										.SeparatorImage(Brush(TEXT("Backlot.Brush.BorderSubtle")))
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(SBox)
										.WidthOverride(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.PinPopover.Width")))
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot()
											.AutoHeight()
											[
												SectionLabel(LOCTEXT("CaptureSummaryLabel", "SUMMARY"))
											]
											+ SVerticalBox::Slot()
											.AutoHeight()
											.Padding(0.0f, 6.0f, 0.0f, 16.0f)
											[
												SNew(SEditableTextBox)
													.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("Backlot.Field")))
													.HintText(LOCTEXT("CaptureSummaryHint", "What is wrong?"))
													.Text(FText::FromString(CaptureTitle))
													.OnTextChanged(this, &SExtendedAtlassianWorkspace::OnCaptureTitleChanged)
											]
											+ SVerticalBox::Slot()
											.AutoHeight()
											[
												SectionLabel(LOCTEXT("CaptureTypeLabel", "TYPE"))
											]
											+ SVerticalBox::Slot()
											.AutoHeight()
											.Padding(0.0f, 6.0f, 0.0f, 16.0f)
											[
												SNew(SUniformGridPanel)
												.SlotPadding(FMargin(3.0f, 0.0f))
												+ SUniformGridPanel::Slot(0, 0)
												[
													CaptureChoice(TEXT("Bug"), CaptureType, [this]() { CaptureType = TEXT("Bug"); })
												]
												+ SUniformGridPanel::Slot(1, 0)
												[
													CaptureChoice(TEXT("Task"), CaptureType, [this]() { CaptureType = TEXT("Task"); })
												]
												+ SUniformGridPanel::Slot(2, 0)
												[
													CaptureChoice(TEXT("Doc"), CaptureType, [this]() { CaptureType = TEXT("Doc"); })
												]
											]
											+ SVerticalBox::Slot()
											.AutoHeight()
											[
												SectionLabel(LOCTEXT("CapturePriorityLabel", "PRIORITY"))
											]
											+ SVerticalBox::Slot()
											.AutoHeight()
											.Padding(0.0f, 6.0f, 0.0f, 16.0f)
											[
												SNew(SUniformGridPanel)
												.SlotPadding(FMargin(3.0f, 0.0f))
												+ SUniformGridPanel::Slot(0, 0)
												[
													CapturePriorityChoice(TEXT("Highest"), CapturePriority, [this]() { CapturePriority = TEXT("Highest"); })
												]
												+ SUniformGridPanel::Slot(1, 0)
												[
													CapturePriorityChoice(TEXT("High"), CapturePriority, [this]() { CapturePriority = TEXT("High"); })
												]
												+ SUniformGridPanel::Slot(2, 0)
												[
													CapturePriorityChoice(TEXT("Medium"), CapturePriority, [this]() { CapturePriority = TEXT("Medium"); })
												]
												+ SUniformGridPanel::Slot(3, 0)
												[
													CapturePriorityChoice(TEXT("Low"), CapturePriority, [this]() { CapturePriority = TEXT("Low"); })
												]
											]
											+ SVerticalBox::Slot()
											.AutoHeight()
											[
												SectionLabel(LOCTEXT("CaptureEpicLabel", "EPIC"))
											]
											+ SVerticalBox::Slot()
											.AutoHeight()
											.Padding(0.0f, 6.0f, 0.0f, 0.0f)
											[
												SNew(SBox)
													.HeightOverride(32.0f)
													[
														SNew(SButton)
															.ButtonStyle(&Button(TEXT("Backlot.Button.CaptureChoice")))
															.ContentPadding(FMargin(10.0f, 0.0f))
															.OnClicked_Lambda(
																[this]()
																{
																	TArray<FWorkspaceMenuItem> Items;
																	for (const FExtendedAtlassianEpic& Epic :
																		Controller->GetSnapshot().Epics)
																		{
																			const FLinearColor EpicColor =
																				Epic.Color.IsEmpty()
																					? FExtendedAtlassianStyle::FromHex(
																						TEXT("#b6a9ff"))
																					: FExtendedAtlassianStyle::FromHex(
																						*Epic.Color);
																			Items.Add({
																				FText::FromString(Epic.Name),
																				EpicColor,
																				CaptureEpic == Epic.Name
																					? TEXT("●")
																					: FString(),
																				[this, Name = Epic.Name]()
																				{
																					CaptureEpic = Name;
																					Rebuild();
																				}
																			});
																		}
																		if (Items.IsEmpty())
																		{
																			Items.Add({
																				FText::FromString(CaptureEpic),
																				FExtendedAtlassianStyle::FromHex(
																					TEXT("#b6a9ff")),
																				TEXT("●"),
																				[]() {}
																			});
																		}
																		OpenMenu(
																			LOCTEXT("CaptureEpicMenu", "EPIC"),
																			MoveTemp(Items));
																		return FReply::Handled();
																	})
															[
																SNew(SHorizontalBox)
																+ SHorizontalBox::Slot()
																.AutoWidth()
																.VAlign(VAlign_Center)
																[
																	SNew(SBox)
																		.WidthOverride(3.0f)
																		.HeightOverride(13.0f)
																		[
																			SNew(SBorder)
																				.BorderImage(Brush(TEXT("Backlot.Brush.BlueSolid")))
																				.BorderBackgroundColor(CaptureEpicColor)
																		]
																]
																+ SHorizontalBox::Slot()
																.AutoWidth()
																.VAlign(VAlign_Center)
																.Padding(9.0f, 0.0f)
																[
																	SNew(STextBlock)
																		.Text(FText::FromString(CaptureEpic))
																		.TextStyle(&Text(TEXT("Backlot.Sans.12")))
																		.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#d7dce3")))
																]
																+ SHorizontalBox::Slot()
																.FillWidth(1.0f)
																[
																	SNullWidget::NullWidget
																]
																+ SHorizontalBox::Slot()
																.AutoWidth()
																.VAlign(VAlign_Center)
																[
																	SNew(SImage)
																		.Image(Brush(TEXT("Backlot.Icon.CaretDown")))
																		.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
																]
															]
													]
											]
											+ SVerticalBox::Slot()
											.FillHeight(1.0f)
											[
												SNullWidget::NullWidget
											]
											+ SVerticalBox::Slot()
											.AutoHeight()
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot()
												.AutoWidth()
												.Padding(0.0f, 0.0f, 8.0f, 0.0f)
												[
													SNew(SBox)
														.HeightOverride(32.0f)
														[
															SNew(SButton)
																.ButtonStyle(&Button(TEXT("Backlot.Button.CaptureChoice")))
																.ContentPadding(FMargin(14.0f, 0.0f))
																.OnClicked(this, &SExtendedAtlassianWorkspace::OnCancelCapture)
																[
																	SNew(STextBlock)
																		.Text(LOCTEXT("CancelCapture", "Cancel"))
																		.TextStyle(&Text(TEXT("Backlot.Sans.12")))
																		.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#a2a9b4")))
																]
														]
												]
												+ SHorizontalBox::Slot()
												.FillWidth(1.0f)
												[
													SNew(SBox)
														.HeightOverride(32.0f)
														[
															SNew(SButton)
																.ButtonStyle(&Button(TEXT("Backlot.Button.Primary")))
																.ContentPadding(FMargin(14.0f, 0.0f))
																.OnClicked(this, &SExtendedAtlassianWorkspace::OnCreateCapture)
																[
																	SNew(STextBlock)
																		.Text(LOCTEXT("CreateKeepWorking", "Create & keep working"))
																		.TextStyle(&Text(TEXT("Backlot.Sans.12.Medium")))
																		.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#cfe0ff")))
																]
														]
												]
											]
										]
								]
							]
						]
				]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::BuildToast()
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FExtendedAtlassianToast& Toast = Controller->GetToast();
	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.Toast")))
		.Padding(FMargin(14.0f, 9.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(Toast.Message)
					.TextStyle(&Text(TEXT("Backlot.Sans.12.Medium")))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(16.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
					.Visibility(Toast.bOffersUndo ? EVisibility::Visible : EVisibility::Collapsed)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Ghost")))
					.ContentPadding(FMargin(8.0f, 3.0f))
					.OnClicked(this, &SExtendedAtlassianWorkspace::OnUndoToast)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("UndoToast", "UNDO"))
							.TextStyle(&Text(TEXT("Backlot.Mono.10.Medium")))
							.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")))
					]
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::SectionLabel(const FText& Label) const
{
	return SNew(STextBlock)
		.Text(Label)
		.TextStyle(&ExtendedAtlassianWorkspacePrivate::Text(TEXT("Backlot.Mono.10.Medium")))
		.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")));
}

TSharedRef<SWidget> SExtendedAtlassianWorkspace::EmptyState(const FText& Label) const
{
	return SNew(SBorder)
		.BorderImage(ExtendedAtlassianWorkspacePrivate::Brush(TEXT("Backlot.Brush.Workspace")))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
				.Text(Label)
				.TextStyle(&ExtendedAtlassianWorkspacePrivate::Text(TEXT("Backlot.Mono.10.Medium")))
		];
}

FReply SExtendedAtlassianWorkspace::OnNavigate(EExtendedAtlassianWorkspaceRoute Route)
{
	if (bDocumentEditing && Route != EExtendedAtlassianWorkspaceRoute::Docs)
	{
		ResetDocumentEditState();
	}
	Controller->Navigate(Route);
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnStartDocumentEdit()
{
	if (const FExtendedAtlassianPage* Page = SelectedPage())
	{
		if (Controller->GetSnapshot().bRefreshing
			|| (!Controller->IsFixtureProvider() && Page->Version <= 0))
		{
			return FReply::Handled();
		}
		EditingDocumentPageId = Page->Id;
		DocumentDraftTitle = Page->Title;
		DocumentDraftMarkdown = Page->Markdown;
		if (DocumentDraftMarkdown.IsEmpty())
		{
			DocumentDraftMarkdown =
				FExtendedAtlassianMarkdown::FromBlocks(Page->Blocks);
		}
		bDocumentEditing = true;
		bDocumentPublishPending = false;
		DocumentExternalChangeWarning = FText::GetEmpty();
		PrepareDocumentWorkingCopy(*Page);
		Rebuild();
	}
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnCancelDocumentEdit()
{
	ResetDocumentEditState();
	Rebuild();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnPublishDocumentEdit()
{
	const FExtendedAtlassianPage* Page = SelectedPage();
	if (!Page
		|| Page->Id != EditingDocumentPageId
		|| !Page->bCanRoundTrip
		|| bDocumentPublishPending)
	{
		return FReply::Handled();
	}

	if (DocumentEditor.IsValid())
	{
		DocumentDraftMarkdown = DocumentEditor->GetMarkdown();
	}
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::UpdatePage;
	Mutation.TargetId = Page->Id;
	Mutation.Fields.Add(
		TEXT("title"),
		DocumentDraftTitle.TrimStartAndEnd().IsEmpty()
			? FString(TEXT("Untitled page"))
			: DocumentDraftTitle.TrimStartAndEnd());
	Mutation.Fields.Add(TEXT("body"), DocumentDraftMarkdown);
	// Providers take the base version and atomically write base + 1.
	Mutation.Fields.Add(TEXT("version"), FString::FromInt(Page->Version));
	bDocumentPublishPending = true;
	Controller->ExecuteMutation(Mutation);
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnToggleCompact()
{
	Controller->ToggleCompact();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnToggleRail()
{
	Controller->ToggleRail();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnRefresh()
{
	Controller->Refresh();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnOpenSelectedIssue()
{
	if (!Controller->GetSelectedIssueKey().IsEmpty())
	{
		Controller->OpenIssue(Controller->GetSelectedIssueKey());
	}
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnStartIssueEdit()
{
	if (const FExtendedAtlassianIssue* Issue = SelectedIssue())
	{
		EditingIssueKey = Issue->Key;
		IssueDraftSummary = Issue->Summary;
		IssueDraftDescription = Issue->Description;
		IssueEditBaseUpdated = Issue->Updated;
		IssueEditConflictWarning = FText::GetEmpty();
		bIssueEditing = true;
		Rebuild();
	}
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnCancelIssueEdit()
{
	bIssueEditing = false;
	EditingIssueKey.Reset();
	IssueDraftSummary.Reset();
	IssueDraftDescription.Reset();
	PendingIssueDraftSummary.Reset();
	PendingIssueDraftDescription.Reset();
	IssueEditBaseUpdated = FDateTime::MinValue();
	IssueEditConflictWarning = FText::GetEmpty();
	Rebuild();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnSaveIssueEdit()
{
	const FExtendedAtlassianIssue* Issue = SelectedIssue();
	if (!Issue)
	{
		return FReply::Handled();
	}
	if (!IssueEditConflictWarning.IsEmpty())
	{
		Controller->ShowToast(IssueEditConflictWarning);
		return FReply::Handled();
	}
	const FString Summary = IssueDraftSummary.TrimStartAndEnd();
	if (Summary.IsEmpty())
	{
		Controller->ShowToast(
			LOCTEXT("IssueSummaryRequired", "Issue summary cannot be empty"));
		return FReply::Handled();
	}
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::UpdateIssue;
	Mutation.TargetId = Issue->Key;
	Mutation.Fields.Add(TEXT("summary"), Summary);
	Mutation.Fields.Add(TEXT("description"), IssueDraftDescription);
	Mutation.Fields.Add(TEXT("baseUpdated"), IssueEditBaseUpdated.ToIso8601());
	PendingIssueDraftSummary = IssueDraftSummary;
	PendingIssueDraftDescription = IssueDraftDescription;
	bIssueEditMutationPending = true;
	Controller->ExecuteMutation(Mutation);
	bIssueEditing = false;
	IssueDraftSummary.Reset();
	IssueDraftDescription.Reset();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnPostIssueComment()
{
	const FExtendedAtlassianIssue* Issue = SelectedIssue();
	const FString Body = NewIssueCommentDraft.TrimStartAndEnd();
	if (!Issue || Body.IsEmpty())
	{
		return FReply::Handled();
	}
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::CreateIssueComment;
	Mutation.TargetId = FString::Printf(
		TEXT("backlot-comment-%d"),
		NextLocalCommentNumber++);
	Mutation.Fields.Add(TEXT("target"), TEXT("issue:") + Issue->Key);
	Mutation.Fields.Add(
		TEXT("body"),
		bIssueCommentAttachCapture
			? Body + TEXT("\n\n[viewport capture attached]")
			: Body);
	if (bIssueCommentAttachCapture)
	{
		FIntPoint CaptureSize;
		HostServices->CaptureViewport(
			Mutation.AttachmentBytes,
			CaptureSize);
		Mutation.Fields.Add(
			TEXT("captureSize"),
			FString::Printf(TEXT("%dx%d"), CaptureSize.X, CaptureSize.Y));
	}
	PendingIssueCommentDraft = NewIssueCommentDraft;
	bPendingIssueCommentAttachCapture = bIssueCommentAttachCapture;
	bIssueComposerMutationPending = true;
	Controller->ExecuteMutation(Mutation);
	NewIssueCommentDraft.Reset();
	bIssueCommentAttachCapture = false;
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnOpenCapture()
{
	if (CaptureEpic.IsEmpty()
		&& !Controller->GetSnapshot().Epics.IsEmpty())
	{
		CaptureEpic = Controller->GetSnapshot().Epics[0].Name;
	}
	if (CapturedViewportBrush.IsValid())
	{
		CapturedViewportBrush->ReleaseResource();
		CapturedViewportBrush.Reset();
	}
	HostServices->CaptureViewport(
		CapturedViewportPng,
		CapturedViewportSize);
	CapturedViewportBrush =
		FExtendedAtlassianScreenshot::CreatePreviewBrush(
			CapturedViewportPng,
			FVector2D(528.0f, 288.0f));
	bCaptureOpen = true;
	Rebuild();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnCancelCapture()
{
	// Preserve the prototype quirk: only the title resets; tools, choices, and annotations persist.
	CaptureTitle.Reset();
	bCaptureOpen = false;
	if (CapturedViewportBrush.IsValid())
	{
		CapturedViewportBrush->ReleaseResource();
		CapturedViewportBrush.Reset();
	}
	Rebuild();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnCreateCapture()
{
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::CreateCaptureIssue;
	FString CreatedKey;
	if (Controller->IsFixtureProvider())
	{
		CreatedKey =
			ExtendedAtlassianWorkspacePrivate::NextIssueKey(
				Controller->GetSnapshot());
		Mutation.TargetId = CreatedKey;
	}
	Mutation.Fields.Add(TEXT("summary"), CaptureTitle);
	Mutation.Fields.Add(TEXT("type"), CaptureType);
	Mutation.Fields.Add(TEXT("priority"), CapturePriority);
	Mutation.Fields.Add(TEXT("epic"), CaptureEpic);
	Mutation.Fields.Add(TEXT("status"), TEXT("Triage"));
	Mutation.Fields.Add(TEXT("assignee"), TEXT("AK"));
	Mutation.Fields.Add(TEXT("points"), TEXT("3"));
	Mutation.Fields.Add(TEXT("annotationTool"), CaptureTool);
	Mutation.Fields.Add(TEXT("annotationCount"), FString::FromInt(CaptureAnnotations.Num()));
	int32 PinCount = 0;
	int32 BoxCount = 0;
	int32 BlurCount = 0;
	for (const FExtendedAtlassianAnnotation& Annotation : CaptureAnnotations)
	{
		switch (Annotation.Kind)
		{
		case EExtendedAtlassianAnnotationKind::Pin: ++PinCount; break;
		case EExtendedAtlassianAnnotationKind::Box: ++BoxCount; break;
		case EExtendedAtlassianAnnotationKind::Blur: ++BlurCount; break;
		}
	}
	Mutation.Fields.Add(TEXT("pinCount"), FString::FromInt(PinCount));
	Mutation.Fields.Add(TEXT("boxCount"), FString::FromInt(BoxCount));
	Mutation.Fields.Add(TEXT("blurCount"), FString::FromInt(BlurCount));
	Mutation.Fields.Add(
		TEXT("annotationsJson"),
		FExtendedAtlassianScreenshot::SerializeAnnotations(
			CaptureAnnotations));
	FExtendedAtlassianCapturedContext Context =
		HostServices->CaptureContext();
	if (const UExtendedAtlassianSettings* Settings =
		UExtendedAtlassianSettings::Get())
	{
		if (!Settings->bCaptureCameraTransform)
		{
			Context.bHasCamera = false;
		}
		if (!Settings->bCaptureSelectedActors)
		{
			Context.SelectedActors.Reset();
			Context.SelectedActorOverflow = 0;
		}
		if (!Settings->bCaptureSourceControlRevision)
		{
			Context.GitBranch.Reset();
			Context.GitCommit.Reset();
		}
		FString ContextBlock = Context.ToContextBlock();
		if (Settings->bCaptureLogTail)
		{
			const FString LogTail =
				FExtendedAtlassianContextCapture::ReadLogTail(
					Settings->LogTailKilobytes);
			if (!LogTail.IsEmpty())
			{
				ContextBlock += TEXT("\n\nEditor log tail:\n") + LogTail;
			}
		}
		Mutation.Fields.Add(TEXT("context"), MoveTemp(ContextBlock));
	}
	if (!FExtendedAtlassianScreenshot::BurnAnnotations(
		CapturedViewportPng,
		CapturedViewportSize,
		CaptureAnnotations,
		Mutation.AttachmentBytes))
	{
		Mutation.AttachmentBytes = CapturedViewportPng;
	}
	bCaptureMutationPending = !Controller->IsFixtureProvider();
	PendingCaptureSummary = CaptureTitle;
	PendingCaptureAnnotationCount = PinCount + BoxCount + BlurCount;
	Controller->ExecuteMutation(Mutation);
	if (!Controller->IsFixtureProvider()
		&& Controller->GetLastMutationError().IsSet()
		&& !bCaptureMutationPending)
	{
		return FReply::Handled();
	}

	CaptureTitle.Reset();
	bCaptureOpen = false;
	if (Controller->IsFixtureProvider())
	{
		CaptureAnnotations.Reset();
	}
	if (CapturedViewportBrush.IsValid())
	{
		CapturedViewportBrush->ReleaseResource();
		CapturedViewportBrush.Reset();
	}
	Controller->ResetIssueFilters();
	if (!CreatedKey.IsEmpty())
	{
		Controller->SelectIssue(CreatedKey);
	}
	Controller->Navigate(EExtendedAtlassianWorkspaceRoute::Issues);
	if (Controller->IsFixtureProvider())
	{
		Controller->ShowToast(FText::Format(
			PinCount + BoxCount + BlurCount == 1
				? LOCTEXT(
					"CaptureCreatedOneToast",
					"Issue created with {0} annotation  {1}")
				: LOCTEXT(
					"CaptureCreatedManyToast",
					"Issue created with {0} annotations  {1}"),
			FText::AsNumber(PinCount + BoxCount + BlurCount),
			FText::FromString(CreatedKey)));
	}
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnUndoToast()
{
	Controller->UndoLastDestructiveMutation();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnDismissPagePopover()
{
	ClosePagePopover();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnDismissPinPopover()
{
	bPinPopoverOpen = false;
	PinPopoverTargetId.Reset();
	PinPopoverName.Reset();
	Rebuild();
	RestoreOverlayFocus();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnDismissCreateCard()
{
	FSlateApplication::Get().DismissAllMenus();
	bMenuOpen = false;
	bCreateCardOpen = false;
	CreateCardEpicSearch.Reset();
	CreateCardEpicRows.Reset();
	MenuItems.Reset();
	Rebuild();
	RestoreOverlayFocus();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnSubmitCreateCard()
{
	if (!bCreateCardOpen)
	{
		return FReply::Handled();
	}
	const FString Summary = CreateCardSummary.TrimStartAndEnd().IsEmpty()
		? TEXT("Untitled card")
		: CreateCardSummary.TrimStartAndEnd();
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = bCreateCardEdit
		? EExtendedAtlassianWorkspaceMutation::UpdateIssue
		: EExtendedAtlassianWorkspaceMutation::CreateIssue;
	Mutation.TargetId = CreateCardTargetKey;
	Mutation.Fields.Add(TEXT("summary"), Summary);
	Mutation.Fields.Add(TEXT("type"), CreateCardType);
	Mutation.Fields.Add(TEXT("priority"), CreateCardPriority);
	Mutation.Fields.Add(TEXT("assignee"), CreateCardAssignee);
	Mutation.Fields.Add(TEXT("epic"), CreateCardEpic);
	Mutation.Fields.Add(TEXT("status"), CreateCardStatus);
	Mutation.Fields.Add(TEXT("points"), TEXT("3"));
	const bool bWasEdit = bCreateCardEdit;
	Mutation.bOpenResultOnSuccess = !bWasEdit;
	const FString ResultKey = CreateCardTargetKey;
	FSlateApplication::Get().DismissAllMenus();
	bCreateCardOpen = false;
	bMenuOpen = false;
	CreateCardEpicSearch.Reset();
	CreateCardEpicRows.Reset();
	Controller->ExecuteMutation(Mutation);
	if (bWasEdit)
	{
		Controller->SelectIssue(ResultKey);
		Controller->ShowToast(FText::Format(
			LOCTEXT("CardUpdatedToast", "Card updated  {0}"),
			FText::FromString(ResultKey)));
	}
	else
	{
		Controller->ShowToast(FText::Format(
			LOCTEXT("CardAddedToast", "Card added to {0}"),
			FText::FromString(CreateCardStatus)));
	}
	RestoreOverlayFocus();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnDeleteCreateCard()
{
	if (!bCreateCardEdit || CreateCardTargetKey.IsEmpty())
	{
		return FReply::Handled();
	}
	const FString Key = CreateCardTargetKey;
	OpenConfirm(
		FText::Format(
			LOCTEXT("DeleteCardConfirmTitle", "Delete {0}?"),
			FText::FromString(Key)),
		LOCTEXT(
			"DeleteCardConfirmBody",
			"This removes the card from the board and the backlog. This cannot be undone."),
		LOCTEXT("DeleteCardConfirmAccept", "Delete card"),
		[this, Key]()
		{
			bCreateCardOpen = false;
			FExtendedAtlassianWorkspaceMutation Mutation;
			Mutation.Type = EExtendedAtlassianWorkspaceMutation::DeleteIssue;
			Mutation.TargetId = Key;
			Controller->ExecuteDestructiveMutation(
				Mutation,
				FText::Format(
					LOCTEXT("CardDeletedUndo", "Card deleted  {0}"),
					FText::FromString(Key)));
		});
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnSubmitPagePopover()
{
	if (!bPagePopoverOpen)
	{
		return FReply::Handled();
	}

	const FString TrimmedTitle = PagePopoverTitle.TrimStartAndEnd();
	if (bPagePopoverRename)
	{
		if (TrimmedTitle.IsEmpty())
		{
			return FReply::Handled();
		}
		FExtendedAtlassianWorkspaceMutation Mutation;
		Mutation.Type = bPagePopoverTargetSection
			? EExtendedAtlassianWorkspaceMutation::RenameSection
			: EExtendedAtlassianWorkspaceMutation::UpdatePage;
		Mutation.TargetId = PagePopoverTargetId;
		Mutation.Fields.Add(TEXT("title"), TrimmedTitle);
		ClosePagePopover(false);
		Controller->ExecuteMutation(Mutation);
		Controller->ShowToast(FText::Format(
			LOCTEXT("RenamedToast", "Renamed  {0}"),
			FText::FromString(TrimmedTitle.ToUpper())));
		RestoreOverlayFocus();
		return FReply::Handled();
	}

	const FString NewId = MakeUniqueDocumentId(TEXT("new"));
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::CreatePage;
	Mutation.TargetId = NewId;
	Mutation.ParentId = PagePopoverParentId;
	Mutation.Fields.Add(
		TEXT("title"),
		TrimmedTitle.IsEmpty() ? TEXT("Untitled page") : TrimmedTitle);
	const FString Location = PagePopoverParentId.IsEmpty()
		? ExtendedAtlassianWorkspacePrivate::SelectedDocumentSection(
			Controller->GetSnapshot(),
			Controller->GetSelectedPageId()).ToUpper()
		: [&]()
		{
			if (const FExtendedAtlassianDocumentTreeNode* Parent =
				Controller->GetSnapshot().DocumentTree.FindByPredicate(
					[this](const FExtendedAtlassianDocumentTreeNode& Node)
					{
						return Node.Id == PagePopoverParentId;
					}))
			{
				return Parent->Label.ToUpper();
			}
			return ExtendedAtlassianWorkspacePrivate::SelectedDocumentSection(
				Controller->GetSnapshot(),
				Controller->GetSelectedPageId()).ToUpper();
		}();
	ClosePagePopover(false);
	Controller->ExecuteMutation(Mutation);
	Controller->SelectPage(NewId);
	Controller->SetPageSearch(FString());
	Controller->ShowToast(FText::Format(
		LOCTEXT("PageCreatedToast", "Page created  {0}"),
		FText::FromString(Location)));
	RestoreOverlayFocus();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnSubmitPinPopover()
{
	if (!bPinPopoverOpen)
	{
		return FReply::Handled();
	}
	const FString Name = PinPopoverName.TrimStartAndEnd();
	if (Name.IsEmpty()
		|| (!bPinPopoverRename
			&& !Controller->IsFixtureProvider()
			&& PinPopoverStableId.IsEmpty()))
	{
		return FReply::Handled();
	}

	FExtendedAtlassianWorkspaceMutation Mutation;
	if (bPinPopoverRename)
	{
		Mutation.Type = EExtendedAtlassianWorkspaceMutation::UpdatePin;
		Mutation.TargetId = PinPopoverTargetId;
		Mutation.Fields.Add(TEXT("name"), Name);
		bPinPopoverOpen = false;
		Controller->ExecuteMutation(Mutation);
		Controller->ShowToast(FText::Format(
			LOCTEXT("PinRenamedToast", "Pin renamed  {0}"),
			FText::FromString(Name.ToUpper())));
	}
	else
	{
		FString StableId = Controller->IsFixtureProvider()
			? Name
			: PinPopoverStableId;
		int32 Suffix = 2;
		while (Controller->IsFixtureProvider()
			&& Controller->GetSnapshot().Pins.ContainsByPredicate(
			[&StableId](const FExtendedAtlassianPin& Pin)
			{
				return Pin.Id == StableId;
			}))
		{
			StableId = FString::Printf(TEXT("%s:%d"), *Name, Suffix++);
		}
		FExtendedAtlassianPinTarget Target;
		Target.Kind = PinPopoverKind;
		Target.StableId = StableId;
		Target.DisplayName = Name;
		Target.SecondaryId = PinPopoverSecondaryId;
		const FString PinId = Controller->IsFixtureProvider()
			? StableId
			: FExtendedAtlassianBacklotStore::MakeStablePinId(Target);
		Mutation.Type = EExtendedAtlassianWorkspaceMutation::CreatePin;
		Mutation.TargetId = PinId;
		Mutation.Fields.Add(TEXT("name"), Name);
		Mutation.Fields.Add(TEXT("stableId"), StableId);
		Mutation.Fields.Add(TEXT("secondaryId"), PinPopoverSecondaryId);
		Mutation.Fields.Add(
			TEXT("kind"),
			ExtendedAtlassianWorkspacePrivate::PinKindKey(PinPopoverKind));
		Mutation.Fields.Add(
			TEXT("color"),
			ExtendedAtlassianWorkspacePrivate::PinKindColor(PinPopoverKind));
		bPinPopoverOpen = false;
		Controller->ExecuteMutation(Mutation);
		Controller->SelectPin(PinId);
		SelectedPinThreadId.Reset();
		Controller->ShowToast(FText::Format(
			LOCTEXT("PinCreatedToast", "Asset pinned  {0}"),
			FText::FromString(Name.ToUpper())));
	}
	PinPopoverTargetId.Reset();
	PinPopoverName.Reset();
	PinPopoverStableId.Reset();
	PinPopoverSecondaryId.Reset();
	PinPopoverTargetError = FText::GetEmpty();
	RestoreOverlayFocus();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnPostPinReply()
{
	const FExtendedAtlassianPin* Pin = SelectedPin();
	const FString Body = PinReplyDraft.TrimStartAndEnd();
	if (!Pin || Body.IsEmpty())
	{
		return FReply::Handled();
	}
	const FString PinId = Pin->Id;
	const FString PinName = Pin->DisplayName;
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::CreatePinReply;
	Mutation.ParentId = PinId;
	Mutation.TargetId = FString::Printf(
		TEXT("%s:thread:%d"),
		*PinId,
		NextLocalCommentNumber++);
	Mutation.Fields.Add(TEXT("body"), Body);
	const FString NewThreadId = Mutation.TargetId;
	Controller->ExecuteMutation(Mutation);
	PinReplyDraft.Reset();
	SelectedPinThreadId = NewThreadId;
	Controller->ShowToast(FText::Format(
		LOCTEXT("PinReplySentToast", "Reply sent to 3 watchers  {0}"),
		FText::FromString(PinName.ToUpper())));
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnDismissMenu()
{
	CloseMenu();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnDismissConfirm()
{
	bConfirmOpen = false;
	ConfirmAction = nullptr;
	Rebuild();
	RestoreOverlayFocus();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnAcceptConfirm()
{
	const TFunction<void()> Action = MoveTemp(ConfirmAction);
	bConfirmOpen = false;
	Rebuild();
	if (Action)
	{
		Action();
	}
	RestoreOverlayFocus();
	return FReply::Handled();
}

FReply SExtendedAtlassianWorkspace::OnCreateSection()
{
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::CreateSection;
	Mutation.TargetId = MakeUniqueDocumentId(TEXT("sec"));
	Mutation.Fields.Add(TEXT("title"), TEXT("New section"));
	Controller->ExecuteMutation(Mutation);
	Controller->ShowToast(FText::Format(
		LOCTEXT("SectionCreatedToast", "Section created  {0}"),
		FText::FromString(
			ExtendedAtlassianWorkspacePrivate::SelectedDocumentSection(
				Controller->GetSnapshot(),
				Controller->GetSelectedPageId()).ToUpper())));
	return FReply::Handled();
}

void SExtendedAtlassianWorkspace::OnPagePopoverTitleChanged(const FText& TextValue)
{
	PagePopoverTitle = TextValue.ToString();
}

void SExtendedAtlassianWorkspace::OnPinPopoverNameChanged(const FText& TextValue)
{
	PinPopoverName = TextValue.ToString();
}

void SExtendedAtlassianWorkspace::OpenCreateCard(const FString& Status)
{
	if (!bCreateCardOpen && !bPagePopoverOpen && !bPinPopoverOpen
		&& !bMenuOpen && !bConfirmOpen)
	{
		OverlayFocusReturn = FSlateApplication::Get().GetKeyboardFocusedWidget();
	}
	CreateCardTargetKey =
		ExtendedAtlassianWorkspacePrivate::NextIssueKey(
			Controller->GetSnapshot());
	CreateCardSummary.Reset();
	CreateCardType =
		Controller->GetSnapshot().IssueTypes.ContainsByPredicate(
			[](const FExtendedAtlassianIssueType& Type)
			{
				return Type.Name == TEXT("Task");
			})
			? FString(TEXT("Task"))
			: (Controller->GetSnapshot().IssueTypes.IsEmpty()
				? FString()
				: Controller->GetSnapshot().IssueTypes[0].Name);
	const UExtendedAtlassianSettings* Settings =
		UExtendedAtlassianSettings::Get();
	CreateCardPriority =
		Settings && !Settings->DefaultPriorityName.IsEmpty()
			? Settings->DefaultPriorityName
			: (Controller->GetSnapshot().Priorities.ContainsByPredicate(
				[](const FExtendedAtlassianPriority& Priority)
				{
					return Priority.Name.Equals(
						TEXT("MEDIUM"),
						ESearchCase::IgnoreCase);
				})
				? FString(TEXT("MEDIUM"))
				: (Controller->GetSnapshot().Priorities.IsEmpty()
					? FString()
					: Controller->GetSnapshot().Priorities[0].Name));
	CreateCardAssignee =
		Controller->GetSnapshot().CurrentUser.AccountId;
	CreateCardEpic.Reset();
	CreateCardEpicSearch.Reset();
	CreateCardEpicRows.Reset();
	CreateCardStatus = Status;
	bCreateCardEdit = false;
	bCreateCardOpen = true;
	CreateCardPosition = PopupPosition(FVector2D(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.CreateCard.Width")), 366.0f), true);
	Rebuild();
}

void SExtendedAtlassianWorkspace::OpenCardEdit(const FString& IssueKey)
{
	const FExtendedAtlassianIssue* Issue =
		Controller->GetSnapshot().Issues.FindByPredicate(
			[&IssueKey](const FExtendedAtlassianIssue& Candidate)
			{
				return Candidate.Key == IssueKey;
			});
	if (!Issue)
	{
		return;
	}
	CreateCardTargetKey = Issue->Key;
	CreateCardSummary = Issue->Summary;
	CreateCardType = Issue->IssueTypeName;
	CreateCardPriority = Issue->PriorityName;
	CreateCardAssignee = Issue->AssigneeAccountId;
	CreateCardEpic = Issue->EpicName;
	CreateCardEpicSearch.Reset();
	CreateCardEpicRows.Reset();
	CreateCardStatus = Issue->StatusName;
	bCreateCardEdit = true;
	bCreateCardOpen = true;
	CreateCardPosition = PopupPosition(FVector2D(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.CreateCard.Width")), 434.0f), false);
	Rebuild();
}

void SExtendedAtlassianWorkspace::DropBoardIssue(
	const FString& IssueKey,
	const FString& PresentationColumn,
	const FString& BeforeIssueKey)
{
	const FExtendedAtlassianWorkspaceSnapshot& Snapshot =
		Controller->GetSnapshot();
	const FExtendedAtlassianIssue* Issue =
		Snapshot.Issues.FindByPredicate(
			[&IssueKey](const FExtendedAtlassianIssue& Candidate)
			{
				return Candidate.Key == IssueKey;
			});
	if (!Issue)
	{
		return;
	}

	const FExtendedAtlassianBoardColumn* TargetColumn =
		Snapshot.BoardColumns.FindByPredicate(
			[&PresentationColumn](
				const FExtendedAtlassianBoardColumn& Column)
			{
				return Column.DisplayName.Equals(
					PresentationColumn,
					ESearchCase::IgnoreCase);
			});
	if (!TargetColumn)
	{
		return;
	}
	auto IsInColumn = [](
		const FExtendedAtlassianIssue& Candidate,
		const FExtendedAtlassianBoardColumn& Column)
	{
		return Column.StatusNames.ContainsByPredicate(
			[&Candidate](const FString& StatusName)
			{
				return StatusName.Equals(
					Candidate.StatusName,
					ESearchCase::IgnoreCase);
			});
	};
	const FExtendedAtlassianBoardColumn* CurrentColumn =
		Snapshot.BoardColumns.FindByPredicate(
			[&Issue, &IsInColumn](
				const FExtendedAtlassianBoardColumn& Column)
			{
				return IsInColumn(*Issue, Column);
			});
	const bool bColumnChanged =
		!CurrentColumn
			|| !CurrentColumn->DisplayName.Equals(
				PresentationColumn,
				ESearchCase::IgnoreCase);
	FString TargetStatus = Issue->StatusName;
	if (bColumnChanged)
	{
		if (const FExtendedAtlassianIssue* ExistingTarget =
			Snapshot.Issues.FindByPredicate(
				[&Issue, &TargetColumn, &IsInColumn](
					const FExtendedAtlassianIssue& Candidate)
				{
					return Candidate.Key != Issue->Key
						&& IsInColumn(Candidate, *TargetColumn)
						&& !Candidate.StatusName.Equals(
							TEXT("Blocked"),
							ESearchCase::IgnoreCase);
				}))
		{
			// An observed status is transition-safe for this configured column.
			TargetStatus = ExistingTarget->StatusName;
		}
		else if (const FString* ExactStatus =
			TargetColumn->StatusNames.FindByPredicate(
				[&PresentationColumn](const FString& StatusName)
				{
					return StatusName.Equals(
						PresentationColumn,
						ESearchCase::IgnoreCase);
				}))
		{
			TargetStatus = *ExactStatus;
		}
		else if (const FString* PreferredStatus =
			TargetColumn->StatusNames.FindByPredicate(
				[](const FString& StatusName)
				{
					return !StatusName.Equals(
						TEXT("Blocked"),
						ESearchCase::IgnoreCase);
				}))
		{
			TargetStatus = *PreferredStatus;
		}
	}
	int32 TargetCountWithoutIssue = 0;
	for (const FExtendedAtlassianIssue& Candidate : Snapshot.Issues)
	{
		TargetCountWithoutIssue +=
			Candidate.Key != IssueKey
				&& IsInColumn(Candidate, *TargetColumn)
					? 1
					: 0;
	}

	TArray<FString> OrderedIds;
	int32 InsertIndex = INDEX_NONE;
	for (const FExtendedAtlassianIssue& Candidate : Snapshot.Issues)
	{
		if (Candidate.Key == IssueKey)
		{
			continue;
		}
		if (!BeforeIssueKey.IsEmpty() && Candidate.Key == BeforeIssueKey)
		{
			InsertIndex = OrderedIds.Num();
		}
		OrderedIds.Add(Candidate.Key);
		if (BeforeIssueKey.IsEmpty()
			&& IsInColumn(Candidate, *TargetColumn))
		{
			InsertIndex = OrderedIds.Num();
		}
	}
	OrderedIds.Insert(
		IssueKey,
		InsertIndex == INDEX_NONE ? OrderedIds.Num() : InsertIndex);

	FExtendedAtlassianWorkspaceMutation Move;
	Move.Type = EExtendedAtlassianWorkspaceMutation::MoveIssue;
	Move.TargetId = IssueKey;
	Move.Fields.Add(TEXT("status"), TargetStatus);
	Move.Fields.Add(TEXT("previousStatus"), Issue->StatusName);
	Move.Fields.Add(TEXT("presentationColumn"), PresentationColumn);
	Move.OrderedIds = MoveTemp(OrderedIds);
	Controller->ExecuteMutation(Move);

	const int32 NewCount = TargetCountWithoutIssue + 1;
	const int32 WipLimit = FMath::Max(1, TargetColumn->WipLimit);
	if (PresentationColumn == TEXT("In progress")
		&& NewCount > WipLimit)
	{
		Controller->ShowToast(FText::Format(
			LOCTEXT(
				"BoardWipExceeded",
				"WIP limit exceeded — {0} in progress, limit {1}  {2}"),
			FText::AsNumber(NewCount),
			FText::AsNumber(WipLimit),
			FText::FromString(IssueKey)));
	}
	else
	{
		Controller->ShowToast(FText::Format(
			bColumnChanged
				? LOCTEXT("BoardIssueMoved", "Moved {0} to {1}")
				: LOCTEXT("BoardIssueReordered", "Reordered in {1}  {0}"),
			FText::FromString(IssueKey),
			FText::FromString(PresentationColumn)));
	}
}

void SExtendedAtlassianWorkspace::OpenStatusMenu(const FString& IssueKey)
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FExtendedAtlassianIssue* Issue =
		Controller->GetSnapshot().Issues.FindByPredicate(
			[&IssueKey](const FExtendedAtlassianIssue& Candidate)
			{
				return Candidate.Key == IssueKey;
			});
	const FString CurrentStatus =
		bCreateCardOpen && CreateCardTargetKey == IssueKey
			? CreateCardStatus
			: (Issue ? Issue->StatusName : FString(TEXT("Triage")));
	TArray<FString> Statuses;
	if (Controller->IsFixtureProvider())
	{
		Statuses = {
			TEXT("Triage"),
			TEXT("In progress"),
			TEXT("In review"),
			TEXT("Blocked"),
			TEXT("Done")
		};
	}
	else
	{
		for (const FExtendedAtlassianBoardColumn& Column :
			Controller->GetSnapshot().BoardColumns)
		{
			for (const FString& Status : Column.StatusNames)
			{
				Statuses.AddUnique(Status);
			}
		}
		if (Statuses.IsEmpty() && !CurrentStatus.IsEmpty())
		{
			Statuses.Add(CurrentStatus);
		}
	}
	TArray<FWorkspaceMenuItem> Items;
	for (const FString& Status : Statuses)
	{
		Items.Add({
			FText::FromString(Status),
			StatusColor(Status),
			CurrentStatus == Status ? TEXT("●") : FString(),
			[this, IssueKey, Status]()
			{
				if (bCreateCardOpen && CreateCardTargetKey == IssueKey)
				{
					CreateCardStatus = Status;
					Rebuild();
					return;
				}
				FExtendedAtlassianWorkspaceMutation Mutation;
				Mutation.Type = EExtendedAtlassianWorkspaceMutation::TransitionIssue;
				Mutation.TargetId = IssueKey;
				Mutation.Fields.Add(TEXT("status"), Status);
				Controller->ExecuteMutation(Mutation);
			}
		});
	}
	OpenMenu(LOCTEXT("StatusMenuTitle", "STATUS"), MoveTemp(Items), ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.InlineStatusPopup.Width")));
}

void SExtendedAtlassianWorkspace::OpenPinPopover(
	bool bRename,
	const FString& TargetId)
{
	if (!bPinPopoverOpen && !bPagePopoverOpen && !bCreateCardOpen
		&& !bMenuOpen && !bConfirmOpen && !bCaptureOpen)
	{
		OverlayFocusReturn = FSlateApplication::Get().GetKeyboardFocusedWidget();
	}
	bPinPopoverRename = bRename;
	PinPopoverTargetId = TargetId;
	PinPopoverKind = EExtendedAtlassianPinKind::Material;
	PinPopoverName.Reset();
	PinPopoverStableId.Reset();
	PinPopoverSecondaryId.Reset();
	PinPopoverTargetError = FText::GetEmpty();
	if (bRename)
	{
		if (const FExtendedAtlassianPin* Pin =
			Controller->GetSnapshot().Pins.FindByPredicate(
				[&TargetId](const FExtendedAtlassianPin& Candidate)
				{
					return Candidate.Id == TargetId;
				}))
		{
			PinPopoverName = Pin->DisplayName;
			PinPopoverKind = Pin->Target.Kind;
			PinPopoverStableId = Pin->Target.StableId;
			PinPopoverSecondaryId = Pin->Target.SecondaryId;
		}
	}
	else if (!Controller->IsFixtureProvider())
	{
		ResolvePinPopoverTarget(PinPopoverKind);
	}
	bPinPopoverOpen = true;
	PinPopoverPosition = PopupPosition(
		FVector2D(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.PinPopover.Width")), bRename ? 150.0f : 210.0f),
		false);
	Rebuild();
}

void SExtendedAtlassianWorkspace::ResolvePinPopoverTarget(
	EExtendedAtlassianPinKind Kind)
{
	PinPopoverKind = Kind;
	FString SelectedPageTitle;
	if (const FExtendedAtlassianPage* Page = SelectedPage())
	{
		SelectedPageTitle = Page->Title;
	}
	FExtendedAtlassianPinTarget Target;
	if (HostServices->ResolveCurrentTarget(
		Kind,
		Controller->GetSelectedPageId(),
		SelectedPageTitle,
		Target,
		PinPopoverTargetError))
	{
		PinPopoverStableId = Target.StableId;
		PinPopoverSecondaryId = Target.SecondaryId;
		PinPopoverName = Target.DisplayName;
	}
	else
	{
		PinPopoverStableId.Reset();
		PinPopoverSecondaryId.Reset();
		PinPopoverName.Reset();
	}
	Rebuild();
}

void SExtendedAtlassianWorkspace::RevealPinTarget(
	const FExtendedAtlassianPin& Pin)
{
	if (Pin.Target.Kind == EExtendedAtlassianPinKind::Page)
	{
		const FExtendedAtlassianPage* Page =
			Controller->GetSnapshot().Pages.FindByPredicate(
				[&Pin](const FExtendedAtlassianPage& Candidate)
				{
					return Candidate.Id == Pin.Target.StableId;
				});
		if (!Page)
		{
			Controller->ShowToast(FText::Format(
				LOCTEXT(
					"PinnedPageMissing",
					"Pinned page is unavailable  {0}"),
				FText::FromString(Pin.Target.DisplayName.ToUpper())));
			return;
		}
		Controller->SelectPage(Page->Id);
		Navigate(EExtendedAtlassianWorkspaceRoute::Docs);
		return;
	}

	FText Error;
	if (!HostServices->RevealTarget(
		Pin.Target,
		Error))
	{
		Controller->ShowToast(Error);
		return;
	}
	Controller->ShowToast(FText::Format(
		LOCTEXT("PinTargetRevealed", "Target revealed  {0}"),
		FText::FromString(Pin.DisplayName.ToUpper())));
}

void SExtendedAtlassianWorkspace::OpenPinActions(const FString& PinId)
{
	TArray<FWorkspaceMenuItem> Items;
	Items.Add({
		LOCTEXT("RenamePinAction", "Rename pin"),
		FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")),
		FString(),
		[this, PinId]() { OpenPinPopover(true, PinId); }
	});
	Items.Add({
		LOCTEXT("RemovePinAction", "Remove pin"),
		FExtendedAtlassianStyle::FromHex(TEXT("#f0665f")),
		FString(),
		[this, PinId]() { ConfirmDeletePin(PinId); }
	});
	OpenMenu(LOCTEXT("PinActionsTitle", "PIN"), MoveTemp(Items));
}

void SExtendedAtlassianWorkspace::ConfirmDeletePin(const FString& PinId)
{
	const FExtendedAtlassianPin* Pin =
		Controller->GetSnapshot().Pins.FindByPredicate(
			[&PinId](const FExtendedAtlassianPin& Candidate)
			{
				return Candidate.Id == PinId;
			});
	if (!Pin)
	{
		return;
	}
	const FString Name = Pin->DisplayName;
	OpenConfirm(
		LOCTEXT("RemovePinConfirmTitle", "Remove this pin?"),
		FText::Format(
			LOCTEXT(
				"RemovePinConfirmBody",
				"The pin on “{0}” and its threads come off the asset."),
			FText::FromString(Name)),
		LOCTEXT("RemovePinConfirmAccept", "Remove pin"),
		[this, PinId, Name]()
		{
			FExtendedAtlassianWorkspaceMutation Mutation;
			Mutation.Type = EExtendedAtlassianWorkspaceMutation::DeletePin;
			Mutation.TargetId = PinId;
			Controller->ExecuteDestructiveMutation(
				Mutation,
				FText::Format(
					LOCTEXT("PinRemovedUndo", "Pin removed  {0}"),
					FText::FromString(Name.ToUpper())));
			SelectedPinThreadId.Reset();
		});
}

void SExtendedAtlassianWorkspace::SelectPinThread(
	const FString& PinId,
	const FString& ThreadId)
{
	Controller->SelectPin(PinId);
	SelectedPinThreadId = ThreadId;
	EditingPinMessageId.Reset();
	PinMessageEditDraft.Reset();
	Rebuild();
}

void SExtendedAtlassianWorkspace::BeginPinMessageEdit(
	const FExtendedAtlassianPinThread& Thread)
{
	EditingPinMessageId = Thread.Id;
	PinMessageEditDraft = Thread.Body;
	Rebuild();
}

void SExtendedAtlassianWorkspace::SavePinMessageEdit(const FString& ThreadId)
{
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::UpdatePinReply;
	Mutation.TargetId = ThreadId;
	Mutation.Fields.Add(TEXT("body"), PinMessageEditDraft);
	Controller->ExecuteMutation(Mutation);
	EditingPinMessageId.Reset();
	PinMessageEditDraft.Reset();
	Controller->ShowToast(LOCTEXT("PinMessageUpdatedToast", "Reply updated"));
}

void SExtendedAtlassianWorkspace::CancelPinMessageEdit()
{
	EditingPinMessageId.Reset();
	PinMessageEditDraft.Reset();
	Rebuild();
}

void SExtendedAtlassianWorkspace::ConfirmDeletePinMessage(
	const FString& PinId,
	const FString& ThreadId)
{
	const FExtendedAtlassianPin* Pin =
		Controller->GetSnapshot().Pins.FindByPredicate(
			[&PinId](const FExtendedAtlassianPin& Candidate)
			{
				return Candidate.Id == PinId;
			});
	const FString Name = Pin ? Pin->DisplayName : PinId;
	OpenConfirm(
		LOCTEXT("DeletePinMessageConfirmTitle", "Delete this reply?"),
		FText::Format(
			LOCTEXT(
				"DeletePinMessageConfirmBody",
				"It comes off the thread on {0}."),
			FText::FromString(Name)),
		LOCTEXT("DeletePinMessageConfirmAccept", "Delete"),
		[this, ThreadId, Name]()
		{
			FExtendedAtlassianWorkspaceMutation Mutation;
			Mutation.Type = EExtendedAtlassianWorkspaceMutation::DeletePinReply;
			Mutation.TargetId = ThreadId;
			Controller->ExecuteDestructiveMutation(
				Mutation,
				FText::Format(
					LOCTEXT("PinMessageDeletedUndo", "Reply deleted  {0}"),
					FText::FromString(Name.ToUpper())));
			EditingPinMessageId.Reset();
			PinMessageEditDraft.Reset();
			if (SelectedPinThreadId == ThreadId)
			{
				SelectedPinThreadId.Reset();
			}
		});
}

void SExtendedAtlassianWorkspace::TogglePinThreadResolved(
	const FExtendedAtlassianPinThread& Thread)
{
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::ResolvePinReply;
	Mutation.TargetId = Thread.Id;
	Mutation.Fields.Add(
		TEXT("resolved"),
		Thread.bResolved ? TEXT("false") : TEXT("true"));
	Controller->ExecuteMutation(Mutation);
	Controller->ShowToast(
		Thread.bResolved
			? LOCTEXT("PinThreadReopenedToast", "Thread reopened")
			: LOCTEXT("PinThreadResolvedToast", "Thread resolved"));
}

void SExtendedAtlassianWorkspace::SetInboxTabAndSelect(const FString& Tab)
{
	Controller->SetInboxTab(Tab);
	const FExtendedAtlassianNotification* First =
		Controller->GetSnapshot().Notifications.FindByPredicate(
			[&Tab](const FExtendedAtlassianNotification& Notification)
			{
				return !Notification.bArchived
					&& ExtendedAtlassianWorkspacePrivate::InboxTabMatches(
						Tab,
						Notification.Kind);
			});
	Controller->SelectNotification(First ? First->Id : FString());
}

void SExtendedAtlassianWorkspace::SelectInboxNotification(
	const FString& NotificationId)
{
	Controller->SelectNotification(NotificationId);
}

void SExtendedAtlassianWorkspace::MarkAllInboxRead()
{
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::MarkAllNotificationsRead;
	Controller->ExecuteMutation(Mutation);
	Controller->ShowToast(LOCTEXT("InboxClearedToast", "Inbox cleared  ALL READ"));
}

void SExtendedAtlassianWorkspace::ArchiveReadInbox()
{
	int32 Count = 0;
	for (const FExtendedAtlassianNotification& Notification :
		Controller->GetSnapshot().Notifications)
	{
		Count += !Notification.bArchived && Notification.bRead ? 1 : 0;
	}
	if (Count == 0)
	{
		Controller->ShowToast(LOCTEXT("NothingToArchiveToast", "Nothing read to archive"));
		return;
	}
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::ArchiveNotifications;
	Controller->ExecuteDestructiveMutation(
		Mutation,
		FText::Format(
			Count == 1
				? LOCTEXT("ArchivedOneNotificationUndo", "Archived {0} notification  INBOX")
				: LOCTEXT("ArchivedManyNotificationsUndo", "Archived {0} notifications  INBOX"),
			FText::AsNumber(Count)));
	const FExtendedAtlassianNotification* First =
		Controller->GetSnapshot().Notifications.FindByPredicate(
			[](const FExtendedAtlassianNotification& Notification)
			{
				return !Notification.bArchived;
			});
	Controller->SelectNotification(First ? First->Id : FString());
}

void SExtendedAtlassianWorkspace::DismissInboxNotification(
	const FString& NotificationId)
{
	const FExtendedAtlassianNotification* Notification =
		Controller->GetSnapshot().Notifications.FindByPredicate(
			[&NotificationId](const FExtendedAtlassianNotification& Candidate)
			{
				return Candidate.Id == NotificationId;
			});
	const FString Kind = Notification
		? ExtendedAtlassianWorkspacePrivate::NotificationKindLabel(Notification->Kind)
		: FString(TEXT("NOTIFICATION"));
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::DismissNotification;
	Mutation.TargetId = NotificationId;
	Controller->ExecuteDestructiveMutation(
		Mutation,
		FText::Format(
			LOCTEXT("NotificationDismissedUndo", "Notification dismissed  {0}"),
			FText::FromString(Kind)));
}

void SExtendedAtlassianWorkspace::MarkInboxNotificationRead(
	const FString& NotificationId,
	bool bShowToast)
{
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::MarkNotificationRead;
	Mutation.TargetId = NotificationId;
	Controller->ExecuteMutation(Mutation);
	if (bShowToast)
	{
		Controller->ShowToast(LOCTEXT("NotificationReadToast", "Marked read"));
	}
}

void SExtendedAtlassianWorkspace::MuteInboxThread(const FString& NotificationId)
{
	if (Controller->IsFixtureProvider())
	{
		// Strict fixture behavior only marks the row read.
		MarkInboxNotificationRead(NotificationId, false);
	}
	else
	{
		FExtendedAtlassianWorkspaceMutation Mutation;
		Mutation.Type = EExtendedAtlassianWorkspaceMutation::MuteNotification;
		Mutation.TargetId = NotificationId;
		Controller->ExecuteMutation(Mutation);
	}
	const FExtendedAtlassianNotification* Notification =
		Controller->GetSnapshot().Notifications.FindByPredicate(
			[&NotificationId](const FExtendedAtlassianNotification& Candidate)
			{
				return Candidate.Id == NotificationId;
			});
	Controller->ShowToast(FText::Format(
		LOCTEXT("ThreadMutedToast", "Thread muted  {0}"),
		FText::FromString(Notification ? Notification->Target.ToUpper() : FString())));
}

void SExtendedAtlassianWorkspace::OpenInboxNotificationTarget(
	const FExtendedAtlassianNotification& Notification)
{
	if (Notification.SourceId == TEXT("docs"))
	{
		Controller->Navigate(EExtendedAtlassianWorkspaceRoute::Docs);
	}
	else if (Notification.SourceId == TEXT("pins"))
	{
		Controller->Navigate(EExtendedAtlassianWorkspaceRoute::Pins);
	}
	else
	{
		Controller->OpenIssue(Notification.SourceId);
	}
}

void SExtendedAtlassianWorkspace::OpenCreatePagePopover(const FString& ParentSectionId)
{
	if (!bPagePopoverOpen && !bPinPopoverOpen && !bMenuOpen && !bConfirmOpen)
	{
		OverlayFocusReturn = FSlateApplication::Get().GetKeyboardFocusedWidget();
	}
	PagePopoverParentId = ParentSectionId;
	if (PagePopoverParentId.IsEmpty())
	{
		if (const FExtendedAtlassianDocumentTreeNode* SelectedNode =
			Controller->GetSnapshot().DocumentTree.FindByPredicate(
				[this](const FExtendedAtlassianDocumentTreeNode& Node)
				{
					return Node.Id == Controller->GetSelectedPageId();
				}))
		{
			PagePopoverParentId = SelectedNode->ParentId;
		}
	}
	PagePopoverTargetId.Reset();
	PagePopoverTitle.Reset();
	bPagePopoverRename = false;
	bPagePopoverTargetSection = false;
	bPagePopoverOpen = true;
	PagePopoverPosition = PopupPosition(FVector2D(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.PagePopover.Width")), 210.0f), true);
	Rebuild();
	if (PageTitleBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(
			PageTitleBox,
			EFocusCause::SetDirectly);
	}
}

void SExtendedAtlassianWorkspace::OpenRenamePopover(
	const FString& TargetId,
	const FString& CurrentLabel,
	bool bSection)
{
	PagePopoverTargetId = TargetId;
	PagePopoverTitle = CurrentLabel;
	PagePopoverParentId.Reset();
	bPagePopoverRename = true;
	bPagePopoverTargetSection = bSection;
	bPagePopoverOpen = true;
	PagePopoverPosition = PopupPosition(FVector2D(ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.PagePopover.Width")), 151.0f), false);
	Rebuild();
	if (PageTitleBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(
			PageTitleBox,
			EFocusCause::SetDirectly);
		PageTitleBox->SelectAllText();
	}
}

void SExtendedAtlassianWorkspace::OpenMenu(
	const FText& Title,
	TArray<FWorkspaceMenuItem> Items,
	float Width)
{
	if (!bPagePopoverOpen && !bPinPopoverOpen && !bMenuOpen && !bConfirmOpen)
	{
		OverlayFocusReturn = FSlateApplication::Get().GetKeyboardFocusedWidget();
	}
	MenuTitle = Title;
	MenuItems = MoveTemp(Items);
	MenuSelectedIndex = 0;
	MenuWidth = Width;
	MenuPosition = PopupPosition(
		FVector2D(MenuWidth, 45.0f + MenuItems.Num() * 30.0f),
		false);
	bMenuOpen = true;
	Rebuild();
	FSlateApplication::Get().SetKeyboardFocus(
		SharedThis(this),
		EFocusCause::SetDirectly);
}

void SExtendedAtlassianWorkspace::OpenConfirm(
	const FText& Title,
	const FText& Body,
	const FText& AcceptLabel,
	TFunction<void()> Action)
{
	CloseMenu(false);
	ConfirmTitle = Title;
	ConfirmBody = Body;
	ConfirmAcceptLabel = AcceptLabel;
	ConfirmAction = MoveTemp(Action);
	bConfirmOpen = true;
	Rebuild();
	FSlateApplication::Get().SetKeyboardFocus(
		SharedThis(this),
		EFocusCause::SetDirectly);
}

void SExtendedAtlassianWorkspace::OpenDocumentActions(const FString& NodeId)
{
	const FExtendedAtlassianDocumentTreeNode* Node =
		Controller->GetSnapshot().DocumentTree.FindByPredicate(
			[&NodeId](const FExtendedAtlassianDocumentTreeNode& Candidate)
			{
				return Candidate.Id == NodeId;
			});
	if (!Node)
	{
		return;
	}

	const FString Label = Node->Label;
	const bool bSection = Node->bSection;
	TArray<FWorkspaceMenuItem> Items;
	Items.Add({
		LOCTEXT("RenameDocumentNode", "Rename"),
		FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")),
		FString(),
		[this, NodeId, Label, bSection]()
		{
			OpenRenamePopover(NodeId, Label, bSection);
		}
	});
	if (bSection)
	{
		Items.Add({
			LOCTEXT("AddPageInside", "Add page inside"),
			FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")),
			FString(),
			[this, NodeId]() { OpenCreatePagePopover(NodeId); }
		});
	}
	else
	{
		Items.Add({
			LOCTEXT("DuplicatePage", "Duplicate"),
			FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")),
			FString(),
			[this, NodeId]() { DuplicatePage(NodeId); }
		});
		Items.Add({
			LOCTEXT("MovePageTo", "Move to…"),
			FExtendedAtlassianStyle::FromHex(TEXT("#b6a9ff")),
			FString(),
			[this, NodeId]() { OpenMoveToMenu(NodeId); }
		});
		Items.Add({
			LOCTEXT("ArchivePageMenu", "Archive page"),
			FExtendedAtlassianStyle::FromHex(TEXT("#e3a54a")),
			FString(),
			[this, NodeId, Label]()
			{
				OpenConfirm(
					LOCTEXT("ArchivePageConfirmTitle", "Archive this page?"),
					FText::Format(
						LOCTEXT(
							"ArchivePageConfirmBody",
							"Archiving “{0}” removes it from the current page tree. Confluence will verify your archive permission."),
						FText::FromString(Label)),
					LOCTEXT("ArchivePageConfirmAccept", "Archive page"),
					[this, NodeId]() { ArchiveDocumentPage(NodeId); });
			}
		});
	}
	Items.Add({
		LOCTEXT("MoveDocumentUp", "Move up"),
		FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")),
		FString(),
		[this, NodeId]() { ReorderDocumentNode(NodeId, -1); }
	});
	Items.Add({
		LOCTEXT("MoveDocumentDown", "Move down"),
		FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")),
		FString(),
		[this, NodeId]() { ReorderDocumentNode(NodeId, 1); }
	});
	Items.Add({
		bSection
			? LOCTEXT("DeleteSectionMenu", "Delete section")
			: LOCTEXT("DeletePageMenu", "Delete page"),
		FExtendedAtlassianStyle::FromHex(TEXT("#f0665f")),
		FString(),
		[this, NodeId, Label, bSection]()
		{
			OpenConfirm(
				bSection
					? LOCTEXT("DeleteSectionConfirmTitle", "Delete this section?")
					: LOCTEXT("DeletePageConfirmTitle", "Delete this page?"),
				FText::Format(
					bSection
						? LOCTEXT(
							"DeleteSectionConfirmBody",
							"Deleting “{0}” removes the section and every page inside it.")
						: LOCTEXT(
							"DeletePageConfirmBody",
							"Deleting “{0}” removes it and everything on it from the space."),
					FText::FromString(Label)),
				bSection
					? LOCTEXT("DeleteSectionConfirmAccept", "Delete section")
					: LOCTEXT("DeletePageConfirmAccept", "Delete page"),
				[this, NodeId]() { DeleteDocumentNode(NodeId); });
		}
	});
	OpenMenu(
		bSection ? LOCTEXT("SectionMenuTitle", "SECTION") : LOCTEXT("PageMenuTitle", "PAGE"),
		MoveTemp(Items));
}

void SExtendedAtlassianWorkspace::OpenMoveToMenu(const FString& PageId)
{
	TArray<FWorkspaceMenuItem> Items;
	Items.Add({
		LOCTEXT("MoveTopLevel", "Top level"),
		FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")),
		FString(),
		[this, PageId]() { MovePageToSection(PageId, FString()); }
	});
	for (const FExtendedAtlassianDocumentTreeNode& Node :
		Controller->GetSnapshot().DocumentTree)
	{
		if (Node.bSection)
		{
			Items.Add({
				FText::FromString(Node.Label),
				FExtendedAtlassianStyle::FromHex(TEXT("#b6a9ff")),
				FString(),
				[this, PageId, SectionId = Node.Id]()
				{
					MovePageToSection(PageId, SectionId);
				}
			});
		}
	}
	OpenMenu(LOCTEXT("MoveToMenuTitle", "MOVE TO"), MoveTemp(Items));
}

void SExtendedAtlassianWorkspace::CloseMenu(bool bRestoreFocus)
{
	bMenuOpen = false;
	MenuItems.Reset();
	MenuSelectedIndex = 0;
	Rebuild();
	if (bRestoreFocus)
	{
		RestoreOverlayFocus();
	}
}

void SExtendedAtlassianWorkspace::ClosePagePopover(bool bRestoreFocus)
{
	bMenuOpen = false;
	bPagePopoverOpen = false;
	MenuItems.Reset();
	PageTitleBox.Reset();
	Rebuild();
	if (bRestoreFocus)
	{
		RestoreOverlayFocus();
	}
}

void SExtendedAtlassianWorkspace::RestoreOverlayFocus()
{
	if (bPagePopoverOpen && PageTitleBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(
			PageTitleBox,
			EFocusCause::SetDirectly);
		return;
	}
	if (bCreateCardOpen || bPinPopoverOpen || bMenuOpen || bConfirmOpen || bCaptureOpen)
	{
		return;
	}
	if (const TSharedPtr<SWidget> FocusReturn = OverlayFocusReturn.Pin())
	{
		FSlateApplication::Get().SetKeyboardFocus(
			FocusReturn,
			EFocusCause::SetDirectly);
	}
	OverlayFocusReturn.Reset();
}

void SExtendedAtlassianWorkspace::DuplicatePage(const FString& PageId)
{
	const FExtendedAtlassianPage* Page =
		Controller->GetSnapshot().Pages.FindByPredicate(
			[&PageId](const FExtendedAtlassianPage& Candidate)
			{
				return Candidate.Id == PageId;
			});
	if (!Page)
	{
		return;
	}
	const FString NewId = MakeUniqueDocumentId(TEXT("new"));
	const FString NewTitle = Page->Title + TEXT(" copy");
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::DuplicatePage;
	Mutation.TargetId = PageId;
	Mutation.Fields.Add(TEXT("newId"), NewId);
	Mutation.Fields.Add(TEXT("title"), NewTitle);
	Controller->ExecuteMutation(Mutation);
	Controller->SelectPage(NewId);
	Controller->ShowToast(FText::Format(
		LOCTEXT("PageDuplicatedToast", "Page duplicated  {0}"),
		FText::FromString(NewTitle.ToUpper())));
}

void SExtendedAtlassianWorkspace::MovePageToSection(
	const FString& PageId,
	const FString& SectionId)
{
	const FExtendedAtlassianDocumentTreeNode* Node =
		Controller->GetSnapshot().DocumentTree.FindByPredicate(
			[&PageId](const FExtendedAtlassianDocumentTreeNode& Candidate)
			{
				return Candidate.Id == PageId;
			});
	// Preserve the reference quirk: the action exists for top-level pages but is a no-op.
	if (!Node || Node->Depth == 0)
	{
		return;
	}
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::MovePage;
	Mutation.TargetId = PageId;
	Mutation.ParentId = SectionId;
	Controller->ExecuteMutation(Mutation);
	Controller->ShowToast(LOCTEXT("PageMovedToast", "Page moved"));
}

void SExtendedAtlassianWorkspace::ReorderDocumentNode(
	const FString& NodeId,
	int32 Direction)
{
	TArray<FExtendedAtlassianDocumentTreeNode> Tree =
		Controller->GetSnapshot().DocumentTree;
	const int32 Index = Tree.IndexOfByPredicate(
		[&NodeId](const FExtendedAtlassianDocumentTreeNode& Node)
		{
			return Node.Id == NodeId;
		});
	if (Index == INDEX_NONE || Direction == 0)
	{
		return;
	}

	if (Tree[Index].Depth == 1)
	{
		const int32 OtherIndex = Index + FMath::Sign(Direction);
		if (!Tree.IsValidIndex(OtherIndex) || Tree[OtherIndex].Depth != 1)
		{
			return;
		}
		Tree.Swap(Index, OtherIndex);
	}
	else
	{
		int32 RunEnd = Index + 1;
		while (Tree.IsValidIndex(RunEnd) && Tree[RunEnd].Depth == 1)
		{
			++RunEnd;
		}
		TArray<int32> TopLevelIndices;
		for (int32 TreeIndex = 0; TreeIndex < Tree.Num(); ++TreeIndex)
		{
			if (Tree[TreeIndex].Depth == 0)
			{
				TopLevelIndices.Add(TreeIndex);
			}
		}
		const int32 TopPosition = TopLevelIndices.IndexOfByKey(Index);
		const int32 OtherPosition = TopPosition + FMath::Sign(Direction);
		if (!TopLevelIndices.IsValidIndex(OtherPosition))
		{
			return;
		}

		const int32 OtherStart = TopLevelIndices[OtherPosition];
		int32 OtherEnd = OtherStart + 1;
		while (Tree.IsValidIndex(OtherEnd) && Tree[OtherEnd].Depth == 1)
		{
			++OtherEnd;
		}
		TArray<FExtendedAtlassianDocumentTreeNode> Run;
		for (int32 RunIndex = Index; RunIndex < RunEnd; ++RunIndex)
		{
			Run.Add(Tree[RunIndex]);
		}
		Tree.RemoveAt(Index, RunEnd - Index);
		const int32 InsertIndex = Direction < 0
			? OtherStart
			: Index + (OtherEnd - OtherStart);
		for (int32 Offset = 0; Offset < Run.Num(); ++Offset)
		{
			Tree.Insert(MoveTemp(Run[Offset]), InsertIndex + Offset);
		}
	}

	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::ReorderPage;
	Mutation.TargetId = NodeId;
	for (const FExtendedAtlassianDocumentTreeNode& Node : Tree)
	{
		Mutation.OrderedIds.Add(Node.Id);
	}
	Controller->ExecuteMutation(Mutation);
}

void SExtendedAtlassianWorkspace::ArchiveDocumentPage(const FString& PageId)
{
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::ArchivePage;
	Mutation.TargetId = PageId;
	if (!Controller->ExecuteMutation(Mutation))
	{
		return;
	}
	Controller->ShowToast(LOCTEXT("ArchivingPageToast", "Archiving page…"));
}

void SExtendedAtlassianWorkspace::DeleteDocumentNode(const FString& NodeId)
{
	const FExtendedAtlassianDocumentTreeNode* Node =
		Controller->GetSnapshot().DocumentTree.FindByPredicate(
			[&NodeId](const FExtendedAtlassianDocumentTreeNode& Candidate)
			{
				return Candidate.Id == NodeId;
			});
	if (!Node)
	{
		return;
	}
	const bool bSection = Node->bSection;
	const FString Label = Node->Label;
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = bSection
		? EExtendedAtlassianWorkspaceMutation::DeleteSection
		: EExtendedAtlassianWorkspaceMutation::DeletePage;
	Mutation.TargetId = NodeId;
	Controller->ExecuteDestructiveMutation(
		Mutation,
		FText::Format(
			bSection
				? LOCTEXT("SectionDeletedUndo", "Section deleted  {0}")
				: LOCTEXT("PageDeletedUndo", "Page deleted  {0}"),
			FText::FromString(Label.ToUpper())));
}

void SExtendedAtlassianWorkspace::ToggleCommentReply(const FString& CommentId)
{
	if (ReplyingCommentId == CommentId)
	{
		ReplyingCommentId.Reset();
		ReplyDraft.Reset();
	}
	else
	{
		ReplyingCommentId = CommentId;
		ReplyDraft.Reset();
	}
	Rebuild();
}

void SExtendedAtlassianWorkspace::ToggleCommentReplies(const FString& CommentId)
{
	if (ExpandedCommentReplies.Contains(CommentId))
	{
		ExpandedCommentReplies.Remove(CommentId);
	}
	else
	{
		ExpandedCommentReplies.Add(CommentId);
	}
	Rebuild();
}

void SExtendedAtlassianWorkspace::BeginCommentEdit(
	const FExtendedAtlassianComment& Comment)
{
	EditingCommentId = Comment.Id;
	CommentEditDraft = Comment.Body;
	Rebuild();
}

void SExtendedAtlassianWorkspace::SaveCommentEdit(
	const FString& CommentId,
	bool bIssueComment)
{
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = bIssueComment
		? EExtendedAtlassianWorkspaceMutation::UpdateIssueComment
		: EExtendedAtlassianWorkspaceMutation::UpdatePageComment;
	Mutation.TargetId = CommentId;
	Mutation.Fields.Add(TEXT("body"), CommentEditDraft);
	if (bIssueComment)
	{
		if (const FExtendedAtlassianIssue* Issue = SelectedIssue())
		{
			Mutation.Fields.Add(TEXT("target"), TEXT("issue:") + Issue->Key);
		}
	}
	bCommentMutationPending = true;
	bPendingCommentReply = false;
	bPendingCommentIssue = bIssueComment;
	PendingCommentId = CommentId;
	PendingCommentDraft = CommentEditDraft;
	EditingCommentId.Reset();
	CommentEditDraft.Reset();
	Controller->ExecuteMutation(Mutation);
}

void SExtendedAtlassianWorkspace::SendCommentReply(
	const FString& Scope,
	const FString& CommentId,
	bool bIssueComment)
{
	const FString Body = ReplyDraft.TrimStartAndEnd();
	if (Body.IsEmpty())
	{
		return;
	}
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = bIssueComment
		? EExtendedAtlassianWorkspaceMutation::CreateIssueComment
		: EExtendedAtlassianWorkspaceMutation::CreatePageComment;
	Mutation.TargetId = FString::Printf(
		TEXT("backlot-comment-%d"),
		NextLocalCommentNumber++);
	Mutation.ParentId = CommentId;
	Mutation.Fields.Add(TEXT("target"), Scope);
	Mutation.Fields.Add(TEXT("body"), Body);
	bCommentMutationPending = true;
	bPendingCommentReply = true;
	bPendingCommentIssue = bIssueComment;
	PendingCommentId = CommentId;
	PendingCommentDraft = ReplyDraft;
	PendingCommentScope = Scope;
	ExpandedCommentReplies.Add(CommentId);
	ReplyingCommentId.Reset();
	ReplyDraft.Reset();
	Controller->ExecuteMutation(Mutation);
}

void SExtendedAtlassianWorkspace::ToggleCommentResolved(
	const FExtendedAtlassianComment& Comment,
	bool bIssueComment)
{
	FExtendedAtlassianWorkspaceMutation Mutation;
	if (bIssueComment)
	{
		Mutation.Type = Comment.bResolved
			? EExtendedAtlassianWorkspaceMutation::ReopenIssueComment
			: EExtendedAtlassianWorkspaceMutation::ResolveIssueComment;
	}
	else
	{
		Mutation.Type = Comment.bResolved
			? EExtendedAtlassianWorkspaceMutation::ReopenPageComment
			: EExtendedAtlassianWorkspaceMutation::ResolvePageComment;
	}
	Mutation.TargetId = Comment.Id;
	if (bIssueComment)
	{
		if (const FExtendedAtlassianIssue* Issue = SelectedIssue())
		{
			Mutation.Fields.Add(TEXT("target"), TEXT("issue:") + Issue->Key);
		}
	}
	Controller->ExecuteMutation(Mutation);
}

void SExtendedAtlassianWorkspace::ConfirmDeleteComment(
	const FString& CommentId,
	bool bIssueComment)
{
	OpenConfirm(
		LOCTEXT("DeleteCommentConfirmTitle", "Delete this comment?"),
		LOCTEXT(
			"DeleteCommentConfirmBody",
			"The comment and its replies come off for everyone."),
		LOCTEXT("DeleteCommentConfirmAccept", "Delete"),
		[this, CommentId, bIssueComment]()
		{
			FExtendedAtlassianWorkspaceMutation Mutation;
			Mutation.Type = bIssueComment
				? EExtendedAtlassianWorkspaceMutation::DeleteIssueComment
				: EExtendedAtlassianWorkspaceMutation::DeletePageComment;
			Mutation.TargetId = CommentId;
			if (bIssueComment)
			{
				if (const FExtendedAtlassianIssue* Issue = SelectedIssue())
				{
					Mutation.Fields.Add(TEXT("target"), TEXT("issue:") + Issue->Key);
				}
			}
			Controller->ExecuteDestructiveMutation(
				Mutation,
				FText::FromString(
					bIssueComment
						? TEXT("Comment deleted  ISSUE")
						: TEXT("Comment deleted  PAGE")));
		});
}

void SExtendedAtlassianWorkspace::OpenIssueFieldMenu(
	const FString& FieldName)
{
	using namespace ExtendedAtlassianWorkspacePrivate;
	const FExtendedAtlassianIssue* Issue = SelectedIssue();
	if (!Issue)
	{
		return;
	}
	if (FieldName == TEXT("status"))
	{
		OpenStatusMenu(Issue->Key);
		return;
	}

	TArray<FWorkspaceMenuItem> Items;
	FText Title;
	if (FieldName == TEXT("assignee"))
	{
		Title = LOCTEXT("IssueAssigneeMenu", "ASSIGN TO");
		for (const FExtendedAtlassianUser& User :
			Controller->GetSnapshot().People)
		{
			Items.Add({
				FText::FromString(User.DisplayName),
				FExtendedAtlassianStyle::FromHex(
					User.AvatarBackground.IsEmpty()
						? TEXT("#58a6ff")
						: *User.AvatarBackground),
				Issue->AssigneeAccountId == User.AccountId
					|| Issue->AssigneeDisplayName == User.DisplayName
						? TEXT("●")
						: FString(),
				[this, AccountId = User.AccountId]()
				{
					MutateIssueField(TEXT("assignee"), AccountId);
				}
			});
		}
		Items.Add({
			LOCTEXT("IssueUnassigned", "Unassigned"),
			FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")),
			Issue->AssigneeAccountId.IsEmpty() ? TEXT("●") : FString(),
			[this]()
			{
				MutateIssueField(TEXT("assignee"), FString());
			}
		});
	}
	else if (FieldName == TEXT("epic"))
	{
		Title = LOCTEXT("IssueEpicMenu", "MOVE TO EPIC");
		for (const FExtendedAtlassianEpic& Epic :
			Controller->GetSnapshot().Epics)
		{
			Items.Add({
				FText::FromString(Epic.Name),
				FExtendedAtlassianStyle::FromHex(
					Epic.Color.IsEmpty() ? TEXT("#b6a9ff") : *Epic.Color),
				Issue->EpicName == Epic.Name ? TEXT("●") : FString(),
				[this, EpicName = Epic.Name]()
				{
					MutateIssueField(TEXT("epic"), EpicName);
				}
			});
		}
		Items.Add({
			LOCTEXT("IssueNoEpic", "None"),
			FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")),
			Issue->EpicName.IsEmpty() ? TEXT("●") : FString(),
			[this]()
			{
				MutateIssueField(TEXT("epic"), FString());
			}
		});
	}
	else if (FieldName == TEXT("priority"))
	{
		Title = LOCTEXT("IssuePriorityMenu", "PRIORITY");
		for (const FExtendedAtlassianPriority& Priority :
			Controller->GetSnapshot().Priorities)
		{
			const bool bHigh =
				Priority.Name.Equals(TEXT("Highest"), ESearchCase::IgnoreCase)
				|| Priority.Name.Equals(TEXT("High"), ESearchCase::IgnoreCase);
			Items.Add({
				FText::FromString(Priority.Name),
				FExtendedAtlassianStyle::FromHex(
					bHigh ? TEXT("#f0665f") : TEXT("#8a919c")),
				Issue->PriorityName.Equals(
					Priority.Name,
					ESearchCase::IgnoreCase)
						? TEXT("●")
						: FString(),
				[this, PriorityName = Priority.Name]()
				{
					MutateIssueField(TEXT("priority"), PriorityName);
				}
			});
		}
	}
	else if (FieldName == TEXT("points"))
	{
		Title = LOCTEXT("IssuePointsMenu", "ESTIMATE");
		const int32 PointValues[] = { 1, 2, 3, 5, 8, 13 };
		for (const int32 Points : PointValues)
		{
			Items.Add({
				FText::Format(
					Points == 1
						? LOCTEXT("OneIssuePoint", "{0} point")
						: LOCTEXT("ManyIssuePoints", "{0} points"),
					FText::AsNumber(Points)),
				FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")),
				FMath::IsNearlyEqual(Issue->Estimate, Points)
					? TEXT("●")
					: FString(),
				[this, Points]()
				{
					MutateIssueField(
						TEXT("points"),
						FString::FromInt(Points));
				}
			});
		}
	}
	if (!Items.IsEmpty())
	{
		OpenMenu(Title, MoveTemp(Items), ExtendedAtlassianWorkspacePrivate::Metric(TEXT("Backlot.Metric.GenericMenu.Width")));
	}
}

void SExtendedAtlassianWorkspace::MutateIssueField(
	const FString& FieldName,
	const FString& Value)
{
	const FExtendedAtlassianIssue* Issue = SelectedIssue();
	if (!Issue)
	{
		return;
	}
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::UpdateIssue;
	Mutation.TargetId = Issue->Key;
	Mutation.Fields.Add(FieldName, Value);
	Controller->ExecuteMutation(Mutation);
}

void SExtendedAtlassianWorkspace::ArchiveIssue(const FString& IssueKey)
{
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::ArchiveIssue;
	Mutation.TargetId = IssueKey;
	if (!Controller->ExecuteMutation(Mutation))
	{
		return;
	}
	Controller->Navigate(EExtendedAtlassianWorkspaceRoute::Issues);
	Controller->ShowToast(FText::Format(
		LOCTEXT("ArchivingIssueToast", "Archiving {0}…"),
		FText::FromString(IssueKey)));
}

void SExtendedAtlassianWorkspace::OpenIssueActions()
{
	const FExtendedAtlassianIssue* Issue = SelectedIssue();
	if (!Issue)
	{
		return;
	}
	const FString Key = Issue->Key;
	TArray<FWorkspaceMenuItem> Items;
	Items.Add({
		LOCTEXT("CopyIssueKey", "Copy key"),
		FExtendedAtlassianStyle::FromHex(TEXT("#8a919c")),
		FString(),
		[this, Key]()
		{
			HostServices->CopyText(Key);
			Controller->ShowToast(FText::Format(
				LOCTEXT("CopiedIssueKeyToast", "Copied  {0}"),
				FText::FromString(Key)));
		}
	});
	Items.Add({
		LOCTEXT("ArchiveIssueMenu", "Archive issue"),
		FExtendedAtlassianStyle::FromHex(TEXT("#e3a54a")),
		FString(),
		[this, Key]()
		{
			OpenConfirm(
				FText::Format(
					LOCTEXT("ArchiveIssueConfirmTitle", "Archive {0}?"),
					FText::FromString(Key)),
				LOCTEXT(
					"ArchiveIssueConfirmBody",
					"This removes the issue from normal Jira search and board views. Jira will verify that the site plan and your administrator role support archiving."),
				LOCTEXT("ArchiveIssueConfirmAccept", "Archive issue"),
				[this, Key]() { ArchiveIssue(Key); });
		}
	});
	Items.Add({
		LOCTEXT("DeleteIssueMenu", "Delete issue"),
		FExtendedAtlassianStyle::FromHex(TEXT("#f0665f")),
		FString(),
		[this, Key]()
		{
			OpenConfirm(
				FText::Format(
					LOCTEXT("DeleteIssueConfirmTitle", "Delete {0}?"),
					FText::FromString(Key)),
				LOCTEXT(
					"DeleteIssueConfirmBody",
					"This removes the issue, its threads and its activity. This cannot be undone."),
				LOCTEXT("DeleteIssueConfirmAccept", "Delete issue"),
				[this, Key]()
				{
					FExtendedAtlassianWorkspaceMutation Mutation;
					Mutation.Type = EExtendedAtlassianWorkspaceMutation::DeleteIssue;
					Mutation.TargetId = Key;
					Controller->ExecuteDestructiveMutation(
						Mutation,
						FText::Format(
							LOCTEXT("IssueDeletedUndo", "Issue deleted  {0}"),
							FText::FromString(Key)));
					Controller->Navigate(EExtendedAtlassianWorkspaceRoute::Issues);
				});
		}
	});
	OpenMenu(LOCTEXT("IssueMenuTitle", "ISSUE"), MoveTemp(Items));
}

FString SExtendedAtlassianWorkspace::MakeUniqueDocumentId(const TCHAR* Prefix) const
{
	for (int32 Number = 1; ; ++Number)
	{
		const FString Candidate = FString::Printf(TEXT("%s%d"), Prefix, Number);
		if (!Controller->GetSnapshot().DocumentTree.ContainsByPredicate(
			[&Candidate](const FExtendedAtlassianDocumentTreeNode& Node)
			{
				return Node.Id == Candidate;
			}))
		{
			return Candidate;
		}
	}
}

FVector2D SExtendedAtlassianWorkspace::PopupPosition(
	const FVector2D& Size,
	bool bAboveCursor) const
{
	const FGeometry& Geometry = GetCachedGeometry();
	const FVector2D LocalSize = Geometry.GetLocalSize();
	if (!FSlateApplication::IsInitialized()
		|| LocalSize.X <= KINDA_SMALL_NUMBER
		|| LocalSize.Y <= KINDA_SMALL_NUMBER)
	{
		return FVector2D::ZeroVector;
	}

	const FVector2D RootTopLeft =
		Geometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D RootBottomRight =
		Geometry.LocalToAbsolute(LocalSize);
	const FVector2D PopupBottomRight =
		Geometry.LocalToAbsolute(Size);
	const FVector2D PopupScreenSize(
		FMath::Abs(PopupBottomRight.X - RootTopLeft.X),
		FMath::Abs(PopupBottomRight.Y - RootTopLeft.Y));

	FSlateApplication& SlateApplication = FSlateApplication::Get();
	const FVector2D Cursor = SlateApplication.GetCursorPos();
	const FSlateRect WorkArea = SlateApplication.GetWorkArea(
		FSlateRect(Cursor.X, Cursor.Y, Cursor.X + 1.0f, Cursor.Y + 1.0f));

	// The popup is painted inside this tab rather than in a separate Slate window. Clamp in
	// absolute screen space against both the monitor work area and the tab's actual allocation,
	// then convert the result back to local canvas coordinates. This remains correct under DPI,
	// dock resizing, and tab/window movement.
	const FSlateRect VisibleBounds(
		FMath::Max(RootTopLeft.X, WorkArea.Left),
		FMath::Max(RootTopLeft.Y, WorkArea.Top),
		FMath::Min(RootBottomRight.X, WorkArea.Right),
		FMath::Min(RootBottomRight.Y, WorkArea.Bottom));
	const float ScaleX = PopupScreenSize.X / FMath::Max(Size.X, 1.0f);
	const float ScaleY = PopupScreenSize.Y / FMath::Max(Size.Y, 1.0f);
	const float HorizontalInset = 8.0f * ScaleX;
	const float VerticalInset = 8.0f * ScaleY;
	const float CursorGap = 5.0f * ScaleY;
	const float MinX = VisibleBounds.Left + HorizontalInset;
	const float MaxX = FMath::Max(
		MinX,
		VisibleBounds.Right - PopupScreenSize.X - HorizontalInset);
	const float MinY = VisibleBounds.Top + VerticalInset;
	const float MaxY = FMath::Max(
		MinY,
		VisibleBounds.Bottom - PopupScreenSize.Y - VerticalInset);
	const float AboveY = Cursor.Y - PopupScreenSize.Y - CursorGap;
	const float BelowY = Cursor.Y + CursorGap;
	const bool bFitsAbove = AboveY >= MinY;
	const bool bFitsBelow = BelowY <= MaxY;
	const float DesiredY = bAboveCursor
		? (bFitsAbove || !bFitsBelow ? AboveY : BelowY)
		: (bFitsBelow || !bFitsAbove ? BelowY : AboveY);
	const FVector2D ScreenPosition(
		FMath::Clamp(Cursor.X, MinX, MaxX),
		FMath::Clamp(DesiredY, MinY, MaxY));
	const FVector2D LocalPosition =
		Geometry.AbsoluteToLocal(ScreenPosition);
	return FVector2D(
		FMath::RoundToFloat(LocalPosition.X),
		FMath::RoundToFloat(LocalPosition.Y));
}

void SExtendedAtlassianWorkspace::OnSearchChanged(const FText& TextValue)
{
	PendingGlobalSearch = TextValue.ToString();
	bHasPendingGlobalSearch = true;
	EnsureSearchDebounceTimer();
}

void SExtendedAtlassianWorkspace::OnPageSearchChanged(
	const FText& TextValue)
{
	PendingPageSearch = TextValue.ToString();
	bHasPendingPageSearch = true;
	EnsureSearchDebounceTimer();
}

void SExtendedAtlassianWorkspace::EnsureSearchDebounceTimer()
{
	SearchDebounceDueAt =
		FPlatformTime::Seconds() + 0.15;
	if (bSearchDebounceTimerRegistered)
	{
		return;
	}
	bSearchDebounceTimerRegistered = true;
	RegisterActiveTimer(
		0.15f,
		FWidgetActiveTimerDelegate::CreateSP(
			this,
			&SExtendedAtlassianWorkspace::TickSearchDebounce));
}

EActiveTimerReturnType SExtendedAtlassianWorkspace::TickSearchDebounce(
	double CurrentTime,
	float DeltaTime)
{
	(void)CurrentTime;
	(void)DeltaTime;
	if (FPlatformTime::Seconds() + KINDA_SMALL_NUMBER < SearchDebounceDueAt)
	{
		return EActiveTimerReturnType::Continue;
	}
	bSearchDebounceTimerRegistered = false;
	const bool bApplyGlobal = bHasPendingGlobalSearch;
	const bool bApplyPage = bHasPendingPageSearch;
	const FString Global = MoveTemp(PendingGlobalSearch);
	const FString Page = MoveTemp(PendingPageSearch);
	bHasPendingGlobalSearch = false;
	bHasPendingPageSearch = false;
	if (bApplyGlobal)
	{
		Controller->SetGlobalSearch(Global);
	}
	if (bApplyPage)
	{
		Controller->SetPageSearch(Page);
	}
	return EActiveTimerReturnType::Stop;
}

void SExtendedAtlassianWorkspace::OnDocumentTitleChanged(
	const FText& TextValue)
{
	DocumentDraftTitle = TextValue.ToString();
}

void SExtendedAtlassianWorkspace::OnDocumentMarkdownChanged(
	const FString& Markdown)
{
	DocumentDraftMarkdown = Markdown;
}

void SExtendedAtlassianWorkspace::StartWatchingDocuments()
{
	if (DirectoryWatcherHandle.IsValid())
	{
		return;
	}
	FDirectoryWatcherModule& Module =
		FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(
			TEXT("DirectoryWatcher"));
	IDirectoryWatcher* Watcher = Module.Get();
	if (!Watcher)
	{
		return;
	}
	const FString Root = FExtendedAtlassianDocumentStore::GetRootDirectory();
	IFileManager::Get().MakeDirectory(*Root, true);
	Watcher->RegisterDirectoryChangedCallback_Handle(
		Root,
		IDirectoryWatcher::FDirectoryChanged::CreateSP(
			this,
			&SExtendedAtlassianWorkspace::HandleDocumentsChanged),
		DirectoryWatcherHandle,
		IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges);
}

void SExtendedAtlassianWorkspace::StopWatchingDocuments()
{
	if (!DirectoryWatcherHandle.IsValid())
	{
		return;
	}
	if (FDirectoryWatcherModule* Module =
		FModuleManager::GetModulePtr<FDirectoryWatcherModule>(
			TEXT("DirectoryWatcher")))
	{
		if (IDirectoryWatcher* Watcher = Module->Get())
		{
			Watcher->UnregisterDirectoryChangedCallback_Handle(
				FExtendedAtlassianDocumentStore::GetRootDirectory(),
				DirectoryWatcherHandle);
		}
	}
	DirectoryWatcherHandle.Reset();
}

void SExtendedAtlassianWorkspace::PrepareDocumentWorkingCopy(
	const FExtendedAtlassianPage& Page)
{
	const UExtendedAtlassianSettings* Settings =
		UExtendedAtlassianSettings::Get();
	FString SpaceKey = Settings ? Settings->PrimarySpaceKey : FString();
	if (SpaceKey.IsEmpty() && Settings && !Settings->SpaceKeys.IsEmpty())
	{
		SpaceKey = Settings->SpaceKeys[0];
	}
	if (!FExtendedAtlassianDocumentStore::Save(
		Page,
		SpaceKey,
		CurrentDocumentFilePath))
	{
		CurrentDocumentFilePath.Reset();
		DocumentExternalChangeWarning = LOCTEXT(
			"DocumentWorkingCopyUnavailable",
			"The Markdown working copy could not be created; external file edits cannot be watched.");
	}
}

void SExtendedAtlassianWorkspace::HandleDocumentsChanged(
	const TArray<FFileChangeData>& Changes)
{
	if (!bDocumentEditing
		|| CurrentDocumentFilePath.IsEmpty()
		|| !DocumentEditor.IsValid())
	{
		return;
	}
	const FString Watched =
		FPaths::ConvertRelativePathToFull(CurrentDocumentFilePath);
	for (const FFileChangeData& Change : Changes)
	{
		if (Change.Action == FFileChangeData::FCA_Removed
			|| FPaths::ConvertRelativePathToFull(Change.Filename) != Watched)
		{
			continue;
		}
		if (IsDocumentDraftDirty())
		{
			DocumentExternalChangeWarning = LOCTEXT(
				"DocumentExternalChangeRetained",
				"The working copy changed on disk. Your unsaved editor buffer was retained; Cancel and reopen Edit to load the file.");
			return;
		}
		FExtendedAtlassianDocumentFile File;
		if (!FExtendedAtlassianDocumentStore::Load(Watched, File)
			|| (!File.PageId.IsEmpty()
				&& File.PageId != EditingDocumentPageId))
		{
			return;
		}
		DocumentDraftTitle = File.Title.IsEmpty()
			? DocumentDraftTitle
			: File.Title;
		DocumentDraftMarkdown = File.Markdown;
		DocumentExternalChangeWarning = FText::GetEmpty();
		DocumentEditor->SetMarkdown(DocumentDraftMarkdown);
		Controller->ShowToast(
			LOCTEXT(
				"DocumentReloadedFromDisk",
				"Working copy reloaded from disk"));
		return;
	}
}

void SExtendedAtlassianWorkspace::OnDocumentTaskToggled(int32 BlockIndex)
{
	const FExtendedAtlassianPage* Page = SelectedPage();
	if (!Page
		|| !Page->Blocks.IsValidIndex(BlockIndex)
		|| Page->Blocks[BlockIndex].Kind
			!= EExtendedAtlassianBlockKind::TaskItem)
	{
		return;
	}
	TArray<FExtendedAtlassianDocBlock> UpdatedBlocks = Page->Blocks;
	UpdatedBlocks[BlockIndex].bChecked =
		!UpdatedBlocks[BlockIndex].bChecked;
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::TogglePageTask;
	Mutation.TargetId = Page->Id;
	Mutation.Fields.Add(TEXT("blockIndex"), FString::FromInt(BlockIndex));
	Mutation.Fields.Add(TEXT("title"), Page->Title);
	Mutation.Fields.Add(
		TEXT("body"),
		FExtendedAtlassianMarkdown::FromBlocks(UpdatedBlocks));
	Mutation.Fields.Add(TEXT("version"), FString::FromInt(Page->Version));
	Controller->ExecuteMutation(Mutation);
}

void SExtendedAtlassianWorkspace::OnDocumentIssueClicked(
	const FString& IssueKey)
{
	Controller->OpenIssue(IssueKey);
}

void SExtendedAtlassianWorkspace::OnDocumentAssetClicked(
	const FString& AssetName,
	const FString& PathOrMeta)
{
	if (PathOrMeta.StartsWith(TEXT("/Game/")))
	{
		FExtendedAtlassianPinTarget Target;
		Target.Kind = EExtendedAtlassianPinKind::Material;
		Target.StableId = PathOrMeta;
		Target.DisplayName = AssetName;
		FText Error;
		if (HostServices->RevealTarget(
			Target,
			Error))
		{
			Controller->ShowToast(FText::Format(
				LOCTEXT(
					"DocumentAssetRevealed",
					"Revealed in the Content Browser  ·  {0}"),
				FText::FromString(AssetName.ToUpper())));
			return;
		}
		if (!Error.IsEmpty())
		{
			Controller->ShowToast(Error);
			return;
		}
	}
	if (PathOrMeta.StartsWith(TEXT("page:"))
		|| PathOrMeta.StartsWith(TEXT("backlot-page:")))
	{
		const int32 Colon = PathOrMeta.Find(TEXT(":"));
		const FString PageId = PathOrMeta.Mid(Colon + 1);
		if (Controller->GetSnapshot().Pages.ContainsByPredicate(
			[&PageId](const FExtendedAtlassianPage& Page)
			{
				return Page.Id == PageId;
			}))
		{
			Controller->Navigate(EExtendedAtlassianWorkspaceRoute::Docs);
			Controller->SelectPage(PageId);
			return;
		}
	}
	FString FileTarget = PathOrMeta;
	if (FileTarget.StartsWith(TEXT("file://")))
	{
		FileTarget.RightChopInline(7);
	}
	if (FPaths::FileExists(FileTarget))
	{
		HostServices->OpenExternal(FileTarget);
		return;
	}
	if (PathOrMeta.StartsWith(TEXT("https://"))
		|| PathOrMeta.StartsWith(TEXT("http://")))
	{
		HostServices->OpenExternal(PathOrMeta);
		return;
	}
	// The strict fixture stores authored display metadata instead of a loadable object path.
	Controller->ShowToast(FText::Format(
		LOCTEXT(
			"DocumentAssetFixtureReveal",
			"Revealed in the Content Browser  ·  {0}"),
		FText::FromString(AssetName.ToUpper())));
}

void SExtendedAtlassianWorkspace::OnCaptureTitleChanged(const FText& TextValue)
{
	CaptureTitle = TextValue.ToString();
}

void SExtendedAtlassianWorkspace::EnsureInteractionTimer()
{
	if (bInteractionTimerRegistered
		|| !Controller.IsValid()
		|| !Controller->HasTimedInteractionState())
	{
		return;
	}
	bInteractionTimerRegistered = true;
	RegisterActiveTimer(
		0.05f,
		FWidgetActiveTimerDelegate::CreateSP(
			this,
			&SExtendedAtlassianWorkspace::TickInteraction));
}

EActiveTimerReturnType SExtendedAtlassianWorkspace::TickInteraction(
	double CurrentTime,
	float DeltaTime)
{
	(void)CurrentTime;
	(void)DeltaTime;
	if (!Controller.IsValid())
	{
		bInteractionTimerRegistered = false;
		return EActiveTimerReturnType::Stop;
	}
	Controller->TickInteractionState();
	if (Controller->HasTimedInteractionState())
	{
		return EActiveTimerReturnType::Continue;
	}
	bInteractionTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}

void SExtendedAtlassianWorkspace::ScheduleBackgroundSync(
	float OverrideDelaySeconds)
{
	if (bBackgroundSyncTimerRegistered
		|| !Controller.IsValid()
		|| Controller->IsFixtureProvider())
	{
		return;
	}
	const UExtendedAtlassianSettings* Settings =
		UExtendedAtlassianSettings::Get();
	if (!Settings || !Settings->bEnablePolling)
	{
		return;
	}
	const float Delay = OverrideDelaySeconds >= 0.0f
		? OverrideDelaySeconds
		: static_cast<float>(
			FMath::Max(60, Settings->PollIntervalSeconds));
	bBackgroundSyncTimerRegistered = true;
	RegisterActiveTimer(
		Delay,
		FWidgetActiveTimerDelegate::CreateSP(
			this,
			&SExtendedAtlassianWorkspace::TickBackgroundSync));
}

EActiveTimerReturnType SExtendedAtlassianWorkspace::TickBackgroundSync(
	double CurrentTime,
	float DeltaTime)
{
	(void)CurrentTime;
	(void)DeltaTime;
	bBackgroundSyncTimerRegistered = false;
	if (!Controller.IsValid() || Controller->IsFixtureProvider())
	{
		return EActiveTimerReturnType::Stop;
	}
	const UExtendedAtlassianSettings* Settings =
		UExtendedAtlassianSettings::Get();
	if (!Settings || !Settings->bEnablePolling)
	{
		return EActiveTimerReturnType::Stop;
	}
	const bool bEditorBusy =
		FEditorBuildUtils::IsBuildCurrentlyRunning()
		|| (GEditor && GEditor->IsPlaySessionInProgress());
	if (bEditorBusy)
	{
		if (!bSyncRefreshDeferred)
		{
			bSyncRefreshDeferred = true;
			Rebuild();
		}
		ScheduleBackgroundSync(1.0f);
		return EActiveTimerReturnType::Stop;
	}

	bSyncRefreshDeferred = false;
	LastSyncPollSeconds = HostServices.IsValid()
		? HostServices->NowSeconds()
		: 0.0;
	Controller->Refresh();
	ScheduleBackgroundSync();
	return EActiveTimerReturnType::Stop;
}

void SExtendedAtlassianWorkspace::HandleSettingsObjectChanged(
	UObject* Object,
	FPropertyChangedEvent& Event)
{
	(void)Event;
	if (!Controller.IsValid()
		|| Controller->IsFixtureProvider()
		|| Object != UExtendedAtlassianSettings::GetMutable())
	{
		return;
	}
	const FString AuthSignature =
		ExtendedAtlassianWorkspacePrivate::AuthConfigurationSignature();
	if (AuthSignature != LastAuthConfigurationSignature)
	{
		LastAuthConfigurationSignature = AuthSignature;
		LastSyncPollSeconds = HostServices.IsValid()
			? HostServices->NowSeconds()
			: 0.0;
		bSyncRefreshDeferred = false;
		Controller->Refresh();
	}
	ScheduleBackgroundSync();
}

FReply SExtendedAtlassianWorkspace::OnKeyDown(
	const FGeometry& MyGeometry,
	const FKeyEvent& InKeyEvent)
{
	(void)MyGeometry;
	const bool bPrimaryModifier = InKeyEvent.IsControlDown() || InKeyEvent.IsCommandDown();
	if (!bMenuOpen
		&& !bConfirmOpen
		&& Controller->GetRoute() == EExtendedAtlassianWorkspaceRoute::Issues
		&& (!GlobalSearchBox.IsValid()
			|| FSlateApplication::Get().GetKeyboardFocusedWidget()
				!= GlobalSearchBox))
	{
		const TArray<const FExtendedAtlassianIssue*> Issues =
			FilteredIssues();
		if ((InKeyEvent.GetKey() == EKeys::Down
				|| InKeyEvent.GetKey() == EKeys::Up)
			&& !Issues.IsEmpty())
		{
			int32 Index = Issues.IndexOfByPredicate(
				[this](const FExtendedAtlassianIssue* Issue)
				{
					return Issue
						&& Issue->Key
							== Controller->GetSelectedIssueKey();
				});
			Index = FMath::Clamp(
				(Index == INDEX_NONE ? 0 : Index)
					+ (InKeyEvent.GetKey() == EKeys::Down ? 1 : -1),
				0,
				Issues.Num() - 1);
			Controller->SelectIssue(Issues[Index]->Key);
			return FReply::Handled();
		}
		if (InKeyEvent.GetKey() == EKeys::Enter
			&& SelectedIssue())
		{
			return OnOpenSelectedIssue();
		}
	}
	if (bMenuOpen && !MenuItems.IsEmpty())
	{
		if (InKeyEvent.GetKey() == EKeys::Down
			|| InKeyEvent.GetKey() == EKeys::Up)
		{
			MenuSelectedIndex = FMath::Clamp(
				MenuSelectedIndex + (InKeyEvent.GetKey() == EKeys::Down ? 1 : -1),
				0,
				MenuItems.Num() - 1);
			Rebuild();
			return FReply::Handled();
		}
		if (InKeyEvent.GetKey() == EKeys::Enter)
		{
			const TFunction<void()> Action = MenuItems[MenuSelectedIndex].Action;
			CloseMenu(false);
			if (Action)
			{
				Action();
			}
			return FReply::Handled();
		}
	}
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (bConfirmOpen)
		{
			return OnDismissConfirm();
		}
		if (bMenuOpen)
		{
			return OnDismissMenu();
		}
		if (bCreateCardOpen)
		{
			return OnDismissCreateCard();
		}
		if (bPinPopoverOpen)
		{
			return OnDismissPinPopover();
		}
		if (bPagePopoverOpen)
		{
			return OnDismissPagePopover();
		}
		if (bCaptureOpen)
		{
			return OnCancelCapture();
		}
		if (bDocumentEditing)
		{
			return OnCancelDocumentEdit();
		}
	}
	if (bPrimaryModifier && InKeyEvent.IsShiftDown() && InKeyEvent.GetKey() == EKeys::B)
	{
		return OnOpenCapture();
	}
	if (bPrimaryModifier && InKeyEvent.GetKey() == EKeys::K && GlobalSearchBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(
			GlobalSearchBox,
			EFocusCause::SetDirectly);
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

const FExtendedAtlassianIssue* SExtendedAtlassianWorkspace::SelectedIssue() const
{
	return Controller->GetSnapshot().Issues.FindByPredicate(
		[this](const FExtendedAtlassianIssue& Issue)
		{
			return Issue.Key == Controller->GetSelectedIssueKey();
		});
}

bool SExtendedAtlassianWorkspace::IssueMatchesCurrentFilters(
	const FExtendedAtlassianIssue& Issue) const
{
	const FExtendedAtlassianWorkspaceSnapshot& Snapshot =
		Controller->GetSnapshot();
	const FString& ViewId = Controller->GetSelectedIssueViewId();
	const FExtendedAtlassianIssueView* View =
		Snapshot.IssueViews.FindByPredicate(
			[&ViewId](const FExtendedAtlassianIssueView& Candidate)
			{
				return Candidate.Id == ViewId;
			});
	const bool bMatchesView =
		!View
		|| View->IssueKeys.Contains(Issue.Key)
		|| (View->IssueKeys.IsEmpty()
			&& (ViewId == TEXT("sprint")
				|| (ViewId == TEXT("mine")
					&& Issue.AssigneeAccountId
						== Snapshot.CurrentUser.AccountId)
				|| (ViewId == TEXT("triage")
					&& Issue.StatusName == TEXT("Triage"))
				|| (ViewId == TEXT("blocked")
					&& Issue.StatusName == TEXT("Blocked"))
				|| (ViewId == TEXT("docs")
					&& Issue.IssueTypeName == TEXT("Doc"))));
	const bool bMatchesStatus =
		Controller->GetStatusFilter() == TEXT("any")
		|| (Controller->GetStatusFilter() == TEXT("Done")
			&& (Issue.StatusCategoryKey == TEXT("done")
				|| Issue.StatusName == TEXT("Done")))
		|| (Controller->GetStatusFilter() == TEXT("not Done")
			&& Issue.StatusCategoryKey != TEXT("done")
			&& Issue.StatusName != TEXT("Done"));
	const bool bMatchesAssignee =
		Controller->GetAssigneeFilter().IsEmpty()
		|| Controller->GetAssigneeFilter() == TEXT("anyone")
		|| Issue.AssigneeAccountId == Controller->GetAssigneeFilter();
	const bool bMatchesEpic =
		Controller->GetEpicFilter().IsEmpty()
		|| Issue.EpicName == Controller->GetEpicFilter()
		|| Issue.ParentId == Controller->GetEpicFilter();
	const FString& Query = Controller->GetGlobalSearch();
	const bool bMatchesSearch =
		Query.IsEmpty()
		|| Issue.Key.Contains(Query, ESearchCase::IgnoreCase)
		|| Issue.Summary.Contains(Query, ESearchCase::IgnoreCase)
		|| Issue.EpicName.Contains(Query, ESearchCase::IgnoreCase)
		|| Issue.StatusName.Contains(Query, ESearchCase::IgnoreCase);
	return bMatchesView
		&& bMatchesStatus
		&& bMatchesAssignee
		&& bMatchesEpic
		&& bMatchesSearch;
}

TArray<const FExtendedAtlassianIssue*>
SExtendedAtlassianWorkspace::FilteredIssues() const
{
	TArray<const FExtendedAtlassianIssue*> Result;
	for (const FExtendedAtlassianIssue& Issue :
		Controller->GetSnapshot().Issues)
	{
		if (IssueMatchesCurrentFilters(Issue))
		{
			Result.Add(&Issue);
		}
	}
	return Result;
}

const FExtendedAtlassianPage* SExtendedAtlassianWorkspace::SelectedPage() const
{
	return Controller->GetSnapshot().Pages.FindByPredicate(
		[this](const FExtendedAtlassianPage& Page)
		{
			return Page.Id == Controller->GetSelectedPageId();
		});
}

const FExtendedAtlassianPin* SExtendedAtlassianWorkspace::SelectedPin() const
{
	return Controller->GetSnapshot().Pins.FindByPredicate(
		[this](const FExtendedAtlassianPin& Pin)
		{
			return Pin.Id == Controller->GetSelectedPinId();
		});
}

const FExtendedAtlassianNotification* SExtendedAtlassianWorkspace::SelectedNotification() const
{
	return Controller->GetSnapshot().Notifications.FindByPredicate(
		[this](const FExtendedAtlassianNotification& Notification)
		{
			return Notification.Id == Controller->GetSelectedNotificationId();
		});
}

bool SExtendedAtlassianWorkspace::IsDocumentDraftDirty() const
{
	const FExtendedAtlassianPage* Page = SelectedPage();
	if (!bDocumentEditing || !Page || Page->Id != EditingDocumentPageId)
	{
		return false;
	}
	FString CurrentMarkdown = DocumentDraftMarkdown;
	if (DocumentEditor.IsValid())
	{
		CurrentMarkdown = DocumentEditor->GetMarkdown();
	}
	const FString OriginalMarkdown = Page->Markdown.IsEmpty()
		? FExtendedAtlassianMarkdown::FromBlocks(Page->Blocks)
		: Page->Markdown;
	return DocumentDraftTitle != Page->Title
		|| CurrentMarkdown != OriginalMarkdown;
}

void SExtendedAtlassianWorkspace::ResetDocumentEditState()
{
	bDocumentEditing = false;
	bDocumentPublishPending = false;
	EditingDocumentPageId.Reset();
	DocumentDraftTitle.Reset();
	DocumentDraftMarkdown.Reset();
	CurrentDocumentFilePath.Reset();
	DocumentExternalChangeWarning = FText::GetEmpty();
	DocumentEditor.Reset();
}

#undef LOCTEXT_NAMESPACE
