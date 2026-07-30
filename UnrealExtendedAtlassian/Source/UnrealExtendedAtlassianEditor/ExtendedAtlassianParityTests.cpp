// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianBacklotStore.h"
#include "ExtendedAtlassianConfluenceComments.h"
#include "ExtendedAtlassianConfluenceProperties.h"
#include "ExtendedAtlassianFixtureWorkspaceData.h"
#include "ExtendedAtlassianInboxState.h"
#include "ExtendedAtlassianJira.h"
#include "ExtendedAtlassianJiraSoftware.h"
#include "ExtendedAtlassianMarkdown.h"
#include "ExtendedAtlassianScreenshot.h"
#include "ExtendedAtlassianSettings.h"
#include "ExtendedAtlassianStyle.h"
#include "ExtendedAtlassianWorkspaceController.h"
#include "ExtendedAtlassianWorkspaceHostServices.h"
#include "SBacklotStylePrimitives.h"
#include "SExtendedAtlassianDocumentView.h"
#include "SExtendedAtlassianWorkspace.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateTypes.h"
#include "Types/ISlateMetaData.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"

namespace ExtendedAtlassianParityTestsPrivate
{
	TSharedPtr<FJsonObject> ParseObject(const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader =
			TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Object);
		return Object;
	}

	class FManualClock final : public IExtendedAtlassianInteractionClock
	{
	public:
		virtual double NowSeconds() const override { return Now; }
		void Advance(double Seconds) { Now += Seconds; }

	private:
		double Now = 1000.0;
	};

	class FRejectingWorkspaceData final : public IExtendedAtlassianWorkspaceData
	{
	public:
		FRejectingWorkspaceData()
		{
			Snapshot.State = EExtendedAtlassianLoadState::Ready;
			Snapshot.Capabilities.bCanEditIssues = true;
			Snapshot.Capabilities.bCanEditPages = true;
			FExtendedAtlassianIssue Issue;
			Issue.Id = TEXT("NFB-1");
			Issue.Key = TEXT("NFB-1");
			Issue.Summary = TEXT("Original");
			Snapshot.Issues.Add(MoveTemp(Issue));
			FExtendedAtlassianPage Page;
			Page.Id = TEXT("page-1");
			Page.Title = TEXT("Original page");
			Page.Markdown = TEXT("# Original\n");
			Page.Body = Page.Markdown;
			Page.Blocks = FExtendedAtlassianMarkdown::ToBlocks(Page.Markdown);
			Page.Version = 14;
			Snapshot.Pages.Add(MoveTemp(Page));
			FExtendedAtlassianDocumentTreeNode Node;
			Node.Id = TEXT("page-1");
			Node.Label = TEXT("Original page");
			Snapshot.DocumentTree.Add(MoveTemp(Node));
		}

		virtual void Load(
			const FExtendedAtlassianWorkspaceRequest& Request,
			FExtendedAtlassianWorkspaceLoadDelegate Completion) override
		{
			Completion.ExecuteIfBound(Request, Snapshot);
		}

		virtual void Mutate(
			const FExtendedAtlassianWorkspaceMutation& Mutation,
			FExtendedAtlassianWorkspaceMutationDelegate Completion) override
		{
			FExtendedAtlassianError Error;
			Error.HttpStatus = 409;
			Error.Code = TEXT("Conflict");
			Error.Message = TEXT("Fixture conflict");
			Completion.ExecuteIfBound(Mutation.ClientMutationId, false, Error);
		}

		virtual void CancelGeneration(uint64 Generation) override { (void)Generation; }
		virtual const FExtendedAtlassianCapabilities& GetCapabilities() const override
		{
			return Snapshot.Capabilities;
		}
		virtual bool IsFixtureProvider() const override { return true; }

	private:
		FExtendedAtlassianWorkspaceSnapshot Snapshot;
	};

	class FWarningWorkspaceData final : public IExtendedAtlassianWorkspaceData
	{
	public:
		FWarningWorkspaceData()
		{
			Snapshot.State = EExtendedAtlassianLoadState::Ready;
			Snapshot.Capabilities.bCanEditIssues = true;
			FExtendedAtlassianIssue Issue;
			Issue.Id = TEXT("NFB-2");
			Issue.Key = TEXT("NFB-2");
			Issue.Summary = TEXT("Original");
			Snapshot.Issues.Add(MoveTemp(Issue));
		}

		virtual void Load(
			const FExtendedAtlassianWorkspaceRequest& Request,
			FExtendedAtlassianWorkspaceLoadDelegate Completion) override
		{
			Completion.ExecuteIfBound(Request, Snapshot);
		}

		virtual void Mutate(
			const FExtendedAtlassianWorkspaceMutation& Mutation,
			FExtendedAtlassianWorkspaceMutationDelegate Completion) override
		{
			if (const FString* Summary = Mutation.Fields.Find(TEXT("summary")))
			{
				Snapshot.Issues[0].Summary = *Summary;
			}
			FExtendedAtlassianError Warning;
			Warning.HttpStatus = 500;
			Warning.Code = TEXT("IssueCreatedAttachmentFailed");
			Warning.Message =
				TEXT("NFB-2 was created, but backlot-capture.png failed to upload.");
			Completion.ExecuteIfBound(Mutation.ClientMutationId, true, Warning);
		}

		virtual void CancelGeneration(uint64 Generation) override { (void)Generation; }
		virtual const FExtendedAtlassianCapabilities& GetCapabilities() const override
		{
			return Snapshot.Capabilities;
		}
		virtual bool IsFixtureProvider() const override { return false; }

	private:
		FExtendedAtlassianWorkspaceSnapshot Snapshot;
	};

	class FPartialRankWorkspaceData final : public IExtendedAtlassianWorkspaceData
	{
	public:
		FPartialRankWorkspaceData()
		{
			Snapshot.State = EExtendedAtlassianLoadState::Ready;
			Snapshot.Capabilities.bCanTransitionIssues = true;
			Snapshot.Capabilities.bCanRankIssues = true;
			FExtendedAtlassianIssue First;
			First.Id = TEXT("NFB-10");
			First.Key = First.Id;
			First.Summary = TEXT("Server rank anchor");
			First.StatusName = TEXT("In progress");
			Snapshot.Issues.Add(MoveTemp(First));
			FExtendedAtlassianIssue Second;
			Second.Id = TEXT("NFB-11");
			Second.Key = Second.Id;
			Second.Summary = TEXT("Transition succeeds");
			Second.StatusName = TEXT("Triage");
			Snapshot.Issues.Add(MoveTemp(Second));
		}

		virtual void Load(
			const FExtendedAtlassianWorkspaceRequest& Request,
			FExtendedAtlassianWorkspaceLoadDelegate Completion) override
		{
			Completion.ExecuteIfBound(Request, Snapshot);
		}

		virtual void Mutate(
			const FExtendedAtlassianWorkspaceMutation& Mutation,
			FExtendedAtlassianWorkspaceMutationDelegate Completion) override
		{
			if (Mutation.Type == EExtendedAtlassianWorkspaceMutation::MoveIssue)
			{
				Snapshot.Issues[1].StatusName =
					Mutation.Fields.FindRef(TEXT("status"));
				FExtendedAtlassianError Warning;
				Warning.HttpStatus = 400;
				Warning.Code = TEXT("PartialRank");
				Warning.Message =
					TEXT("Transition succeeded; Jira retained its previous rank.");
				Completion.ExecuteIfBound(
					Mutation.ClientMutationId,
					true,
					Warning);
				return;
			}
			Completion.ExecuteIfBound(
				Mutation.ClientMutationId,
				true,
				FExtendedAtlassianError());
		}

		virtual void CancelGeneration(uint64 Generation) override
		{
			(void)Generation;
		}
		virtual const FExtendedAtlassianCapabilities& GetCapabilities() const override
		{
			return Snapshot.Capabilities;
		}
		virtual bool IsFixtureProvider() const override { return false; }

	private:
		FExtendedAtlassianWorkspaceSnapshot Snapshot;
	};

	class FScriptedWorkspaceData final : public IExtendedAtlassianWorkspaceData
	{
	public:
		struct FPendingLoad
		{
			FExtendedAtlassianWorkspaceRequest Request;
			FExtendedAtlassianWorkspaceLoadDelegate Completion;
		};

		struct FPendingMutation
		{
			FExtendedAtlassianWorkspaceMutation Mutation;
			FExtendedAtlassianWorkspaceMutationDelegate Completion;
		};

		FScriptedWorkspaceData()
		{
			Capabilities.bCanReadIssues = true;
			Capabilities.bCanCreateIssues = true;
			Capabilities.bCanEditIssues = true;
			Capabilities.bCanDeleteIssues = true;
			Capabilities.bCanAssignIssues = true;
			Capabilities.bCanTransitionIssues = true;
			Capabilities.bCanRankIssues = true;
			Capabilities.bCanReadBoards = true;
			Capabilities.bCanReadPages = true;
			Capabilities.bCanEditPages = true;
			Capabilities.bCanDeletePages = true;
			Capabilities.bCanComment = true;
			Capabilities.bCanUseSharedMetadata = true;
		}

		virtual void Load(
			const FExtendedAtlassianWorkspaceRequest& Request,
			FExtendedAtlassianWorkspaceLoadDelegate Completion) override
		{
			OrderedCalls.Add(FString::Printf(
				TEXT("load:%llu"),
				Request.Generation));
			FPendingLoad& Pending = PendingLoads.AddDefaulted_GetRef();
			Pending.Request = Request;
			Pending.Completion = MoveTemp(Completion);
		}

		virtual void Mutate(
			const FExtendedAtlassianWorkspaceMutation& Mutation,
			FExtendedAtlassianWorkspaceMutationDelegate Completion) override
		{
			OrderedCalls.Add(FString::Printf(
				TEXT("mutate:%d:%s"),
				static_cast<int32>(Mutation.Type),
				*Mutation.TargetId));
			RecordedMutations.Add(Mutation);
			if (bAutoCompleteMutations)
			{
				Completion.ExecuteIfBound(
					Mutation.ClientMutationId,
					true,
					FExtendedAtlassianError());
			}
			else
			{
				FPendingMutation& Pending =
					PendingMutations.AddDefaulted_GetRef();
				Pending.Mutation = Mutation;
				Pending.Completion = MoveTemp(Completion);
			}
		}

		virtual void CancelGeneration(uint64 Generation) override
		{
			OrderedCalls.Add(FString::Printf(
				TEXT("cancel:%llu"),
				Generation));
			CancelledGenerations.Add(Generation);
		}

		virtual const FExtendedAtlassianCapabilities& GetCapabilities() const override
		{
			return Capabilities;
		}

		virtual bool IsFixtureProvider() const override { return false; }

		void CompleteLoad(
			int32 Index,
			const FExtendedAtlassianWorkspaceSnapshot& Snapshot)
		{
			check(PendingLoads.IsValidIndex(Index));
			FPendingLoad Pending = MoveTemp(PendingLoads[Index]);
			PendingLoads.RemoveAt(Index);
			OrderedCalls.Add(FString::Printf(
				TEXT("load-complete:%llu:%d"),
				Pending.Request.Generation,
				static_cast<int32>(Snapshot.State)));
			Pending.Completion.ExecuteIfBound(Pending.Request, Snapshot);
		}

		void CompleteMutation(
			int32 Index,
			bool bSuccess,
			const FExtendedAtlassianError& Error =
				FExtendedAtlassianError())
		{
			check(PendingMutations.IsValidIndex(Index));
			FPendingMutation Pending = MoveTemp(PendingMutations[Index]);
			PendingMutations.RemoveAt(Index);
			OrderedCalls.Add(FString::Printf(
				TEXT("complete:%llu:%s"),
				Pending.Mutation.ClientMutationId,
				bSuccess ? TEXT("success") : TEXT("failure")));
			Pending.Completion.ExecuteIfBound(
				Pending.Mutation.ClientMutationId,
				bSuccess,
				Error);
		}

		TArray<FPendingLoad> PendingLoads;
		TArray<FPendingMutation> PendingMutations;
		TArray<uint64> CancelledGenerations;
		TArray<FExtendedAtlassianWorkspaceMutation> RecordedMutations;
		TArray<FString> OrderedCalls;
		bool bAutoCompleteMutations = true;

	private:
		FExtendedAtlassianCapabilities Capabilities;
	};

	class FInMemoryHostServices final
		: public IExtendedAtlassianWorkspaceHostServices
	{
	public:
		virtual double NowSeconds() const override { return Now; }
		virtual bool ShouldReduceMotion() const override { return bReduceMotion; }
		virtual bool ShouldUseHighContrast() const override
		{
			return bHighContrast;
		}

		virtual bool CaptureViewport(
			TArray<uint8>& OutPngData,
			FIntPoint& OutSize) override
		{
			Calls.Add(TEXT("capture"));
			OutPngData = { 0x89, 0x50, 0x4e, 0x47 };
			OutSize = FIntPoint(1920, 1080);
			return true;
		}

		virtual FExtendedAtlassianCapturedContext CaptureContext() override
		{
			Calls.Add(TEXT("context"));
			FExtendedAtlassianCapturedContext Context;
			Context.LevelName = TEXT("AutomationMap");
			return Context;
		}

		virtual void CopyText(const FString& Text) override
		{
			Calls.Add(TEXT("copy:") + Text);
		}

		virtual void OpenExternal(const FString& Url) override
		{
			Calls.Add(TEXT("open:") + Url);
		}

		virtual bool ResolveCurrentTarget(
			EExtendedAtlassianPinKind Kind,
			const FString& SelectedPageId,
			const FString& SelectedPageTitle,
			FExtendedAtlassianPinTarget& OutTarget,
			FText& OutError) override
		{
			(void)OutError;
			Calls.Add(TEXT("resolve:") + SelectedPageId);
			OutTarget.Kind = Kind;
			OutTarget.StableId = SelectedPageId;
			OutTarget.DisplayName = SelectedPageTitle;
			return true;
		}

		virtual bool RevealTarget(
			const FExtendedAtlassianPinTarget& Target,
			FText& OutError) override
		{
			Calls.Add(TEXT("reveal:") + Target.StableId);
			if (bRevealSucceeds)
			{
				return true;
			}
			OutError = FText::FromString(TEXT("Asset no longer exists"));
			return false;
		}

		int32 CountCalls(const FString& Value) const
		{
			int32 Count = 0;
			for (const FString& Call : Calls)
			{
				Count += Call == Value ? 1 : 0;
			}
			return Count;
		}

		mutable TArray<FString> Calls;
		double Now = 42.0;
		bool bReduceMotion = false;
		bool bHighContrast = false;
		bool bRevealSucceeds = true;
	};

	class FLargeFixtureWorkspaceData final
		: public IExtendedAtlassianWorkspaceData
	{
	public:
		virtual void Load(
			const FExtendedAtlassianWorkspaceRequest& Request,
			FExtendedAtlassianWorkspaceLoadDelegate Completion) override
		{
			Fixture.Load(
				Request,
				FExtendedAtlassianWorkspaceLoadDelegate::CreateLambda(
					[Completion](
						const FExtendedAtlassianWorkspaceRequest& CompletedRequest,
						const FExtendedAtlassianWorkspaceSnapshot& BaseSnapshot)
					{
						FExtendedAtlassianWorkspaceSnapshot Snapshot =
							BaseSnapshot;

						const FExtendedAtlassianPage PagePrototype =
							Snapshot.Pages.IsEmpty()
								? FExtendedAtlassianPage()
								: Snapshot.Pages[0];
						const FExtendedAtlassianDocumentTreeNode TreePrototype =
							Snapshot.DocumentTree.IsEmpty()
								? FExtendedAtlassianDocumentTreeNode()
								: Snapshot.DocumentTree[0];
						Snapshot.Pages.Reset(2000);
						Snapshot.DocumentTree.Reset(2000);
						for (int32 Index = 0; Index < 2000; ++Index)
						{
							FExtendedAtlassianPage Page = PagePrototype;
							Page.Id = Index == 0
								? FString(TEXT("wet"))
								: FString::Printf(TEXT("perf-page-%04d"), Index);
							Page.Title = FString::Printf(
								TEXT("Performance page %04d"),
								Index);
							Page.Blocks.Reset(Index == 0 ? 5000 : 0);
							if (Index == 0)
							{
								for (int32 BlockIndex = 0;
									BlockIndex < 5000;
									++BlockIndex)
								{
									FExtendedAtlassianDocBlock Block;
									Block.Kind =
										EExtendedAtlassianBlockKind::Paragraph;
									Block.Markup = FString::Printf(
										TEXT("Performance block %04d"),
										BlockIndex);
									Page.Blocks.Add(MoveTemp(Block));
								}
							}
							Snapshot.Pages.Add(MoveTemp(Page));

							FExtendedAtlassianDocumentTreeNode Node =
								TreePrototype;
							Node.Id = Snapshot.Pages.Last().Id;
							Node.Label = Snapshot.Pages.Last().Title;
							Node.ParentId.Reset();
							Node.Depth = 0;
							Node.bSection = false;
							Node.bExpanded = false;
							Snapshot.DocumentTree.Add(MoveTemp(Node));
						}

						if (!Snapshot.Issues.IsEmpty())
						{
							const FExtendedAtlassianIssue Prototype =
								Snapshot.Issues[0];
							while (Snapshot.Issues.Num() < 200)
							{
								const int32 Index = Snapshot.Issues.Num();
								FExtendedAtlassianIssue Issue = Prototype;
								Issue.Id = FString::Printf(
									TEXT("perf-issue-%04d"),
									Index);
								Issue.Key = FString::Printf(
									TEXT("PERF-%04d"),
									Index);
								Issue.Summary = FString::Printf(
									TEXT("Performance issue %04d"),
									Index);
								Snapshot.Issues.Add(MoveTemp(Issue));
							}
						}

						if (!Snapshot.Notifications.IsEmpty())
						{
							const FExtendedAtlassianNotification Prototype =
								Snapshot.Notifications[0];
							while (Snapshot.Notifications.Num() < 500)
							{
								const int32 Index =
									Snapshot.Notifications.Num();
								FExtendedAtlassianNotification Notification =
									Prototype;
								Notification.Id = FString::Printf(
									TEXT("perf-notification-%04d"),
									Index);
								Notification.Target = FString::Printf(
									TEXT("Performance notification %04d"),
									Index);
								Snapshot.Notifications.Add(
									MoveTemp(Notification));
							}
						}

						if (!Snapshot.Pins.IsEmpty())
						{
							const FExtendedAtlassianPin Prototype =
								Snapshot.Pins[0];
							while (Snapshot.Pins.Num() < 200)
							{
								const int32 Index = Snapshot.Pins.Num();
								FExtendedAtlassianPin Pin = Prototype;
								Pin.Id = FString::Printf(
									TEXT("perf-pin-%04d"),
									Index);
								Pin.DisplayName = FString::Printf(
									TEXT("Performance pin %04d"),
									Index);
								Pin.Target.StableId = Pin.Id;
								Pin.Target.DisplayName = Pin.DisplayName;
								Snapshot.Pins.Add(MoveTemp(Pin));
							}
						}

						Completion.ExecuteIfBound(
							CompletedRequest,
							Snapshot);
					}));
		}

		virtual void Mutate(
			const FExtendedAtlassianWorkspaceMutation& Mutation,
			FExtendedAtlassianWorkspaceMutationDelegate Completion) override
		{
			Fixture.Mutate(Mutation, MoveTemp(Completion));
		}

		virtual void CancelGeneration(uint64 Generation) override
		{
			Fixture.CancelGeneration(Generation);
		}

		virtual const FExtendedAtlassianCapabilities&
		GetCapabilities() const override
		{
			return Fixture.GetCapabilities();
		}

		virtual bool IsFixtureProvider() const override { return true; }

	private:
		FExtendedAtlassianFixtureWorkspaceData Fixture;
	};

	void AuditActionableMetadata(
		FAutomationTestBase& Test,
		const TSharedRef<SWidget>& Widget,
		int32& OutActionableCount)
	{
		const FString Type = Widget->GetTypeAsString();
		const bool bActionable =
			Type.Contains(TEXT("Button"))
			|| Type.Contains(TEXT("CheckBox"))
			|| Type.Contains(TEXT("EditableText"))
			|| Type.Contains(TEXT("Combo"))
			|| Type.Contains(TEXT("ScrollBar"))
			|| Type.Contains(TEXT("BoardCard"))
			|| Type.Contains(TEXT("AnnotationSurface"))
			|| Type.Contains(TEXT("DocumentEditor"));
		if (bActionable)
		{
			++OutActionableCount;
			const TSharedPtr<FTagMetaData> Tag =
				Widget->GetMetaData<FTagMetaData>();
			Test.TestTrue(
				TEXT("Actionable widget has a stable Backlot automation tag"),
				Tag.IsValid()
					&& Tag->Tag.ToString().StartsWith(TEXT("Backlot.Action.")));
			Test.TestFalse(
				TEXT("Actionable widget has an accessible name"),
				Widget->GetAccessibleText().IsEmpty());
		}
		FChildren* Children = Widget->GetChildren();
		if (!Children)
		{
			return;
		}
		for (int32 Index = 0; Index < Children->Num(); ++Index)
		{
			AuditActionableMetadata(
				Test,
				Children->GetChildAt(Index),
				OutActionableCount);
		}
	}

	TSharedPtr<SButton> FindButtonByAccessibleText(
		const TSharedRef<SWidget>& Widget,
		const FString& Needle)
	{
		if (Widget->GetTypeAsString() == TEXT("SButton")
			&& Widget->GetAccessibleText().ToString().Contains(
				Needle,
				ESearchCase::IgnoreCase))
		{
			return StaticCastSharedRef<SButton>(Widget);
		}
		FChildren* Children = Widget->GetChildren();
		if (!Children)
		{
			return nullptr;
		}
		for (int32 Index = 0; Index < Children->Num(); ++Index)
		{
			if (TSharedPtr<SButton> Match = FindButtonByAccessibleText(
					Children->GetChildAt(Index),
					Needle))
			{
				return Match;
			}
		}
		return nullptr;
	}

	bool EncodePng(
		const TArray<FColor>& Colors,
		const FIntVector& Size,
		TArray64<uint8>& OutPng)
	{
		if (Size.X <= 0 || Size.Y <= 0
			|| Colors.Num() != Size.X * Size.Y)
		{
			return false;
		}
		IImageWrapperModule& ImageWrapperModule =
			FModuleManager::LoadModuleChecked<IImageWrapperModule>(
				TEXT("ImageWrapper"));
		const TSharedPtr<IImageWrapper> Encoder =
			ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		if (!Encoder.IsValid()
			|| !Encoder->SetRaw(
				Colors.GetData(),
				Colors.Num() * static_cast<int64>(sizeof(FColor)),
				Size.X,
				Size.Y,
				ERGBFormat::BGRA,
				8))
		{
			return false;
		}
		OutPng = Encoder->GetCompressed(100);
		return !OutPng.IsEmpty();
	}

	bool CaptureSlateWidget(
		const TSharedRef<SWidget>& Widget,
		const FVector2D& ClientSize,
		TArray<FColor>& OutColors,
		FIntVector& OutSize,
		FString& OutGeometryJson)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}
		const TSharedRef<SWindow> Window =
			SNew(SWindow)
			.SizingRule(ESizingRule::FixedSize)
			.ClientSize(ClientSize)
			.UseOSWindowBorder(false)
			.CreateTitleBar(false)
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			.IsInitiallyMaximized(false)
			[
				Widget
			];
		Window->SetDPIScaleFactor(1.0f);
		FSlateApplication::Get().AddWindow(Window, true);
		Widget->SlatePrepass(1.0f);
		FSlateApplication::Get().Tick();
		FSlateApplication::Get().ForceRedrawWindow(Window);
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
		const bool bExceedsDesktop =
			ClientSize.X > static_cast<float>(DisplayMetrics.PrimaryDisplayWidth)
			|| ClientSize.Y > static_cast<float>(DisplayMetrics.PrimaryDisplayHeight);
		bool bCaptured = false;
		if (bExceedsDesktop)
		{
			// Render into a linear target, then perform the same single sRGB encode
			// that TakeScreenshot applies. Enabling renderer gamma correction here
			// double-encodes the pixels on D3D12 (for example #23272E becomes
			// approximately #6C7179).
			FWidgetRenderer Renderer(false, true);
			if (UTextureRenderTarget2D* Target =
					Renderer.DrawWidget(Widget, ClientSize))
			{
				FlushRenderingCommands();
				FTextureRenderTargetResource* Resource =
					Target->GameThread_GetRenderTargetResource();
				const FIntPoint TargetSize = Resource->GetSizeXY();
				OutSize = FIntVector(TargetSize.X, TargetSize.Y, 0);
				bCaptured =
					TargetSize.X == FMath::RoundToInt(ClientSize.X)
					&& TargetSize.Y == FMath::RoundToInt(ClientSize.Y)
					&& Resource->ReadPixels(OutColors);
				if (bCaptured)
				{
					for (FColor& Color : OutColors)
					{
						const uint8 Alpha = Color.A;
						Color = FLinearColor(
							static_cast<float>(Color.R) / 255.0f,
							static_cast<float>(Color.G) / 255.0f,
							static_cast<float>(Color.B) / 255.0f,
							static_cast<float>(Alpha) / 255.0f)
								.ToFColorSRGB();
						Color.A = Alpha;
					}
				}
			}
		}
		else
		{
			bCaptured = FSlateApplication::Get().TakeScreenshot(
				Widget,
				OutColors,
				OutSize);
		}

		TArray<TSharedPtr<FJsonValue>> GeometryEntries;
		TFunction<void(const TSharedRef<SWidget>&, const FString&)> AppendWidget;
		AppendWidget =
			[&GeometryEntries, &AppendWidget](
				const TSharedRef<SWidget>& Current,
				const FString& Path)
			{
				const FGeometry Geometry = Current->GetCachedGeometry();
				const FVector2D TopLeft =
					Geometry.LocalToAbsolute(FVector2D::ZeroVector);
				const FVector2D BottomRight =
					Geometry.LocalToAbsolute(Geometry.GetLocalSize());
				const TSharedRef<FJsonObject> Entry =
					MakeShared<FJsonObject>();
				Entry->SetStringField(TEXT("path"), Path);
				Entry->SetStringField(
					TEXT("type"),
					Current->GetTypeAsString());
				Entry->SetStringField(
					TEXT("accessible"),
					Current->GetAccessibleText().ToString());
				if (const TSharedPtr<FTagMetaData> Tag =
					Current->GetMetaData<FTagMetaData>())
				{
					Entry->SetStringField(
						TEXT("automationId"),
						Tag->Tag.ToString());
				}
				Entry->SetNumberField(TEXT("x"), TopLeft.X);
				Entry->SetNumberField(TEXT("y"), TopLeft.Y);
				Entry->SetNumberField(
					TEXT("width"),
					BottomRight.X - TopLeft.X);
				Entry->SetNumberField(
					TEXT("height"),
					BottomRight.Y - TopLeft.Y);
				Entry->SetBoolField(
					TEXT("enabled"),
					Current->IsEnabled());
				GeometryEntries.Add(
					MakeShared<FJsonValueObject>(Entry));
				if (FChildren* Children = Current->GetChildren())
				{
					for (int32 Index = 0; Index < Children->Num(); ++Index)
					{
						AppendWidget(
							Children->GetChildAt(Index),
							FString::Printf(
								TEXT("%s.%d"),
								*Path,
								Index));
					}
				}
			};
		AppendWidget(Widget, TEXT("0"));
		const TSharedRef<FJsonObject> GeometryRoot =
			MakeShared<FJsonObject>();
		GeometryRoot->SetNumberField(TEXT("width"), ClientSize.X);
		GeometryRoot->SetNumberField(TEXT("height"), ClientSize.Y);
		GeometryRoot->SetArrayField(
			TEXT("widgets"),
			MoveTemp(GeometryEntries));
		const TSharedRef<TJsonWriter<>> Writer =
			TJsonWriterFactory<>::Create(&OutGeometryJson);
		FJsonSerializer::Serialize(GeometryRoot, Writer);

		FSlateApplication::Get().RequestDestroyWindow(Window);
		return bCaptured;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianFixtureContractTest,
	"ExtendedAtlassian.Parity.FixtureContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianFixtureContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Fixture =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	TestTrue(TEXT("Frozen fixture loads"), Fixture->IsValid());

	const TSharedRef<ExtendedAtlassianParityTestsPrivate::FManualClock> Clock =
		MakeShared<ExtendedAtlassianParityTestsPrivate::FManualClock>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Fixture, Clock);
	Controller->Refresh();

	const FExtendedAtlassianWorkspaceSnapshot& Snapshot = Controller->GetSnapshot();
	TestEqual(TEXT("Fixture pages"), Snapshot.Pages.Num(), 12);
	TestEqual(TEXT("Fixture tree rows"), Snapshot.DocumentTree.Num(), 17);
	TestEqual(
		TEXT("Fixture Confluence space name"),
		Snapshot.ConfluenceSpaceName,
		FString(TEXT("Production Bible")));
	TestEqual(
		TEXT("Fixture Confluence space key"),
		Snapshot.ConfluenceSpaceKey,
		FString(TEXT("PB")));
	TestEqual(TEXT("Fixture issues"), Snapshot.Issues.Num(), 12);
	TestEqual(TEXT("Fixture views"), Snapshot.IssueViews.Num(), 5);
	TestEqual(TEXT("Fixture people"), Snapshot.People.Num(), 5);
	TestEqual(TEXT("Fixture comment sets"), Snapshot.CommentCollections.Num(), 5);
	TestEqual(TEXT("Fixture activity"), Snapshot.Activity.Num(), 4);
	TestEqual(TEXT("Fixture issue threads"), Snapshot.IssueThreads.Num(), 3);
	TestEqual(TEXT("Fixture pins"), Snapshot.Pins.Num(), 6);
	TestEqual(TEXT("Fixture inbox"), Snapshot.Notifications.Num(), 8);
	TestEqual(
		TEXT("Fixture linked pin state"),
		Snapshot.Pins[2].Threads[1].LinkedLabel,
		FString(TEXT("LINKED NFB-1024")));
	TestEqual(TEXT("Fixture team load"), Snapshot.TeamLoad.Num(), 5);
	TestEqual(TEXT("Fixture authored done total"), Snapshot.SprintSummary.Done, 28);
	TestEqual(TEXT("Fixture unread"), Controller->GetUnreadCount(), 4);
	TestEqual(TEXT("Initial page"), Controller->GetSelectedPageId(), FString(TEXT("wet")));
	const FExtendedAtlassianPage* WetPage = Snapshot.Pages.FindByPredicate(
		[](const FExtendedAtlassianPage& Page)
		{
			return Page.Id == TEXT("wet");
		});
	TestNotNull(TEXT("Selected fixture page exists"), WetPage);
	if (WetPage)
	{
		TestEqual(
			TEXT("Selected fixture page has all authored Linked Work rows"),
			WetPage->LinkedIssueKeys.Num(),
			3);
	}
	TestEqual(TEXT("Initial issue"), Controller->GetSelectedIssueKey(), FString(TEXT("NFB-1042")));
	TestEqual(TEXT("Initial pin"), Controller->GetSelectedPinId(), FString(TEXT("M_WetStone_Master")));

	FExtendedAtlassianWorkspaceMutation Delete;
	Delete.Type = EExtendedAtlassianWorkspaceMutation::DeleteIssue;
	Delete.TargetId = TEXT("NFB-1042");
	Controller->ExecuteDestructiveMutation(Delete, FText::FromString(TEXT("Issue deleted")));
	TestEqual(TEXT("Delete applies optimistically"), Controller->GetSnapshot().Issues.Num(), 11);
	TestTrue(TEXT("Delete offers Undo"), Controller->GetToast().bOffersUndo);
	TestTrue(TEXT("Undo succeeds"), Controller->UndoLastDestructiveMutation());
	TestEqual(TEXT("Undo restores issue"), Controller->GetSnapshot().Issues.Num(), 12);

	Controller->ExecuteDestructiveMutation(Delete, FText::FromString(TEXT("Issue deleted")));
	Clock->Advance(7.01);
	Controller->TickInteractionState();
	TestEqual(TEXT("Expired Undo commits delete"), Controller->GetSnapshot().Issues.Num(), 11);
	TestFalse(TEXT("Undo toast expires"), Controller->GetToast().bOffersUndo);

	Controller->ShowToast(FText::FromString(TEXT("Saved")));
	TestTrue(TEXT("Normal toast appears"), Controller->GetToast().IsSet());
	Clock->Advance(2.61);
	Controller->TickInteractionState();
	TestFalse(TEXT("Normal toast expires at 2.6 seconds"), Controller->GetToast().IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianRollbackContractTest,
	"ExtendedAtlassian.Parity.MutationRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianRollbackContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<ExtendedAtlassianParityTestsPrivate::FRejectingWorkspaceData> Provider =
		MakeShared<ExtendedAtlassianParityTestsPrivate::FRejectingWorkspaceData>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Provider);
	Controller->Refresh();

	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::UpdateIssue;
	Mutation.TargetId = TEXT("NFB-1");
	Mutation.Fields.Add(TEXT("summary"), TEXT("Optimistic"));
	Controller->ExecuteMutation(Mutation);

	TestEqual(
		TEXT("Rejected mutation restores original summary"),
		Controller->GetSnapshot().Issues[0].Summary,
		FString(TEXT("Original")));
	TestEqual(TEXT("Conflict is retained"), Controller->GetLastMutationError().HttpStatus, 409);
	TestTrue(TEXT("Conflict produces a visible toast"), Controller->GetToast().IsSet());

	FExtendedAtlassianWorkspaceMutation PageMutation;
	PageMutation.Type = EExtendedAtlassianWorkspaceMutation::UpdatePage;
	PageMutation.TargetId = TEXT("page-1");
	PageMutation.Fields.Add(TEXT("title"), TEXT("Optimistic page"));
	PageMutation.Fields.Add(TEXT("body"), TEXT("# Optimistic\n"));
	PageMutation.Fields.Add(TEXT("version"), TEXT("14"));
	Controller->ExecuteMutation(PageMutation);
	TestEqual(
		TEXT("Rejected page publish restores title"),
		Controller->GetSnapshot().Pages[0].Title,
		FString(TEXT("Original page")));
	TestEqual(
		TEXT("Rejected page publish restores Markdown"),
		Controller->GetSnapshot().Pages[0].Markdown,
		FString(TEXT("# Original\n")));
	TestEqual(
		TEXT("Rejected page publish restores base version"),
		Controller->GetSnapshot().Pages[0].Version,
		14);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianPartialSuccessContractTest,
	"ExtendedAtlassian.Parity.PartialSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianPartialSuccessContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<ExtendedAtlassianParityTestsPrivate::FWarningWorkspaceData> Provider =
		MakeShared<ExtendedAtlassianParityTestsPrivate::FWarningWorkspaceData>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Provider);
	Controller->Refresh();

	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::UpdateIssue;
	Mutation.TargetId = TEXT("NFB-2");
	Mutation.Fields.Add(TEXT("summary"), TEXT("Created despite attachment warning"));
	Controller->ExecuteMutation(Mutation);

	TestEqual(
		TEXT("Partial success does not roll back created issue state"),
		Controller->GetSnapshot().Issues[0].Summary,
		FString(TEXT("Created despite attachment warning")));
	TestFalse(
		TEXT("Partial success is not retained as an error"),
		Controller->GetLastMutationError().IsSet());
	TestEqual(
		TEXT("Partial success retains a structured warning"),
		Controller->GetLastMutationWarning().Code,
		FString(TEXT("IssueCreatedAttachmentFailed")));
	TestTrue(TEXT("Partial success warning is visible"), Controller->GetToast().IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianIssueFilterContractTest,
	"ExtendedAtlassian.Parity.IssueFilters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianIssueFilterContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Fixture =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Fixture);
	Controller->Refresh();

	TestEqual(TEXT("Initial status filter is any"), Controller->GetStatusFilter(),
		FString(TEXT("any")));
	Controller->CycleStatusFilter();
	TestEqual(TEXT("Status cycles to not Done"), Controller->GetStatusFilter(),
		FString(TEXT("not Done")));
	Controller->CycleStatusFilter();
	TestEqual(TEXT("Status cycles to Done"), Controller->GetStatusFilter(),
		FString(TEXT("Done")));
	Controller->CycleStatusFilter();
	TestEqual(TEXT("Status cycles back to any"), Controller->GetStatusFilter(),
		FString(TEXT("any")));

	const FString ExpectedAssignees[] = {
		TEXT("AK"), TEXT("MR"), TEXT("JT"), TEXT("SO"), TEXT("LN"), TEXT("anyone")
	};
	for (const FString& Expected : ExpectedAssignees)
	{
		Controller->CycleAssigneeFilter();
		TestEqual(
			*FString::Printf(TEXT("Assignee cycle reaches %s"), *Expected),
			Controller->GetAssigneeFilter(),
			Expected);
	}

	Controller->SelectIssueView(TEXT("blocked"));
	TestEqual(TEXT("View selection is retained"), Controller->GetSelectedIssueViewId(),
		FString(TEXT("blocked")));
	TestEqual(TEXT("View selection resets assignee to anyone"),
		Controller->GetAssigneeFilter(), FString(TEXT("anyone")));
	Controller->ToggleEpicFilter(TEXT("Rendering"));
	TestEqual(TEXT("Epic filter selects"), Controller->GetEpicFilter(),
		FString(TEXT("Rendering")));
	Controller->ToggleEpicFilter(TEXT("Rendering"));
	TestTrue(TEXT("Epic filter toggles off"), Controller->GetEpicFilter().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianIssueDetailContractTest,
	"ExtendedAtlassian.Parity.IssueDetailOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianIssueDetailContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Fixture =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Fixture);
	Controller->Refresh();
	Controller->OpenIssue(TEXT("NFB-1042"));
	const int32 SeededThreadCount =
		Controller->GetSnapshot().IssueThreads.Num();
	const int32 SeededActivityCount =
		Controller->GetSnapshot().Activity.Num();
	TestEqual(TEXT("Detail fixture has exact three independent threads"),
		SeededThreadCount, 3);
	TestEqual(TEXT("Detail fixture has exact four activities"),
		SeededActivityCount, 4);
	TestEqual(
		TEXT("Default selected-thread contract retains second fixture card"),
		Controller->GetSnapshot().IssueThreads[1].Label,
		FString(TEXT("STREAMING")));
	TestEqual(
		TEXT("Newest fixture activity is first"),
		Controller->GetSnapshot().Activity[0].RelativeTime,
		FString(TEXT("8m")));

	FExtendedAtlassianWorkspaceMutation Edit;
	Edit.Type = EExtendedAtlassianWorkspaceMutation::UpdateIssue;
	Edit.TargetId = TEXT("NFB-1042");
	Edit.Fields.Add(TEXT("summary"), TEXT("Edited fog streaming issue"));
	Edit.Fields.Add(TEXT("description"), TEXT("Edited description"));
	Controller->ExecuteMutation(Edit);
	const FExtendedAtlassianIssue* Issue =
		Controller->GetSnapshot().Issues.FindByPredicate(
			[](const FExtendedAtlassianIssue& Candidate)
			{
				return Candidate.Key == TEXT("NFB-1042");
			});
	TestNotNull(TEXT("Edited detail issue remains"), Issue);
	if (Issue)
	{
		TestEqual(TEXT("Detail summary edits"), Issue->Summary,
			FString(TEXT("Edited fog streaming issue")));
		TestEqual(TEXT("Detail description edits"), Issue->Description,
			FString(TEXT("Edited description")));
	}

	const struct
	{
		const TCHAR* Field;
		const TCHAR* Value;
	} PropertyMutations[] = {
		{ TEXT("assignee"), TEXT("JT") },
		{ TEXT("epic"), TEXT("Rendering") },
		{ TEXT("priority"), TEXT("LOW") },
		{ TEXT("points"), TEXT("13") },
	};
	for (const auto& Property : PropertyMutations)
	{
		FExtendedAtlassianWorkspaceMutation Mutation;
		Mutation.Type = EExtendedAtlassianWorkspaceMutation::UpdateIssue;
		Mutation.TargetId = TEXT("NFB-1042");
		Mutation.Fields.Add(Property.Field, Property.Value);
		Controller->ExecuteMutation(Mutation);
	}
	FExtendedAtlassianWorkspaceMutation Status;
	Status.Type = EExtendedAtlassianWorkspaceMutation::TransitionIssue;
	Status.TargetId = TEXT("NFB-1042");
	Status.Fields.Add(TEXT("status"), TEXT("In review"));
	Controller->ExecuteMutation(Status);
	Issue = Controller->GetSnapshot().Issues.FindByPredicate(
		[](const FExtendedAtlassianIssue& Candidate)
		{
			return Candidate.Key == TEXT("NFB-1042");
		});
	if (Issue)
	{
		TestEqual(TEXT("Status property mutates"), Issue->StatusName,
			FString(TEXT("In review")));
		TestEqual(TEXT("Assignee property mutates"), Issue->AssigneeAccountId,
			FString(TEXT("JT")));
		TestEqual(TEXT("Epic property mutates"), Issue->EpicName,
			FString(TEXT("Rendering")));
		TestEqual(TEXT("Priority property mutates"), Issue->PriorityName,
			FString(TEXT("LOW")));
		TestEqual(TEXT("Points property mutates"), Issue->Estimate, 13.0);
	}

	FExtendedAtlassianWorkspaceMutation Comment;
	Comment.Type = EExtendedAtlassianWorkspaceMutation::CreateIssueComment;
	Comment.TargetId = TEXT("detail-comment");
	Comment.Fields.Add(TEXT("target"), TEXT("issue:NFB-1042"));
	Comment.Fields.Add(TEXT("body"), TEXT("Root detail comment"));
	Controller->ExecuteMutation(Comment);
	FExtendedAtlassianWorkspaceMutation Reply = Comment;
	Reply.TargetId = TEXT("detail-reply");
	Reply.ParentId = TEXT("detail-comment");
	Reply.Fields.Add(TEXT("body"), TEXT("Threaded detail reply"));
	Controller->ExecuteMutation(Reply);
	FExtendedAtlassianWorkspaceMutation Resolve;
	Resolve.Type =
		EExtendedAtlassianWorkspaceMutation::ResolveIssueComment;
	Resolve.TargetId = TEXT("detail-comment");
	Resolve.Fields.Add(TEXT("target"), TEXT("issue:NFB-1042"));
	Controller->ExecuteMutation(Resolve);
	const FExtendedAtlassianCommentCollection* Collection =
		Controller->GetSnapshot().CommentCollections.FindByPredicate(
			[](const FExtendedAtlassianCommentCollection& Candidate)
			{
				return Candidate.TargetId == TEXT("issue:NFB-1042");
			});
	TestNotNull(TEXT("Issue comment collection remains"), Collection);
	if (Collection)
	{
		const FExtendedAtlassianComment* Created =
			Collection->Comments.FindByPredicate(
				[](const FExtendedAtlassianComment& Candidate)
				{
					return Candidate.Id == TEXT("detail-comment");
				});
		TestNotNull(TEXT("Root issue comment creates"), Created);
		if (Created)
		{
			TestTrue(TEXT("Issue comment resolves"), Created->bResolved);
			TestEqual(TEXT("Issue reply stays threaded"), Created->Replies.Num(), 1);
		}
	}
	TestEqual(
		TEXT("Ordinary comments never replace viewport threads"),
		Controller->GetSnapshot().IssueThreads.Num(),
		SeededThreadCount);
	TestTrue(
		TEXT("Issue edits, properties and comments append activity"),
		Controller->GetSnapshot().Activity.Num() > SeededActivityCount);

	FExtendedAtlassianWorkspaceMutation Delete;
	Delete.Type = EExtendedAtlassianWorkspaceMutation::DeleteIssueComment;
	Delete.TargetId = TEXT("detail-comment");
	Delete.Fields.Add(TEXT("target"), TEXT("issue:NFB-1042"));
	Controller->ExecuteDestructiveMutation(
		Delete,
		FText::FromString(TEXT("Comment deleted")));
	TestTrue(TEXT("Issue-comment delayed delete offers Undo"),
		Controller->GetToast().bOffersUndo);
	TestTrue(TEXT("Issue-comment delete Undo restores"),
		Controller->UndoLastDestructiveMutation());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianBoardOperationContractTest,
	"ExtendedAtlassian.Parity.BoardOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianBoardOperationContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Fixture =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Fixture);
	Controller->Refresh();

	TestEqual(TEXT("Board has exactly four presentation columns"),
		Controller->GetSnapshot().BoardColumns.Num(), 4);
	const TCHAR* ExpectedColumns[] = {
		TEXT("Triage"), TEXT("In progress"), TEXT("In review"), TEXT("Done")
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedColumns); ++Index)
	{
		TestEqual(
			FString::Printf(TEXT("Board column %d has authored name"), Index),
			Controller->GetSnapshot().BoardColumns[Index].DisplayName,
			FString(ExpectedColumns[Index]));
	}
	TestTrue(
		TEXT("Blocked maps to Triage presentation"),
		Controller->GetSnapshot().BoardColumns[0].StatusNames.Contains(
			TEXT("Blocked")));
	TestEqual(TEXT("Fixture selected sprint is explicit"),
		Controller->GetSnapshot().SelectedSprintId, FString(TEXT("24")));
	TestEqual(TEXT("Fixture sidebar preserves authored days-left"),
		Controller->GetSnapshot().SprintSummary.DaysLeft,
		FString(TEXT("4d LEFT")));
	TestEqual(TEXT("Fixture sidebar preserves authored done percentage"),
		Controller->GetSnapshot().SprintSummary.DoneFraction, 0.44);
	TestEqual(TEXT("Fixture team threshold follows HTML at 100 percent"),
		Controller->GetSnapshot().TeamLoad[1].ThresholdColor,
		FString(TEXT("#f0665f")));

	FExtendedAtlassianWorkspaceMutation Move;
	Move.Type = EExtendedAtlassianWorkspaceMutation::MoveIssue;
	Move.TargetId = TEXT("NFB-1051");
	Move.Fields.Add(TEXT("previousStatus"), TEXT("Triage"));
	Move.Fields.Add(TEXT("status"), TEXT("In progress"));
	Move.OrderedIds.Add(TEXT("NFB-1051"));
	for (const FExtendedAtlassianIssue& Issue : Controller->GetSnapshot().Issues)
	{
		if (Issue.Key != Move.TargetId)
		{
			Move.OrderedIds.Add(Issue.Key);
		}
	}
	Controller->ExecuteMutation(Move);
	const FExtendedAtlassianIssue* Moved =
		Controller->GetSnapshot().Issues.FindByPredicate(
			[](const FExtendedAtlassianIssue& Issue)
			{
				return Issue.Key == TEXT("NFB-1051");
			});
	TestNotNull(TEXT("Board transition retains card"), Moved);
	if (Moved)
	{
		TestEqual(TEXT("Board transition changes status"), Moved->StatusName,
			FString(TEXT("In progress")));
	}
	int32 InProgressCount = 0;
	for (const FExtendedAtlassianIssue& Issue : Controller->GetSnapshot().Issues)
	{
		InProgressCount += Issue.StatusName == TEXT("In progress") ? 1 : 0;
	}
	TestEqual(TEXT("Fourth in-progress card breaches WIP 3 threshold"), InProgressCount, 4);
	TestEqual(TEXT("Atomic board move changes deterministic order"),
		Controller->GetSnapshot().Issues[0].Key, FString(TEXT("NFB-1051")));

	FExtendedAtlassianWorkspaceMutation Create;
	Create.Type = EExtendedAtlassianWorkspaceMutation::CreateIssue;
	Create.TargetId = TEXT("NFB-1065");
	Create.Fields.Add(TEXT("summary"), TEXT("New board card"));
	Create.Fields.Add(TEXT("type"), TEXT("Task"));
	Create.Fields.Add(TEXT("status"), TEXT("Triage"));
	Create.Fields.Add(TEXT("priority"), TEXT("MEDIUM"));
	Create.Fields.Add(TEXT("assignee"), TEXT("AK"));
	Create.Fields.Add(TEXT("epic"), TEXT("Systems"));
	Create.Fields.Add(TEXT("points"), TEXT("3"));
	Controller->ExecuteMutation(Create);
	TestEqual(TEXT("Board card creates"), Controller->GetSnapshot().Issues.Num(), 13);

	FExtendedAtlassianWorkspaceMutation Update;
	Update.Type = EExtendedAtlassianWorkspaceMutation::UpdateIssue;
	Update.TargetId = TEXT("NFB-1065");
	Update.Fields.Add(TEXT("summary"), TEXT("Edited board card"));
	Controller->ExecuteMutation(Update);
	TestEqual(TEXT("Board card edits"), Controller->GetSnapshot().Issues[0].Summary,
		FString(TEXT("Edited board card")));

	FExtendedAtlassianWorkspaceMutation Delete;
	Delete.Type = EExtendedAtlassianWorkspaceMutation::DeleteIssue;
	Delete.TargetId = TEXT("NFB-1065");
	Controller->ExecuteDestructiveMutation(Delete, FText::FromString(TEXT("Card deleted")));
	TestEqual(TEXT("Board card delete is optimistic"),
		Controller->GetSnapshot().Issues.Num(), 12);
	TestTrue(TEXT("Board card delete offers undo"), Controller->UndoLastDestructiveMutation());
	TestEqual(TEXT("Board card undo restores card"),
		Controller->GetSnapshot().Issues.Num(), 13);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianBoardPartialRankContractTest,
	"ExtendedAtlassian.Parity.BoardPartialRank",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianBoardPartialRankContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<ExtendedAtlassianParityTestsPrivate::FPartialRankWorkspaceData>
		Provider =
			MakeShared<
				ExtendedAtlassianParityTestsPrivate::FPartialRankWorkspaceData>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Provider);
	Controller->Refresh();

	FExtendedAtlassianWorkspaceMutation Move;
	Move.Type = EExtendedAtlassianWorkspaceMutation::MoveIssue;
	Move.TargetId = TEXT("NFB-11");
	Move.Fields.Add(TEXT("previousStatus"), TEXT("Triage"));
	Move.Fields.Add(TEXT("status"), TEXT("In progress"));
	Move.OrderedIds = { TEXT("NFB-11"), TEXT("NFB-10") };
	Controller->ExecuteMutation(Move);

	const FExtendedAtlassianIssue* Moved =
		Controller->GetSnapshot().Issues.FindByPredicate(
			[](const FExtendedAtlassianIssue& Issue)
			{
				return Issue.Key == TEXT("NFB-11");
			});
	TestNotNull(TEXT("Partially moved issue remains"), Moved);
	if (Moved)
	{
		TestEqual(
			TEXT("Accepted transition remains after rank failure"),
			Moved->StatusName,
			FString(TEXT("In progress")));
	}
	TestEqual(
		TEXT("Rejected optimistic order reconciles to Jira's retained rank"),
		Controller->GetSnapshot().Issues[0].Key,
		FString(TEXT("NFB-10")));
	TestEqual(
		TEXT("Partial rank reports a structured warning"),
		Controller->GetLastMutationWarning().Code,
		FString(TEXT("PartialRank")));
	TestTrue(
		TEXT("Partial rank explanation is visible"),
		Controller->GetToast().IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianDocumentMutationContractTest,
	"ExtendedAtlassian.Parity.DocumentMutations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianDocumentMutationContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Fixture =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	const TSharedRef<ExtendedAtlassianParityTestsPrivate::FManualClock> Clock =
		MakeShared<ExtendedAtlassianParityTestsPrivate::FManualClock>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Fixture, Clock);
	Controller->Refresh();

	FExtendedAtlassianWorkspaceMutation CreateSection;
	CreateSection.Type = EExtendedAtlassianWorkspaceMutation::CreateSection;
	CreateSection.TargetId = TEXT("sec-test");
	CreateSection.Fields.Add(TEXT("title"), TEXT("Test section"));
	Controller->ExecuteMutation(CreateSection);
	TestEqual(TEXT("Section appends"), Controller->GetSnapshot().DocumentTree.Num(), 18);
	TestTrue(
		TEXT("Created section is expanded"),
		Controller->GetSnapshot().DocumentTree.Last().bExpanded);

	FExtendedAtlassianWorkspaceMutation CreatePage;
	CreatePage.Type = EExtendedAtlassianWorkspaceMutation::CreatePage;
	CreatePage.TargetId = TEXT("new-test");
	CreatePage.ParentId = TEXT("sec-test");
	CreatePage.Fields.Add(TEXT("title"), TEXT("Test page"));
	Controller->ExecuteMutation(CreatePage);
	Controller->SelectPage(TEXT("new-test"));
	TestEqual(TEXT("Page creates"), Controller->GetSnapshot().Pages.Num(), 13);
	TestEqual(TEXT("Page tree row creates"), Controller->GetSnapshot().DocumentTree.Num(), 19);
	TestEqual(
		TEXT("Page is inserted under section"),
		Controller->GetSnapshot().DocumentTree.Last().ParentId,
		FString(TEXT("sec-test")));

	const FString AllBlockMarkdown =
		TEXT("## Heading\n\n")
		TEXT("A paragraph with **bold** text.\n\n")
		TEXT("> A production callout.\n\n")
		TEXT("1. Linked rule NFB-1042\n\n")
		TEXT("| PARAM | RANGE | DEFAULT | OWNER |\n")
		TEXT("| --- | --- | --- | --- |\n")
		TEXT("| Tide | 0..1 | 0.5 | AK |\n\n")
		TEXT("```cpp TideController.cpp\nfloat Tide = 0.5f;\n```\n\n")
		TEXT("- [ ] Validate the harbour\n\n")
		TEXT("![M_WetStone](/Game/Art/M_WetStone \"MATERIAL THUMB|2048² · 4 INST\")\n");
	FExtendedAtlassianWorkspaceMutation Publish;
	Publish.Type = EExtendedAtlassianWorkspaceMutation::UpdatePage;
	Publish.TargetId = TEXT("new-test");
	Publish.Fields.Add(TEXT("title"), TEXT("Published test page"));
	Publish.Fields.Add(TEXT("body"), AllBlockMarkdown);
	Publish.Fields.Add(TEXT("version"), TEXT("1"));
	Controller->ExecuteMutation(Publish);
	const FExtendedAtlassianPage* Published =
		Controller->GetSnapshot().Pages.FindByPredicate(
			[](const FExtendedAtlassianPage& Page)
			{
				return Page.Id == TEXT("new-test");
			});
	TestNotNull(TEXT("Published page remains selected by stable id"), Published);
	if (Published)
	{
		TestEqual(TEXT("Fixture publish increments version exactly once"), Published->Version, 2);
		TestEqual(TEXT("Fixture publish stamps editor"), Published->EditedByLabel,
			FString(TEXT("A. KWAN")));
		TestEqual(TEXT("Fixture publish stamps relative time"), Published->EditedAtLabel,
			FString(TEXT("JUST NOW")));
		TestEqual(TEXT("Fixture publish retains clean Markdown"), Published->Markdown,
			AllBlockMarkdown);
		auto HasKind = [Published](EExtendedAtlassianBlockKind Kind)
		{
			return Published->Blocks.ContainsByPredicate(
				[Kind](const FExtendedAtlassianDocBlock& Block)
				{
					return Block.Kind == Kind;
				});
		};
		TestTrue(TEXT("Publish hydrates heading block"),
			HasKind(EExtendedAtlassianBlockKind::Heading));
		TestTrue(TEXT("Publish hydrates paragraph block"),
			HasKind(EExtendedAtlassianBlockKind::Paragraph));
		TestTrue(TEXT("Publish hydrates callout block"),
			HasKind(EExtendedAtlassianBlockKind::Quote));
		TestTrue(TEXT("Publish hydrates ordered-rule block"),
			HasKind(EExtendedAtlassianBlockKind::OrderedItem));
		TestTrue(TEXT("Publish hydrates table blocks"),
			HasKind(EExtendedAtlassianBlockKind::TableRow));
		TestTrue(TEXT("Publish hydrates code block"),
			HasKind(EExtendedAtlassianBlockKind::CodeBlock));
		const FExtendedAtlassianDocBlock* CodeBlock =
			Published->Blocks.FindByPredicate(
				[](const FExtendedAtlassianDocBlock& Block)
				{
					return Block.Kind == EExtendedAtlassianBlockKind::CodeBlock;
				});
		TestNotNull(TEXT("Published code block is present"), CodeBlock);
		if (CodeBlock)
		{
			TestEqual(TEXT("Code language round-trips"), CodeBlock->CodeLanguage,
				FString(TEXT("cpp")));
			TestEqual(TEXT("Code file round-trips"), CodeBlock->ImageAlt,
				FString(TEXT("TideController.cpp")));
		}
		TestTrue(TEXT("Publish hydrates to-do block"),
			HasKind(EExtendedAtlassianBlockKind::TaskItem));
		TestTrue(TEXT("Publish hydrates asset-embed block"),
			HasKind(EExtendedAtlassianBlockKind::Image));
		const FExtendedAtlassianDocBlock* AssetBlock =
			Published->Blocks.FindByPredicate(
				[](const FExtendedAtlassianDocBlock& Block)
				{
					return Block.Kind == EExtendedAtlassianBlockKind::Image;
				});
		TestNotNull(TEXT("Published asset block is present"), AssetBlock);
		if (AssetBlock)
		{
			TestEqual(TEXT("Asset stable path round-trips"), AssetBlock->ImageUrl,
				FString(TEXT("/Game/Art/M_WetStone")));
			TestEqual(TEXT("Asset slot round-trips"), AssetBlock->EmbedSlot,
				FString(TEXT("MATERIAL THUMB")));
			TestEqual(TEXT("Asset display metadata round-trips"), AssetBlock->ImageMeta,
				FString(TEXT("2048² · 4 INST")));
		}
		const FString RoundTripped = FExtendedAtlassianMarkdown::FromBlocks(
			FExtendedAtlassianMarkdown::ToBlocks(Published->Markdown));
		TestEqual(
			TEXT("Structured Markdown serialization is idempotent"),
			FExtendedAtlassianMarkdown::FromBlocks(
				FExtendedAtlassianMarkdown::ToBlocks(RoundTripped)),
			RoundTripped);
	}
	const FExtendedAtlassianDocumentTreeNode* PublishedNode =
		Controller->GetSnapshot().DocumentTree.FindByPredicate(
			[](const FExtendedAtlassianDocumentTreeNode& Node)
			{
				return Node.Id == TEXT("new-test");
			});
	TestNotNull(TEXT("Published page retains a tree row"), PublishedNode);
	if (PublishedNode)
	{
		TestEqual(TEXT("Publish renames the tree row"), PublishedNode->Label,
			FString(TEXT("Published test page")));
	}
	const int32 TaskIndex = Controller->GetSnapshot().Pages[
		Controller->GetSnapshot().Pages.IndexOfByPredicate(
			[](const FExtendedAtlassianPage& Page)
			{
				return Page.Id == TEXT("new-test");
			})].Blocks.IndexOfByPredicate(
				[](const FExtendedAtlassianDocBlock& Block)
				{
					return Block.Kind
						== EExtendedAtlassianBlockKind::TaskItem;
				});
	FExtendedAtlassianWorkspaceMutation ToggleTask;
	ToggleTask.Type = EExtendedAtlassianWorkspaceMutation::TogglePageTask;
	ToggleTask.TargetId = TEXT("new-test");
	ToggleTask.Fields.Add(TEXT("blockIndex"), FString::FromInt(TaskIndex));
	ToggleTask.Fields.Add(TEXT("title"), TEXT("Published test page"));
	ToggleTask.Fields.Add(TEXT("body"), AllBlockMarkdown);
	ToggleTask.Fields.Add(TEXT("version"), TEXT("2"));
	Controller->ExecuteMutation(ToggleTask);
	const FExtendedAtlassianPage* TaskPage =
		Controller->GetSnapshot().Pages.FindByPredicate(
			[](const FExtendedAtlassianPage& Page)
			{
				return Page.Id == TEXT("new-test");
			});
	TestNotNull(TEXT("Task page remains available"), TaskPage);
	if (TaskPage)
	{
		TestTrue(TEXT("Read-mode task toggle persists immediately"),
			TaskPage->Blocks[TaskIndex].bChecked);
		TestEqual(TEXT("Fixture task toggle does not increment page version"),
			TaskPage->Version, 2);
	}

	FExtendedAtlassianWorkspaceMutation RenameSection;
	RenameSection.Type = EExtendedAtlassianWorkspaceMutation::RenameSection;
	RenameSection.TargetId = TEXT("sec-test");
	RenameSection.Fields.Add(TEXT("title"), TEXT("Renamed section"));
	Controller->ExecuteMutation(RenameSection);
	TestEqual(
		TEXT("Section renames"),
		Controller->GetSnapshot().DocumentTree[17].Label,
		FString(TEXT("Renamed section")));

	FExtendedAtlassianWorkspaceMutation Duplicate;
	Duplicate.Type = EExtendedAtlassianWorkspaceMutation::DuplicatePage;
	Duplicate.TargetId = TEXT("new-test");
	Duplicate.Fields.Add(TEXT("newId"), TEXT("new-copy"));
	Duplicate.Fields.Add(TEXT("title"), TEXT("Test page copy"));
	Controller->ExecuteMutation(Duplicate);
	TestEqual(TEXT("Page duplicate creates content"), Controller->GetSnapshot().Pages.Num(), 14);
	TestEqual(
		TEXT("Duplicate follows source"),
		Controller->GetSnapshot().DocumentTree[19].Id,
		FString(TEXT("new-copy")));

	FExtendedAtlassianWorkspaceMutation DeleteSection;
	DeleteSection.Type = EExtendedAtlassianWorkspaceMutation::DeleteSection;
	DeleteSection.TargetId = TEXT("sec-test");
	Controller->ExecuteDestructiveMutation(
		DeleteSection,
		FText::FromString(TEXT("Section deleted")));
	TestEqual(TEXT("Section delete removes children"), Controller->GetSnapshot().Pages.Num(), 12);
	TestNotEqual(
		TEXT("Section delete selects a safe page"),
		Controller->GetSelectedPageId(),
		FString(TEXT("new-test")));
	TestTrue(TEXT("Section delete Undo succeeds"), Controller->UndoLastDestructiveMutation());
	TestEqual(TEXT("Section Undo restores pages"), Controller->GetSnapshot().Pages.Num(), 14);
	TestEqual(
		TEXT("Section Undo restores selection"),
		Controller->GetSelectedPageId(),
		FString(TEXT("new-test")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianCommentMutationContractTest,
	"ExtendedAtlassian.Parity.CommentMutations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianCommentMutationContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Fixture =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	const TSharedRef<ExtendedAtlassianParityTestsPrivate::FManualClock> Clock =
		MakeShared<ExtendedAtlassianParityTestsPrivate::FManualClock>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Fixture, Clock);
	Controller->Refresh();

	auto WetCollection = [&Controller]()
	{
		return Controller->GetSnapshot().CommentCollections.FindByPredicate(
			[](const FExtendedAtlassianCommentCollection& Collection)
			{
				return Collection.TargetId == TEXT("page:wet");
			});
	};
	auto WetTreeNode = [&Controller]()
	{
		return Controller->GetSnapshot().DocumentTree.FindByPredicate(
			[](const FExtendedAtlassianDocumentTreeNode& Node)
			{
				return Node.Id == TEXT("wet");
			});
	};

	TestEqual(TEXT("Wet page starts with three comment threads"), WetCollection()->Comments.Num(), 3);
	TestEqual(TEXT("Wet page starts with two open badges"), WetTreeNode()->CommentBadge, 2);

	FExtendedAtlassianWorkspaceMutation Reply;
	Reply.Type = EExtendedAtlassianWorkspaceMutation::CreatePageComment;
	Reply.TargetId = TEXT("comment-test-reply");
	Reply.ParentId = TEXT("c1");
	Reply.Fields.Add(TEXT("target"), TEXT("page:wet"));
	Reply.Fields.Add(TEXT("body"), TEXT("A fixture reply"));
	Controller->ExecuteMutation(Reply);
	TestEqual(TEXT("Reply appends under parent"), WetCollection()->Comments[0].Replies.Num(), 3);
	TestEqual(TEXT("Reply does not change top-level badge"), WetTreeNode()->CommentBadge, 2);

	FExtendedAtlassianWorkspaceMutation Resolve;
	Resolve.Type = EExtendedAtlassianWorkspaceMutation::ResolvePageComment;
	Resolve.TargetId = TEXT("c1");
	Controller->ExecuteMutation(Resolve);
	TestTrue(TEXT("Resolve updates the comment"), WetCollection()->Comments[0].bResolved);
	TestEqual(TEXT("Resolve decrements open badge"), WetTreeNode()->CommentBadge, 1);

	FExtendedAtlassianWorkspaceMutation Update;
	Update.Type = EExtendedAtlassianWorkspaceMutation::UpdatePageComment;
	Update.TargetId = TEXT("c1");
	Update.Fields.Add(TEXT("body"), TEXT("Edited fixture comment"));
	Controller->ExecuteMutation(Update);
	TestEqual(
		TEXT("Edit updates body"),
		WetCollection()->Comments[0].Body,
		FString(TEXT("Edited fixture comment")));

	FExtendedAtlassianWorkspaceMutation Delete;
	Delete.Type = EExtendedAtlassianWorkspaceMutation::DeletePageComment;
	Delete.TargetId = TEXT("c2");
	Controller->ExecuteDestructiveMutation(Delete, FText::FromString(TEXT("Comment deleted")));
	TestEqual(TEXT("Delete removes a top-level thread"), WetCollection()->Comments.Num(), 2);
	TestTrue(TEXT("Comment delete Undo succeeds"), Controller->UndoLastDestructiveMutation());
	TestEqual(TEXT("Undo restores a top-level thread"), WetCollection()->Comments.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianPinMutationContractTest,
	"ExtendedAtlassian.Parity.PinMutations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianPinMutationContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Fixture =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	const TSharedRef<ExtendedAtlassianParityTestsPrivate::FManualClock> Clock =
		MakeShared<ExtendedAtlassianParityTestsPrivate::FManualClock>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Fixture, Clock);
	Controller->Refresh();

	auto FindPin = [&Controller](const FString& Id)
	{
		return Controller->GetSnapshot().Pins.FindByPredicate(
			[&Id](const FExtendedAtlassianPin& Pin)
			{
				return Pin.Id == Id;
			});
	};

	FExtendedAtlassianWorkspaceMutation Create;
	Create.Type = EExtendedAtlassianWorkspaceMutation::CreatePin;
	Create.TargetId = TEXT("/Game/Maps/L_Test");
	Create.Fields.Add(TEXT("name"), TEXT("L_Test"));
	Create.Fields.Add(TEXT("stableId"), TEXT("/Game/Maps/L_Test"));
	Create.Fields.Add(TEXT("kind"), TEXT("LEVEL"));
	Create.Fields.Add(TEXT("color"), TEXT("#58a6ff"));
	Controller->ExecuteMutation(Create);
	TestEqual(TEXT("Pin create prepends"), Controller->GetSnapshot().Pins.Num(), 7);
	TestEqual(
		TEXT("Created pin is first"),
		Controller->GetSnapshot().Pins[0].Id,
		FString(TEXT("/Game/Maps/L_Test")));
	TestEqual(
		TEXT("Created pin keeps target kind"),
		static_cast<uint8>(Controller->GetSnapshot().Pins[0].Target.Kind),
		static_cast<uint8>(EExtendedAtlassianPinKind::Level));

	FExtendedAtlassianWorkspaceMutation Rename;
	Rename.Type = EExtendedAtlassianWorkspaceMutation::UpdatePin;
	Rename.TargetId = TEXT("/Game/Maps/L_Test");
	Rename.Fields.Add(TEXT("name"), TEXT("L_Test_Renamed"));
	Controller->ExecuteMutation(Rename);
	TestEqual(
		TEXT("Rename preserves stable id and changes display name"),
		FindPin(TEXT("/Game/Maps/L_Test"))->DisplayName,
		FString(TEXT("L_Test_Renamed")));

	FExtendedAtlassianWorkspaceMutation Reply;
	Reply.Type = EExtendedAtlassianWorkspaceMutation::CreatePinReply;
	Reply.ParentId = TEXT("/Game/Maps/L_Test");
	Reply.TargetId = TEXT("pin-thread-test");
	Reply.Fields.Add(TEXT("body"), TEXT("Fixture pin reply"));
	Controller->ExecuteMutation(Reply);
	TestEqual(TEXT("Reply appends"), FindPin(TEXT("/Game/Maps/L_Test"))->Threads.Num(), 1);

	FExtendedAtlassianWorkspaceMutation Edit;
	Edit.Type = EExtendedAtlassianWorkspaceMutation::UpdatePinReply;
	Edit.TargetId = TEXT("pin-thread-test");
	Edit.Fields.Add(TEXT("body"), TEXT("Edited pin reply"));
	Controller->ExecuteMutation(Edit);
	TestEqual(
		TEXT("Reply edit applies"),
		FindPin(TEXT("/Game/Maps/L_Test"))->Threads[0].Body,
		FString(TEXT("Edited pin reply")));

	FExtendedAtlassianWorkspaceMutation Resolve;
	Resolve.Type = EExtendedAtlassianWorkspaceMutation::ResolvePinReply;
	Resolve.TargetId = TEXT("pin-thread-test");
	Controller->ExecuteMutation(Resolve);
	TestTrue(
		TEXT("Reply resolve toggles on"),
		FindPin(TEXT("/Game/Maps/L_Test"))->Threads[0].bResolved);
	Controller->ExecuteMutation(Resolve);
	TestFalse(
		TEXT("Reply resolve toggles back to reopen"),
		FindPin(TEXT("/Game/Maps/L_Test"))->Threads[0].bResolved);

	FExtendedAtlassianWorkspaceMutation DeleteReply;
	DeleteReply.Type = EExtendedAtlassianWorkspaceMutation::DeletePinReply;
	DeleteReply.TargetId = TEXT("pin-thread-test");
	Controller->ExecuteDestructiveMutation(
		DeleteReply,
		FText::FromString(TEXT("Reply deleted")));
	TestEqual(
		TEXT("Reply delete applies optimistically"),
		FindPin(TEXT("/Game/Maps/L_Test"))->Threads.Num(),
		0);
	TestTrue(TEXT("Reply delete Undo succeeds"), Controller->UndoLastDestructiveMutation());
	TestEqual(
		TEXT("Reply delete Undo restores"),
		FindPin(TEXT("/Game/Maps/L_Test"))->Threads.Num(),
		1);

	FExtendedAtlassianWorkspaceMutation DeletePin;
	DeletePin.Type = EExtendedAtlassianWorkspaceMutation::DeletePin;
	DeletePin.TargetId = TEXT("/Game/Maps/L_Test");
	Controller->ExecuteDestructiveMutation(
		DeletePin,
		FText::FromString(TEXT("Pin removed")));
	TestEqual(TEXT("Pin delete applies optimistically"), Controller->GetSnapshot().Pins.Num(), 6);
	TestTrue(TEXT("Pin delete Undo succeeds"), Controller->UndoLastDestructiveMutation());
	TestEqual(TEXT("Pin delete Undo restores"), Controller->GetSnapshot().Pins.Num(), 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianInboxMutationContractTest,
	"ExtendedAtlassian.Parity.InboxMutations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianInboxMutationContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Fixture =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	const TSharedRef<ExtendedAtlassianParityTestsPrivate::FManualClock> Clock =
		MakeShared<ExtendedAtlassianParityTestsPrivate::FManualClock>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Fixture, Clock);
	Controller->Refresh();

	FExtendedAtlassianWorkspaceMutation Read;
	Read.Type = EExtendedAtlassianWorkspaceMutation::MarkNotificationRead;
	Read.TargetId = TEXT("fixture-inbox-0");
	Controller->ExecuteMutation(Read);
	TestEqual(TEXT("Single read decrements unread badge"), Controller->GetUnreadCount(), 3);
	TestTrue(TEXT("Selected row becomes read"), Controller->GetSnapshot().Notifications[0].bRead);

	FExtendedAtlassianWorkspaceMutation Dismiss;
	Dismiss.Type = EExtendedAtlassianWorkspaceMutation::DismissNotification;
	Dismiss.TargetId = TEXT("fixture-inbox-0");
	Controller->ExecuteDestructiveMutation(
		Dismiss,
		FText::FromString(TEXT("Notification dismissed")));
	TestEqual(
		TEXT("Dismiss removes notification optimistically"),
		Controller->GetSnapshot().Notifications.Num(),
		7);
	TestTrue(TEXT("Dismiss Undo succeeds"), Controller->UndoLastDestructiveMutation());
	TestEqual(
		TEXT("Dismiss Undo restores notification"),
		Controller->GetSnapshot().Notifications.Num(),
		8);

	int32 InitiallyRead = 0;
	for (const FExtendedAtlassianNotification& Notification :
		Controller->GetSnapshot().Notifications)
	{
		InitiallyRead += Notification.bRead ? 1 : 0;
	}
	FExtendedAtlassianWorkspaceMutation Archive;
	Archive.Type = EExtendedAtlassianWorkspaceMutation::ArchiveNotifications;
	Controller->ExecuteDestructiveMutation(
		Archive,
		FText::FromString(TEXT("Notifications archived")));
	int32 Archived = 0;
	for (const FExtendedAtlassianNotification& Notification :
		Controller->GetSnapshot().Notifications)
	{
		Archived += Notification.bArchived ? 1 : 0;
	}
	TestEqual(TEXT("Archive marks every read row"), Archived, InitiallyRead);
	TestTrue(TEXT("Archive Undo succeeds"), Controller->UndoLastDestructiveMutation());
	for (const FExtendedAtlassianNotification& Notification :
		Controller->GetSnapshot().Notifications)
	{
		TestFalse(TEXT("Archive Undo clears archived state"), Notification.bArchived);
	}

	FExtendedAtlassianWorkspaceMutation ReadAll;
	ReadAll.Type = EExtendedAtlassianWorkspaceMutation::MarkAllNotificationsRead;
	Controller->ExecuteMutation(ReadAll);
	TestEqual(TEXT("Mark all clears unread badge"), Controller->GetUnreadCount(), 0);
	for (const FExtendedAtlassianNotification& Notification :
		Controller->GetSnapshot().Notifications)
	{
		TestTrue(TEXT("Mark all updates every row"), Notification.bRead);
	}

	Controller->SetInboxTab(TEXT("Pins"));
	TestEqual(TEXT("Inbox tab state persists"), Controller->GetInboxTab(), FString(TEXT("Pins")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianInboxSynthesisContractTest,
	"ExtendedAtlassian.Parity.InboxSynthesis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianInboxSynthesisContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	FExtendedAtlassianWorkspaceSnapshot Snapshot;
	Snapshot.CurrentUser.AccountId = TEXT("current-account");
	Snapshot.CurrentUser.DisplayName = TEXT("Ada Kwan");

	FExtendedAtlassianIssue Issue;
	Issue.Key = TEXT("AK-42");
	Issue.Summary = TEXT("Ship stable event synthesis");
	Issue.AssigneeAccountId = Snapshot.CurrentUser.AccountId;
	Issue.ReporterDisplayName = TEXT("Rin Alvarez");
	Issue.Updated = FDateTime(2026, 7, 29, 10, 30);
	Issue.RelativeUpdated = TEXT("2m");
	Snapshot.Issues.Add(Issue);

	FExtendedAtlassianPin Pin;
	Pin.Id = TEXT("pin-1");
	Pin.DisplayName = TEXT("M_WetStone_Master");
	Pin.Version = 4;
	FExtendedAtlassianPinThread Thread;
	Thread.Id = TEXT("pin-message-1");
	Thread.AuthorAccountId = TEXT("other-account");
	Thread.AuthorDisplayName = TEXT("Mira Chen");
	Thread.Body = TEXT("Please check this material before review.");
	Thread.Created = FDateTime(2026, 7, 29, 10, 31);
	Thread.Updated = Thread.Created;
	Thread.RelativeTime = TEXT("1m");
	Pin.Threads.Add(Thread);
	Snapshot.Pins.Add(Pin);

	FExtendedAtlassianInboxUserState State;
	FExtendedAtlassianInboxState::SynthesizeAndApply(Snapshot, State);
	TestTrue(TEXT("First-run Inbox cursor initializes"), State.bInitialized);
	TestEqual(TEXT("Two normalized sources produce two events"), Snapshot.Notifications.Num(), 2);
	for (const FExtendedAtlassianNotification& Event : Snapshot.Notifications)
	{
		TestTrue(TEXT("First-run history is read"), Event.bRead);
		TestTrue(TEXT("Stable event ID has namespace"), Event.Id.StartsWith(TEXT("evt-")));
	}
	const TArray<FString> FirstIds = {
		Snapshot.Notifications[0].Id,
		Snapshot.Notifications[1].Id
	};

	FExtendedAtlassianWorkspaceSnapshot SameSnapshot;
	SameSnapshot.CurrentUser = Snapshot.CurrentUser;
	SameSnapshot.Issues.Add(Issue);
	SameSnapshot.Pins.Add(Pin);
	FExtendedAtlassianInboxState::SynthesizeAndApply(SameSnapshot, State);
	TestEqual(TEXT("Repeated poll does not duplicate events"), SameSnapshot.Notifications.Num(), 2);
	TestEqual(TEXT("Repeated poll preserves first stable ID"), SameSnapshot.Notifications[0].Id, FirstIds[0]);
	TestEqual(TEXT("Repeated poll preserves second stable ID"), SameSnapshot.Notifications[1].Id, FirstIds[1]);

	Issue.Updated = FDateTime(2026, 7, 29, 10, 35);
	FExtendedAtlassianWorkspaceSnapshot ChangedSnapshot;
	ChangedSnapshot.CurrentUser = Snapshot.CurrentUser;
	ChangedSnapshot.Issues.Add(Issue);
	ChangedSnapshot.Pins.Add(Pin);
	FExtendedAtlassianInboxState::SynthesizeAndApply(ChangedSnapshot, State);
	const FExtendedAtlassianNotification* UpdatedIssueEvent =
		ChangedSnapshot.Notifications.FindByPredicate(
			[](const FExtendedAtlassianNotification& Event)
			{
				return Event.SourceId == TEXT("AK-42");
			});
	TestNotNull(TEXT("Changed issue emits an event"), UpdatedIssueEvent);
	if (UpdatedIssueEvent)
	{
		TestFalse(TEXT("New revision is unread"), UpdatedIssueEvent->bRead);
		FExtendedAtlassianWorkspaceMutation Mute;
		Mute.Type = EExtendedAtlassianWorkspaceMutation::MuteNotification;
		Mute.TargetId = UpdatedIssueEvent->Id;
		FExtendedAtlassianInboxState::ApplyMutation(
			Mute,
			ChangedSnapshot.Notifications,
			State);
		TestTrue(TEXT("Mute persists read state"), State.ReadEventIds.Contains(Mute.TargetId));
		TestTrue(TEXT("Mute persists mute state"), State.MutedEventIds.Contains(Mute.TargetId));
	}
	TestTrue(TEXT("Jira cursor advances"), State.SourceCursors.Contains(TEXT("jira")));
	TestTrue(TEXT("Pins cursor advances"), State.SourceCursors.Contains(TEXT("pins")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianWorkspaceFaultStatesContractTest,
	"ExtendedAtlassian.Parity.WorkspaceFaultStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianWorkspaceFaultStatesContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace ExtendedAtlassianParityTestsPrivate;

	const TSharedRef<FScriptedWorkspaceData> Data =
		MakeShared<FScriptedWorkspaceData>();
	TSharedPtr<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Data);
	Controller->Refresh();
	TestEqual(TEXT("Initial load is queued"), Data->PendingLoads.Num(), 1);

	FExtendedAtlassianWorkspaceSnapshot Ready;
	Ready.State = EExtendedAtlassianLoadState::Ready;
	Ready.CurrentUser.AccountId = TEXT("account-ada");
	Ready.CurrentUser.DisplayName = TEXT("Ada Kwan");
	Ready.CurrentUser.Initials = TEXT("AK");
	FExtendedAtlassianIssue ReadyIssue;
	ReadyIssue.Id = TEXT("NFB-42");
	ReadyIssue.Key = ReadyIssue.Id;
	ReadyIssue.Summary = TEXT("Cached issue remains visible");
	Ready.Issues.Add(ReadyIssue);
	FExtendedAtlassianPage ReadyPage;
	ReadyPage.Id = TEXT("wet");
	ReadyPage.Title = TEXT("Cached page remains visible");
	Ready.Pages.Add(ReadyPage);
	Data->CompleteLoad(0, Ready);
	Controller->SelectIssue(TEXT("NFB-42"));
	Controller->SetGlobalSearch(TEXT("retained query"));

	Controller->Refresh();
	TestTrue(
		TEXT("Background refresh is explicit"),
		Controller->GetSnapshot().bRefreshing);
	TestEqual(
		TEXT("Background refresh retains issue content"),
		Controller->GetSnapshot().Issues.Num(),
		1);

	FExtendedAtlassianWorkspaceSnapshot Offline;
	Offline.State = EExtendedAtlassianLoadState::Offline;
	Offline.Error.Code = TEXT("Network");
	Offline.Error.Message = TEXT("Connection timed out.");
	Offline.Error.bRetryable = true;
	Data->CompleteLoad(0, Offline);
	TestEqual(
		TEXT("Failed background refresh keeps ready state"),
		Controller->GetSnapshot().State,
		EExtendedAtlassianLoadState::Ready);
	TestTrue(
		TEXT("Failed background refresh labels cached data stale"),
		Controller->GetSnapshot().bStale);
	TestFalse(
		TEXT("Failed background refresh clears refreshing state"),
		Controller->GetSnapshot().bRefreshing);
	TestEqual(
		TEXT("Transient failure preserves issue selection"),
		Controller->GetSelectedIssueKey(),
		FString(TEXT("NFB-42")));
	TestEqual(
		TEXT("Transient failure preserves search draft"),
		Controller->GetGlobalSearch(),
		FString(TEXT("retained query")));

	Controller->Refresh();
	Data->CompleteLoad(0, Ready);
	TestFalse(
		TEXT("Successful reconciliation clears stale state"),
		Controller->GetSnapshot().bStale);

	struct FFaultCase
	{
		const TCHAR* Name;
		const TCHAR* Code;
		int32 HttpStatus;
		EExtendedAtlassianLoadState State;
		bool bRetryable;
	};
	const FFaultCase Cases[] = {
		{ TEXT("Timeout"), TEXT("Network"), 0, EExtendedAtlassianLoadState::Offline, true },
		{ TEXT("Disconnect"), TEXT("Network"), 0, EExtendedAtlassianLoadState::Offline, true },
		{ TEXT("401"), TEXT("Unauthorized"), 401, EExtendedAtlassianLoadState::Error, false },
		{ TEXT("403"), TEXT("Forbidden"), 403, EExtendedAtlassianLoadState::PermissionDenied, false },
		{ TEXT("404"), TEXT("NotFound"), 404, EExtendedAtlassianLoadState::Error, false },
		{ TEXT("409"), TEXT("Conflict"), 409, EExtendedAtlassianLoadState::Error, false },
		{ TEXT("429"), TEXT("RateLimited"), 429, EExtendedAtlassianLoadState::Error, true },
		{ TEXT("500"), TEXT("ServerError"), 500, EExtendedAtlassianLoadState::Error, true },
		{ TEXT("Malformed"), TEXT("MalformedJson"), 200, EExtendedAtlassianLoadState::Error, false },
	};
	for (const FFaultCase& Fault : Cases)
	{
		const TSharedRef<FScriptedWorkspaceData> FaultData =
			MakeShared<FScriptedWorkspaceData>();
		const TSharedRef<FExtendedAtlassianWorkspaceController> FaultController =
			MakeShared<FExtendedAtlassianWorkspaceController>(FaultData);
		FaultController->Refresh();
		FExtendedAtlassianWorkspaceSnapshot Failed;
		Failed.State = Fault.State;
		Failed.Error.Code = Fault.Code;
		Failed.Error.HttpStatus = Fault.HttpStatus;
		Failed.Error.bRetryable = Fault.bRetryable;
		Failed.Error.Message =
			Fault.HttpStatus == 429
				? TEXT("Retry after 17 seconds.")
				: FString::Printf(TEXT("%s injected failure."), Fault.Name);
		FaultData->CompleteLoad(0, Failed);
		TestEqual(
			FString::Printf(TEXT("%s state"), Fault.Name),
			FaultController->GetSnapshot().State,
			Fault.State);
		TestEqual(
			FString::Printf(TEXT("%s code"), Fault.Name),
			FaultController->GetSnapshot().Error.Code,
			FString(Fault.Code));
		TestEqual(
			FString::Printf(TEXT("%s retryability"), Fault.Name),
			FaultController->GetSnapshot().Error.bRetryable,
			Fault.bRetryable);
	}

	const TSharedRef<FScriptedWorkspaceData> PermissionData =
		MakeShared<FScriptedWorkspaceData>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> PermissionController =
		MakeShared<FExtendedAtlassianWorkspaceController>(PermissionData);
	PermissionController->Refresh();
	FExtendedAtlassianWorkspaceSnapshot Restricted = Ready;
	Restricted.Capabilities.bCanCreateIssues = false;
	Restricted.Capabilities.bCanEditIssues = true;
	PermissionData->CompleteLoad(0, Restricted);
	FText PermissionReason;
	TestFalse(
		TEXT("Only denied create operation is disabled"),
		PermissionController->CanExecuteMutation(
			EExtendedAtlassianWorkspaceMutation::CreateIssue,
			&PermissionReason));
	TestTrue(
		TEXT("Denied operation provides an actionable tooltip reason"),
		PermissionReason.ToString().Contains(TEXT("Create Issues")));
	TestTrue(
		TEXT("Independent edit operation remains enabled"),
		PermissionController->CanExecuteMutation(
			EExtendedAtlassianWorkspaceMutation::UpdateIssue));
	FExtendedAtlassianWorkspaceMutation DeniedCreate;
	DeniedCreate.Type = EExtendedAtlassianWorkspaceMutation::CreateIssue;
	DeniedCreate.Fields.Add(TEXT("summary"), TEXT("Must not reach provider"));
	PermissionController->ExecuteMutation(DeniedCreate);
	TestEqual(
		TEXT("Denied operation never reaches provider"),
		PermissionData->RecordedMutations.Num(),
		0);
	TestEqual(
		TEXT("Denied operation exposes normalized forbidden error"),
		PermissionController->GetLastMutationError().HttpStatus,
		403);
	FExtendedAtlassianWorkspaceMutation AllowedEdit;
	AllowedEdit.Type = EExtendedAtlassianWorkspaceMutation::UpdateIssue;
	AllowedEdit.TargetId = TEXT("NFB-42");
	AllowedEdit.Fields.Add(TEXT("summary"), TEXT("Allowed edit"));
	PermissionController->ExecuteMutation(AllowedEdit);
	TestEqual(
		TEXT("Allowed operation reaches provider"),
		PermissionData->RecordedMutations.Num(),
		1);

	const TSharedRef<FScriptedWorkspaceData> OrderedData =
		MakeShared<FScriptedWorkspaceData>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> OrderedController =
		MakeShared<FExtendedAtlassianWorkspaceController>(OrderedData);
	OrderedController->Refresh();
	OrderedController->Refresh();
	TestEqual(TEXT("Two generations are queued"), OrderedData->PendingLoads.Num(), 2);
	FExtendedAtlassianWorkspaceSnapshot Old;
	Old.State = EExtendedAtlassianLoadState::Ready;
	FExtendedAtlassianIssue OldIssue;
	OldIssue.Key = TEXT("OLD-1");
	Old.Issues.Add(OldIssue);
	OrderedData->CompleteLoad(0, Old);
	TestTrue(
		TEXT("Stale callback cannot replace current generation"),
		OrderedController->GetSnapshot().Issues.IsEmpty());
	FExtendedAtlassianWorkspaceSnapshot New;
	New.State = EExtendedAtlassianLoadState::Ready;
	FExtendedAtlassianIssue NewIssue;
	NewIssue.Key = TEXT("NEW-2");
	New.Issues.Add(NewIssue);
	OrderedData->CompleteLoad(0, New);
	TestEqual(
		TEXT("Current generation completes normally"),
		OrderedController->GetSnapshot().Issues[0].Key,
		FString(TEXT("NEW-2")));

	const TSharedRef<FScriptedWorkspaceData> ShutdownData =
		MakeShared<FScriptedWorkspaceData>();
	{
		TSharedPtr<FExtendedAtlassianWorkspaceController> ShutdownController =
			MakeShared<FExtendedAtlassianWorkspaceController>(ShutdownData);
		ShutdownController->Refresh();
		ShutdownController.Reset();
	}
	TestEqual(
		TEXT("Controller shutdown cancels its live generation"),
		ShutdownData->CancelledGenerations.Num(),
		1);
	ShutdownData->CompleteLoad(0, Ready);
	TestTrue(
		TEXT("Late callback after shutdown is safely unbound"),
		true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianCaptureAnnotationContractTest,
	"ExtendedAtlassian.Parity.CaptureAnnotations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianCaptureAnnotationContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const FIntPoint Size(96, 64);
	TArray<FColor> Pixels;
	Pixels.Init(FColor(25, 30, 38, 255), Size.X * Size.Y);
	IImageWrapperModule& ImageWrapperModule =
		FModuleManager::LoadModuleChecked<IImageWrapperModule>(
			TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> Encoder =
		ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	TestTrue(
		TEXT("Fixture PNG encoder accepts pixels"),
		Encoder.IsValid()
			&& Encoder->SetRaw(
				Pixels.GetData(),
				Pixels.Num() * sizeof(FColor),
				Size.X,
				Size.Y,
				ERGBFormat::BGRA,
				8));
	if (!Encoder.IsValid())
	{
		return false;
	}
	const TArray64<uint8>& Compressed = Encoder->GetCompressed(100);
	TArray<uint8> SourcePng;
	SourcePng.Append(Compressed.GetData(), Compressed.Num());

	TArray<FExtendedAtlassianAnnotation> Annotations;
	FExtendedAtlassianAnnotation Pin;
	Pin.Id = TEXT("pin");
	Pin.Kind = EExtendedAtlassianAnnotationKind::Pin;
	Pin.NormalizedPosition = FVector2D(0.25f, 0.5f);
	Pin.ColorIndex = 0;
	Annotations.Add(Pin);
	FExtendedAtlassianAnnotation Box;
	Box.Id = TEXT("box");
	Box.Kind = EExtendedAtlassianAnnotationKind::Box;
	Box.NormalizedPosition = FVector2D(0.55f, 0.5f);
	Box.ColorIndex = 1;
	Annotations.Add(Box);
	FExtendedAtlassianAnnotation Blur;
	Blur.Id = TEXT("blur");
	Blur.Kind = EExtendedAtlassianAnnotationKind::Blur;
	Blur.NormalizedPosition = FVector2D(0.8f, 0.5f);
	Blur.ColorIndex = 2;
	Annotations.Add(Blur);

	TArray<uint8> AnnotatedPng;
	TestTrue(
		TEXT("PIN BOX BLUR burn into upload copy"),
		FExtendedAtlassianScreenshot::BurnAnnotations(
			SourcePng,
			Size,
			Annotations,
			AnnotatedPng));
	TestTrue(TEXT("Annotated PNG is non-empty"), !AnnotatedPng.IsEmpty());
	TestTrue(
		TEXT("Original capture bytes remain distinct"),
		AnnotatedPng != SourcePng);

	const TSharedPtr<IImageWrapper> Decoder =
		ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	TestTrue(
		TEXT("Annotated PNG remains decodable"),
		Decoder.IsValid()
			&& Decoder->SetCompressed(
				AnnotatedPng.GetData(),
				AnnotatedPng.Num()));
	TArray64<uint8> AnnotatedRaw;
	TestTrue(
		TEXT("Annotated PNG preserves BGRA payload"),
		Decoder.IsValid()
			&& Decoder->GetRaw(ERGBFormat::BGRA, 8, AnnotatedRaw));
	TestEqual(
		TEXT("Annotated PNG preserves dimensions"),
		static_cast<int32>(AnnotatedRaw.Num()),
		Size.X * Size.Y * static_cast<int32>(sizeof(FColor)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianCaptureFixtureContractTest,
	"ExtendedAtlassian.Parity.CaptureFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianCaptureFixtureContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Fixture =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Fixture);
	Controller->Refresh();
	Controller->SelectIssueView(TEXT("mine"));
	Controller->CycleStatusFilter();
	Controller->CycleAssigneeFilter();
	Controller->ToggleEpicFilter(TEXT("harbour"));

	FExtendedAtlassianWorkspaceMutation Capture;
	Capture.Type = EExtendedAtlassianWorkspaceMutation::CreateCaptureIssue;
	Capture.TargetId = TEXT("NFB-1065");
	Capture.Fields.Add(TEXT("summary"), TEXT("Viewport rain seam"));
	Capture.Fields.Add(TEXT("type"), TEXT("Bug"));
	Capture.Fields.Add(TEXT("status"), TEXT("Triage"));
	Capture.Fields.Add(TEXT("priority"), TEXT("HIGH"));
	Capture.Fields.Add(TEXT("assignee"), TEXT("AK"));
	Capture.Fields.Add(TEXT("epic"), TEXT("Harbour District"));
	Capture.Fields.Add(TEXT("points"), TEXT("3"));
	Capture.Fields.Add(TEXT("annotationCount"), TEXT("3"));
	Capture.Fields.Add(TEXT("pinCount"), TEXT("1"));
	Capture.Fields.Add(TEXT("boxCount"), TEXT("1"));
	Capture.Fields.Add(TEXT("blurCount"), TEXT("1"));
	Controller->ExecuteMutation(Capture);
	Controller->ResetIssueFilters();
	Controller->SelectIssue(TEXT("NFB-1065"));

	const FExtendedAtlassianWorkspaceSnapshot& Snapshot = Controller->GetSnapshot();
	TestEqual(TEXT("Capture creates next deterministic fixture key"), Snapshot.Issues[0].Key,
		FString(TEXT("NFB-1065")));
	TestEqual(TEXT("Capture retains typed summary"), Snapshot.Issues[0].Summary,
		FString(TEXT("Viewport rain seam")));
	TestEqual(TEXT("Capture retains issue type"), Snapshot.Issues[0].IssueTypeName,
		FString(TEXT("Bug")));
	TestEqual(TEXT("Capture uses Triage"), Snapshot.Issues[0].StatusName,
		FString(TEXT("Triage")));
	TestEqual(TEXT("Capture uses uppercase priority"), Snapshot.Issues[0].PriorityName,
		FString(TEXT("HIGH")));
	TestEqual(TEXT("Capture assigns current fixture user"), Snapshot.Issues[0].AssigneeAccountId,
		FString(TEXT("AK")));
	TestEqual(TEXT("Capture uses selected epic"), Snapshot.Issues[0].EpicName,
		FString(TEXT("Harbour District")));
	TestEqual(TEXT("Capture uses three story points"), Snapshot.Issues[0].Estimate, 3.0);
	TestEqual(TEXT("Capture annotation thread count"), Snapshot.Issues[0].CommentCount, 1);

	const FExtendedAtlassianCommentCollection* CaptureComments =
		Snapshot.CommentCollections.FindByPredicate(
			[](const FExtendedAtlassianCommentCollection& Collection)
			{
				return Collection.TargetId == TEXT("issue:NFB-1065");
			});
	TestNotNull(TEXT("Capture emits annotation comment"), CaptureComments);
	if (CaptureComments)
	{
		TestEqual(TEXT("Capture emits one summary comment"), CaptureComments->Comments.Num(), 1);
		TestTrue(
			TEXT("Capture comment names all annotation tools"),
			CaptureComments->Comments[0].Body.Contains(TEXT("pin"))
				&& CaptureComments->Comments[0].Body.Contains(TEXT("box"))
				&& CaptureComments->Comments[0].Body.Contains(TEXT("blur")));
	}
	const FExtendedAtlassianActivity* CaptureActivity =
		Snapshot.Activity.FindByPredicate(
			[](const FExtendedAtlassianActivity& Activity)
			{
				return Activity.IssueKey == TEXT("NFB-1065");
			});
	TestNotNull(TEXT("Capture emits activity"), CaptureActivity);
	TestEqual(TEXT("Capture selects created issue"), Controller->GetSelectedIssueKey(),
		FString(TEXT("NFB-1065")));
	TestEqual(TEXT("Capture resets issue view"), Controller->GetSelectedIssueViewId(),
		FString(TEXT("sprint")));
	TestEqual(TEXT("Capture resets status filter"), Controller->GetStatusFilter(),
		FString(TEXT("any")));
	TestEqual(TEXT("Capture resets assignee filter"), Controller->GetAssigneeFilter(),
		FString(TEXT("anyone")));
	TestTrue(TEXT("Capture clears epic filter"), Controller->GetEpicFilter().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianBacklotStoreContractTest,
	"ExtendedAtlassian.Parity.BacklotStore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianBacklotStoreContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FExtendedAtlassianPinTarget Target;
	Target.Kind = EExtendedAtlassianPinKind::Blueprint;
	Target.StableId = TEXT("/Game/Systems/BP_TideController.BP_TideController");
	Target.DisplayName = TEXT("BP_TideController");
	const FString StableId = FExtendedAtlassianBacklotStore::MakeStablePinId(Target);
	TestEqual(TEXT("Stable Pin hash has SHA-256 length"), StableId.Len(), 64);
	TestEqual(
		TEXT("Stable Pin hash is deterministic"),
		FExtendedAtlassianBacklotStore::MakeStablePinId(Target),
		StableId);

	TArray<FExtendedAtlassianPin> Pins;
	FExtendedAtlassianError Error;
	FExtendedAtlassianPinStoreMutation Create;
	Create.Type = EExtendedAtlassianPinStoreMutation::CreatePin;
	Create.PinId = StableId;
	Create.DisplayName = Target.DisplayName;
	Create.Target = Target;
	Create.Color = TEXT("#57cc8a");
	TestTrue(
		TEXT("Shared store creates Pin"),
		FExtendedAtlassianBacklotStore::ApplyPinMutation(Pins, Create, Error));

	FExtendedAtlassianPinStoreMutation Reply;
	Reply.Type = EExtendedAtlassianPinStoreMutation::CreateMessage;
	Reply.PinId = StableId;
	Reply.MessageId = TEXT("message-1");
	Reply.Body = TEXT("The clock needs reseeding.");
	Reply.AuthorAccountId = TEXT("account-1");
	Reply.AuthorDisplayName = TEXT("Ada Kwan");
	TestTrue(
		TEXT("Shared store creates message"),
		FExtendedAtlassianBacklotStore::ApplyPinMutation(Pins, Reply, Error));

	const TSharedRef<FJsonObject> Previous = MakeShared<FJsonObject>();
	Previous->SetStringField(TEXT("futureField"), TEXT("preserve-me"));
	Previous->SetNumberField(TEXT("revision"), 8);
	const TSharedRef<FJsonObject> Envelope =
		FExtendedAtlassianBacklotStore::BuildPinsEnvelope(
			Pins,
			Previous,
			TEXT("account-1"));
	TestEqual(
		TEXT("Envelope increments revision"),
		static_cast<int32>(Envelope->GetNumberField(TEXT("revision"))),
		9);
	TestEqual(
		TEXT("Envelope preserves unknown top-level values"),
		Envelope->GetStringField(TEXT("futureField")),
		FString(TEXT("preserve-me")));

	TArray<FExtendedAtlassianPin> Parsed;
	TestTrue(
		TEXT("Shared Pin envelope round-trips"),
		FExtendedAtlassianBacklotStore::ParsePinsEnvelope(Envelope, Parsed, Error));
	TestEqual(TEXT("Round-trip Pin count"), Parsed.Num(), 1);
	TestEqual(TEXT("Round-trip message count"), Parsed[0].Threads.Num(), 1);
	TestEqual(
		TEXT("Round-trip target identity"),
		Parsed[0].Target.StableId,
		Target.StableId);

	FExtendedAtlassianPinStoreMutation Resolve;
	Resolve.Type = EExtendedAtlassianPinStoreMutation::ToggleResolved;
	Resolve.PinId = StableId;
	Resolve.MessageId = TEXT("message-1");
	Resolve.bResolved = true;
	TestTrue(
		TEXT("Shared store resolves message"),
		FExtendedAtlassianBacklotStore::ApplyPinMutation(Parsed, Resolve, Error));
	TestTrue(TEXT("Resolved state is absolute"), Parsed[0].Threads[0].bResolved);

	const FString CacheKey = TEXT("parity-pin-cache");
	TArray<FExtendedAtlassianPinStoreMutation> PendingCache;
	PendingCache.Add(Resolve);
	FString CacheError;
	TestTrue(
		TEXT("Pin cache and offline queue save"),
		FExtendedAtlassianBacklotStore::SavePinsCache(
			CacheKey,
			Parsed,
			PendingCache,
			CacheError));
	TArray<FExtendedAtlassianPin> CachedPins;
	TArray<FExtendedAtlassianPinStoreMutation> CachedPending;
	TestTrue(
		TEXT("Pin cache and offline queue load"),
		FExtendedAtlassianBacklotStore::LoadPinsCache(
			CacheKey,
			CachedPins,
			CachedPending,
			CacheError));
	TestEqual(TEXT("Pin cache preserves pins"), CachedPins.Num(), 1);
	TestEqual(TEXT("Pin cache preserves pending count"), CachedPending.Num(), 1);
	if (!CachedPending.IsEmpty())
	{
		TestTrue(
			TEXT("Offline resolution queue is idempotent and absolute"),
			CachedPending[0].bResolved);
	}
	IFileManager::Get().Delete(
		*FExtendedAtlassianBacklotStore::PinsCachePath(CacheKey),
		false,
		true);

	TArray<FExtendedAtlassianIssueCommentMetadata> CommentMetadata;
	FExtendedAtlassianIssueCommentMetadata ReplyMetadata;
	ReplyMetadata.IssueKey = TEXT("NFB-1042");
	ReplyMetadata.CommentId = TEXT("jira-comment-2");
	ReplyMetadata.ParentId = TEXT("jira-comment-1");
	ReplyMetadata.bResolved = true;
	ReplyMetadata.Updated = FDateTime(2026, 7, 29, 13, 0);
	CommentMetadata.Add(ReplyMetadata);
	const TSharedRef<FJsonObject> MetadataEnvelope =
		FExtendedAtlassianBacklotStore::BuildIssueCommentMetadataEnvelope(
			CommentMetadata,
			Previous,
			TEXT("account-1"));
	TestEqual(
		TEXT("Comment companion uses its reviewed schema"),
		MetadataEnvelope->GetStringField(TEXT("schema")),
		FString(TEXT("ue.backlot.issue-comments")));
	TestEqual(
		TEXT("Comment companion preserves unknown fields"),
		MetadataEnvelope->GetStringField(TEXT("futureField")),
		FString(TEXT("preserve-me")));
	TArray<FExtendedAtlassianIssueCommentMetadata> ParsedMetadata;
	TestTrue(
		TEXT("Comment companion envelope round-trips"),
		FExtendedAtlassianBacklotStore::ParseIssueCommentMetadataEnvelope(
			MetadataEnvelope,
			ParsedMetadata,
			Error));
	TestEqual(TEXT("Comment companion keeps one row"),
		ParsedMetadata.Num(), 1);
	if (!ParsedMetadata.IsEmpty())
	{
		TestEqual(
			TEXT("Comment companion stores only parent presentation metadata"),
			ParsedMetadata[0].ParentId,
			FString(TEXT("jira-comment-1")));
		TestTrue(
			TEXT("Comment companion stores resolution presentation metadata"),
			ParsedMetadata[0].bResolved);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianExpandedCodecContractTest,
	"ExtendedAtlassian.Parity.ExpandedCodecs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianExpandedCodecContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace ExtendedAtlassianParityTestsPrivate;

	UExtendedAtlassianSettings* Settings =
		GetMutableDefault<UExtendedAtlassianSettings>();
	const FString PreviousEstimateField = Settings->DiscoveredEstimateFieldId;
	const FString PreviousRankField = Settings->DiscoveredRankFieldId;
	Settings->DiscoveredEstimateFieldId = TEXT("customfield_10016");
	Settings->DiscoveredRankFieldId = TEXT("customfield_10019");

	const TSharedPtr<FJsonObject> IssueObject = ParseObject(TEXT(R"json(
		{
			"id":"10001","key":"NFB-42",
			"fields":{
				"summary":"Expanded issue",
				"status":{"id":"3","name":"In progress","statusCategory":{"key":"indeterminate"}},
				"issuetype":{"name":"Task","iconUrl":"https://example.invalid/task.svg"},
				"priority":{"name":"High"},
				"assignee":{"accountId":"account-1","displayName":"Ada","avatarUrls":{"48x48":"https://example.invalid/a.png"}},
				"reporter":{"displayName":"Grace"},
				"labels":["rendering","ue5"],
				"created":"2026-01-02T03:04:05.000+0000",
				"updated":"2026-01-03T04:05:06.000+0000",
				"parent":{"id":"900","key":"NFB-EPIC","fields":{"summary":"Harbour District","issuetype":{"name":"Epic"}}},
				"comment":{"total":7},
				"description":{"version":1,"type":"doc","content":[{"type":"paragraph","content":[{"type":"text","text":"ADF body"}]}]},
				"customfield_10016":8,
				"customfield_10019":"0|i0001:"
			}
		})json"));
	const FExtendedAtlassianIssue Issue =
		FExtendedAtlassianJira::ParseIssue(IssueObject);
	TestEqual(TEXT("Expanded issue parses key"), Issue.Key,
		FString(TEXT("NFB-42")));
	TestEqual(TEXT("Expanded issue parses status category"),
		Issue.StatusCategoryKey, FString(TEXT("indeterminate")));
	TestEqual(TEXT("Expanded issue parses avatar"),
		Issue.AssigneeAvatarUrl,
		FString(TEXT("https://example.invalid/a.png")));
	TestEqual(TEXT("Expanded issue parses parent epic"),
		Issue.EpicName, FString(TEXT("Harbour District")));
	TestEqual(TEXT("Expanded issue parses comments"), Issue.CommentCount, 7);
	TestEqual(TEXT("Expanded issue parses estimate"), Issue.Estimate, 8.0);
	TestEqual(TEXT("Expanded issue parses rank"), Issue.Rank,
		FString(TEXT("0|i0001:")));
	TestEqual(TEXT("Expanded issue flattens ADF"), Issue.Description,
		FString(TEXT("ADF body")));
	FExtendedAtlassianError Error;
	TArray<FExtendedAtlassianActivity> Changelog;
	int32 HistoryCount = 0;
	int32 ChangelogTotal = 0;
	TestTrue(TEXT("Changelog page parses"),
		FExtendedAtlassianJira::ParseChangelogPage(
			ParseObject(TEXT(R"json(
				{"total":2,"maxResults":1,"values":[
				 {"id":"history-1","created":"2026-01-03T04:05:06.000+0000",
				  "author":{"accountId":"account-1","displayName":"Ada"},
				  "items":[
				   {"field":"status","fromString":"Triage","toString":"In progress"},
				   {"field":"Story Points","fromString":"3","toString":"8"}
				  ]}
				]})json")),
			TEXT("NFB-42"),
			Changelog,
			HistoryCount,
			ChangelogTotal,
			Error));
	TestEqual(TEXT("Changelog parser exposes page history count"),
		HistoryCount, 1);
	TestEqual(TEXT("Changelog parser exposes total for paging"),
		ChangelogTotal, 2);
	TestEqual(TEXT("Changelog parser expands each item"),
		Changelog.Num(), 2);
	TestTrue(TEXT("Changelog parser normalizes status detail"),
		Changelog[0].Detail.Contains(TEXT("moved this")));
	TestTrue(TEXT("Changelog parser normalizes estimate detail"),
		Changelog[1].Detail.Contains(TEXT("8 points")));

	const FExtendedAtlassianUser User =
		FExtendedAtlassianJira::ParseUser(ParseObject(TEXT(R"json(
			{"accountId":"account-2","displayName":"Lin","emailAddress":"lin@example.invalid",
			 "avatarUrls":{"48x48":"https://example.invalid/lin.png"}})json")));
	TestEqual(TEXT("User parser keeps account id"), User.AccountId,
		FString(TEXT("account-2")));
	TestEqual(TEXT("User parser keeps avatar"), User.AvatarUrl,
		FString(TEXT("https://example.invalid/lin.png")));

	TArray<FExtendedAtlassianBoard> Boards;
	bool bIsLast = true;
	TestTrue(TEXT("Boards page parses"),
		FExtendedAtlassianJiraSoftware::ParseBoardsPage(
			ParseObject(TEXT(R"json(
				{"isLast":false,"values":[{"id":12,"name":"Backlot","type":"scrum",
			 "self":"https://example.invalid/board/12","location":{"projectKey":"NFB"}}]})json")),
			Boards,
			bIsLast,
			Error));
	TestFalse(TEXT("Boards parser exposes paging"), bIsLast);
	TestEqual(TEXT("Boards parser normalizes numeric id"), Boards[0].Id,
		FString(TEXT("12")));

	TArray<FExtendedAtlassianSprint> Sprints;
	TestTrue(TEXT("Sprints page parses"),
		FExtendedAtlassianJiraSoftware::ParseSprintsPage(
			ParseObject(TEXT(R"json(
				{"isLast":true,"values":[{"id":44,"name":"Sprint 44","state":"active",
			 "goal":"Polish","startDate":"2026-01-01T00:00:00.000Z",
			 "endDate":"2026-01-14T00:00:00.000Z"}]})json")),
			Sprints,
			bIsLast,
			Error));
	TestTrue(TEXT("Sprints parser completes paging"), bIsLast);
	TestEqual(TEXT("Sprints parser keeps state"), Sprints[0].State,
		FString(TEXT("active")));

	FExtendedAtlassianBoardConfiguration Configuration;
	TestTrue(TEXT("Board configuration parses"),
		FExtendedAtlassianJiraSoftware::ParseBoardConfiguration(
			ParseObject(TEXT(R"json(
				{
				 "columnConfig":{"columns":[{"name":"Triage","max":3,
				 "statuses":[{"id":"10000"},{"id":"10001"}]}]},
				 "estimation":{"field":{"fieldId":"customfield_10016","displayName":"Story points"}},
				 "ranking":{"rankCustomFieldId":10019}
				})json")),
			Configuration,
			Error));
	TestEqual(TEXT("Board configuration parses WIP"),
		Configuration.Columns[0].WipLimit, 3);
	TestEqual(TEXT("Board configuration parses estimate field"),
		Configuration.EstimateFieldId,
		FString(TEXT("customfield_10016")));
	TestEqual(TEXT("Board configuration normalizes rank field"),
		Configuration.RankFieldId,
		FString(TEXT("customfield_10019")));
	TestTrue(TEXT("Rank request serializes before anchor"),
		FExtendedAtlassianJiraSoftware::BuildRankBody(
			TEXT("NFB-42"),
			TEXT("NFB-41"),
			true).Contains(TEXT("\"rankBeforeIssue\":\"NFB-41\"")));
	TestTrue(TEXT("Estimate request serializes value"),
		FExtendedAtlassianJiraSoftware::BuildEstimateBody(TEXT("8"))
			.Contains(TEXT("\"value\":\"8\"")));
	FString EstimateValue;
	FString EstimateField;
	TestTrue(TEXT("Estimate response parses"),
		FExtendedAtlassianJiraSoftware::ParseEstimate(
			ParseObject(TEXT(R"json(
				{"value":"8","fieldId":"customfield_10016"})json")),
			EstimateValue,
			EstimateField,
			Error));
	TestEqual(TEXT("Estimate response keeps field id"), EstimateField,
		FString(TEXT("customfield_10016")));

	const FExtendedAtlassianComment Comment =
		FExtendedAtlassianConfluenceComments::ParseComment(
			ParseObject(TEXT(R"json(
				{
				 "id":"comment-1","parentCommentId":"comment-root",
				 "body":{"view":{"value":"<p>Inline <strong>review</strong></p>"}},
				 "version":{"number":4,"authorId":"account-3","createdAt":"2026-01-04T05:06:07.000Z"},
				 "resolutionStatus":"resolved",
				 "properties":{"inlineOriginalSelection":"selected shader code"},
				 "operations":{"results":[{"operation":"update"},{"operation":"delete"}]}
				})json")),
			TEXT("page-1"),
			true);
	TestEqual(TEXT("Confluence comment parser flattens body"), Comment.Body,
		FString(TEXT("Inline review")));
	TestTrue(TEXT("Confluence comment parser keeps resolved"), Comment.bResolved);
	TestTrue(TEXT("Confluence comment parser keeps permissions"),
		Comment.bCanEdit && Comment.bCanDelete);
	TestEqual(TEXT("Confluence comment parser keeps quote"), Comment.Quote,
		FString(TEXT("selected shader code")));

	const TSharedPtr<FJsonObject> PropertyObject = ParseObject(TEXT(R"json(
		{"id":"property-1","key":"ue.backlot.test","value":{"future":"keep"},
		 "version":{"number":9}})json"));
	const FExtendedAtlassianContentProperty Property =
		FExtendedAtlassianConfluenceProperties::ParseProperty(PropertyObject);
	TestEqual(TEXT("Property parser keeps version"), Property.Version, 9);
	TestTrue(TEXT("Property body serializes next version"),
		FExtendedAtlassianConfluenceProperties::BuildPropertyBody(
			Property.Key,
			Property.Value.ToSharedRef(),
			10).Contains(TEXT("\"number\":10")));

	FExtendedAtlassianInboxUserState InboxState;
	InboxState.bInitialized = true;
	InboxState.KnownEventIds.Add(TEXT("event-1"));
	InboxState.ReadEventIds.Add(TEXT("event-1"));
	InboxState.MutedEventIds.Add(TEXT("event-2"));
	InboxState.ArchivedEventIds.Add(TEXT("event-3"));
	InboxState.DismissedEventIds.Add(TEXT("event-4"));
	InboxState.SourceCursors.Add(TEXT("jira"), TEXT("cursor-9"));
	FString InboxJson;
	FString InboxError;
	TestTrue(TEXT("Inbox state serializes"),
		FExtendedAtlassianInboxState::Serialize(
			InboxState,
			InboxJson,
			InboxError));
	FExtendedAtlassianInboxUserState ParsedInboxState;
	TestTrue(TEXT("Inbox state deserializes"),
		FExtendedAtlassianInboxState::Deserialize(
			InboxJson,
			ParsedInboxState,
			InboxError));
	TestTrue(TEXT("Inbox codec preserves exactly-once identity sets"),
		ParsedInboxState.KnownEventIds.Contains(TEXT("event-1"))
			&& ParsedInboxState.ReadEventIds.Contains(TEXT("event-1"))
			&& ParsedInboxState.MutedEventIds.Contains(TEXT("event-2"))
			&& ParsedInboxState.ArchivedEventIds.Contains(TEXT("event-3"))
			&& ParsedInboxState.DismissedEventIds.Contains(TEXT("event-4")));
	TestEqual(TEXT("Inbox codec preserves source cursors"),
		ParsedInboxState.SourceCursors.FindRef(TEXT("jira")),
		FString(TEXT("cursor-9")));

	TArray<FExtendedAtlassianAnnotation> Annotations;
	FExtendedAtlassianAnnotation PinAnnotation;
	PinAnnotation.Id = TEXT("pin-1");
	PinAnnotation.Kind = EExtendedAtlassianAnnotationKind::Pin;
	PinAnnotation.NormalizedPosition = FVector2D(0.25f, 0.5f);
	PinAnnotation.ColorIndex = 2;
	Annotations.Add(PinAnnotation);
	FExtendedAtlassianAnnotation BoxAnnotation;
	BoxAnnotation.Id = TEXT("box-1");
	BoxAnnotation.Kind = EExtendedAtlassianAnnotationKind::Box;
	BoxAnnotation.NormalizedPosition = FVector2D(0.1f, 0.2f);
	BoxAnnotation.NormalizedSize = FVector2D(0.3f, 0.4f);
	Annotations.Add(BoxAnnotation);
	const FString AnnotationJson =
		FExtendedAtlassianScreenshot::SerializeAnnotations(Annotations);
	TArray<FExtendedAtlassianAnnotation> ParsedAnnotations;
	FString AnnotationError;
	TestTrue(TEXT("Annotation payload deserializes"),
		FExtendedAtlassianScreenshot::DeserializeAnnotations(
			AnnotationJson,
			ParsedAnnotations,
			AnnotationError));
	TestEqual(TEXT("Annotation codec preserves rows"),
		ParsedAnnotations.Num(), 2);
	TestEqual(TEXT("Annotation codec preserves box kind"),
		static_cast<int32>(ParsedAnnotations[1].Kind),
		static_cast<int32>(EExtendedAtlassianAnnotationKind::Box));
	TestTrue(TEXT("Annotation codec preserves normalized geometry"),
		ParsedAnnotations[1].NormalizedPosition.Equals(
			FVector2D(0.1f, 0.2f),
			KINDA_SMALL_NUMBER)
			&& ParsedAnnotations[1].NormalizedSize.Equals(
				FVector2D(0.3f, 0.4f),
				KINDA_SMALL_NUMBER));

	Settings->DiscoveredEstimateFieldId = PreviousEstimateField;
	Settings->DiscoveredRankFieldId = PreviousRankField;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianAutomationInfrastructureContractTest,
	"ExtendedAtlassian.Parity.AutomationInfrastructure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianAutomationInfrastructureContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace ExtendedAtlassianParityTestsPrivate;

	const TSharedRef<FScriptedWorkspaceData> Data =
		MakeShared<FScriptedWorkspaceData>();
	Data->bAutoCompleteMutations = false;
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Data);

	Controller->Refresh();
	Controller->Refresh();
	TestEqual(
		TEXT("Repeated refresh preserves both deliberately delayed requests"),
		Data->PendingLoads.Num(),
		2);
	TestTrue(
		TEXT("Superseded request generation is cancelled"),
		Data->CancelledGenerations.Contains(1));

	FExtendedAtlassianWorkspaceSnapshot Newer;
	Newer.State = EExtendedAtlassianLoadState::Ready;
	Newer.Capabilities = Data->GetCapabilities();
	FExtendedAtlassianIssue NewIssue;
	NewIssue.Id = TEXT("NFB-NEW");
	NewIssue.Key = NewIssue.Id;
	NewIssue.Summary = TEXT("New generation");
	NewIssue.IssueTypeId = TEXT("10001");
	NewIssue.IssueTypeName = TEXT("Bug");
	NewIssue.PriorityId = TEXT("3");
	NewIssue.PriorityName = TEXT("High");
	NewIssue.StatusName = TEXT("Triage");
	NewIssue.StatusId = TEXT("10000");
	NewIssue.StatusCategoryKey = TEXT("new");
	Newer.Issues.Add(NewIssue);
	FExtendedAtlassianIssue InProgressIssue = NewIssue;
	InProgressIssue.Id = TEXT("NFB-STATUS-CATALOG");
	InProgressIssue.Key = InProgressIssue.Id;
	InProgressIssue.StatusName = TEXT("In progress");
	InProgressIssue.StatusId = TEXT("10003");
	InProgressIssue.StatusCategoryKey = TEXT("indeterminate");
	Newer.Issues.Add(InProgressIssue);
	FExtendedAtlassianIssueType IssueType;
	IssueType.Id = TEXT("10001");
	IssueType.Name = TEXT("Bug");
	Newer.IssueTypes.Add(IssueType);
	FExtendedAtlassianPriority Priority;
	Priority.Id = TEXT("3");
	Priority.Name = TEXT("High");
	Newer.Priorities.Add(Priority);
	FExtendedAtlassianEpic Epic;
	Epic.Id = TEXT("10002");
	Epic.Name = TEXT("Harbour");
	Newer.Epics.Add(Epic);
	FExtendedAtlassianUser Assignee;
	Assignee.AccountId = TEXT("account-ak");
	Assignee.DisplayName = TEXT("A. Kwan");
	Assignee.Initials = TEXT("AK");
	Newer.People.Add(Assignee);
	Data->CompleteLoad(1, Newer);

	FExtendedAtlassianWorkspaceSnapshot Older = Newer;
	Older.Issues[0].Id = TEXT("NFB-OLD");
	Older.Issues[0].Key = TEXT("NFB-OLD");
	Older.Issues[0].Summary = TEXT("Old generation");
	Data->CompleteLoad(0, Older);
	TestEqual(
		TEXT("Out-of-order old response cannot mutate current state"),
		Controller->GetSnapshot().Issues[0].Key,
		FString(TEXT("NFB-NEW")));

	Controller->SelectIssue(TEXT("NFB-NEW"));
	const FString BeforeMutation = Controller->ExportNormalizedState();
	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::UpdateIssue;
	Mutation.TargetId = TEXT("NFB-NEW");
	Mutation.Fields.Add(TEXT("summary"), TEXT("Optimistic summary"));
	Mutation.Fields.Add(TEXT("type"), TEXT("Bug"));
	Mutation.Fields.Add(TEXT("priority"), TEXT("High"));
	Mutation.Fields.Add(TEXT("status"), TEXT("In progress"));
	Mutation.Fields.Add(TEXT("assignee"), TEXT("AK"));
	Mutation.Fields.Add(TEXT("epic"), TEXT("Harbour"));
	Controller->ExecuteMutation(Mutation);
	const FExtendedAtlassianWorkspaceMutation& StableMutation =
		Data->RecordedMutations.Last();
	TestEqual(
		TEXT("Issue type mutation carries stable catalog id"),
		StableMutation.Fields.FindRef(TEXT("typeId")),
		FString(TEXT("10001")));
	TestEqual(
		TEXT("Priority mutation carries stable catalog id"),
		StableMutation.Fields.FindRef(TEXT("priorityId")),
		FString(TEXT("3")));
	TestEqual(
		TEXT("Status mutation carries stable catalog id"),
		StableMutation.Fields.FindRef(TEXT("statusId")),
		FString(TEXT("10003")));
	TestEqual(
		TEXT("Assignee mutation carries stable account id"),
		StableMutation.Fields.FindRef(TEXT("assignee")),
		FString(TEXT("account-ak")));
	TestEqual(
		TEXT("Epic mutation carries stable issue id"),
		StableMutation.Fields.FindRef(TEXT("epicId")),
		FString(TEXT("10002")));
	TestTrue(
		TEXT("Normalized state reports optimistic text/status presentation"),
		Controller->ExportNormalizedState().Contains(
			TEXT("Optimistic summary,In progress,new")));
	FExtendedAtlassianError Failure;
	Failure.HttpStatus = 409;
	Failure.Code = TEXT("Conflict");
	Failure.Message = TEXT("deliberate rollback");
	Data->CompleteMutation(0, false, Failure);
	TestEqual(
		TEXT("Rollback restores selection, ordering, text, counts, and colors"),
		Controller->ExportNormalizedState(),
		BeforeMutation);

	Controller->ExecuteMutation(Mutation);
	Data->CompleteMutation(0, true);
	TestTrue(
		TEXT("Delayed success keeps optimistic state"),
		Controller->ExportNormalizedState().Contains(
			TEXT("Optimistic summary,In progress,new")));
	TestTrue(
		TEXT("In-memory provider records ordered load/cancel/mutation completion"),
		Data->OrderedCalls.Num() >= 8
			&& Data->OrderedCalls[0] == TEXT("load:1")
			&& Data->OrderedCalls.Contains(TEXT("cancel:1")));

	const TSharedRef<FInMemoryHostServices> Host =
		MakeShared<FInMemoryHostServices>();
	TArray<uint8> Png;
	FIntPoint Size;
	TestTrue(TEXT("Host fake captures without the real viewport"),
		Host->CaptureViewport(Png, Size));
	TestEqual(TEXT("Host fake capture width"), Size.X, 1920);
	TestEqual(TEXT("Host fake supplies deterministic time"),
		Host->NowSeconds(), 42.0);
	TestEqual(TEXT("Host fake context is deterministic"),
		Host->CaptureContext().LevelName, FString(TEXT("AutomationMap")));
	Host->CopyText(TEXT("NFB-NEW"));
	Host->OpenExternal(TEXT("https://example.invalid/NFB-NEW"));
	FExtendedAtlassianPinTarget Target;
	FText HostError;
	TestTrue(TEXT("Host fake resolves editor targets"),
		Host->ResolveCurrentTarget(
			EExtendedAtlassianPinKind::Page,
			TEXT("page-1"),
			TEXT("Page one"),
			Target,
			HostError));
	TestTrue(TEXT("Host fake reveals editor targets"),
		Host->RevealTarget(Target, HostError));
	TestEqual(TEXT("Host fake records ordered calls"),
		FString::Join(Host->Calls, TEXT("|")),
		FString(
			TEXT("capture|context|copy:NFB-NEW|")
			TEXT("open:https://example.invalid/NFB-NEW|")
			TEXT("resolve:page-1|reveal:page-1")));

	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> ServiceFixture =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	const TSharedPtr<IExtendedAtlassianWorkspaceData> ServiceFixtureData =
		StaticCastSharedRef<IExtendedAtlassianWorkspaceData>(ServiceFixture);
	const TSharedRef<SExtendedAtlassianWorkspace> ServiceWorkspace =
		SNew(SExtendedAtlassianWorkspace)
		.WorkspaceData(ServiceFixtureData)
		.InteractionClock(MakeShared<FManualClock>())
		.HostServices(Host);
	TestEqual(
		TEXT("Fixture workspace has no continuous idle timers"),
		ServiceWorkspace->ExportSchedulingStateForAutomation(),
		FString(TEXT(
			"interaction=0|searchDebounce=0|backgroundSync=0")));
	ServiceWorkspace->SetGlobalSearchForAutomation(TEXT("wet"));
	TestTrue(
		TEXT("Search uses a deferred trailing-edge timer"),
		ServiceWorkspace->ExportSchedulingStateForAutomation().Contains(
			TEXT("searchDebounce=1")));
	Host->bRevealSucceeds = false;
	ServiceWorkspace->RevealDocumentAssetForAutomation(
		TEXT("Missing material"),
		TEXT("/Game/Missing/M_Missing.M_Missing"));
	TestTrue(
		TEXT("Missing Content Browser assets surface an interaction toast"),
		ServiceWorkspace->ExportSchedulingStateForAutomation().Contains(
			TEXT("interaction=1")));
	Host->bRevealSucceeds = true;

	const TSharedRef<FInMemoryHostServices> HighContrastHost =
		MakeShared<FInMemoryHostServices>();
	HighContrastHost->bHighContrast = true;
	const TSharedRef<SExtendedAtlassianWorkspace> HighContrastWorkspace =
		SNew(SExtendedAtlassianWorkspace)
		.WorkspaceData(ServiceFixtureData)
		.InteractionClock(MakeShared<FManualClock>())
		.HostServices(HighContrastHost);
	TestTrue(
		TEXT("Windows high-contrast preference is represented by the root surface"),
		HighContrastWorkspace->IsHighContrastForAutomation());
	TestEqual(
		TEXT("High contrast disables decorative overlay motion"),
		HighContrastWorkspace->ExportOverlayAnimationProgressForAutomation(),
		1.0f);

	int32 ActionableCount = 0;
	for (int32 Iteration = 0; Iteration < 3; ++Iteration)
	{
		const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Fixture =
			MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
		const TSharedPtr<IExtendedAtlassianWorkspaceData> FixtureData =
			StaticCastSharedRef<IExtendedAtlassianWorkspaceData>(Fixture);
		const TSharedRef<SExtendedAtlassianWorkspace> Workspace =
			SNew(SExtendedAtlassianWorkspace)
			.WorkspaceData(FixtureData)
			.InteractionClock(MakeShared<FManualClock>())
			.HostServices(Host);
		Workspace->Navigate(
			Iteration == 0
				? EExtendedAtlassianWorkspaceRoute::Docs
				: EExtendedAtlassianWorkspaceRoute::Issues);
		Workspace->Refresh();
		if (Iteration == 0)
		{
			AuditActionableMetadata(*this, Workspace, ActionableCount);
			const TSharedPtr<SButton> BoardButton =
				FindButtonByAccessibleText(Workspace, TEXT("BOARD"));
			TestTrue(
				TEXT("Board navigation button is discoverable by accessible name"),
				BoardButton.IsValid());
			if (BoardButton.IsValid())
			{
				BoardButton->SimulateClick();
				TestTrue(
					TEXT("Mouse-equivalent button activation changes route"),
					Workspace->ExportNormalizedStateForAutomation().Contains(
						TEXT("route=3")));
			}

			Workspace->Navigate(EExtendedAtlassianWorkspaceRoute::Issues);
			const FString BeforeDown =
				Workspace->ExportNormalizedStateForAutomation();
			const FKeyEvent DownEvent(
				EKeys::Down,
				FModifierKeysState(),
				0,
				false,
				0,
				0);
			TestTrue(
				TEXT("Keyboard issue navigation handles Down"),
				Workspace->OnKeyDown(FGeometry(), DownEvent).IsEventHandled());
			TestNotEqual(
				TEXT("Keyboard issue navigation changes selection"),
				Workspace->ExportNormalizedStateForAutomation(),
				BeforeDown);
			const FKeyEvent EnterEvent(
				EKeys::Enter,
				FModifierKeysState(),
				0,
				false,
				0,
				0);
			TestTrue(
				TEXT("Enter opens selected issue detail"),
				Workspace->OnKeyDown(FGeometry(), EnterEvent).IsEventHandled());
			TestTrue(
				TEXT("Enter routes to Issue Detail"),
				Workspace->ExportNormalizedStateForAutomation().Contains(
					TEXT("route=2")));

			const int32 CaptureCallsBefore =
				Host->CountCalls(TEXT("capture"));
			const FModifierKeysState CtrlShift(
				true,
				false,
				true,
				false,
				false,
				false,
				false,
				false,
				false);
			const FKeyEvent CaptureShortcut(
				EKeys::B,
				CtrlShift,
				0,
				false,
				0,
				0);
			TestTrue(
				TEXT("Capture shortcut is handled"),
				Workspace->OnKeyDown(
					FGeometry(),
					CaptureShortcut).IsEventHandled());
			TestTrue(
				TEXT("Capture shortcut opens the modal"),
				Workspace->ExportOverlayStateForAutomation().Contains(
					TEXT("capture=1")));
			TestEqual(
				TEXT("Capture shortcut uses the injected viewport service"),
				Host->CountCalls(TEXT("capture")),
				CaptureCallsBefore + 1);
			const FKeyEvent EscapeEvent(
				EKeys::Escape,
				FModifierKeysState(),
				0,
				false,
				0,
				0);
			TestTrue(
				TEXT("Escape closes the top capture overlay"),
				Workspace->OnKeyDown(
					FGeometry(),
					EscapeEvent).IsEventHandled());
			TestTrue(
				TEXT("Capture overlay is closed after Escape"),
				Workspace->ExportOverlayStateForAutomation().Contains(
					TEXT("capture=0")));
			const FModifierKeysState CtrlOnly(
				false,
				false,
				true,
				false,
				false,
				false,
				false,
				false,
				false);
			const FKeyEvent SearchShortcut(
				EKeys::K,
				CtrlOnly,
				0,
				false,
				0,
				0);
			TestTrue(
				TEXT("Global search shortcut is handled"),
				Workspace->OnKeyDown(
					FGeometry(),
					SearchShortcut).IsEventHandled());
		}
	}
	TestTrue(
		TEXT("Workspace tree exposes actionable controls to automation"),
		ActionableCount > 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianStyleGalleryContractTest,
	"ExtendedAtlassian.Parity.StyleGallery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianStyleGalleryContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	FExtendedAtlassianStyle::Register();
	const ISlateStyle& Style = FExtendedAtlassianStyle::Get();
	TestEqual(
		TEXT("Backlot style set identity is stable"),
		FExtendedAtlassianStyle::GetStyleSetName(),
		FName(TEXT("ExtendedAtlassianBacklotStyle")));
	TestTrue(
		TEXT("Canvas color matches frozen HTML"),
		FExtendedAtlassianStyle::Color(TEXT("Backlot.Color.Canvas"))
			.Equals(
				FExtendedAtlassianStyle::FromHex(TEXT("#0b0c0e")),
				KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Primary blue matches frozen HTML"),
		FExtendedAtlassianStyle::Color(TEXT("Backlot.Color.Blue"))
			.Equals(
				FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")),
				KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Body typography is registered"),
		Style.HasWidgetStyle<FTextBlockStyle>(TEXT("Backlot.Sans.12")));
	TestTrue(
		TEXT("Monospace typography is registered"),
		Style.HasWidgetStyle<FTextBlockStyle>(TEXT("Backlot.Mono.10")));
	TestTrue(
		TEXT("Primary button state style is registered"),
		Style.HasWidgetStyle<FButtonStyle>(TEXT("Backlot.Button.Primary")));
	TestTrue(
		TEXT("Field style is registered"),
		Style.HasWidgetStyle<FEditableTextBoxStyle>(
			TEXT("Backlot.Field")));
	const TSharedRef<SBacklotDiagonalPattern> Diagonal =
		SNew(SBacklotDiagonalPattern)
		.ColorA(FExtendedAtlassianStyle::FromHex(TEXT("#22262c")))
		.ColorB(FExtendedAtlassianStyle::FromHex(TEXT("#1c2025")))
		.StripeWidth(9.0f);
	TestEqual(
		TEXT("Repeating diagonal placeholder primitive is registered"),
		Diagonal->GetTypeAsString(),
		FString(TEXT("SBacklotDiagonalPattern")));
	const TSharedRef<SBacklotPixelRule> Rule =
		SNew(SBacklotPixelRule)
		.Orientation(Orient_Horizontal)
		.Color(FExtendedAtlassianStyle::FromHex(TEXT("#24282e")));
	TestEqual(
		TEXT("Pixel rule resolves to one device pixel at 200 percent scale"),
		static_cast<float>(Rule->ComputeDesiredSize(2.0f).Y),
		0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianIssueOperationsContractTest,
	"ExtendedAtlassian.Parity.IssueOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianIssueOperationsContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Data =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Data);
	Controller->Refresh();
	Controller->Navigate(EExtendedAtlassianWorkspaceRoute::Issues);
	Controller->SelectIssueView(TEXT("all"));
	Controller->CycleStatusFilter();
	Controller->CycleAssigneeFilter();
	Controller->ToggleEpicFilter(TEXT("Harbour District"));
	Controller->SetGlobalSearch(TEXT("fog"));
	TestEqual(
		TEXT("Issue view selection is observable"),
		Controller->GetSelectedIssueViewId(),
		FString(TEXT("all")));
	TestNotEqual(
		TEXT("Status filter cycles away from Any"),
		Controller->GetStatusFilter(),
		FString(TEXT("any")));
	TestNotEqual(
		TEXT("Assignee filter cycles away from Anyone"),
		Controller->GetAssigneeFilter(),
		FString(TEXT("anyone")));
	TestEqual(
		TEXT("Epic filter toggles on"),
		Controller->GetEpicFilter(),
		FString(TEXT("Harbour District")));
	TestEqual(
		TEXT("Search state is retained"),
		Controller->GetGlobalSearch(),
		FString(TEXT("fog")));
	Controller->ResetIssueFilters();
	TestEqual(
		TEXT("Filter reset restores status"),
		Controller->GetStatusFilter(),
		FString(TEXT("any")));
	TestEqual(
		TEXT("Filter reset restores assignee"),
		Controller->GetAssigneeFilter(),
		FString(TEXT("anyone")));
	TestTrue(
		TEXT("Filter reset clears epic"),
		Controller->GetEpicFilter().IsEmpty());
	Controller->OpenIssue(TEXT("NFB-1042"));
	TestEqual(
		TEXT("Open issue selects the requested issue"),
		Controller->GetSelectedIssueKey(),
		FString(TEXT("NFB-1042")));
	TestEqual(
		TEXT("Open issue enters detail route"),
		static_cast<int32>(Controller->GetRoute()),
		static_cast<int32>(EExtendedAtlassianWorkspaceRoute::IssueDetail));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianRightRailOperationsContractTest,
	"ExtendedAtlassian.Parity.RightRailOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianRightRailOperationsContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Data =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Data);
	Controller->Refresh();
	TestTrue(TEXT("Right rail starts open"), Controller->IsRailOpen());
	for (EExtendedAtlassianWorkspaceRoute Route : {
			EExtendedAtlassianWorkspaceRoute::Docs,
			EExtendedAtlassianWorkspaceRoute::Issues,
			EExtendedAtlassianWorkspaceRoute::Board,
			EExtendedAtlassianWorkspaceRoute::Pins,
			EExtendedAtlassianWorkspaceRoute::Inbox })
	{
		Controller->Navigate(Route);
		TestEqual(
			TEXT("Route-specific rail keeps the requested route"),
			static_cast<int32>(Controller->GetRoute()),
			static_cast<int32>(Route));
	}
	Controller->ToggleRail();
	TestFalse(TEXT("Right rail closes"), Controller->IsRailOpen());
	TestTrue(
		TEXT("Normalized state exposes closed rail"),
		Controller->ExportNormalizedState().Contains(TEXT("rail=0")));
	Controller->ToggleRail();
	TestTrue(TEXT("Right rail reopens"), Controller->IsRailOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianCompactShellContractTest,
	"ExtendedAtlassian.Parity.CompactShell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianCompactShellContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Data =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Data);
	Controller->Refresh();
	Controller->SetCompact(true);
	TestTrue(TEXT("Compact shell enables"), Controller->IsCompact());
	TestTrue(
		TEXT("Normalized state exposes compact shell"),
		Controller->ExportNormalizedState().Contains(TEXT("compact=1")));
	Controller->Navigate(EExtendedAtlassianWorkspaceRoute::Inbox);
	TestTrue(
		TEXT("Compact preference survives navigation"),
		Controller->IsCompact());
	Controller->ToggleCompact();
	TestFalse(TEXT("Compact shell disables"), Controller->IsCompact());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianOverlayOperationsContractTest,
	"ExtendedAtlassian.Parity.OverlayOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianOverlayOperationsContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace ExtendedAtlassianParityTestsPrivate;
	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Fixture =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	const TSharedPtr<IExtendedAtlassianWorkspaceData> Data =
		StaticCastSharedRef<IExtendedAtlassianWorkspaceData>(Fixture);
	const TSharedRef<FInMemoryHostServices> Host =
		MakeShared<FInMemoryHostServices>();
	const TSharedRef<SExtendedAtlassianWorkspace> Workspace =
		SNew(SExtendedAtlassianWorkspace)
		.WorkspaceData(Data)
		.InteractionClock(MakeShared<FManualClock>())
		.HostServices(Host);
	const FModifierKeysState CtrlShift(
		true, false, true, false, false, false, false, false, false);
	const FKeyEvent CaptureShortcut(
		EKeys::B, CtrlShift, 0, false, 0, 0);
	TestTrue(
		TEXT("Capture overlay opens from the reference shortcut"),
		Workspace->OnKeyDown(FGeometry(), CaptureShortcut).IsEventHandled());
	TestTrue(
		TEXT("Capture is the only open overlay"),
		Workspace->ExportOverlayStateForAutomation().StartsWith(
			TEXT("capture=1|createCard=0|pagePopover=0|pinPopover=0|")
			TEXT("menu=0|confirm=0")));
	const FKeyEvent Escape(
		EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
	TestTrue(
		TEXT("Escape dismisses the capture overlay"),
		Workspace->OnKeyDown(FGeometry(), Escape).IsEventHandled());
	TestTrue(
		TEXT("Overlay state returns to clean"),
		Workspace->ExportOverlayStateForAutomation().Contains(
			TEXT("capture=0")));
	TestEqual(
		TEXT("Overlay capture uses exactly one host capture"),
		Host->CountCalls(TEXT("capture")),
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianKeyboardTimersContractTest,
	"ExtendedAtlassian.Parity.KeyboardTimers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianKeyboardTimersContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace ExtendedAtlassianParityTestsPrivate;
	const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Data =
		MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
	const TSharedRef<FManualClock> Clock = MakeShared<FManualClock>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Data, Clock);
	Controller->Refresh();
	Controller->ShowToast(FText::FromString(TEXT("Exact timer")));
	TestTrue(TEXT("2.6-second toast starts visible"), Controller->GetToast().IsSet());
	Clock->Advance(2.59);
	Controller->TickInteractionState();
	TestTrue(TEXT("Toast remains before 2.6 seconds"), Controller->GetToast().IsSet());
	Clock->Advance(0.02);
	Controller->TickInteractionState();
	TestFalse(TEXT("Toast expires after 2.6 seconds"), Controller->GetToast().IsSet());

	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::DeleteIssue;
	Mutation.TargetId = TEXT("NFB-1042");
	Controller->ExecuteDestructiveMutation(
		Mutation,
		FText::FromString(TEXT("Issue deleted")));
	TestTrue(
		TEXT("Destructive mutation offers Undo during grace period"),
		Controller->GetToast().bOffersUndo);
	Clock->Advance(6.99);
	Controller->TickInteractionState();
	TestTrue(
		TEXT("Undo remains available before seven seconds"),
		Controller->GetToast().bOffersUndo);
	TestTrue(
		TEXT("Undo restores the pending deletion"),
		Controller->UndoLastDestructiveMutation());
	TestFalse(
		TEXT("Undo clears the destructive pending state"),
		Controller->IsMutating());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianOperationDefinitionsContractTest,
	"ExtendedAtlassian.Parity.OperationDefinitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianOperationDefinitionsContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const FString ManifestPath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("UnrealExtendedFramework"),
		TEXT("UnrealExtendedAtlassian"),
		TEXT("Tests"),
		TEXT("Parity"),
		TEXT("BacklotReferenceManifest.json"));
	FString ManifestJson;
	TestTrue(
		TEXT("Reference manifest is readable"),
		FFileHelper::LoadFileToString(ManifestJson, *ManifestPath));
	const TSharedPtr<FJsonObject> Manifest =
		ExtendedAtlassianParityTestsPrivate::ParseObject(ManifestJson);
	TestTrue(TEXT("Reference manifest parses"), Manifest.IsValid());
	if (!Manifest.IsValid())
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Operations = nullptr;
	TestTrue(TEXT("Manifest has source regions"),
		Manifest->TryGetArrayField(TEXT("regions"), Regions));
	TestTrue(TEXT("Manifest has event bindings"),
		Manifest->TryGetArrayField(TEXT("eventBindings"), Events));
	TestTrue(TEXT("Manifest has operation definitions"),
		Manifest->TryGetArrayField(TEXT("operationDefinitions"), Operations));
	if (!Regions || !Events || !Operations)
	{
		return false;
	}
	TestEqual(TEXT("Every authored source region is present"), Regions->Num(), 15);
	TestEqual(TEXT("Every HTML event binding is present"), Events->Num(), 169);
	TestEqual(TEXT("Every extracted application operation is present"), Operations->Num(), 113);

	TSet<FString> ContractIds;
	auto AuditRows = [this, &ContractIds](
		const TCHAR* Collection,
		const TArray<TSharedPtr<FJsonValue>>& Rows,
		const TCHAR* RegionField)
	{
		for (const TSharedPtr<FJsonValue>& Value : Rows)
		{
			const TSharedPtr<FJsonObject> Row = Value->AsObject();
			if (!Row.IsValid())
			{
				AddError(FString::Printf(TEXT("%s row is not an object"), Collection));
				continue;
			}
			const FString ContractId = Row->GetStringField(TEXT("contractId"));
			TestFalse(
				FString::Printf(TEXT("%s contract id is unique"), Collection),
				ContractIds.Contains(ContractId));
			ContractIds.Add(ContractId);
			TestNotEqual(
				FString::Printf(TEXT("%s row is classified"), Collection),
				Row->GetStringField(RegionField),
				FString(TEXT("unclassified")));
			TestFalse(
				FString::Printf(TEXT("%s row has a concrete test id"), Collection),
				Row->GetStringField(TEXT("testId")).IsEmpty());
		}
	};
	AuditRows(TEXT("region"), *Regions, TEXT("id"));
	AuditRows(TEXT("event"), *Events, TEXT("region"));
	AuditRows(TEXT("operation"), *Operations, TEXT("region"));

	int32 ClickCount = 0;
	for (const TSharedPtr<FJsonValue>& Value : *Events)
	{
		const TSharedPtr<FJsonObject> Row = Value->AsObject();
		ClickCount += Row.IsValid()
			&& Row->GetStringField(TEXT("event")) == TEXT("onClick")
			? 1
			: 0;
	}
	TestEqual(TEXT("All click bindings are contract rows"), ClickCount, 123);

	const TSharedRef<ExtendedAtlassianParityTestsPrivate::FScriptedWorkspaceData> Data =
		MakeShared<ExtendedAtlassianParityTestsPrivate::FScriptedWorkspaceData>();
	const TSharedRef<FExtendedAtlassianWorkspaceController> Controller =
		MakeShared<FExtendedAtlassianWorkspaceController>(Data);
	Controller->Refresh();
	FExtendedAtlassianWorkspaceSnapshot Loaded;
	Loaded.State = EExtendedAtlassianLoadState::Ready;
	Loaded.Capabilities = Data->GetCapabilities();
	Data->CompleteLoad(0, Loaded);
	Controller->SetGlobalSearch(TEXT("net"));
	Controller->SetPageSearch(TEXT("water"));
	Controller->SetInboxTab(TEXT("Pins"));
	Controller->SetCompact(true);
	Controller->SetRailOpen(false);
	TestTrue(
		TEXT("Interaction helpers feed normalized controller state"),
		Controller->ExportNormalizedState().Contains(
			TEXT("globalSearch=net|pageSearch=water")));
	TestTrue(
		TEXT("Cycle/presentation helpers feed normalized controller state"),
		Controller->ExportNormalizedState().Contains(TEXT("inboxTab=Pins"))
			&& Controller->ExportNormalizedState().Contains(TEXT("compact=1"))
			&& Controller->ExportNormalizedState().Contains(TEXT("rail=0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianLargeFixturePerformanceTest,
	"ExtendedAtlassian.Performance.LargeFixtureContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianLargeFixturePerformanceTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace ExtendedAtlassianParityTestsPrivate;

	TArray<FExtendedAtlassianDocBlock> LongDocument;
	LongDocument.SetNum(5000);
	for (int32 Index = 0; Index < LongDocument.Num(); ++Index)
	{
		LongDocument[Index].Kind = EExtendedAtlassianBlockKind::Paragraph;
		LongDocument[Index].Markup = FString::Printf(
			TEXT("Performance document block %04d"),
			Index);
	}
	const TSharedRef<SExtendedAtlassianDocumentView> Reader =
		SNew(SExtendedAtlassianDocumentView);
	Reader->SetBlocks(LongDocument);
	TestEqual(
		TEXT("Large document retains every logical block"),
		Reader->GetTotalBlockCountForAutomation(),
		5000);
	TestEqual(
		TEXT("Large document initially materializes one bounded chunk"),
		Reader->GetMaterializedBlockCountForAutomation(),
		120);

	struct FProfileCase
	{
		EExtendedAtlassianWorkspaceRoute Route;
		const TCHAR* Name;
		double BudgetMilliseconds;
	};
	const FProfileCase Cases[] = {
		{ EExtendedAtlassianWorkspaceRoute::Docs, TEXT("2,000 pages / 5,000 blocks"), 2000.0 },
		{ EExtendedAtlassianWorkspaceRoute::Issues, TEXT("200 issues"), 500.0 },
		{ EExtendedAtlassianWorkspaceRoute::Pins, TEXT("200 pins"), 750.0 },
		{ EExtendedAtlassianWorkspaceRoute::Inbox, TEXT("500 notifications"), 750.0 },
	};

	bool bWithinBudgets = true;
	for (const FProfileCase& Profile : Cases)
	{
		const TSharedRef<FLargeFixtureWorkspaceData> Fixture =
			MakeShared<FLargeFixtureWorkspaceData>();
		const TSharedPtr<IExtendedAtlassianWorkspaceData> Data =
			StaticCastSharedRef<IExtendedAtlassianWorkspaceData>(Fixture);
		const double StartedAt = FPlatformTime::Seconds();
		const TSharedRef<SExtendedAtlassianWorkspace> Workspace =
			SNew(SExtendedAtlassianWorkspace)
			.StartRoute(Profile.Route)
			.WorkspaceData(Data)
			.InteractionClock(MakeShared<FManualClock>())
			.HostServices(MakeShared<FInMemoryHostServices>())
			.AnimationsEnabled(false);
		const double ElapsedMilliseconds =
			(FPlatformTime::Seconds() - StartedAt) * 1000.0;
		AddInfo(FString::Printf(
			TEXT("%s constructed in %.2f ms (budget %.2f ms)"),
			Profile.Name,
			ElapsedMilliseconds,
			Profile.BudgetMilliseconds));
		bWithinBudgets &= TestTrue(
			FString::Printf(TEXT("%s stays within its initial-load budget"), Profile.Name),
			ElapsedMilliseconds <= Profile.BudgetMilliseconds);
		TestFalse(
			FString::Printf(TEXT("%s exported a non-empty normalized state"), Profile.Name),
			Workspace->ExportNormalizedStateForAutomation().IsEmpty());
	}
	return bWithinBudgets;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianGoldenCaptureTest,
	"ExtendedAtlassian.Visual.GoldenCapture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianGoldenCaptureTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace ExtendedAtlassianParityTestsPrivate;
	AddExpectedError(
		TEXT("Missing inputs parameters"),
		EAutomationExpectedErrorFlags::Contains,
		20,
		false);
	const FString OutputDirectory = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation"),
		TEXT("ExtendedAtlassian"),
		TEXT("Visual"),
		TEXT("Actual"));
	IFileManager::Get().MakeDirectory(*OutputDirectory, true);

	auto CaptureRoute = [this, &OutputDirectory](
		EExtendedAtlassianWorkspaceRoute Route,
		const TCHAR* Name,
		const FVector2D& Size,
		bool bCompact,
		bool bCaptureOverlay)
	{
		const TSharedRef<FExtendedAtlassianFixtureWorkspaceData> Fixture =
			MakeShared<FExtendedAtlassianFixtureWorkspaceData>();
		const TSharedPtr<IExtendedAtlassianWorkspaceData> Data =
			StaticCastSharedRef<IExtendedAtlassianWorkspaceData>(Fixture);
		const TSharedRef<FInMemoryHostServices> Host =
			MakeShared<FInMemoryHostServices>();
		const TSharedRef<SExtendedAtlassianWorkspace> Workspace =
			SNew(SExtendedAtlassianWorkspace)
			.StartRoute(Route)
			.WorkspaceData(Data)
			.InteractionClock(MakeShared<FManualClock>())
			.HostServices(Host);
		if (bCompact
			&& !Workspace->ExportNormalizedStateForAutomation().Contains(
				TEXT("compact=1")))
		{
			const TSharedPtr<SButton> CompactButton =
				FindButtonByAccessibleText(Workspace, TEXT("NARROW"));
			if (!CompactButton.IsValid())
			{
				AddError(FString::Printf(
					TEXT("%s compact toggle was not discoverable"),
					Name));
				return false;
			}
			CompactButton->SimulateClick();
		}
		if (bCaptureOverlay)
		{
			const FModifierKeysState CtrlShift(
				true, false, true, false, false, false, false, false, false);
			const FKeyEvent CaptureShortcut(
				EKeys::B, CtrlShift, 0, false, 0, 0);
			if (!Workspace->OnKeyDown(
					FGeometry(),
					CaptureShortcut).IsEventHandled())
			{
				AddError(FString::Printf(
					TEXT("%s capture shortcut was not handled"),
					Name));
				return false;
			}
		}

		TArray<FColor> Colors;
		FIntVector CapturedSize;
		FString GeometryJson;
		if (!CaptureSlateWidget(
				Workspace,
				Size,
				Colors,
				CapturedSize,
				GeometryJson))
		{
			AddError(FString::Printf(
				TEXT("%s Slate screenshot failed"),
				Name));
			return false;
		}
		TestEqual(
			FString::Printf(TEXT("%s capture width"), Name),
			CapturedSize.X,
			FMath::RoundToInt(Size.X));
		TestEqual(
			FString::Printf(TEXT("%s capture height"), Name),
			CapturedSize.Y,
			FMath::RoundToInt(Size.Y));
		TArray64<uint8> Png;
		if (!EncodePng(Colors, CapturedSize, Png))
		{
			AddError(FString::Printf(TEXT("%s PNG encode failed"), Name));
			return false;
		}
		const FString OutputPath =
			FPaths::Combine(OutputDirectory, FString(Name) + TEXT(".png"));
		if (!FFileHelper::SaveArrayToFile(Png, *OutputPath))
		{
			AddError(FString::Printf(
				TEXT("%s PNG could not be saved to %s"),
				Name,
				*OutputPath));
			return false;
		}
		const FString GeometryPath =
			FPaths::Combine(
				OutputDirectory,
				FString(Name) + TEXT(".geometry.json"));
		if (!FFileHelper::SaveStringToFile(
			GeometryJson,
			*GeometryPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			AddError(FString::Printf(
				TEXT("%s geometry JSON could not be saved to %s"),
				Name,
				*GeometryPath));
			return false;
		}
		AddInfo(FString::Printf(
			TEXT("Wrote visual candidate: %s"),
			*OutputPath));
		return true;
	};

	bool bAllCaptured = true;
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Docs,
		TEXT("actual_docs_wide_1920x1080"),
		FVector2D(1920.0f, 1080.0f),
		false,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Docs,
		TEXT("actual_docs_1280x720"),
		FVector2D(1280.0f, 720.0f),
		false,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Docs,
		TEXT("actual_docs_1440x900"),
		FVector2D(1440.0f, 900.0f),
		false,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Docs,
		TEXT("actual_docs_2560x1440"),
		FVector2D(2560.0f, 1440.0f),
		false,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Docs,
		TEXT("actual_docs_879x600"),
		FVector2D(879.0f, 600.0f),
		false,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Docs,
		TEXT("actual_docs_880x600"),
		FVector2D(880.0f, 600.0f),
		false,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Docs,
		TEXT("actual_docs_881x600"),
		FVector2D(881.0f, 600.0f),
		false,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Issues,
		TEXT("actual_issues_wide_1920x1080"),
		FVector2D(1920.0f, 1080.0f),
		false,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::IssueDetail,
		TEXT("actual_issue_detail_wide_1920x1080"),
		FVector2D(1920.0f, 1080.0f),
		false,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Board,
		TEXT("actual_board_wide_1920x1080"),
		FVector2D(1920.0f, 1080.0f),
		false,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Pins,
		TEXT("actual_pins_wide_1920x1080"),
		FVector2D(1920.0f, 1080.0f),
		false,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Inbox,
		TEXT("actual_inbox_wide_1920x1080"),
		FVector2D(1920.0f, 1080.0f),
		false,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Docs,
		TEXT("actual_docs_compact_560x900"),
		FVector2D(560.0f, 900.0f),
		true,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Issues,
		TEXT("actual_issues_compact_560x900"),
		FVector2D(560.0f, 900.0f),
		true,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::IssueDetail,
		TEXT("actual_issue_detail_compact_560x900"),
		FVector2D(560.0f, 900.0f),
		true,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Board,
		TEXT("actual_board_compact_560x900"),
		FVector2D(560.0f, 900.0f),
		true,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Pins,
		TEXT("actual_pins_compact_560x900"),
		FVector2D(560.0f, 900.0f),
		true,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::Inbox,
		TEXT("actual_inbox_compact_560x900"),
		FVector2D(560.0f, 900.0f),
		true,
		false);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::IssueDetail,
		TEXT("actual_capture_wide_1920x1080"),
		FVector2D(1920.0f, 1080.0f),
		false,
		true);
	bAllCaptured &= CaptureRoute(
		EExtendedAtlassianWorkspaceRoute::IssueDetail,
		TEXT("actual_capture_compact_560x900"),
		FVector2D(560.0f, 900.0f),
		true,
		true);
	return bAllCaptured;
}

#endif
