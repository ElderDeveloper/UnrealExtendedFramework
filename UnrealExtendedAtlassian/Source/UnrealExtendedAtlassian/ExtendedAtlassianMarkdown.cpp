// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianMarkdown.h"

namespace ExtendedAtlassianMarkdownPrivate
{
	/** Markdown nests lists by two spaces (or a tab) per level. */
	constexpr int32 SpacesPerIndentLevel = 2;

	bool IsHorizontalRule(const FString& Trimmed)
	{
		if (Trimmed.Len() < 3)
		{
			return false;
		}

		const TCHAR First = Trimmed[0];
		if (First != TEXT('-') && First != TEXT('*') && First != TEXT('_'))
		{
			return false;
		}

		for (const TCHAR Char : Trimmed)
		{
			if (Char != First && Char != TEXT(' '))
			{
				return false;
			}
		}

		return true;
	}

	/** Leading whitespace converted to a nesting depth, tabs counting as one level. */
	int32 MeasureIndent(const FString& Line)
	{
		int32 Spaces = 0;
		for (const TCHAR Char : Line)
		{
			if (Char == TEXT(' '))
			{
				++Spaces;
			}
			else if (Char == TEXT('\t'))
			{
				Spaces += SpacesPerIndentLevel;
			}
			else
			{
				break;
			}
		}

		return Spaces / SpacesPerIndentLevel;
	}

	/** A pipe-table separator, e.g. |---|:--:|. */
	bool IsTableSeparator(const FString& Trimmed)
	{
		if (!Trimmed.Contains(TEXT("|")) || !Trimmed.Contains(TEXT("-")))
		{
			return false;
		}

		for (const TCHAR Char : Trimmed)
		{
			if (Char != TEXT('|') && Char != TEXT('-') && Char != TEXT(':') && Char != TEXT(' '))
			{
				return false;
			}
		}

		return true;
	}

	void SplitTableRow(const FString& Line, TArray<FString>& OutCells)
	{
		FString Working = Line.TrimStartAndEnd();

		// Outer pipes are optional in GFM; strip them so they do not produce empty cells.
		if (Working.StartsWith(TEXT("|")))
		{
			Working.RightChopInline(1);
		}
		if (Working.EndsWith(TEXT("|")))
		{
			Working.LeftChopInline(1);
		}

		Working.ParseIntoArray(OutCells, TEXT("|"), false);
		for (FString& Cell : OutCells)
		{
			Cell.TrimStartAndEndInline();
		}
	}

	/** True when the run of Marker starting at Index is a closing delimiter we can pair with. */
	int32 FindClosingDelimiter(const FString& Text, int32 SearchStart, const FString& Delimiter)
	{
		return Text.Find(Delimiter, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchStart);
	}
}

FString FExtendedAtlassianMarkdown::InlineToMarkup(const FString& Line)
{
	using namespace ExtendedAtlassianMarkdownPrivate;

	FString Out;
	Out.Reserve(Line.Len() + 32);

	FString Pending;

	// Plain text accumulates unescaped and is escaped in one go, so escaping never touches the tags
	// emitted for formatting.
	auto FlushPending = [&Out, &Pending]()
	{
		if (!Pending.IsEmpty())
		{
			Out += FExtendedAtlassianMarkup::Escape(Pending);
			Pending.Reset();
		}
	};

	const int32 Length = Line.Len();
	int32 Index = 0;

	while (Index < Length)
	{
		const TCHAR Char = Line[Index];

		// Inline code first: its contents are literal and must not be scanned for other markers.
		if (Char == TEXT('`'))
		{
			const int32 Close = FindClosingDelimiter(Line, Index + 1, TEXT("`"));
			if (Close != INDEX_NONE)
			{
				FlushPending();
				const FString Code = Line.Mid(Index + 1, Close - Index - 1);
				Out += FExtendedAtlassianMarkup::Styled(TEXT("Code"), FExtendedAtlassianMarkup::Escape(Code));
				Index = Close + 1;
				continue;
			}
		}

		// [text](url)
		if (Char == TEXT('['))
		{
			const int32 CloseBracket = FindClosingDelimiter(Line, Index + 1, TEXT("]"));
			if (CloseBracket != INDEX_NONE &&
				CloseBracket + 1 < Length &&
				Line[CloseBracket + 1] == TEXT('('))
			{
				const int32 CloseParen = FindClosingDelimiter(Line, CloseBracket + 2, TEXT(")"));
				if (CloseParen != INDEX_NONE)
				{
					FlushPending();

					const FString Text = Line.Mid(Index + 1, CloseBracket - Index - 1);
					const FString Url = Line.Mid(CloseBracket + 2, CloseParen - CloseBracket - 2);

					Out += FExtendedAtlassianMarkup::Link(Url, FExtendedAtlassianMarkup::Escape(Text));
					Index = CloseParen + 1;
					continue;
				}
			}
		}

		// ~~strike~~
		if (Char == TEXT('~') && Index + 1 < Length && Line[Index + 1] == TEXT('~'))
		{
			const int32 Close = FindClosingDelimiter(Line, Index + 2, TEXT("~~"));
			if (Close != INDEX_NONE)
			{
				FlushPending();
				const FString Inner = Line.Mid(Index + 2, Close - Index - 2);
				Out += FExtendedAtlassianMarkup::Styled(TEXT("Strike"), FExtendedAtlassianMarkup::Escape(Inner));
				Index = Close + 2;
				continue;
			}
		}

		// **bold** / __bold__ before the single-character italic forms.
		if ((Char == TEXT('*') || Char == TEXT('_')) && Index + 1 < Length && Line[Index + 1] == Char)
		{
			const FString Delimiter = FString::ChrN(2, Char);
			const int32 Close = FindClosingDelimiter(Line, Index + 2, Delimiter);
			if (Close != INDEX_NONE)
			{
				FlushPending();
				const FString Inner = Line.Mid(Index + 2, Close - Index - 2);
				Out += FExtendedAtlassianMarkup::Styled(TEXT("Bold"), FExtendedAtlassianMarkup::Escape(Inner));
				Index = Close + 2;
				continue;
			}
		}

		// *italic* / _italic_
		if (Char == TEXT('*') || Char == TEXT('_'))
		{
			const FString Delimiter = FString::Chr(Char);
			const int32 Close = FindClosingDelimiter(Line, Index + 1, Delimiter);

			// Require content: "a * b" should stay literal rather than swallowing the rest.
			if (Close != INDEX_NONE && Close > Index + 1)
			{
				FlushPending();
				const FString Inner = Line.Mid(Index + 1, Close - Index - 1);
				Out += FExtendedAtlassianMarkup::Styled(TEXT("Italic"), FExtendedAtlassianMarkup::Escape(Inner));
				Index = Close + 1;
				continue;
			}
		}

		// Bare URLs, so pasted links are still clickable.
		if (Char == TEXT('h') && (Line.Mid(Index, 7) == TEXT("http://") || Line.Mid(Index, 8) == TEXT("https://")))
		{
			int32 End = Index;
			while (End < Length && !FChar::IsWhitespace(Line[End]))
			{
				++End;
			}

			// Trailing punctuation is almost always sentence punctuation, not part of the URL.
			while (End > Index && (Line[End - 1] == TEXT('.') || Line[End - 1] == TEXT(',') || Line[End - 1] == TEXT(')')))
			{
				--End;
			}

			FlushPending();
			const FString Url = Line.Mid(Index, End - Index);
			Out += FExtendedAtlassianMarkup::Link(Url, FExtendedAtlassianMarkup::Escape(Url));
			Index = End;
			continue;
		}

		Pending.AppendChar(Char);
		++Index;
	}

	FlushPending();
	return Out;
}

FString FExtendedAtlassianMarkdown::MarkupToInline(const FString& Markup)
{
	FString Out;
	Out.Reserve(Markup.Len());

	const int32 Length = Markup.Len();
	int32 Index = 0;

	while (Index < Length)
	{
		if (Markup[Index] != TEXT('<'))
		{
			Out.AppendChar(Markup[Index]);
			++Index;
			continue;
		}

		const int32 TagEnd = Markup.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);
		if (TagEnd == INDEX_NONE)
		{
			Out.AppendChar(Markup[Index]);
			++Index;
			continue;
		}

		const FString Tag = Markup.Mid(Index + 1, TagEnd - Index - 1);

		// "</>" closes whatever run is open; the delimiter was already emitted on open.
		if (Tag == TEXT("/"))
		{
			Index = TagEnd + 1;
			continue;
		}

		const int32 ContentStart = TagEnd + 1;
		const int32 CloseTag = Markup.Find(TEXT("</>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ContentStart);
		const int32 ContentEnd = CloseTag == INDEX_NONE ? Length : CloseTag;

		const FString Inner = Markup.Mid(ContentStart, ContentEnd - ContentStart);

		if (Tag == TEXT("Bold"))
		{
			Out += FString::Printf(TEXT("**%s**"), *MarkupToInline(Inner));
		}
		else if (Tag == TEXT("Italic"))
		{
			Out += FString::Printf(TEXT("*%s*"), *MarkupToInline(Inner));
		}
		else if (Tag == TEXT("Strike"))
		{
			Out += FString::Printf(TEXT("~~%s~~"), *MarkupToInline(Inner));
		}
		else if (Tag == TEXT("Code"))
		{
			// Code content is literal: emit it verbatim rather than recursing.
			Out += FString::Printf(TEXT("`%s`"), *Inner);
		}
		else if (Tag.StartsWith(TEXT("a ")))
		{
			FString Href;
			const int32 HrefStart = Tag.Find(TEXT("href=\""));
			if (HrefStart != INDEX_NONE)
			{
				const int32 ValueStart = HrefStart + 6;
				const int32 ValueEnd = Tag.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
				if (ValueEnd != INDEX_NONE)
				{
					Href = Tag.Mid(ValueStart, ValueEnd - ValueStart);
				}
			}

			const FString Text = MarkupToInline(Inner);
			Out += Href.IsEmpty() ? Text : FString::Printf(TEXT("[%s](%s)"), *Text, *Href);
		}
		else
		{
			Out += MarkupToInline(Inner);
		}

		Index = CloseTag == INDEX_NONE ? Length : CloseTag + 3;
	}

	// Entities were introduced when building markup; the file must hold the real characters.
	Out.ReplaceInline(TEXT("&lt;"), TEXT("<"));
	Out.ReplaceInline(TEXT("&gt;"), TEXT(">"));
	Out.ReplaceInline(TEXT("&quot;"), TEXT("\""));
	Out.ReplaceInline(TEXT("&amp;"), TEXT("&"));

	return Out;
}

FString FExtendedAtlassianMarkdown::FromBlocks(const TArray<FExtendedAtlassianDocBlock>& Blocks)
{
	TArray<FString> Lines;

	for (int32 Index = 0; Index < Blocks.Num(); ++Index)
	{
		const FExtendedAtlassianDocBlock& Block = Blocks[Index];
		const FString Indent = FString::ChrN(Block.IndentDepth * 2, TEXT(' '));
		const FString Inline = MarkupToInline(Block.Markup);

		switch (Block.Kind)
		{
		case EExtendedAtlassianBlockKind::Heading:
			Lines.Add(FString::ChrN(FMath::Clamp(Block.Level, 1, 6), TEXT('#')) + TEXT(" ") + Inline);
			Lines.Add(FString());
			break;

		case EExtendedAtlassianBlockKind::BulletItem:
			Lines.Add(Indent + TEXT("- ") + Inline);
			break;

		case EExtendedAtlassianBlockKind::OrderedItem:
			Lines.Add(Indent + FString::Printf(TEXT("%d. "), Block.OrderedIndex > 0 ? Block.OrderedIndex : 1) + Inline);
			break;

		case EExtendedAtlassianBlockKind::TaskItem:
			Lines.Add(Indent + (Block.bChecked ? TEXT("- [x] ") : TEXT("- [ ] ")) + Inline);
			break;

		case EExtendedAtlassianBlockKind::Quote:
			Lines.Add(TEXT("> ") + Inline);
			Lines.Add(FString());
			break;

		case EExtendedAtlassianBlockKind::CodeBlock:
			Lines.Add(
				TEXT("```")
				+ Block.CodeLanguage
				+ (Block.ImageAlt.IsEmpty()
					? FString()
					: TEXT(" ") + Block.ImageAlt));
			Lines.Add(Block.RawText);
			Lines.Add(TEXT("```"));
			Lines.Add(FString());
			break;

		case EExtendedAtlassianBlockKind::Rule:
			Lines.Add(TEXT("---"));
			Lines.Add(FString());
			break;

		case EExtendedAtlassianBlockKind::Image:
		{
			const FString Target = Block.ImageUrl.IsEmpty()
				? FString(TEXT("backlot-embed"))
				: Block.ImageUrl;
			FString CardData = Block.EmbedSlot;
			if (!Block.ImageMeta.IsEmpty())
			{
				CardData += (CardData.IsEmpty() ? FString() : TEXT("|"))
					+ Block.ImageMeta;
			}
			CardData.ReplaceInline(TEXT("\""), TEXT("&quot;"));
			Lines.Add(
				CardData.IsEmpty()
					? FString::Printf(
						TEXT("![%s](%s)"),
						*Block.ImageAlt,
						*Target)
					: FString::Printf(
						TEXT("![%s](%s \"%s\")"),
						*Block.ImageAlt,
						*Target,
						*CardData));
			Lines.Add(FString());
			break;
		}

		case EExtendedAtlassianBlockKind::TableRow:
		{
			TArray<FString> Cells;
			for (const FString& Cell : Block.Cells)
			{
				Cells.Add(MarkupToInline(Cell));
			}
			Lines.Add(TEXT("| ") + FString::Join(Cells, TEXT(" | ")) + TEXT(" |"));

			// A GFM table is only a table if a separator follows the header row.
			if (Block.bIsHeaderRow)
			{
				TArray<FString> Dashes;
				for (int32 CellIndex = 0; CellIndex < Block.Cells.Num(); ++CellIndex)
				{
					Dashes.Add(TEXT("---"));
				}
				Lines.Add(TEXT("| ") + FString::Join(Dashes, TEXT(" | ")) + TEXT(" |"));
			}
			break;
		}

		default:
			Lines.Add(Inline);
			Lines.Add(FString());
			break;
		}

		// A list run needs a blank line after it, but not between its items.
		const bool bIsListItem =
			Block.Kind == EExtendedAtlassianBlockKind::BulletItem ||
			Block.Kind == EExtendedAtlassianBlockKind::OrderedItem ||
			Block.Kind == EExtendedAtlassianBlockKind::TaskItem ||
			Block.Kind == EExtendedAtlassianBlockKind::TableRow;

		if (bIsListItem)
		{
			const bool bNextContinuesRun =
				Index + 1 < Blocks.Num() &&
				(Blocks[Index + 1].Kind == Block.Kind ||
				 (Block.Kind == EExtendedAtlassianBlockKind::TableRow &&
				  Blocks[Index + 1].Kind == EExtendedAtlassianBlockKind::TableRow));

			if (!bNextContinuesRun)
			{
				Lines.Add(FString());
			}
		}
	}

	FString Result = FString::Join(Lines, TEXT("\n"));
	Result.TrimEndInline();
	return Result + TEXT("\n");
}

TArray<FExtendedAtlassianDocBlock> FExtendedAtlassianMarkdown::ToBlocks(const FString& Markdown)
{
	using namespace ExtendedAtlassianMarkdownPrivate;

	TArray<FExtendedAtlassianDocBlock> Blocks;

	if (Markdown.IsEmpty())
	{
		return Blocks;
	}

	FString Normalized = Markdown;
	Normalized.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
	Normalized.ReplaceInline(TEXT("\r"), TEXT("\n"));

	TArray<FString> Lines;
	Normalized.ParseIntoArray(Lines, TEXT("\n"), false);

	FString ParagraphBuffer;

	auto FlushParagraph = [&Blocks, &ParagraphBuffer]()
	{
		if (ParagraphBuffer.IsEmpty())
		{
			return;
		}

		FExtendedAtlassianDocBlock Block;
		Block.Kind = EExtendedAtlassianBlockKind::Paragraph;
		Block.Markup = InlineToMarkup(ParagraphBuffer);
		Blocks.Add(Block);

		ParagraphBuffer.Reset();
	};

	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		const FString& Line = Lines[LineIndex];
		const FString Trimmed = Line.TrimStartAndEnd();

		// --- Fenced code -----------------------------------------------
		if (Trimmed.StartsWith(TEXT("```")))
		{
			FlushParagraph();

			FExtendedAtlassianDocBlock Block;
			Block.Kind = EExtendedAtlassianBlockKind::CodeBlock;
			const FString FenceInfo = Trimmed.RightChop(3).TrimStartAndEnd();
			int32 FirstSpace = INDEX_NONE;
			if (FenceInfo.FindChar(TEXT(' '), FirstSpace))
			{
				Block.CodeLanguage = FenceInfo.Left(FirstSpace);
				Block.ImageAlt = FenceInfo.Mid(FirstSpace + 1).TrimStartAndEnd();
			}
			else
			{
				Block.CodeLanguage = FenceInfo;
			}

			TArray<FString> CodeLines;
			++LineIndex;
			while (LineIndex < Lines.Num() && !Lines[LineIndex].TrimStartAndEnd().StartsWith(TEXT("```")))
			{
				CodeLines.Add(Lines[LineIndex]);
				++LineIndex;
			}

			Block.RawText = FString::Join(CodeLines, TEXT("\n"));
			Blocks.Add(Block);
			continue;
		}

		// --- Blank line ------------------------------------------------
		if (Trimmed.IsEmpty())
		{
			FlushParagraph();
			continue;
		}

		// --- Horizontal rule -------------------------------------------
		if (IsHorizontalRule(Trimmed))
		{
			FlushParagraph();

			FExtendedAtlassianDocBlock Block;
			Block.Kind = EExtendedAtlassianBlockKind::Rule;
			Blocks.Add(Block);
			continue;
		}

		// --- Heading ---------------------------------------------------
		if (Trimmed.StartsWith(TEXT("#")))
		{
			int32 Level = 0;
			while (Level < Trimmed.Len() && Trimmed[Level] == TEXT('#'))
			{
				++Level;
			}

			// "#Text" without a space is not a heading in Markdown, and treating it as one would
			// mangle things like "#1 priority".
			if (Level >= 1 && Level <= 6 && Level < Trimmed.Len() && Trimmed[Level] == TEXT(' '))
			{
				FlushParagraph();

				FExtendedAtlassianDocBlock Block;
				Block.Kind = EExtendedAtlassianBlockKind::Heading;
				Block.Level = Level;
				Block.Markup = InlineToMarkup(Trimmed.RightChop(Level).TrimStartAndEnd());
				Blocks.Add(Block);
				continue;
			}
		}

		// --- Blockquote ------------------------------------------------
		if (Trimmed.StartsWith(TEXT(">")))
		{
			FlushParagraph();

			FExtendedAtlassianDocBlock Block;
			Block.Kind = EExtendedAtlassianBlockKind::Quote;
			Block.Markup = InlineToMarkup(Trimmed.RightChop(1).TrimStartAndEnd());
			Blocks.Add(Block);
			continue;
		}

		// --- Table -----------------------------------------------------
		if (Trimmed.Contains(TEXT("|")) &&
			LineIndex + 1 < Lines.Num() &&
			IsTableSeparator(Lines[LineIndex + 1].TrimStartAndEnd()))
		{
			FlushParagraph();

			// Header row, then the separator, then body rows until the pipes stop.
			TArray<FString> HeaderCells;
			SplitTableRow(Trimmed, HeaderCells);

			FExtendedAtlassianDocBlock HeaderBlock;
			HeaderBlock.Kind = EExtendedAtlassianBlockKind::TableRow;
			HeaderBlock.bIsHeaderRow = true;
			for (const FString& Cell : HeaderCells)
			{
				HeaderBlock.Cells.Add(InlineToMarkup(Cell));
			}
			Blocks.Add(HeaderBlock);

			LineIndex += 2;
			while (LineIndex < Lines.Num() && Lines[LineIndex].Contains(TEXT("|")))
			{
				TArray<FString> BodyCells;
				SplitTableRow(Lines[LineIndex], BodyCells);

				FExtendedAtlassianDocBlock RowBlock;
				RowBlock.Kind = EExtendedAtlassianBlockKind::TableRow;
				for (const FString& Cell : BodyCells)
				{
					RowBlock.Cells.Add(InlineToMarkup(Cell));
				}
				Blocks.Add(RowBlock);

				++LineIndex;
			}

			--LineIndex; // The outer loop advances again.
			continue;
		}

		// --- Image / Backlot asset embed -------------------------------
		if (Trimmed.StartsWith(TEXT("![")))
		{
			const int32 AltEnd = Trimmed.Find(
				TEXT("]("),
				ESearchCase::CaseSensitive,
				ESearchDir::FromStart,
				2);
			const int32 UrlEnd = Trimmed.EndsWith(TEXT(")"))
				? Trimmed.Len() - 1
				: INDEX_NONE;
			if (AltEnd != INDEX_NONE && UrlEnd > AltEnd + 2)
			{
				FlushParagraph();

				FExtendedAtlassianDocBlock Block;
				Block.Kind = EExtendedAtlassianBlockKind::Image;
				Block.ImageAlt = Trimmed.Mid(2, AltEnd - 2);
				FString TargetAndTitle = Trimmed.Mid(
					AltEnd + 2,
					UrlEnd - AltEnd - 2);
				int32 LastQuote = INDEX_NONE;
				if (TargetAndTitle.FindLastChar(TEXT('"'), LastQuote)
					&& LastQuote == TargetAndTitle.Len() - 1)
				{
					const int32 OpeningQuote = TargetAndTitle.Find(
						TEXT(" \""),
						ESearchCase::CaseSensitive,
						ESearchDir::FromStart);
					if (OpeningQuote != INDEX_NONE)
					{
						FString CardData = TargetAndTitle.Mid(
							OpeningQuote + 2,
							TargetAndTitle.Len() - OpeningQuote - 3);
						CardData.ReplaceInline(TEXT("&quot;"), TEXT("\""));
						int32 Separator = INDEX_NONE;
						if (CardData.FindChar(TEXT('|'), Separator))
						{
							Block.EmbedSlot = CardData.Left(Separator);
							Block.ImageMeta = CardData.Mid(Separator + 1);
						}
						else
						{
							Block.ImageMeta = CardData;
						}
						TargetAndTitle.LeftInline(OpeningQuote);
					}
				}
				Block.ImageUrl =
					TargetAndTitle == TEXT("backlot-embed")
						? FString()
						: TargetAndTitle;
				Blocks.Add(MoveTemp(Block));
				continue;
			}
		}

		// --- List items ------------------------------------------------
		{
			const int32 Indent = MeasureIndent(Line);

			// Task list: "- [ ] text" or "- [x] text"
			if ((Trimmed.StartsWith(TEXT("- [")) || Trimmed.StartsWith(TEXT("* [")) || Trimmed.StartsWith(TEXT("+ ["))) &&
				Trimmed.Len() > 5 && Trimmed[4] == TEXT(']'))
			{
				FlushParagraph();

				FExtendedAtlassianDocBlock Block;
				Block.Kind = EExtendedAtlassianBlockKind::TaskItem;
				Block.IndentDepth = Indent;
				Block.bChecked = Trimmed[3] == TEXT('x') || Trimmed[3] == TEXT('X');
				Block.Markup = InlineToMarkup(Trimmed.RightChop(5).TrimStartAndEnd());
				Blocks.Add(Block);
				continue;
			}

			if (Trimmed.StartsWith(TEXT("- ")) || Trimmed.StartsWith(TEXT("* ")) || Trimmed.StartsWith(TEXT("+ ")))
			{
				FlushParagraph();

				FExtendedAtlassianDocBlock Block;
				Block.Kind = EExtendedAtlassianBlockKind::BulletItem;
				Block.IndentDepth = Indent;
				Block.Markup = InlineToMarkup(Trimmed.RightChop(2).TrimStartAndEnd());
				Blocks.Add(Block);
				continue;
			}

			// Ordered: "1. text"
			int32 DigitCount = 0;
			while (DigitCount < Trimmed.Len() && FChar::IsDigit(Trimmed[DigitCount]))
			{
				++DigitCount;
			}

			if (DigitCount > 0 &&
				DigitCount + 1 < Trimmed.Len() &&
				Trimmed[DigitCount] == TEXT('.') &&
				Trimmed[DigitCount + 1] == TEXT(' '))
			{
				FlushParagraph();

				FExtendedAtlassianDocBlock Block;
				Block.Kind = EExtendedAtlassianBlockKind::OrderedItem;
				Block.IndentDepth = Indent;
				Block.OrderedIndex = FCString::Atoi(*Trimmed.Left(DigitCount));
				Block.Markup = InlineToMarkup(Trimmed.RightChop(DigitCount + 2).TrimStartAndEnd());
				Blocks.Add(Block);
				continue;
			}
		}

		// --- Paragraph continuation ------------------------------------
		if (!ParagraphBuffer.IsEmpty())
		{
			ParagraphBuffer += TEXT(" ");
		}
		ParagraphBuffer += Trimmed;
	}

	FlushParagraph();
	return Blocks;
}
