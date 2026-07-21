// Copyright Moon Punch Games. All Rights Reserved.

#include "EEUILabUtils.h"

#if WITH_EDITOR

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Interfaces/UI/EFWidgetAutomationIdentity.h"

namespace EEUILabUtils
{
	UWidget* FindWidgetByAutomationIdentity(UUserWidget& Root, const FName Identity)
	{
		if (Identity.IsNone() || !Root.WidgetTree)
		{
			return nullptr;
		}

		UWidget* Found = nullptr;
		Root.WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (Found || !Widget)
			{
				return;
			}

			if (Widget->GetClass()->ImplementsInterface(UEFWidgetAutomationIdentity::StaticClass()))
			{
				if (IEFWidgetAutomationIdentity::Execute_GetAutomationIdentity(Widget) == Identity)
				{
					Found = Widget;
					return;
				}
			}

			// Recurse into nested user widgets.
			if (UUserWidget* NestedUserWidget = Cast<UUserWidget>(Widget))
			{
				Found = FindWidgetByAutomationIdentity(*NestedUserWidget, Identity);
			}
		});

		return Found;
	}

	FName FindAutomationIdentityForSlateWidget(UUserWidget& Root, const TSharedPtr<SWidget>& SlateWidget)
	{
		if (!SlateWidget.IsValid() || !Root.WidgetTree)
		{
			return NAME_None;
		}

		FName Found = NAME_None;
		Root.WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (!Found.IsNone() || !Widget)
			{
				return;
			}

			if (UUserWidget* Nested = Cast<UUserWidget>(Widget))
			{
				Found = FindAutomationIdentityForSlateWidget(*Nested, SlateWidget);
				if (!Found.IsNone())
				{
					return;
				}
			}

			if (!Widget->GetClass()->ImplementsInterface(UEFWidgetAutomationIdentity::StaticClass()))
			{
				return;
			}

			const TSharedPtr<SWidget> Cached = Widget->GetCachedWidget();
			if (!Cached.IsValid())
			{
				return;
			}

			// Match the widget itself or any Slate descendant (focus often lands inside composites).
			for (TSharedPtr<SWidget> Walker = SlateWidget; Walker.IsValid(); Walker = Walker->GetParentWidget())
			{
				if (Walker == Cached)
				{
					Found = IEFWidgetAutomationIdentity::Execute_GetAutomationIdentity(Widget);
					return;
				}
			}
		});

		return Found;
	}
}

#endif // WITH_EDITOR
