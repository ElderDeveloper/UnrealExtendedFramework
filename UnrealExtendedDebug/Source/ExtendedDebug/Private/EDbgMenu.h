// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "InputCoreTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SVerticalBox;
class SWidgetSwitcher;
class UEDbgSubsystem;

/** The in-game debug menu: cheats, status, messages, analytics and watches on tabs. */
class SEDbgMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEDbgMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UEDbgSubsystem>, Subsystem)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Rebuilds the cheat list, e.g. after cheats were registered while the menu is open. */
	void RebuildCheats();

	/** The first cheat button, for gamepad focus. */
	TSharedPtr<SWidget> GetInitialFocus() const;

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	FReply HandleTab(int32 TabIndex);
	FReply HandleRunCheat(FName CheatName);
	FReply HandleClose();

	FText GetStatusText() const;
	FText GetMessagesText() const;
	FText GetAnalyticsText() const;
	FText GetWatchesText() const;

	TWeakObjectPtr<UEDbgSubsystem> Subsystem;
	TSharedPtr<SWidgetSwitcher> Switcher;
	TSharedPtr<SVerticalBox> CheatList;
	TSharedPtr<SWidget> FirstCheatButton;
	TMap<FName, TSharedPtr<SEditableTextBox>> ArgumentBoxes;

	mutable FText CachedStatus;
	mutable double CachedStatusSeconds = -1.0;
};

/** Opens the debug menu on the menu key or the thumbstick chord, whatever has focus. */
class FEDbgInputProcessor : public IInputProcessor
{
public:
	explicit FEDbgInputProcessor(TWeakObjectPtr<UEDbgSubsystem> InSubsystem) : Subsystem(InSubsystem) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("ExtendedDebug"); }

private:
	TWeakObjectPtr<UEDbgSubsystem> Subsystem;
	bool bLeftStickDown = false;
	bool bRightStickDown = false;
};
