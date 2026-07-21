// Copyright Moon Punch Games. All Rights Reserved.

#include "SEEUILabFocusOverlay.h"

#if WITH_EDITOR

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetNavigation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/UI/EFWidgetAutomationIdentity.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "UILab/Input/EEUILabInputLog.h"

void SEEUILabFocusOverlay::Construct(const FArguments& InArgs, const TSharedRef<FEEUILabInputLog>& InLog)
{
	HostedWidget = InArgs._HostedWidget;
	ShowOverlay = InArgs._ShowOverlay;
	ClickToFocus = InArgs._ClickToFocus;
	Log = InLog;

	SetVisibility(TAttribute<EVisibility>::CreateLambda([this]()
	{
		if (!ShowOverlay.Get(false))
		{
			return EVisibility::Collapsed;
		}
		return ClickToFocus.Get(false) ? EVisibility::Visible : EVisibility::HitTestInvisible;
	}));
}

void SEEUILabFocusOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Focus history: log every focus change while the overlay is active.
	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);
	if (Focused != LastFocusedWidget.Pin())
	{
		LastFocusedWidget = Focused;

		FEEUILabInputEvent Event;
		Event.EventType = TEXT("Focus");
		Event.Detail = Focused.IsValid()
			? FString::Printf(TEXT("Focus -> %s"), *Focused->GetTypeAsString())
			: TEXT("Focus -> (none)");
		Log->Add(MoveTemp(Event));
	}
}

FString SEEUILabFocusOverlay::DescribeWidget(const UWidget& Widget)
{
	if (Widget.GetClass()->ImplementsInterface(UEFWidgetAutomationIdentity::StaticClass()))
	{
		const FName Identity = IEFWidgetAutomationIdentity::Execute_GetAutomationIdentity(const_cast<UWidget*>(&Widget));
		if (!Identity.IsNone())
		{
			return Identity.ToString();
		}
	}
	return Widget.GetName();
}

FString SEEUILabFocusOverlay::DescribeNavigation(const UWidget& Widget)
{
	if (!Widget.Navigation)
	{
		return TEXT("nav: (auto)");
	}

	auto DescribeRule = [](const TCHAR* Prefix, const FWidgetNavigationData& Data) -> FString
	{
		switch (Data.Rule)
		{
		case EUINavigationRule::Explicit:
			return FString::Printf(TEXT("%s:%s "), Prefix, *Data.WidgetToFocus.ToString());
		case EUINavigationRule::Stop:
			return FString::Printf(TEXT("%s:stop "), Prefix);
		case EUINavigationRule::Wrap:
			return FString::Printf(TEXT("%s:wrap "), Prefix);
		default:
			return FString();
		}
	};

	FString Summary;
	Summary += DescribeRule(TEXT("U"), Widget.Navigation->Up);
	Summary += DescribeRule(TEXT("D"), Widget.Navigation->Down);
	Summary += DescribeRule(TEXT("L"), Widget.Navigation->Left);
	Summary += DescribeRule(TEXT("R"), Widget.Navigation->Right);

	return Summary.IsEmpty() ? TEXT("nav: (auto)") : FString::Printf(TEXT("nav: %s"), *Summary.TrimEnd());
}

void SEEUILabFocusOverlay::CollectEntries(UUserWidget& Root, TArray<FFocusEntry>& OutEntries) const
{
	if (!Root.WidgetTree)
	{
		return;
	}

	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);

	Root.WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (!Widget)
		{
			return;
		}

		if (UUserWidget* Nested = Cast<UUserWidget>(Widget))
		{
			CollectEntries(*Nested, OutEntries);
			return;
		}

		const TSharedPtr<SWidget> SlateWidget = Widget->GetCachedWidget();
		if (!SlateWidget.IsValid() || !SlateWidget->SupportsKeyboardFocus())
		{
			return;
		}

		FFocusEntry Entry;
		Entry.SlateWidget = SlateWidget;
		Entry.Label = DescribeWidget(*Widget);
		Entry.NavSummary = DescribeNavigation(*Widget);
		Entry.bFocused = SlateWidget == Focused;
		Entry.bEligible = Widget->GetIsEnabled() && Widget->IsVisible();
		OutEntries.Add(MoveTemp(Entry));
	});
}

int32 SEEUILabFocusOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	UUserWidget* Root = HostedWidget.Get(nullptr);
	if (!Root)
	{
		return LayerId;
	}

	TArray<FFocusEntry> Entries;
	CollectEntries(*Root, Entries);

	const FSlateBrush* Brush = FAppStyle::GetBrush("WhiteBrush");
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 8);
	int32 IneligibleCount = 0;

	for (const FFocusEntry& Entry : Entries)
	{
		const TSharedPtr<SWidget> SlateWidget = Entry.SlateWidget.Pin();
		if (!SlateWidget.IsValid())
		{
			continue;
		}

		if (!Entry.bEligible)
		{
			++IneligibleCount;
		}

		const FGeometry& TargetGeometry = SlateWidget->GetCachedGeometry();
		const FVector2D TopLeft = AllottedGeometry.AbsoluteToLocal(TargetGeometry.GetAbsolutePosition());
		const FVector2D BottomRight = AllottedGeometry.AbsoluteToLocal(
			TargetGeometry.GetAbsolutePosition() + TargetGeometry.GetAbsoluteSize());
		const FVector2D Size = BottomRight - TopLeft;

		if (Size.X <= 0.0f || Size.Y <= 0.0f)
		{
			continue;
		}

		const FLinearColor Color = Entry.bFocused
			? FLinearColor(0.2f, 1.0f, 0.2f, 0.9f)
			: Entry.bEligible
				? FLinearColor(0.2f, 0.8f, 1.0f, 0.6f)
				: FLinearColor(1.0f, 0.6f, 0.2f, 0.6f);

		const float Thickness = Entry.bFocused ? 2.0f : 1.0f;
		auto DrawRect = [&](const FVector2D& Pos, const FVector2D& RectSize)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
				AllottedGeometry.ToPaintGeometry(RectSize, FSlateLayoutTransform(Pos)), Brush,
				ESlateDrawEffect::None, Color);
		};

		DrawRect(TopLeft, FVector2D(Size.X, Thickness));                                        // top
		DrawRect(FVector2D(TopLeft.X, BottomRight.Y - Thickness), FVector2D(Size.X, Thickness)); // bottom
		DrawRect(TopLeft, FVector2D(Thickness, Size.Y));                                        // left
		DrawRect(FVector2D(BottomRight.X - Thickness, TopLeft.Y), FVector2D(Thickness, Size.Y)); // right

		const FString LabelText = Entry.bFocused
			? FString::Printf(TEXT("%s  [FOCUS]  %s"), *Entry.Label, *Entry.NavSummary)
			: FString::Printf(TEXT("%s  %s"), *Entry.Label, *Entry.NavSummary);

		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2,
			AllottedGeometry.ToPaintGeometry(FVector2D(Size.X, 12.0f), FSlateLayoutTransform(TopLeft + FVector2D(2.0f, 1.0f))),
			LabelText, Font, ESlateDrawEffect::None, Color);
	}

	// Summary line: focusable count and how many are currently ineligible.
	const FString Summary = FString::Printf(TEXT("Focusables: %d  (%d ineligible)"), Entries.Num(), IneligibleCount);
	FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2,
		AllottedGeometry.ToPaintGeometry(FVector2D(400.0f, 14.0f), FSlateLayoutTransform(FVector2D(4.0f, 4.0f))),
		Summary, Font, ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 0.4f));

	return LayerId + 2;
}

FReply SEEUILabFocusOverlay::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!ClickToFocus.Get(false))
	{
		return FReply::Unhandled();
	}

	UUserWidget* Root = HostedWidget.Get(nullptr);
	if (!Root)
	{
		return FReply::Unhandled();
	}

	TArray<FFocusEntry> Entries;
	CollectEntries(*Root, Entries);

	// Pick the smallest focusable under the cursor so nested targets win over containers.
	const FVector2D CursorAbsolute = MouseEvent.GetScreenSpacePosition();
	TSharedPtr<SWidget> Best;
	float BestArea = TNumericLimits<float>::Max();

	for (const FFocusEntry& Entry : Entries)
	{
		const TSharedPtr<SWidget> SlateWidget = Entry.SlateWidget.Pin();
		if (!SlateWidget.IsValid())
		{
			continue;
		}

		const FGeometry& TargetGeometry = SlateWidget->GetCachedGeometry();
		const FVector2D Position = FVector2D(TargetGeometry.GetAbsolutePosition());
		const FVector2D Size = FVector2D(TargetGeometry.GetAbsoluteSize());
		const FSlateRect Rect(Position.X, Position.Y, Position.X + Size.X, Position.Y + Size.Y);

		if (Rect.ContainsPoint(CursorAbsolute))
		{
			const float Area = Size.X * Size.Y;
			if (Area < BestArea)
			{
				BestArea = Area;
				Best = SlateWidget;
			}
		}
	}

	if (Best.IsValid())
	{
		FSlateApplication::Get().SetUserFocus(0, Best, EFocusCause::SetDirectly);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

#endif // WITH_EDITOR
