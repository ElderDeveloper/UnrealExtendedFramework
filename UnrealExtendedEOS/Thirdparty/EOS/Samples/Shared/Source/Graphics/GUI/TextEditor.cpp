// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "Main.h"
#include "Game.h"
#include "Input.h"
#include "TextLabel.h"
#include "Button.h"
#include "TextEditor.h"
#include "TextField.h"

FTextEditorWidget::FTextEditorWidget(Vector2 TextEditorPos,
								 Vector2 TextEditorSize,
								 UILayer TextEditorLayer,
								 const std::wstring& InitialText,
								 const std::wstring& TextViewTextureFile,
								 FontPtr TextEditorFont,
								 FColor BackCol,
								 FColor TextCol) :
	FTextViewWidget(TextEditorPos, TextEditorSize, TextEditorLayer, InitialText, TextViewTextureFile, TextEditorFont, BackCol, TextCol)
{
	Lines.IncreaseMaxSize(10000);
	bCanSelectText = false;
}

void FTextEditorWidget::Create()
{
	FTextViewWidget::Create();

	EditingLine = std::make_shared<FTextFieldWidget>(
		Position,
		Vector2(50.0f, 20.0f),
		Layer - 1,
		L"",
		L"Assets/textfield.dds",
		Font,
		FTextFieldWidget::EInputType::Normal,
		EAlignmentType::Left);

	EditingLine->Create();
	EditingLine->SetHorizontalOffset(0.0f);
	EditingLine->Hide();
}

void FTextEditorWidget::Release()
{
	EditingLine->Release();

	FTextViewWidget::Release();
}

void FTextEditorWidget::Update()
{
	if (Lines.Num() > NumLinesPerPage)
	{
		bScrollable = true;
	}
	else
	{
		bScrollable = false;
	}

	//Check that cursor is outside viewed area. Stop editing in that case.
	if (EditingLine->IsShown())
	{
		if (FirstSelectedLine < FirstViewedEntry() || FirstSelectedLine >(FirstViewedEntry() + NumEntries()))
		{
			StopEditing();
		}
	}

	UpdateEditingFieldTransform();

	EditingLine->Update();

	FTextViewWidget::Update();
}

void FTextEditorWidget::Render(FSpriteBatchPtr& Batch)
{
	FTextViewWidget::Render(Batch);

	if (EditingLine->IsShown())
	{
		EditingLine->Render(Batch);
	}
}

void FTextEditorWidget::SetPosition(Vector2 Pos)
{
	FTextViewWidget::SetPosition(Pos);

	UpdateEditingFieldTransform();
}

void FTextEditorWidget::SetSize(Vector2 NewSize)
{
	FTextViewWidget::SetSize(NewSize);

	UpdateEditingFieldTransform();
}

void FTextEditorWidget::SetFocused(bool bValue)
{
	FTextViewWidget::SetFocused(bValue);

	if (!bValue)
	{
		StopEditing();

		if (IsEmpty())
		{
			Reset();
		}
	}
	else 
	{
		if (HasInitialText())
		{
			Clear();
		}
		else if (!bFocused && !EditingLine->IsShown())
		{
			ChangeEditingLine(0);
		}
	}
}


void FTextEditorWidget::Clear()
{
	FTextViewWidget::Clear(false);
}

std::wstring FTextEditorWidget::GetText()
{
	SaveEditedLine();

	//concatenate strings
	std::wstring Result;
	size_t TotalSize = 0;
	for (size_t i = 0; i < Lines.Num(); ++i)
	{
		TotalSize += Lines[i].GetMessage().size() + 1;
	}

	Result.reserve(TotalSize);

	for (size_t i = 0; i < Lines.Num(); ++i)
	{
		Result += Lines[i].GetMessage();
		if (i != Lines.Num() - 1)
		{
			Result += L"\n";
		}
	}

	return Result;
}

void FTextEditorWidget::SetText(const std::wstring& Text, FColor Color)
{
	StopEditing();

	Lines.Clear();

	//split text by new-line characters and generate lines
	std::wstring::size_type TokenizeStartPos = 0;
	std::wstring::size_type TokenizeEndPos = Text.find_first_of(L'\n', TokenizeStartPos);

	while (TokenizeStartPos != std::wstring::npos && TokenizeEndPos != std::wstring::npos)
	{
		Lines.PushBack(FColoredLine(Text.substr(TokenizeStartPos, TokenizeEndPos - TokenizeStartPos), Color));
		TokenizeStartPos = Text.find_first_of(L'\n', TokenizeEndPos) + 1;
		TokenizeEndPos = Text.find_first_of(L'\n', TokenizeStartPos);
	}

	Lines.PushBack(FColoredLine(Text.substr(TokenizeStartPos), Color));

	if (Lines.Num() > NumLinesPerPage)
	{
		bScrollable = true;
	}

	MarkDirty();
}

void FTextEditorWidget::UpdateEditingFieldTransform()
{
	const float YTop = Position.y + BorderOffsets.y;
	const Vector2 TestTextSize = Font->MeasureString(FFont::GetTestString().c_str());
	const float LineHeight = ceilf(TestTextSize.y);
	const float LineStartY = YTop + (FirstSelectedLine - FirstViewedLine) * (LineHeight + LineSpacing);
	EditingLine->SetPosition(Vector2(Position.x + BorderOffsets.x, LineStartY));
	EditingLine->SetSize(Vector2(Size.x - 2 * BorderOffsets.x - ((Scroller) ? Scroller->GetSize().x : 0.0f), LineHeight));
}

void FTextEditorWidget::ChangeEditingLine(size_t NewLineIndex)
{
	if (Lines.IsEmpty())
	{
		AddLine(L"");

		//start editing
		FirstSelectedLine = 0;
		CheckAndScroll();
		EditingLine->SetText(Lines[0].GetMessage());
		UpdateEditingFieldTransform();
		EditingLine->Show();
		MarkDirty();
		CheckAndScroll();

		return;
	}

	if (EditingLine)
	{
		if (NewLineIndex >= Lines.Num())
		{
			NewLineIndex = (Lines.IsEmpty()) ? 0 : Lines.Num() - 1;
		}

		if (EditingLine->IsShown() && NewLineIndex != FirstSelectedLine)
		{
			//'save' line
			Lines[FirstSelectedLine].GetMessage() = EditingLine->GetText();

			FirstSelectedLine = NewLineIndex;
			
			CheckAndScroll();
			
			EditingLine->SetText(Lines[FirstSelectedLine].GetMessage());

			UpdateEditingFieldTransform();

			MarkDirty();
		}
		else if (!EditingLine->IsShown())
		{
			//start editing
			FirstSelectedLine = NewLineIndex;
			
			CheckAndScroll();
			
			EditingLine->SetText(Lines[FirstSelectedLine].GetMessage());

			UpdateEditingFieldTransform();

			EditingLine->Show();

			MarkDirty();
		}
		EditingLine->SetFocused(true);

		CheckAndScroll();
	}
}

void FTextEditorWidget::StopEditing()
{
	SaveEditedLine();

	FirstSelectedLine = 0;

	if (EditingLine)
	{
		EditingLine->Hide();
	}
}

void FTextEditorWidget::SaveEditedLine()
{
	if (EditingLine && EditingLine->IsShown() && FirstSelectedLine < Lines.Num())
	{
		//'save' line
		Lines[FirstSelectedLine].GetMessage() = EditingLine->GetText();

		MarkDirty();
	}
}

void FTextEditorWidget::CheckAndScroll()
{
	if (FirstViewedEntry() > FirstSelectedLine)
	{
		ScrollUp(FirstViewedEntry() - FirstSelectedLine);
	}

	if (LastViewedEntry() < FirstSelectedLine)
	{
		ScrollDown(FirstSelectedLine - LastViewedEntry());
	}

	if (FirstViewedEntry() > 0 && FirstViewedEntry() + NumLinesPerPage > NumEntries())
	{
		ScrollUp(FirstViewedEntry() + NumLinesPerPage - NumEntries());
	}
}


void FTextEditorWidget::PasteText()
{
	const auto& Input = FGame::Get().GetInput();
	if (IsFocused() && Input)
	{
		const std::wstring& PastedText = Input->GetClipboardText();
		if (!PastedText.empty())
		{
			//Take first line and paste into currently selected line
			std::wstring::size_type TokenizeStartPos = 0;
			std::wstring::size_type TokenizeEndPos = PastedText.find_first_of(L'\n', TokenizeStartPos);

			//Pasting single line case
			if (TokenizeEndPos == std::wstring::npos)
			{
				if (Lines.IsEmpty() || !EditingLine)
				{
					Lines.PushBack(FColoredLine(std::move(PastedText), Color::White));
					FirstSelectedLine = 0;
				}
				else
				{
					EditingLine->InsertTextAtCursor(std::move(std::wstring(PastedText)));
				}
			}
			else
			{
				std::wstring CurrentLineText = EditingLine->GetText();
				std::wstring CurrentLineTextLeftOver = CurrentLineText.substr(EditingLine->GetCursorPosition());

				EditingLine->SetText(CurrentLineText.substr(0, EditingLine->GetCursorPosition()) + PastedText.substr(TokenizeStartPos, TokenizeEndPos - TokenizeStartPos));

				TokenizeStartPos = PastedText.find_first_of(L'\n', TokenizeEndPos) + 1;
				TokenizeEndPos = PastedText.find_first_of(L'\n', TokenizeStartPos);

				//Go through other pasted lines and insert them right after currently selected one
				while (TokenizeStartPos != std::wstring::npos && TokenizeEndPos != std::wstring::npos)
				{
					InsertTextAfterSelectedLine(std::move(PastedText.substr(TokenizeStartPos, TokenizeEndPos - TokenizeStartPos)));
					TokenizeStartPos = PastedText.find_first_of(L'\n', TokenizeEndPos) + 1;
					TokenizeEndPos = PastedText.find_first_of(L'\n', TokenizeStartPos);
				}

				InsertTextAfterSelectedLine(std::move(PastedText.substr(TokenizeStartPos) + CurrentLineTextLeftOver));
			}
		}
	}
}


void FTextEditorWidget::InsertTextAfterSelectedLine(std::wstring&& InsertedText)
{
	Lines.Insert(FirstSelectedLine + 1, FColoredLine(InsertedText, Color::White));
	ChangeEditingLine(FirstSelectedLine + 1);
	if (EditingLine)
	{
		EditingLine->SetCursorPosition(EditingLine->GetText().size());
	}
}

void FTextEditorWidget::OnUIEvent(const FUIEvent& Event)
{
	if (!IsShown())
		return;

	if (!bEditingEnabled)
	{
		FTextViewWidget::OnUIEvent(Event);
		return;
	}

	bool bPassEventToTextView = true;
	bool bPassEventToEditingLine = true;

	if (Event.GetType() == EUIEventType::MousePressed)
	{
		float YTop = Position.y + BorderOffsets.y;
		float YBottom = Position.y + Size.y - BorderOffsets.y;
		float XLeft = Position.x + BorderOffsets.x;
		float XRight = Position.x + Size.x - BorderOffsets.x;
		if (Event.GetVector().y > YTop && Event.GetVector().y < YBottom &&
			Event.GetVector().x > XLeft && Event.GetVector().x < XRight)
		{
			size_t NewSelectedLine = ScreenPositionToLineNum(FGame::Get().GetInput()->GetMousePosition());
			ChangeEditingLine(NewSelectedLine);
		}
		else
		{
			StopEditing();
		}
	}
	else if (Event.GetType() == EUIEventType::KeyPressed)
	{
		if (Event.GetKey() == FInput::Enter)
		{
			if (OnEnterPressedCallback)
			{
				//Pass the whole line
				OnEnterPressedCallback(EditingLine->GetText());
			}

			//Split current string by cursor position
			std::wstring Text = EditingLine->GetText();
			std::wstring TextToMoveToNextLine = Text.substr(EditingLine->GetCursorPosition());
			Text = Text.substr(0, EditingLine->GetCursorPosition());
			EditingLine->SetText(Text);
			InsertTextAfterSelectedLine(std::move(TextToMoveToNextLine));
			EditingLine->SetCursorPosition(0);
		}
		else if (Event.GetKey() == FInput::Up)
		{
			if (FirstSelectedLine > 0)
			{
				ChangeEditingLine(FirstSelectedLine - 1);
			}
			bPassEventToTextView = false;
		}
		else if (Event.GetKey() == FInput::Down)
		{
			if (FirstSelectedLine < Lines.Num() - 1)
			{
				ChangeEditingLine(FirstSelectedLine + 1);
			}
			bPassEventToTextView = false;
		}
		else if (Event.GetKey() == FInput::Delete)
		{
			if (EditingLine->IsShown())
			{
				if (EditingLine->GetCursorPosition() == (EditingLine->GetText().size()))
				{
					//Take contents of next line and append it to current one
					if (!Lines.IsEmpty() && FirstSelectedLine != (Lines.Num() - 1))
					{
						Lines[FirstSelectedLine] = FColoredLine(EditingLine->GetText() + Lines[FirstSelectedLine + 1].GetMessage(), Lines[FirstSelectedLine + 1].GetColor());
						EditingLine->SetText(Lines[FirstSelectedLine].GetMessage());

						//Remove next line
						Lines.Erase(FirstSelectedLine + 1);
						UpdateEditingFieldTransform();

						CheckAndScroll();

						MarkDirty();

						bPassEventToEditingLine = false;
					}
				}
			}
		}
		else if (Event.GetKey() == FInput::Back)
		{
			if (EditingLine->IsShown())
			{
				if (EditingLine->GetCursorPosition() == 0)
				{
					//Take contents of current line and append it to previous one
					std::wstring CurrentLineText = EditingLine->GetText();
					
					if (!Lines.IsEmpty() && FirstSelectedLine > 0)
					{
						size_t PrevLineLength = Lines[FirstSelectedLine - 1].GetMessage().size();
						Lines[FirstSelectedLine - 1].GetMessage().append(CurrentLineText);
						ChangeEditingLine(FirstSelectedLine - 1);

						//Remove previous line
						Lines.Erase(FirstSelectedLine + 1);

						UpdateEditingFieldTransform();

						CheckAndScroll();

						EditingLine->SetCursorPosition(PrevLineLength);

						MarkDirty();

						bPassEventToEditingLine = false;
					}
				}
			}
		}
	}
	else if (Event.GetType() == EUIEventType::MouseWheelScrolled)
	{
		StopEditing();
	}
	else
	{
		if (IsFocused() && Event.GetType() == EUIEventType::PasteText)
		{
			PasteText();
			return; //we don't want individual text field to handle this as well
		}
	}

	if (bPassEventToTextView)
	{
		FTextViewWidget::OnUIEvent(Event);
	}

	if (EditingLine->IsShown() && bPassEventToEditingLine)
	{
		EditingLine->OnUIEvent(Event);
	}
}
