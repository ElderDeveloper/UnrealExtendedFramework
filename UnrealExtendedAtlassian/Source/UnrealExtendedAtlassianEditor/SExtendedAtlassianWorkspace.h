// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianTypes.h"
#include "Widgets/SCompoundWidget.h"

class FExtendedAtlassianWorkspaceController;
class IExtendedAtlassianInteractionClock;
class IExtendedAtlassianWorkspaceData;
class IExtendedAtlassianWorkspaceHostServices;
class SExtendedAtlassianDocumentEditor;
class SEditableTextBox;
class SOverlay;
class SVerticalBox;
struct FFileChangeData;
struct FSlateDynamicImageBrush;

/** Unified native Slate workspace matching the Backlot HTML shell. */
class SExtendedAtlassianWorkspace : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExtendedAtlassianWorkspace)
		: _StartRoute(EExtendedAtlassianWorkspaceRoute::Docs)
		, _AnimationsEnabled(true)
	{}
		SLATE_ARGUMENT(EExtendedAtlassianWorkspaceRoute, StartRoute)
		SLATE_ARGUMENT(bool, AnimationsEnabled)
		SLATE_ARGUMENT(TSharedPtr<IExtendedAtlassianWorkspaceData>, WorkspaceData)
		SLATE_ARGUMENT(TSharedPtr<IExtendedAtlassianInteractionClock>, InteractionClock)
		SLATE_ARGUMENT(TSharedPtr<IExtendedAtlassianWorkspaceHostServices>, HostServices)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SExtendedAtlassianWorkspace() override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(
		const FGeometry& MyGeometry,
		const FKeyEvent& InKeyEvent) override;

	void Navigate(EExtendedAtlassianWorkspaceRoute Route);
	void Refresh();
	/** Secret-free state exports consumed by headless interaction/golden automation. */
	FString ExportNormalizedStateForAutomation() const;
	FString ExportOverlayStateForAutomation() const;
	float ExportOverlayAnimationProgressForAutomation() const;
	FString ExportSchedulingStateForAutomation() const;
	bool IsHighContrastForAutomation() const { return bHighContrastEnabled; }
	void SetGlobalSearchForAutomation(const FString& Value);
	void RevealDocumentAssetForAutomation(
		const FString& AssetName,
		const FString& PathOrMeta);

private:
	struct FWorkspaceMenuItem
	{
		FText Label;
		FLinearColor Color = FLinearColor::White;
		FString Mark;
		TFunction<void()> Action;
	};

	void HandleControllerChanged();
	void HandleAuthStateChanged();
	void Rebuild();
	float OverlayAnimationProgress() const;
	float ToastAnimationProgress() const;

	/**
	 * Resolves the authored `min(<px>, <n>vw)` and `<n>vh` overlay sizes against the
	 * live workspace geometry instead of assuming the authored 1920x1080 preview.
	 */
	float ClampedOverlayWidth(
		const TCHAR* PixelMetric,
		const TCHAR* ViewportPercentMetric) const;
	float ClampedOverlayHeight(const TCHAR* ViewportPercentMetric) const;
	FOptionalSize CaptureWidth() const;
	FOptionalSize CaptureMaxHeight() const;
	FOptionalSize ConfirmWidth() const;
	TSharedRef<SWidget> AnimatedPanel(
		const TSharedRef<SWidget>& Content,
		float DurationSeconds,
		float RisePixels,
		bool bScale);

	TSharedRef<SWidget> BuildShell();
	TSharedRef<SWidget> BuildProjectStrip();
	TSharedRef<SWidget> BuildNavigationRail();
	TSharedRef<SWidget> BuildCommandHeader();
	TSharedRef<SWidget> BuildContextSidebar();
	TSharedRef<SWidget> BuildMainView();
	TSharedRef<SWidget> BuildWorkspaceState();
	TSharedRef<SWidget> BuildWorkspaceStatusBanner();
	TSharedRef<SWidget> BuildRightRail();
	TSharedRef<SWidget> BuildCaptureOverlay();
	TSharedRef<SWidget> BuildToast();
	TSharedRef<SWidget> BuildCreateCardPopover();
	TSharedRef<SWidget> BuildCreateCardEpicPicker();
	void RebuildCreateCardEpicRows();
	TSharedRef<SWidget> BuildPagePopover();
	TSharedRef<SWidget> BuildPinPopover();
	TSharedRef<SWidget> BuildGenericMenu();
	TSharedRef<SWidget> BuildConfirmDialog();
	TSharedRef<SWidget> BuildDocsSidebar();
	TSharedRef<SWidget> BuildIssuesSidebar();
	TSharedRef<SWidget> BuildBoardSidebar();
	TSharedRef<SWidget> BuildPinsSidebar();
	TSharedRef<SWidget> BuildDocsMain();
	TSharedRef<SWidget> BuildIssuesMain();
	TSharedRef<SWidget> BuildIssueDescription(
		const FExtendedAtlassianIssue& Issue);
	TSharedRef<SWidget> BuildIssueDetailMain();
	TSharedRef<SWidget> BuildIssueDetailMainDynamic();
	TSharedRef<SWidget> BuildBoardMain();
	TSharedRef<SWidget> BuildPinsMain();
	TSharedRef<SWidget> BuildInboxMain();
	TSharedRef<SWidget> BuildDocsRail();
	TSharedRef<SWidget> BuildDocsRailDynamic();
	TSharedRef<SWidget> BuildIssueRail();
	TSharedRef<SWidget> BuildBoardRail();
	TSharedRef<SWidget> BuildPinsRail();
	TSharedRef<SWidget> BuildInboxRail();
	TSharedRef<SWidget> BuildCommentCard(
		const FString& Scope,
		const FExtendedAtlassianComment& Comment,
		bool bIssueComment);

	TSharedRef<SWidget> NavButton(
		EExtendedAtlassianWorkspaceRoute Route,
		const FName& IconName,
		const FString& Label,
		int32 Badge = 0);
	TSharedRef<SWidget> SectionLabel(const FText& Text) const;
	TSharedRef<SWidget> EmptyState(const FText& Text) const;
	FReply OnNavigate(EExtendedAtlassianWorkspaceRoute Route);
	FReply OnToggleCompact();
	FReply OnToggleRail();
	FReply OnRefresh();
	FReply OnOpenSelectedIssue();
	FReply OnStartDocumentEdit();
	FReply OnCancelDocumentEdit();
	FReply OnPublishDocumentEdit();
	FReply OnStartIssueEdit();
	FReply OnCancelIssueEdit();
	FReply OnSaveIssueEdit();
	FReply OnPostIssueComment();
	FReply OnOpenCapture();
	FReply OnCancelCapture();
	FReply OnCreateCapture();
	FReply OnUndoToast();
	FReply OnDismissPagePopover();
	FReply OnDismissPinPopover();
	FReply OnDismissCreateCard();
	FReply OnSubmitCreateCard();
	FReply OnDeleteCreateCard();
	FReply OnSubmitPagePopover();
	FReply OnSubmitPinPopover();
	FReply OnPostPinReply();
	FReply OnDismissMenu();
	FReply OnDismissConfirm();
	FReply OnAcceptConfirm();
	FReply OnCreateSection();
	void OnSearchChanged(const FText& Text);
	void OnPageSearchChanged(const FText& Text);
	void OnDocumentTitleChanged(const FText& Text);
	void OnDocumentMarkdownChanged(const FString& Markdown);
	void OnDocumentTaskToggled(int32 BlockIndex);
	void OnDocumentIssueClicked(const FString& IssueKey);
	void OnDocumentAssetClicked(
		const FString& AssetName,
		const FString& PathOrMeta);
	void StartWatchingDocuments();
	void StopWatchingDocuments();
	void HandleDocumentsChanged(const TArray<FFileChangeData>& Changes);
	void PrepareDocumentWorkingCopy(const FExtendedAtlassianPage& Page);
	void OnCaptureTitleChanged(const FText& Text);
	void OnPagePopoverTitleChanged(const FText& Text);
	void OnPinPopoverNameChanged(const FText& Text);
	void OpenCreatePagePopover(const FString& ParentSectionId = FString());
	void OpenCreateCard(const FString& Status = TEXT("Triage"));
	void OpenCardEdit(const FString& IssueKey);
	void DropBoardIssue(
		const FString& IssueKey,
		const FString& PresentationColumn,
		const FString& BeforeIssueKey);
	void OpenStatusMenu(const FString& IssueKey);
	void OpenPinPopover(
		bool bRename,
		const FString& TargetId = FString());
	void OpenPinActions(const FString& PinId);
	void ResolvePinPopoverTarget(EExtendedAtlassianPinKind Kind);
	void RevealPinTarget(const FExtendedAtlassianPin& Pin);
	void ConfirmDeletePin(const FString& PinId);
	void SelectPinThread(const FString& PinId, const FString& ThreadId);
	void BeginPinMessageEdit(const FExtendedAtlassianPinThread& Thread);
	void SavePinMessageEdit(const FString& ThreadId);
	void CancelPinMessageEdit();
	void ConfirmDeletePinMessage(const FString& PinId, const FString& ThreadId);
	void TogglePinThreadResolved(const FExtendedAtlassianPinThread& Thread);
	void SetInboxTabAndSelect(const FString& Tab);
	void SelectInboxNotification(const FString& NotificationId);
	void MarkAllInboxRead();
	void ArchiveReadInbox();
	void DismissInboxNotification(const FString& NotificationId);
	void MarkInboxNotificationRead(const FString& NotificationId, bool bShowToast);
	void MuteInboxThread(const FString& NotificationId);
	void OpenInboxNotificationTarget(const FExtendedAtlassianNotification& Notification);
	void OpenRenamePopover(
		const FString& TargetId,
		const FString& CurrentLabel,
		bool bSection);
	void OpenMenu(
		const FText& Title,
		TArray<FWorkspaceMenuItem> Items,
		float Width = 196.0f);
	void OpenConfirm(
		const FText& Title,
		const FText& Body,
		const FText& AcceptLabel,
		TFunction<void()> Action);
	void OpenDocumentActions(const FString& NodeId);
	void OpenMoveToMenu(const FString& PageId);
	void CloseMenu(bool bRestoreFocus = true);
	void ClosePagePopover(bool bRestoreFocus = true);
	void RestoreOverlayFocus();
	void DuplicatePage(const FString& PageId);
	void MovePageToSection(const FString& PageId, const FString& SectionId);
	void ReorderDocumentNode(const FString& NodeId, int32 Direction);
	void ArchiveDocumentPage(const FString& PageId);
	void DeleteDocumentNode(const FString& NodeId);
	void ToggleCommentReply(const FString& CommentId);
	void ToggleCommentReplies(const FString& CommentId);
	void BeginCommentEdit(const FExtendedAtlassianComment& Comment);
	void SaveCommentEdit(const FString& CommentId, bool bIssueComment);
	void SendCommentReply(
		const FString& Scope,
		const FString& CommentId,
		bool bIssueComment);
	void ToggleCommentResolved(
		const FExtendedAtlassianComment& Comment,
		bool bIssueComment);
	void ConfirmDeleteComment(
		const FString& CommentId,
		bool bIssueComment);
	void OpenIssueActions();
	void ArchiveIssue(const FString& IssueKey);
	void OpenIssueFieldMenu(const FString& FieldName);
	void MutateIssueField(const FString& FieldName, const FString& Value);
	FString MakeUniqueDocumentId(const TCHAR* Prefix) const;
	FVector2D PopupPosition(const FVector2D& Size, bool bAboveCursor) const;
	void EnsureInteractionTimer();
	void EnsureSearchDebounceTimer();
	void ScheduleBackgroundSync(float OverrideDelaySeconds = -1.0f);
	void HandleSettingsObjectChanged(
		UObject* Object,
		struct FPropertyChangedEvent& Event);
	EActiveTimerReturnType TickInteraction(
		double CurrentTime,
		float DeltaTime);
	EActiveTimerReturnType TickSearchDebounce(
		double CurrentTime,
		float DeltaTime);
	EActiveTimerReturnType TickBackgroundSync(
		double CurrentTime,
		float DeltaTime);

	const FExtendedAtlassianIssue* SelectedIssue() const;
	bool IssueMatchesCurrentFilters(
		const FExtendedAtlassianIssue& Issue) const;
	TArray<const FExtendedAtlassianIssue*> FilteredIssues() const;
	const FExtendedAtlassianPage* SelectedPage() const;
	const FExtendedAtlassianPin* SelectedPin() const;
	const FExtendedAtlassianNotification* SelectedNotification() const;
	bool IsDocumentDraftDirty() const;
	void ResetDocumentEditState();

	TSharedPtr<FExtendedAtlassianWorkspaceController> Controller;
	TSharedPtr<IExtendedAtlassianWorkspaceHostServices> HostServices;
	TSharedPtr<SOverlay> RootOverlay;
	TSharedPtr<SEditableTextBox> GlobalSearchBox;
	TSharedPtr<SEditableTextBox> PageTitleBox;
	TSharedPtr<SExtendedAtlassianDocumentEditor> DocumentEditor;
	FDelegateHandle ChangedHandle;
	FDelegateHandle AuthChangedHandle;
	FDelegateHandle SettingsObjectChangedHandle;
	bool bInteractionTimerRegistered = false;
	bool bSearchDebounceTimerRegistered = false;
	bool bBackgroundSyncTimerRegistered = false;
	FString PendingGlobalSearch;
	FString PendingPageSearch;
	bool bHasPendingGlobalSearch = false;
	bool bHasPendingPageSearch = false;
	double SearchDebounceDueAt = 0.0;
	bool bAnimationsEnabled = true;
	bool bHighContrastEnabled = false;
	float ContextSidebarFraction = 0.17f;
	float RightRailFraction = 0.24f;
	uint8 LastOverlayMask = 0;
	double OverlayAnimationStartedAt = 0.0;
	double ToastAnimationStartedAt = 0.0;
	uint64 LastAnimatedToastKey = 0;
	FString CaptureTitle;
	FString CaptureType = TEXT("Bug");
	FString CapturePriority = TEXT("High");
	FString CaptureEpic;
	FString CaptureTool = TEXT("PIN");
	TArray<FExtendedAtlassianAnnotation> CaptureAnnotations;
	TArray<uint8> CapturedViewportPng;
	FIntPoint CapturedViewportSize = FIntPoint::ZeroValue;
	TSharedPtr<FSlateDynamicImageBrush> CapturedViewportBrush;
	bool bCaptureOpen = false;
	bool bCaptureMutationPending = false;
	FString PendingCaptureSummary;
	int32 PendingCaptureAnnotationCount = 0;
	bool bPagePopoverOpen = false;
	bool bPinPopoverOpen = false;
	bool bCreateCardOpen = false;
	bool bCreateCardEdit = false;
	FString CreateCardTargetKey;
	FString CreateCardSummary;
	FString CreateCardType = TEXT("Task");
	FString CreateCardPriority = TEXT("MEDIUM");
	FString CreateCardAssignee;
	FString CreateCardEpic;
	FString CreateCardEpicSearch;
	TSharedPtr<SVerticalBox> CreateCardEpicRows;
	FString CreateCardStatus = TEXT("Triage");
	FVector2D CreateCardPosition = FVector2D::ZeroVector;
	bool bPagePopoverRename = false;
	bool bPagePopoverTargetSection = false;
	FString PagePopoverTargetId;
	FString PagePopoverTitle;
	FString PagePopoverParentId;
	FVector2D PagePopoverPosition = FVector2D::ZeroVector;
	bool bPinPopoverRename = false;
	FString PinPopoverTargetId;
	FString PinPopoverName;
	FString PinPopoverStableId;
	FString PinPopoverSecondaryId;
	FText PinPopoverTargetError;
	EExtendedAtlassianPinKind PinPopoverKind = EExtendedAtlassianPinKind::Material;
	FVector2D PinPopoverPosition = FVector2D::ZeroVector;
	FString PinKindFilter;
	FString SelectedPinThreadId;
	FString PinReplyDraft;
	FString EditingPinMessageId;
	FString PinMessageEditDraft;
	bool bMenuOpen = false;
	FText MenuTitle;
	TArray<FWorkspaceMenuItem> MenuItems;
	int32 MenuSelectedIndex = 0;
	float MenuWidth = 196.0f;
	FVector2D MenuPosition = FVector2D::ZeroVector;
	bool bConfirmOpen = false;
	FText ConfirmTitle;
	FText ConfirmBody;
	FText ConfirmAcceptLabel;
	TFunction<void()> ConfirmAction;
	TWeakPtr<SWidget> OverlayFocusReturn;
	TSet<FString> ExpandedCommentReplies;
	FString ReplyingCommentId;
	FString ReplyDraft;
	FString EditingCommentId;
	FString CommentEditDraft;
	bool bCommentMutationPending = false;
	bool bPendingCommentReply = false;
	bool bPendingCommentIssue = false;
	FString PendingCommentId;
	FString PendingCommentDraft;
	FString PendingCommentScope;
	int32 NextLocalCommentNumber = 1;
	bool bDocumentEditing = false;
	bool bDocumentPublishPending = false;
	FString EditingDocumentPageId;
	FString DocumentDraftTitle;
	FString DocumentDraftMarkdown;
	FString CurrentDocumentFilePath;
	FText DocumentExternalChangeWarning;
	FDelegateHandle DirectoryWatcherHandle;
	bool bIssueEditing = false;
	bool bIssueEditMutationPending = false;
	FString EditingIssueKey;
	FString IssueDraftSummary;
	FString IssueDraftDescription;
	FString PendingIssueDraftSummary;
	FString PendingIssueDraftDescription;
	FDateTime IssueEditBaseUpdated = FDateTime::MinValue();
	FText IssueEditConflictWarning;
	FString SelectedIssueThreadId;
	FString NewIssueCommentDraft;
	bool bIssueCommentAttachCapture = false;
	bool bIssueComposerMutationPending = false;
	FString PendingIssueCommentDraft;
	bool bPendingIssueCommentAttachCapture = false;
	double LastSyncPollSeconds = 0.0;
	bool bSyncRefreshDeferred = false;
	FString LastAuthConfigurationSignature;
};
