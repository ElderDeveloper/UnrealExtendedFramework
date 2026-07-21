// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "Input.h"
#include "Game.h"
#include "Main.h"
#include "Console.h"
#include "TextLabel.h"
#include "TextField.h"
#include "TextView.h"
#include "Button.h"
#include "UIEvent.h"
#include "ConsoleDialog.h"

namespace
{
	static constexpr float HDSize = 1000.f;
	static constexpr float UHDSize = 2000.f;
	static constexpr bool bAllowToggleConsole = false;
	static constexpr float FooterHeight = 30.0f;
	static constexpr float TitleHeight = 30.0f;
}

FConsoleDialog::FConsoleDialog(Vector2 ConsolePos,
							   Vector2 ConsoleSize,
							   UILayer ConsoleLayer,
							   std::weak_ptr<FConsole> InConsole,
							   FontPtr ConsoleNormalFont,
							   FontPtr ConsoleLargeFont,
							   FontPtr ConsoleLargestFont,
							   FontPtr ConsoleButtonFont,
							   FontPtr ConsoleTitleFont) :
	FDialog(ConsolePos, ConsoleSize, ConsoleLayer),
	Console(InConsole),
	NormalFont(ConsoleNormalFont),
	LargeFont(ConsoleLargeFont),
	LargestFont(ConsoleLargestFont),
	ButtonFont(ConsoleButtonFont),
	TitleFont(ConsoleTitleFont)
{
	Background = std::make_shared<FSpriteWidget>(
		Position,
		Size,
		ConsoleLayer,
		L"Assets/console.dds"
		);
	AddWidget(Background);

	TitleLabel = std::make_shared<FTextLabelWidget>(
		Vector2(Position.x, Position.y),
		Vector2(Size.x, TitleHeight),
		ConsoleLayer - 1,
		L"CONSOLE",
		L"Assets/dialog_title.dds",
		FColor(1.f, 1.f, 1.f, 1.f),
		FColor(1.f, 1.f, 1.f, 1.f),
		EAlignmentType::Left);
	TitleLabel->SetFont(TitleFont);
	TitleLabel->SetBorderColor(Color::UIBorderGrey);
	AddWidget(TitleLabel);

	TextInputField = std::make_shared<FTextFieldWidget>(
		Position + Vector2(50, 0.0f),
		Vector2(400, FooterHeight),
		ConsoleLayer - 1,
		L"Start typing...",
		L"Assets/bordered_horizontal_grey_box.dds",
		NormalFont,
		FTextFieldWidget::EInputType::Normal,
		EAlignmentType::Left);
	TextInputField->SetBorderColor(Color::UIBorderGrey);
	AddWidget(TextInputField);

	SearchInputField = std::make_shared<FTextFieldWidget>(
		TitleLabel->GetPosition() + Vector2(410.0f, 0.0f),
		Vector2(100, 30),
		ConsoleLayer - 1,
		L"",
		L"Assets/textfield.dds",
		NormalFont,
		FTextFieldWidget::EInputType::Normal,
		EAlignmentType::Left,
		FColor(0.03f, 0.03f, 0.03f, 1.f));
	SearchInputField->SetBorderColor(Color::UIBorderGrey);
	AddWidget(SearchInputField);
	SearchInputField->Hide();

	SearchLabel = std::make_shared<FTextLabelWidget>(
		Vector2(SearchInputField->GetPosition() - Vector2(50.f, 0.0f)),
		Vector2(50.0f, 30.f),
		ConsoleLayer - 1,
		L"Search:",
		L"",
		FColor(1.f, 1.f, 1.f, 1.f),
		FColor(1.f, 1.f, 1.f, 1.f),
		EAlignmentType::Left);
	SearchLabel->SetFont(TitleFont);
	AddWidget(SearchLabel);
	SearchLabel->Hide();

	SearchSprite = std::make_shared<FSpriteWidget>(
		SearchLabel->GetPosition() - Vector2(15.0f, -8.0f),
		Vector2(15.f, 15.f),
		ConsoleLayer - 1,
		L"Assets/search.dds");
	AddWidget(SearchSprite);
	SearchSprite->Hide();

	NotFoundLabel = std::make_shared<FTextLabelWidget>(
		Vector2(SearchInputField->GetPosition() + Vector2(SearchInputField->GetSize().x + 10.0f, 0.0f)),
		Vector2(50.0f, 30.f),
		ConsoleLayer - 1,
		L"Not found.",
		L"",
		FColor(1.f, 1.f, 1.f, 1.f),
		FColor(1.f, 1.f, 1.f, 1.f),
		EAlignmentType::Left);
	NotFoundLabel->SetFont(NormalFont);
	AddWidget(NotFoundLabel);
	NotFoundLabel->Hide();

	ClearButton = std::make_shared<FButtonWidget>(
		Position + Vector2(500, 0.0f),
		Vector2(200, 30),
		ConsoleLayer - 1,
		L"CLEAR",
		std::vector<std::wstring>({ L"Assets/bordered_horizontal_grey_box.dds", L"Assets/bordered_horizontal_grey_box.dds" }),
		ButtonFont);
	ClearButton->SetBorderColor(Color::UIBorderGrey);
	AddWidget(ClearButton);

	Vector2 TextViewPos = Position + Vector2(0.0f, TitleHeight) +
		Vector2(10.0f, 10.0f);
	Vector2 TextViewSize = Vector2(Size.x, Position.y - TextViewPos.y - FooterHeight);
	TextView = std::make_shared<FTextViewWidget>(
		TextViewPos,
		TextViewSize,
		ConsoleLayer - 1,
		L"",
		L"",
		NormalFont);
	TextView->SetBorderOffsets(Vector2(10, Size.y / 30.0f));
	TextView->SetScrollerOffsets(Vector2(5.f, 4.0f));
	AddWidget(TextView);

	SetTextViewFont();
}

void FConsoleDialog::Create()
{
	FDialog::Create();

	// Callback when clear button is pressed
	std::weak_ptr<FTextViewWidget> ConsoleViewWidget = TextView;
	ClearButton->SetOnPressedCallback([ConsoleViewWidget]()
	{
		if (auto ConsoleViewWidgetPtr = ConsoleViewWidget.lock())
		{
			ConsoleViewWidgetPtr->Clear();
		}
		GetConsole()->Clear();
	});

	if (auto ConsoleLocked = Console.lock())
	{
		ConsoleLocked->SetDirty(true);
	}
}

void FConsoleDialog::Update()
{
	std::unique_ptr<FInput> const& Input = FGame::Get().GetInput();

	if (bAllowToggleConsole && Input)
	{
		if (Input->IsKeyPressed(FInput::InputCommands::ConsoleToggle) ||
			Input->IsKeyPressed(FInput::InputCommands::ConsoleToggleAlt))
		{
			Toggle();
		}
	}

	if (bEnabled)
	{
		if (auto ConsoleLocked = Console.lock())
		{
			if (ConsoleLocked->IsDirty())
			{
				const size_t NewNumLines = ConsoleLocked->GetNumLines();
				const size_t NewNumLinesDropped = ConsoleLocked->GetNumLinesDropped();

				bool bDataChanged = false;
				std::vector<FConsoleLine> Lines;

				if (NewNumLines < NumConsoleLinesLastTime || NewNumLinesDropped < NumLinesDroppedTotalLastTime)
				{
					//Rebuild all text from scratch as console must have been cleared
					TextView->Clear(true);
					Lines = ConsoleLocked->GetLinesCopy(0, NewNumLines - 1);
				}
				else
				{
					const size_t NumLinesAddedBeforeReachingEnd = (NumConsoleLinesLastTime < NewNumLines) ? (NewNumLines - NumConsoleLinesLastTime) : 0;
					const size_t NumLinesOverwritten = std::min(NewNumLinesDropped - NumLinesDroppedTotalLastTime, NewNumLines);
					const size_t NumLinesToCopy = std::min(NumLinesAddedBeforeReachingEnd + NumLinesOverwritten, NewNumLines);

					if (NumLinesToCopy != 0)
					{
						Lines = ConsoleLocked->GetLinesCopy(NewNumLines - NumLinesToCopy, NewNumLines - 1);
					}
				}

				for (size_t Index = 0; Index < Lines.size(); Index++)
				{
					TextView->AddLine(std::move(Lines[Index].GetMessage()), Lines[Index].GetColor());
				}
				bDataChanged = !Lines.empty();

				NumConsoleLinesLastTime = NewNumLines;
				NumLinesDroppedTotalLastTime = NewNumLinesDropped;

				if (bDataChanged)
				{
					ConsoleLocked->SetDirty(true);
					if (TextView->IsAutoScrolling())
					{
						TextView->ScrollToBottom();
					}
				}
			}
		}

		for (auto& Widget : Widgets)
		{
			Widget->Update();
		}

		if (FGame::Get().GetInput()->IsKeyPressed(FInput::InputCommands::SearchText))
		{
			//Generate event for itself
			OnUIEvent(FUIEvent(EUIEventType::SearchText));
		}
	}
}

void FConsoleDialog::Render(FSpriteBatchPtr& Batch)
{
	if (bShown)
	{
		FDialog::Render(Batch);
	}
}

void FConsoleDialog::SetPosition(Vector2 Pos)
{
	IWidget::SetPosition(Pos);

	//Back texture
	Background->SetPosition(Position);

	// Title
	TitleLabel->SetPosition(Position);

	// Text View
	TextView->SetPosition(Vector2(Position.x, Position.y + TitleHeight));

	float FooterY = Position.y + Size.y - FooterHeight;

	// Clear Button
	ClearButton->SetPosition(Vector2(TextView->GetPosition().x + TextView->GetSize().x - (ClearButton->GetSize().x), FooterY));

	// Text Input
	if (TextInputField) TextInputField->SetPosition(Vector2(TextView->GetPosition().x, FooterY));

	// Search Input
	if (SearchInputField) SearchInputField->SetPosition(TitleLabel->GetPosition() + Vector2(410.0f, 0.0f));
	if (SearchLabel && SearchInputField) SearchLabel->SetPosition(SearchInputField->GetPosition() - Vector2(50.0f, 0.0f));
	if (SearchSprite && SearchLabel) SearchSprite->SetPosition(SearchLabel->GetPosition() - Vector2(15.0f, -8.0f));
	if (NotFoundLabel && SearchInputField) NotFoundLabel->SetPosition(SearchInputField->GetPosition() + Vector2(SearchInputField->GetSize().x + 10.0f, 0.0f));
}

void FConsoleDialog::SetSize(Vector2 NewSize)
{
	IWidget::SetSize(NewSize);

	SetTextViewFont();

	Background->SetSize(GetSize());

	TitleLabel->SetSize(Vector2(Size.x, TitleHeight));

	// Text View
	TextView->SetBorderOffsets(Vector2(10.0f, Size.y / 30.0f));
	TextView->SetSize(GetSize() - Vector2(0.0f, TitleHeight + FooterHeight + 4.0f));

	// Clear Button
	ClearButton->SetSize(Vector2(GetSize().x * 0.144f, FooterHeight));

	// Text Input
	if (TextInputField )TextInputField->SetSize(Vector2(TextView->GetSize().x - ClearButton->GetSize().x,
												ClearButton->GetSize().y));

	// Search Input
	if (SearchInputField) SearchInputField->SetSize(Vector2(100, ClearButton->GetSize().y));
	if (SearchLabel && SearchInputField) SearchLabel->SetSize(Vector2(50, SearchInputField->GetSize().y));

	if (auto ConsoleLocked = Console.lock())
	{
		ConsoleLocked->SetDirty(true);
	}
}

bool FConsoleDialog::CheckCollision(Vector2 Pos) const
{
	for (WidgetPtr Widget : Widgets)
	{
		if (Widget)
		{
			if (Widget->CheckCollision(Pos))
			{
				return true;
			}
		}
	}

	return false;
}

void FConsoleDialog::OnUIEvent(const FUIEvent& Event)
{
	FDialog::OnUIEvent(Event);

	if (Event.GetType() == EUIEventType::KeyPressed)
	{
		if (Event.GetKey() == FInput::Keys::Enter && TextInputField && TextInputField->IsFocused())
		{
			std::wstring TextValue = TextInputField->GetText();

			if (TextValue != TextInputField->GetInitialText())
			{
				if (auto ConsoleLocked = Console.lock())
				{
					ConsoleLocked->RunCommand(TextValue);
				}
				TextInputField->Clear();
			}
		}
		else if (Event.GetKey() == FInput::Keys::Enter && SearchInputField && SearchInputField->IsFocused())
		{
			//Perform search
			std::wstring SearchText = SearchInputField->GetText();

			SearchInputField->Hide();
			SearchLabel->Hide();
			SearchSprite->Hide();
			FocusedWidget = nullptr;

			bool bFound = TextView->Search(SearchText);
			if (!bFound)
			{
				//Show not found message
				if (NotFoundLabel)
				{
					NotFoundLabel->Show();
				}
			}
		}
	}
	else if (Event.GetType() == EUIEventType::SearchText)
	{
		//Open search widget
		if (SearchInputField)
		{
			if (NotFoundLabel)
			{
				NotFoundLabel->Hide();
			}

			if (!SearchInputField->IsShown())
			{
				SearchInputField->Show();
				SearchLabel->Show();
				SearchSprite->Show();
			}

			SearchInputField->SetFocused(true);
			if (FocusedWidget)
			{
				FocusedWidget->SetFocused(false);
			}
			FocusedWidget = SearchInputField;
		}
	}

	//We want to pass key presses to console's text view at all times to handle shortcuts
	if (Event.GetType() == EUIEventType::KeyPressed &&
		(!FocusedWidget || FocusedWidget != TextView))
	{
		if (TextView)
		{
			TextView->OnUIEvent(Event);
		}
	}
}

void FConsoleDialog::SetTextViewFont()
{
	if (Size.x >= UHDSize)
	{
		// Use largest font size for UHD res
		TextView->SetFont(LargestFont);
	}
	else if (Size.x >= HDSize)
	{
		// Use a larger font size for HD res
		TextView->SetFont(LargeFont);
	}
	else
	{
		// Use a regular font size for lower res
		TextView->SetFont(NormalFont);
	}
}