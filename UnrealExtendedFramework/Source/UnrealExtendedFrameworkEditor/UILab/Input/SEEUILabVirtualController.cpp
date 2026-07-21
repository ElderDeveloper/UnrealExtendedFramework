// Copyright Moon Punch Games. All Rights Reserved.

#include "SEEUILabVirtualController.h"

#if WITH_EDITOR

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SEEUILabVirtualController"

void SEEUILabVirtualController::Construct(const FArguments& InArgs)
{
	FocusTarget = InArgs._FocusTarget;

	ChildSlot
	[
		SNew(SHorizontalBox)

		// D-pad
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f).VAlign(VAlign_Center)
		[
			SNew(SUniformGridPanel).SlotPadding(1.0f)
			+ SUniformGridPanel::Slot(1, 0)[MakePadButton(LOCTEXT("DUp", "▲"), EKeys::Gamepad_DPad_Up, LOCTEXT("DUpTip", "D-pad Up"))]
			+ SUniformGridPanel::Slot(0, 1)[MakePadButton(LOCTEXT("DLeft", "◀"), EKeys::Gamepad_DPad_Left, LOCTEXT("DLeftTip", "D-pad Left"))]
			+ SUniformGridPanel::Slot(2, 1)[MakePadButton(LOCTEXT("DRight", "▶"), EKeys::Gamepad_DPad_Right, LOCTEXT("DRightTip", "D-pad Right"))]
			+ SUniformGridPanel::Slot(1, 2)[MakePadButton(LOCTEXT("DDown", "▼"), EKeys::Gamepad_DPad_Down, LOCTEXT("DDownTip", "D-pad Down"))]
		]

		// Left stick nudges
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f).VAlign(VAlign_Center)
		[
			SNew(SUniformGridPanel).SlotPadding(1.0f)
			+ SUniformGridPanel::Slot(1, 0)[MakePadButton(LOCTEXT("LSUp", "LS↑"), EKeys::Gamepad_LeftStick_Up, LOCTEXT("LSUpTip", "Left stick up"))]
			+ SUniformGridPanel::Slot(0, 1)[MakePadButton(LOCTEXT("LSLeft", "LS←"), EKeys::Gamepad_LeftStick_Left, LOCTEXT("LSLeftTip", "Left stick left"))]
			+ SUniformGridPanel::Slot(2, 1)[MakePadButton(LOCTEXT("LSRight", "LS→"), EKeys::Gamepad_LeftStick_Right, LOCTEXT("LSRightTip", "Left stick right"))]
			+ SUniformGridPanel::Slot(1, 2)[MakePadButton(LOCTEXT("LSDown", "LS↓"), EKeys::Gamepad_LeftStick_Down, LOCTEXT("LSDownTip", "Left stick down"))]
		]

		// Shoulders/triggers
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(1.0f)[MakePadButton(LOCTEXT("LB", "LB"), EKeys::Gamepad_LeftShoulder, LOCTEXT("LBTip", "Left shoulder"))]
				+ SHorizontalBox::Slot().AutoWidth().Padding(1.0f)[MakePadButton(LOCTEXT("RB", "RB"), EKeys::Gamepad_RightShoulder, LOCTEXT("RBTip", "Right shoulder"))]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(1.0f)[MakePadButton(LOCTEXT("LT", "LT"), EKeys::Gamepad_LeftTrigger, LOCTEXT("LTTip", "Left trigger"))]
				+ SHorizontalBox::Slot().AutoWidth().Padding(1.0f)[MakePadButton(LOCTEXT("RT", "RT"), EKeys::Gamepad_RightTrigger, LOCTEXT("RTTip", "Right trigger"))]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(1.0f)[MakePadButton(LOCTEXT("View", "View"), EKeys::Gamepad_Special_Left, LOCTEXT("ViewTip", "View / Select"))]
				+ SHorizontalBox::Slot().AutoWidth().Padding(1.0f)[MakePadButton(LOCTEXT("Menu", "Menu"), EKeys::Gamepad_Special_Right, LOCTEXT("MenuTip", "Menu / Start"))]
			]
		]

		// Face buttons
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f).VAlign(VAlign_Center)
		[
			SNew(SUniformGridPanel).SlotPadding(1.0f)
			+ SUniformGridPanel::Slot(1, 0)[MakePadButton(LOCTEXT("FaceTop", "Y"), EKeys::Gamepad_FaceButton_Top, LOCTEXT("FaceTopTip", "Face button top"))]
			+ SUniformGridPanel::Slot(0, 1)[MakePadButton(LOCTEXT("FaceLeft", "X"), EKeys::Gamepad_FaceButton_Left, LOCTEXT("FaceLeftTip", "Face button left"))]
			+ SUniformGridPanel::Slot(2, 1)[MakePadButton(LOCTEXT("FaceRight", "B"), EKeys::Gamepad_FaceButton_Right, LOCTEXT("FaceRightTip", "Face button right / Back"))]
			+ SUniformGridPanel::Slot(1, 2)[MakePadButton(LOCTEXT("FaceBottom", "A"), EKeys::Gamepad_FaceButton_Bottom, LOCTEXT("FaceBottomTip", "Face button bottom / Accept"))]
		]
	];
}

TSharedRef<SWidget> SEEUILabVirtualController::MakePadButton(const FText& Label, FKey Key, const FText& Tooltip)
{
	return SNew(SButton)
		.ToolTipText(Tooltip)
		.ContentPadding(FMargin(8.0f, 4.0f))
		.OnClicked(this, &SEEUILabVirtualController::InjectKey, Key)
		[
			SNew(STextBlock).Text(Label)
		];
}

FReply SEEUILabVirtualController::InjectKey(const FKey Key)
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	// Route to the hosted widget, not to this button.
	if (const TSharedPtr<SWidget> Target = FocusTarget.Get(nullptr))
	{
		SlateApp.SetUserFocus(0, Target, EFocusCause::SetDirectly);
	}

	const FKeyEvent DownEvent(Key, FModifierKeysState(), /*UserIndex*/ 0, /*bIsRepeat*/ false, 0, 0);
	SlateApp.ProcessKeyDownEvent(DownEvent);

	const FKeyEvent UpEvent(Key, FModifierKeysState(), /*UserIndex*/ 0, /*bIsRepeat*/ false, 0, 0);
	SlateApp.ProcessKeyUpEvent(UpEvent);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
