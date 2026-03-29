// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "Main.h"
#include "Game.h"
#include "Input.h"
#include "TextLabel.h"
#include "Button.h"
#include "TextView.h"

FTextViewWidget::FTextViewWidget(Vector2 TextViewPos,
								 Vector2 TextViewSize,
								 UILayer TextViewLayer,
								 const std::wstring& InitialText,
								 const std::wstring& TextViewTextureFile,
								 FontPtr TextViewFont,
								 FColor BackCol,
								 FColor TextCol) :
	IWidget(TextViewPos, TextViewSize, TextViewLayer),
	InitialTextValue(InitialText),
	Lines(FConsole::MaxLines),
	TextureFile(TextViewTextureFile),
	Font(TextViewFont),
	TextCol(TextCol),
	Scroller(nullptr)
{
	Lines.PushBack(FColoredLine(InitialText, TextCol));

	BackgroundImage = std::make_shared<FSpriteWidget>(
		Vector2(0.f, 0.f),
		Vector2(200.f, 100.f),
		TextViewLayer,
		TextureFile.c_str());

	TextBackgroundImage = std::make_shared<FSpriteWidget>(
		Vector2(0.f, 0.f),
		Vector2(200.f, 100.f),
		TextViewLayer,
		L"",
		FColor(1.f, 1.f, 1.f, 1.f));
}

void FTextViewWidget::Create()
{
	BackgroundImage->Create();

	TextBackgroundImage->Create();

	if (!bScrollerDisabled)
	{
		CreateScroller();
		EnableScroller();
	}

	UpdateNumLinesPerPage();

	if (Lines.Num() > NumLinesPerPage)
	{
		bScrollable = true;
	}
}

void FTextViewWidget::Release()
{
	BackgroundImage->Release();
	TextBackgroundImage->Release();

	if (Scroller)
	{
		Scroller->Release();
		Scroller.reset();
	}

#ifdef EOS_DEMO_SDL
	FontTextures.clear();
#endif
}

void FTextViewWidget::Update()
{
	BackgroundImage->Update();
	TextBackgroundImage->Update();

	if (bSelectionEnabled && bSelectingLines)
	{
		size_t CurrentLine = ScreenPositionToLineNum(FGame::Get().GetInput()->GetMousePosition());
		if (CurrentLine < FirstSelectedLine)
		{
			FirstSelectedLine = CurrentLine;
			bSelectingUp = true;

			MarkDirty();
		}
		else if (CurrentLine > LastSelectedLine)
		{
			LastSelectedLine = CurrentLine;
			bSelectingUp = false;

			MarkDirty();
		}
		else if (CurrentLine > FirstSelectedLine && CurrentLine < LastSelectedLine)
		{
			if (bSelectingUp)
			{
				FirstSelectedLine = CurrentLine;
				MarkDirty();
			}
			else
			{
				LastSelectedLine = CurrentLine;
				MarkDirty();
			}
		}
	}

	if (CanScroll())
	{
		Scroller->Update();
	}
}

void FTextViewWidget::Render(FSpriteBatchPtr& Batch)
{
	IWidget::Render(Batch);
	BackgroundImage->Render(Batch);
	TextBackgroundImage->Render(Batch);

	if (Batch == nullptr)
	{
		return;
	}

	if (Font == nullptr)
	{
		return;
	}

	// Render text
	float TextCursorOffsetY = 0;
	size_t TextureIndex = 0;
	float LineHeight = Vector2(Font->MeasureString(FFont::GetTestString().c_str())).y;

	float AvailableHorizontalSpace = ((GetSize() - BorderOffsets).x) - 10.f;
	if (CanScroll())
	{
		const float HorizontalOffsetFromRightSide = 30.0f; //this is an additional offset to make sure our estimations don't fail (that could lead to line overlapping right-side border)
		AvailableHorizontalSpace = std::max(0.0f, Scroller->GetPosition().x - (Position + BorderOffsets).x - HorizontalOffsetFromRightSide);
	}

	for (size_t LineIndex = FirstViewedLine; (LineIndex < (FirstViewedLine + NumLinesPerPage)) && (LineIndex < Lines.Num()); ++LineIndex)
	{
		FColoredLine Line = Lines[LineIndex];
		std::wstring LineToRender = Line.GetMessage();
		FColor LineTextCol = Line.GetColor();
		Vector2 LabelSize = Font->MeasureString(LineToRender.c_str());
		float TextHeight = LabelSize.y;

		//Make sure line fits horizontally
		if (LabelSize.x > 0 && LabelSize.x > AvailableHorizontalSpace)
		{
			//We have to cut text
			size_t NumCharsThatFit = static_cast<size_t>((AvailableHorizontalSpace / LabelSize.x) * (float)(LineToRender.size()));
			LineToRender.resize(NumCharsThatFit);

			if (LineToRender.size() > 3)
			{
				LineToRender[LineToRender.size() - 1] = L'.';
				LineToRender[LineToRender.size() - 2] = L'.';
				LineToRender[LineToRender.size() - 3] = L'.';
			}

			LabelSize = Font->MeasureString(LineToRender.c_str());
			TextHeight = LabelSize.y;
		}

		Vector2 LabelPos = Position + BorderOffsets;
		LabelPos.y = LabelPos.y + TextCursorOffsetY;

		LabelPos.x = ceilf(LabelPos.x);
		LabelPos.y = ceilf(LabelPos.y);

		bool bIsSelected = bSelectionEnabled && LineIndex >= FirstSelectedLine && LineIndex <= LastSelectedLine;

#ifdef DXTK
		Font->DrawString(Batch.get(), LineToRender.c_str(), LabelPos, (bIsSelected) ? Color::Yellow : LineTextCol);
#endif //DXTK

#ifdef EOS_DEMO_SDL
		FTexturePtr TextTexture;
		if (TextureIndex < FontTextures.size())
		{
			TextTexture = FontTextures[TextureIndex];
		}
		else
		{
			TextTexture = Font->RenderString(LineToRender, (bIsSelected) ? Color::Yellow : LineTextCol);
			FontTextures.push_back(TextTexture);
		}

		if (TextTexture)
		{
			FSimpleMesh Mesh;
			Mesh.Texture = TextTexture;
			Vector2 QuadSize = Font->MeasureString(LineToRender);
			QuadSize.y = TextHeight;
			Mesh.Add2DQuad(LabelPos, QuadSize, Layer - 1);

			Batch->Draw(Mesh);
		}
#endif //EOS_DEMO_SDL
		
		TextCursorOffsetY += LineHeight + LineSpacing;
		TextCursorOffsetY = ceilf(TextCursorOffsetY);

		++TextureIndex;
	}

	if (CanScroll())
	{
		Scroller->Render(Batch);
	}

	bDirtyFlag = false;
}

void FTextViewWidget::SetPosition(Vector2 Pos)
{
	IWidget::SetPosition(Pos);

	BackgroundImage->SetPosition(Pos);

	TextBackgroundImage->SetPosition(Vector2(Pos.x + (Size.x * 0.01f), Pos.y + (Size.y * 0.08f)));

	if (Scroller)
	{
		Vector2 ScrollerPosition = Vector2(Pos.x + Size.x - ScrollerWidth - ScrollerOffsets.x, Pos.y + ScrollerOffsets.y);
		Scroller->SetPosition(ScrollerPosition);
	}
}

void FTextViewWidget::SetSize(Vector2 NewSize)
{
	IWidget::SetSize(NewSize);

	BackgroundImage->SetSize(NewSize);

	TextBackgroundImage->SetSize(Vector2(NewSize.x * 0.96f, NewSize.y * 0.8f));

	if (Scroller)
	{
		Scroller->SetSize(Vector2(ScrollerWidth, Size.y - (ScrollerOffsets.y * 2.f)));
	}

	UpdateNumLinesPerPage();

	if (Lines.Num() > NumLinesPerPage)
	{
		if (FirstViewedLine > (Lines.Num() - NumLinesPerPage))
		{
			FirstViewedLine = Lines.Num() - NumLinesPerPage;
		}
	}
}

void FTextViewWidget::UpdateNumLinesPerPage()
{
	if (Font == nullptr)
	{
		return;
	}
	Vector2 LabelSize = Font->MeasureString(FFont::GetTestString().c_str());
	float TextHeight = LabelSize.y;
	float LineHeight = TextHeight + LineSpacing;
	size_t OldNumLinesPerPage = NumLinesPerPage;
	NumLinesPerPage = size_t((GetSize().y - 2 * BorderOffsets.y) / LineHeight);

	if (NumLinesPerPage != OldNumLinesPerPage)
	{
		MarkDirty();
	}
}

void FTextViewWidget::Reset()
{
	Lines.Clear();
	Lines.PushBack(FColoredLine(InitialTextValue, TextCol));

#ifdef EOS_DEMO_SDL
	FontTextures.clear();
#endif

	MarkDirty();
}

void FTextViewWidget::Clear(bool bKeepCursor)
{
	Lines.Clear();
	bScrollable = false;
	if (!bKeepCursor)
	{
		FirstViewedLine = 0;
	}

	MarkDirty();
}

void FTextViewWidget::OnUIEvent(const FUIEvent& Event)
{
	if (!IsShown())
		return;

	if (Event.GetType() == EUIEventType::MousePressed)
	{
		if (CanScroll() && Scroller->CheckCollision(Event.GetVector()))
		{
			Scroller->OnUIEvent(Event);
			return;
		}

		if (bCanSelectText)
		{
			float YTop = Position.y + BorderOffsets.y;
			float YBottom = Position.y + Size.y - BorderOffsets.y;
			if (Event.GetVector().y > YTop && Event.GetVector().y < YBottom)
			{
				// Start selection
				bSelectionEnabled = true;
				bSelectingLines = true;

				StopSearch();

				FirstSelectedLine = LastSelectedLine = ScreenPositionToLineNum(FGame::Get().GetInput()->GetMousePosition());
				MarkDirty();
			}
		}
	}
	else if (Event.GetType() == EUIEventType::MouseReleased)
	{
		// Stop selection
		bSelectingLines = false;

		if (CanScroll())
		{
			Scroller->OnUIEvent(Event);
		}

		StopSearch();
	}

	if (Event.GetType() == EUIEventType::SelectAll)
	{
		SelectAllText();
	}
	else if (Event.GetType() == EUIEventType::CopyText)
	{
		if (bSelectionEnabled)
		{
			std::vector<std::wstring> SelectedLines;
			for (size_t Index = FirstSelectedLine; Index < Lines.Num() && Index <= LastSelectedLine; ++Index)
			{
				SelectedLines.push_back(Lines[Index].GetMessage());
			}
			if (!SelectedLines.empty())
			{
				FGame::Get().GetInput()->CopyToClipboard(SelectedLines);
			}
		}
	}

	if (CanScroll() && Event.GetType() != EUIEventType::MousePressed)
	{
		Scroller->OnUIEvent(Event);
	}
}

void FTextViewWidget::SetFocused(bool bFocused)
{
	IWidget::SetFocused(bFocused);

	if (!bFocused && bSelectionEnabled)
	{
		bSelectionEnabled = false;
		bSelectingLines = false;
		MarkDirty();
	}
}

void FTextViewWidget::WrapLine(const std::wstring& Line, float LineWidth, std::vector<std::wstring>& WrappedLines)
{
	//Some implementations may return 0 width when measuring space-only strings. Let's use underscore instead.
	float SpaceWidth = Vector2(Font->MeasureString("_")).x;

	std::wstring TabStr = L"     ";
	float TabWidth = SpaceWidth * TabStr.size();

	std::wstring Word;
	std::wstring WrappedLine;
	bool bNewLine = true;
	bool bNewWrappedLine = false;
	float SpaceLeft = 0.f;

	std::wstringstream WordsStream(Line); // splits line into words
	std::istream_iterator<std::wstring, wchar_t> WordsBeginIter(WordsStream), WordEndIter;
	std::vector<std::wstring> Words(WordsBeginIter, WordEndIter);

	for (size_t WordIndex = 0; WordIndex < Words.size(); ++WordIndex)
	{
		Word = Words[WordIndex];

		float WordWidth = Vector2(Font->MeasureString(Word.c_str())).x;

		const bool bJustAddedNewLine = bNewLine || bNewWrappedLine;

		//Add string prelude (tab or nothing)
		if (bJustAddedNewLine)
		{
			// We've just wrapped the line, we want to add tab to show that this is the continuation of the same line
			if (bNewWrappedLine)
			{
				WrappedLine = TabStr;
				SpaceLeft = LineWidth - TabWidth;
			}
			else
			{
				WrappedLine.clear();
				SpaceLeft = LineWidth;
			}

			bNewLine = false;
			bNewWrappedLine = false;
		}

		// Check next word will fit on a line
		if (SpaceLeft >= (WordWidth + ((bJustAddedNewLine) ? 0 : SpaceWidth)))
		{
			// No wrapping, add word to current wrapped line
			WrappedLine += ((bJustAddedNewLine) ? Word : (L' ' + Word));
			SpaceLeft -= (WordWidth + ((bJustAddedNewLine) ? 0 : SpaceWidth));
		}
		else
		{
			// Time to wrap, store current line (if present)
			if (!bJustAddedNewLine)
			{
				WrappedLines.emplace_back(std::move(WrappedLine));
			}

			if ((WordWidth <= (LineWidth - TabWidth)) && !bJustAddedNewLine)
			{
				// New line starts with wrapped word
				WrappedLine = TabStr + Word;
				SpaceLeft = LineWidth - WordWidth - TabWidth;
			}
			else
			{
				// Split long word over two or more lines
				if (!bJustAddedNewLine)
				{
					WrappedLine = TabStr;
				}

				for (size_t CharIndex = 0; CharIndex < Word.size(); ++CharIndex)
				{
					WrappedLine += Word[CharIndex];
					float WrappedLineWidth = Vector2(Font->MeasureString(WrappedLine.c_str())).x;
					if (WrappedLineWidth > LineWidth)
					{
						// Add new line
						WrappedLines.emplace_back(std::move(WrappedLine));
						WrappedLine = TabStr;

						if (CharIndex < Word.size() - 1)
						{
							bNewWrappedLine = true;
						}
					}
				}
				WrappedLines.emplace_back(std::move(WrappedLine));
			}
		}
	}

	WrappedLines.emplace_back(std::move(WrappedLine));
}

void FTextViewWidget::MarkDirty()
{
	bDirtyFlag = true;

#ifdef EOS_DEMO_SDL
	FontTextures.clear();
#endif //EOS_DEMO_SDL
}

void FTextViewWidget::AddLine(const std::wstring& Line, FColor Col)
{
	if (Font)
	{
		Vector2 LabelSize = Font->MeasureString(Line.c_str());
		float TextWidth = LabelSize.x;
		float TextViewWidth = 0.f;
		if (!bScrollerDisabled)
		{
			TextViewWidth = (Size.x - ScrollerWidth - BorderOffsets.x) * 0.95f;
		}
		else
		{
			TextViewWidth = (Size.x - BorderOffsets.x) * 0.95f;
		}
		if (TextWidth > TextViewWidth)
		{
			std::vector<std::wstring> WrappedLines;
			WrapLine(Line, TextViewWidth, WrappedLines);
			for (std::vector<std::wstring>::iterator WordIter = WrappedLines.begin(); WordIter != WrappedLines.end(); ++WordIter)
			{
				Lines.PushBack(FColoredLine(*WordIter, Col));
			}
		}
		else
		{
			Lines.PushBack(FColoredLine(Line, Col));
		}
	}
	else
	{
		Lines.PushBack(FColoredLine(Line, Col));
	}

	if (Lines.Num() > NumLinesPerPage)
	{
		bScrollable = true;
	}

	MarkDirty();
}

std::wstring FTextViewWidget::GetLine(size_t LineIndex)
{
	if (LineIndex < Lines.Num())
	{
		FColoredLine Line = Lines[LineIndex];
		return Line.GetMessage();
	}
	return std::wstring();
}

void FTextViewWidget::DropFirstLines(size_t NumLinesToDrop)
{
	NumLinesToDrop = std::min(NumLinesToDrop, Lines.Num());

	for (size_t Index = 0; Index < NumLinesToDrop; ++Index)
	{
		Lines.PopFront();
	}

#ifdef EOS_DEMO_SDL
	FontTextures.clear();
#endif

	MarkDirty();
}

bool FTextViewWidget::Search(const std::wstring& SearchText)
{
	size_t SearchStart = 0;
	if (bSearching)
	{
		SearchStart = FirstSelectedLine + 1;

		if (SearchStart > Lines.Num())
		{
			SearchStart = 0;
		}
	}

	for (size_t i = 0; i < Lines.Num(); ++i)
	{
		size_t Index = i;
		if (SearchStart != 0)
		{
			Index += SearchStart;
			if (Index >= Lines.Num())
			{
				Index -= Lines.Num();
			}
		}

		size_t Pos = Lines[Index].GetMessage().find(SearchText);
		if (Pos != std::wstring::npos)
		{
			bSelectionEnabled = true;
			bSelectingLines = false;
			FirstSelectedLine = LastSelectedLine = Index;

			//scroll
			if (CanScroll())
			{
				if (FirstViewedEntry() > Index)
				{
					ScrollUp(FirstViewedEntry() - Index);
				}

				if (LastViewedEntry() < Index)
				{
					ScrollDown(Index - LastViewedEntry());
				}
			}

			MarkDirty();

			bSearching = true;

			return true;
		}
	}

	StopSearch();
	return false;
}

void FTextViewWidget::StopSearch()
{
	bSearching = false;
}

bool FTextViewWidget::IsAutoScrolling() const
{
	return !bSearching && !bSelectionEnabled;
}


void FTextViewWidget::SelectAllText()
{
	if (bCanSelectText)
	{
		// Select all
		bSelectionEnabled = true;
		FirstSelectedLine = 0;
		LastSelectedLine = Lines.Num() - 1;
		MarkDirty();
	}
}

void FTextViewWidget::ScrollUp(size_t Length)
{
	if (!CanScroll())
	{
		return;
	}

	if (FirstViewedLine < Length)
	{
		FirstViewedLine = 0;
	}
	else
	{
		FirstViewedLine -= Length;
	}

	MarkDirty();
}

void FTextViewWidget::ScrollDown(size_t length)
{
	if (!CanScroll())
	{
		return;
	}

	if (Lines.Num() < NumLinesPerPage)
	{
		FirstViewedLine = 0;
	}
	else
	{
		FirstViewedLine += length;
		const size_t MaxFirstViewedLine = Lines.Num() - NumLinesPerPage;
		if (FirstViewedLine > MaxFirstViewedLine)
		{
			FirstViewedLine = MaxFirstViewedLine;
		}
	}

	MarkDirty();
}

void FTextViewWidget::ScrollToTop()
{
	ScrollUp(Lines.Num());
}

void FTextViewWidget::ScrollToBottom()
{
	ScrollDown(Lines.Num());
}

size_t FTextViewWidget::NumEntries() const
{
	return Lines.Num();
}

size_t FTextViewWidget::GetNumLinesPerPage() const
{
	return NumLinesPerPage;
}

size_t FTextViewWidget::FirstViewedEntry() const
{
	return FirstViewedLine;
}

size_t FTextViewWidget::LastViewedEntry() const
{
	return FirstViewedLine + NumLinesPerPage - 1;
}

size_t FTextViewWidget::ScreenPositionToLineNum(Vector2 ScreenPos) const
{
	float YCoordinate = ScreenPos.y;
	float YTop = Position.y + BorderOffsets.y;
	float YBottom = Position.y + Size.y - BorderOffsets.y;
	float LineHeight = (YBottom - YTop) / NumLinesPerPage;
		
	// Clamp to working region
	if (YCoordinate < YTop)
	{
		YCoordinate = YTop;
	}
	else if (YCoordinate > YBottom)
	{
		YCoordinate = YBottom;
	}

	size_t SelectedLineIndex = FirstViewedLine + static_cast<size_t>((YCoordinate - YTop) / LineHeight);
	return SelectedLineIndex;
}

void FTextViewWidget::CreateScroller()
{
	Vector2 scrollerPosition = Vector2(Position.x + Size.x - ScrollerWidth - BorderOffsets.x, Position.y + BorderOffsets.y);
	scrollerPosition += ScrollerOffsets;
	Scroller = std::make_unique<FScroller>(std::static_pointer_cast<FTextViewWidget>(shared_from_this()),
		scrollerPosition,
		Vector2(ScrollerWidth, Size.y - (2.f * BorderOffsets.y)),
		Layer - 1,
		L"Assets/scrollbar.dds");

	Scroller->Create();
}

void FTextViewWidget::EnableScroller()
{
	bScrollerDisabled = false;

	// Create scroller if not already created
	if (Scroller == nullptr)
	{
		CreateScroller();
	}
}

bool FTextViewWidget::HasInitialText() const
{
	if (Lines.Num() == 1 &&
		!InitialTextValue.empty() &&
		InitialTextValue == Lines[0].GetMessage())
	{
		return true;
	}

	return false;
}

bool FTextViewWidget::IsEmpty() const
{
	return Lines.Num() == 0 || 
		(Lines.Num() == 1 && Lines[0].GetMessage().empty());
}