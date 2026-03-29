// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "StringUtils.h"
#include "Main.h"
#include "Game.h"
#include "TextLabel.h"
#include "TextField.h"

namespace
{
	std::vector<WCHAR> SpecialCharsNormal
	{
		'!',  '"',  '#',  '$',  '%',  '&',  '\'',
		'(',  ')',  '*',  '+',  '-',  '.',  '/',
		':',  ';',  '<',  '=',  '>',  '?',  '@' ,
		'[',  '\\', ']',  '^',  '_',  '{',  '|',
		'}',  '~',  ',',  ' '
	};

	std::vector<WCHAR> SpecialCharsPassword
	{
		'!',  '"',  '#',  '$',  '%',  '&',  '\'',
		'(',  ')',  '*',  '+',  '-',  '.',  '/',
		':',  ';',  '<',  '=',  '>',  '?',  '@' ,
		'[',  '\\', ']',  '^',  '_',  '{',  '|',
		'}',  '~',  ','
	};

	std::vector<WCHAR> SpecialCharsEmail
	{
		'@',  '!',  '#',  '$',  '%',  '&',  '\'', '*', '+',
		'-',  '/',  '=',  '?',  '^',  '_' , '{',  '|', '.'
	};
}

FTextFieldWidget::FTextFieldWidget(Vector2 TextFieldPosition,
								   Vector2 TextFieldSize,
								   UILayer TextFieldLayer,
								   const std::wstring& InitialText,
								   const std::wstring& TextureFile,
								   FontPtr TextFieldFont,
								   FTextFieldWidget::EInputType InputType,
								   EAlignmentType AlignmentType,
								   FColor BackCol,
								   FColor TextCol) :
	IWidget(TextFieldPosition, TextFieldSize, TextFieldLayer),
	InitialTextValue(InitialText),
	Text(InitialText),
	CaretBlinkTimer(0.f),
	InputType(InputType)
{
	Label = std::make_shared<FTextLabelWidget>(
		TextFieldPosition,
		TextFieldSize,
		TextFieldLayer,
		InitialTextValue,
		TextureFile,
		BackCol,
		TextCol,
		AlignmentType);
	Label->SetFont(TextFieldFont);
}

void FTextFieldWidget::Create()
{
	Label->Create();
}

void FTextFieldWidget::Release()
{
	Label->Release();
	OnEnterPressedCallback = nullptr;
}

void FTextFieldWidget::Update()
{
	if (Label == nullptr || Label->GetFont() == nullptr)
	{
		return;
	}

	Label->Update();

	UpdateCaret();

	UpdateText();
}

void FTextFieldWidget::Render(FSpriteBatchPtr& Batch)
{
	IWidget::Render(Batch);
	Label->Render(Batch);
}

void FTextFieldWidget::Clear()
{
	CursorPosition = 0;
	Label->ClearText();
	Text = L"";
}

void FTextFieldWidget::OnUIEvent(const FUIEvent& Event)
{
	if (!IsEnabled())
	{
		return;
	}

	if (InputType == EInputType::None)
	{
		return;
	}

#ifdef EOS_DEMO_SDL
	if (IsFocused() && Event.GetType() == EUIEventType::TextInput)
	{
		const std::string InputText = Event.GetInputText();
		std::wstring InputTextW = FStringUtils::Widen(InputText);
		std::wstring TextValue = Text;
		if (!TextValue.empty() && CursorPosition <= TextValue.size())
		{
			TextValue = TextValue.substr(0, CursorPosition) + InputTextW + TextValue.substr(CursorPosition);
		}
		else
		{
			TextValue = InputTextW;
			CursorPosition = 0;
		}
		Label->SetText(TextValue);
		Text = TextValue;

		CursorPosition++;
	}
	else
#endif // EOS_DEMO_SDL
	if (IsFocused() && Event.GetType() == EUIEventType::PasteText)
	{
		UpdatePasteText();
	}
	else if (IsFocused() && Event.GetType() == EUIEventType::KeyPressed)
	{
		const FInput::Keys TypedKey = Event.GetKey();

		if (TypedKey == FInput::Keys::Enter)
		{
			if (OnEnterPressedCallback)
			{
				OnEnterPressedCallback(Text);
			}
		}
		else if (TypedKey == FInput::Keys::Back)
		{
			if (CursorPosition != 0)
			{
				std::wstring TextValue = Text;
				if (!TextValue.empty() && CursorPosition <= TextValue.size())
				{
					TextValue = TextValue.substr(0, CursorPosition - 1) + TextValue.substr(CursorPosition);

					Label->SetText(TextValue);
					Text = TextValue;

					CursorPosition--;
				}
				else
				{
					CursorPosition = 0;
				}
			}
		}
		else if (TypedKey == FInput::Keys::Delete)
		{
			std::wstring TextValue = Text;
			if (!TextValue.empty())
			{
				if (CursorPosition < TextValue.size())
				{
					TextValue = TextValue.substr(0, CursorPosition) + TextValue.substr(CursorPosition + 1);
					Label->SetText(TextValue);
					Text = TextValue;
				}
			}
		}
		else if (TypedKey == FInput::Keys::Left)
		{
			if (CursorPosition != 0)
			{
				--CursorPosition;
			}
		}
		else if (TypedKey == FInput::Keys::Right)
		{
			std::wstring TextValue = Text;
			if (!TextValue.empty())
			{
				if (CursorPosition < TextValue.size())
				{
					++CursorPosition;
				}
			}
		}
#ifdef DXTK
		else
		{
			if (InputType != EInputType::Normal && TypedKey == FInput::Keys::Space)
			{
				return;
			}

			bool bIsValidChar = false;
			WCHAR KeyChar;
			WCHAR Key = WCHAR(TypedKey);
			KeyChar = FInput::GetKeyChar(Key);

			if (IsAlphaCharacter(KeyChar) ||
				(IsNumericCharacter(KeyChar) && !FInput::IsKeyShifted(KeyChar)) ||
				IsSpecialInputCharacter(KeyChar))
			{
				bIsValidChar = true;
			}
			else if (IsAlphaCharacter(Key) ||
				(IsNumericCharacter(Key) && !FInput::IsKeyShifted(Key)))
			{
				bIsValidChar = true;
				KeyChar = Key;
			}
			else if (IsSpecialInputCharacter(KeyChar))
			{
				KeyChar = FInput::GetKeyChar(Key);
				bIsValidChar = true;
			}

			if (bIsValidChar && Label->GetFont()->ContainsCharacter(KeyChar))
			{
				std::wstring TextValue = Text;
				if (!TextValue.empty() && CursorPosition <= TextValue.size())
				{
					TextValue = TextValue.substr(0, CursorPosition) + KeyChar + TextValue.substr(CursorPosition);
				}
				else
				{
					TextValue = KeyChar;
					CursorPosition = 0;
				}
				Label->SetText(TextValue);
				Text = TextValue;

				CursorPosition++;
			}
		}
#endif // DXTK
	}
	else if (Event.GetType() == EUIEventType::MousePressed)
	{
		FontPtr Font = Label->GetFont();
		const std::wstring& TextValue = VisibleText;

		SetFocused(true);

		// Find position of the cursor
		if (!TextValue.empty())
		{
			Vector2 LabelSize = Font->MeasureString(TextValue.c_str());
			float TextWidth = LabelSize.x;
			Vector2 LabelPos = Label->GetTextPosition();
			
			// Clamp
			float ClampedClickPosX = Event.GetVector().x;
			if (Event.GetVector().x < LabelPos.x)
			{
				ClampedClickPosX = LabelPos.x;
			}
			else if (Event.GetVector().x > LabelPos.x + TextWidth)
			{
				ClampedClickPosX = LabelPos.x + TextWidth;
			}
			float ClampedClickPosXObjectSpace = ClampedClickPosX - LabelPos.x;
			
			float Ratio = (ClampedClickPosX - LabelPos.x) / TextWidth;
			Ratio = (Ratio < 0) ? 0.0f : Ratio;
			Ratio = (Ratio > 1.0f) ? 1.0f : Ratio;

			// First approximation
			CursorPosition = size_t(TextValue.size() * Ratio);

			// Find precise position
			float Epsilon = Vector2(Font->MeasureString("-")).x;
			float Diff = Vector2(Font->MeasureString(TextValue.substr(0, CursorPosition).c_str())).x - ClampedClickPosXObjectSpace;
			while (Epsilon < fabsf(Diff))
			{
				size_t NewCursorPosition = CursorPosition;
				if (Diff < 0.0f)
				{
					if (NewCursorPosition == 0)
					{
						break;
					}
					NewCursorPosition--;
				}
				else
				{
					NewCursorPosition++;
					if (NewCursorPosition >= TextValue.size())
					{
						NewCursorPosition = TextValue.size() - 1;
						break;
					}
				}
				float NewDiff = Vector2(Font->MeasureString(TextValue.substr(0, NewCursorPosition).c_str())).x - ClampedClickPosXObjectSpace;
				if (fabsf(NewDiff) > fabsf(Diff))
				{
					break;
				}
				Diff = NewDiff;
				CursorPosition = NewCursorPosition;
			}
		}
	}
}

void FTextFieldWidget::SetPosition(Vector2 Pos)
{
	IWidget::SetPosition(Pos);

	Label->SetPosition(Pos);
}

void FTextFieldWidget::SetSize(Vector2 NewSize)
{
	IWidget::SetSize(NewSize);

	Label->SetSize(NewSize);
}

void FTextFieldWidget::SetFont(const FontPtr& NewFont)
{
	Label->SetFont(NewFont);
}

void FTextFieldWidget::SetOnEnterPressedCallback(std::function<void(const std::wstring&)> Callback)
{
	OnEnterPressedCallback = Callback;
}

void FTextFieldWidget::SetHorizontalOffset(float Value)
{
	if (Label)
	{
		Label->SetHorizontalOffset(Value);
	}
}


void FTextFieldWidget::InsertTextAtCursor(std::wstring&& PastedText)
{
	size_t PastedTextLength = PastedText.size();
	if (!PastedText.empty())
	{
		std::wstring TextValue = Text;
		if (CursorPosition <= TextValue.size())
		{
			TextValue = TextValue.substr(0, CursorPosition) + PastedText + TextValue.substr(CursorPosition);
			Label->SetText(TextValue);
			Text = TextValue;

			CursorPosition += PastedTextLength;
		}
	}
}

void FTextFieldWidget::SetFocused(bool bValue)
{
	IWidget::SetFocused(bValue);

	if (bValue && Text == InitialTextValue)
	{
		Clear();
	}

	if (!bValue && Text.empty())
	{
		Text = InitialTextValue;
	}
}

void FTextFieldWidget::UpdatePasteText()
{
	const auto& Input = FGame::Get().GetInput();
	if (IsFocused() && Input)
	{
		std::wstring PastedText = Input->GetClipboardText();
		if (!PastedText.empty())
		{
			//Get rid of any new line characters at the end as they are of no use but can look bad.
			PastedText = PastedText.substr(0, PastedText.find_last_not_of(L'\n') + 1);
			InsertTextAtCursor(std::move(PastedText));
		}
	}
}

void FTextFieldWidget::UpdateCaret()
{
	// Update blinking
	CaretBlinkTimer += static_cast<float>(Main->GetTimer().GetElapsedSeconds());
	if (CaretBlinkTimer >= CaretBlinkTime)
	{
		CaretBlinkTimer = 0.f;
		bCaretVisible = !bCaretVisible;
	}
}

void FTextFieldWidget::UpdateText()
{
	std::wstring TextValue = Text;

	// Update text based on whether there should be a caret visible
	std::wstring CaretStr = L"|";
	if (!IsFocused() || !bCaretVisible)
	{
		CaretStr = L" ";
	}

	VisibleText = TextValue;

	if (TextValue.empty())
	{
		Label->SetText(CaretStr);
		return;
	}

	//In case of multi-line text we only want to render the line where the cursor is
	size_t StartLineIndex = 0;
	if (CursorPosition > 0)
	{
		size_t SearchResult = TextValue.find_last_of(L'\n', CursorPosition);

		//There is an often case when there is a \n just before cursor when pasting text. We want to skip that and show previous line.
 		if (SearchResult == CursorPosition - 1 && SearchResult > 0)
 		{
 			SearchResult = TextValue.find_last_of(L'\n', SearchResult - 1);
 		}

		StartLineIndex = (SearchResult != std::wstring::npos) ? SearchResult + 1 : 0;
		if (StartLineIndex >= CursorPosition)
		{
			StartLineIndex = CursorPosition;
		}
	}

	size_t EndLineIndex = TextValue.size() - 1;
	if (CursorPosition < EndLineIndex)
	{
		size_t SearchResult = TextValue.find_first_of(L'\n', CursorPosition);
		EndLineIndex = (SearchResult != std::wstring::npos) ? SearchResult - 1 : TextValue.size() - 1;
	}

	VisibleText = TextValue.substr(StartLineIndex, EndLineIndex - StartLineIndex + 1);
	const size_t AdjustedCursorPos = CursorPosition - StartLineIndex;
	if (AdjustedCursorPos > VisibleText.size())
	{
		// Don't continue if we somehow have a cursor position beyond the size of the visible text
		return;
	}

	std::wstring TextToRenderNoCaret = VisibleText.substr(0, AdjustedCursorPos) + VisibleText.substr(AdjustedCursorPos);
	std::wstring TextToRender = VisibleText.substr(0, AdjustedCursorPos) + CaretStr + VisibleText.substr(AdjustedCursorPos);
	if (InputType == EInputType::Password && !TextToRender.empty() && TextValue != InitialTextValue)
	{
		TextToRender = std::wstring(AdjustedCursorPos, L'*') + CaretStr + std::wstring(VisibleText.size() - AdjustedCursorPos, L'*');
	}

	// Update text if it is overflowing text field width
	Vector2 LabelSize = Label->GetFont()->MeasureString(TextToRenderNoCaret.c_str());
	float TextWidth = LabelSize.x;
	if ((TextWidth) > (Size.x - 10.0f))
	{
		float Ratio = TextWidth / Size.x;
		Ratio *= 1.15f;
		size_t NewTextLength = size_t(TextToRender.size() / Ratio);

		size_t NewVisibleTextStart = (AdjustedCursorPos > NewTextLength) ? AdjustedCursorPos - NewTextLength + 5 : 0;
		size_t NewVisibleTextEnd = NewVisibleTextStart + NewTextLength;
		if (NewVisibleTextEnd > TextToRender.size())
		{
			NewVisibleTextEnd = TextToRender.size();
		}

		TextToRender = std::wstring(L"...") + TextToRender.substr(NewVisibleTextStart, (NewVisibleTextEnd - NewVisibleTextStart));
	}

	Label->SetText(TextToRender);
}

bool FTextFieldWidget::IsAlphaCharacter(WCHAR KeyChar)
{
	if ((KeyChar >= 'a' && KeyChar <= 'z') ||
		(KeyChar >= 'A' && KeyChar <= 'Z'))
	{
		return true;
	}

	return false;
}

bool FTextFieldWidget::IsNumericCharacter(WCHAR KeyChar)
{
	if (KeyChar >= '0' && KeyChar <= '9')
	{
		return true;
	}

	return false;
}

bool FTextFieldWidget::IsSpecialInputCharacter(WCHAR InputChar)
{
	switch (InputType)
	{
		case EInputType::Normal:
		{
			if (std::find(SpecialCharsNormal.begin(), SpecialCharsNormal.end(), InputChar) != SpecialCharsNormal.end())
			{
				return true;
			}
			break;
		}
		case EInputType::Email:
		{
			if (std::find(SpecialCharsEmail.begin(), SpecialCharsEmail.end(), InputChar) != SpecialCharsEmail.end())
			{
				return true;
			}
			break;
		}
		case EInputType::Password:
		{
			if (std::find(SpecialCharsPassword.begin(), SpecialCharsPassword.end(), InputChar) != SpecialCharsPassword.end())
			{
				return true;
			}
			break;
		}
		default:
			break;
	}

	return false;
}
