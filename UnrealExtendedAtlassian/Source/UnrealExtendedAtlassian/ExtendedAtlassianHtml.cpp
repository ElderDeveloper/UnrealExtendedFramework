// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianHtml.h"

namespace ExtendedAtlassianHtmlPrivate
{
	/** Tag name, lowercased, with any attributes stripped. */
	FString ParseTagName(const FString& TagBody)
	{
		FString Name;
		for (const TCHAR Char : TagBody)
		{
			if (FChar::IsWhitespace(Char) || Char == TEXT('/'))
			{
				break;
			}
			Name.AppendChar(FChar::ToLower(Char));
		}
		return Name;
	}

	/** Reads a quoted attribute value out of a raw tag body. Returns empty when absent. */
	FString GetAttribute(const FString& TagBody, const FString& AttributeName)
	{
		const FString Needle = AttributeName.ToLower() + TEXT("=");

		// Match case-insensitively on a lowered copy, but slice the value out of the original so
		// the returned URL keeps its casing.
		const int32 Start = TagBody.ToLower().Find(Needle, ESearchCase::CaseSensitive);
		if (Start == INDEX_NONE)
		{
			return FString();
		}

		int32 ValueStart = Start + Needle.Len();
		if (ValueStart >= TagBody.Len())
		{
			return FString();
		}

		const TCHAR Quote = TagBody[ValueStart];
		if (Quote == TEXT('"') || Quote == TEXT('\''))
		{
			++ValueStart;
			const int32 ValueEnd = TagBody.Find(FString::Chr(Quote), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
			if (ValueEnd == INDEX_NONE)
			{
				return FString();
			}
			return TagBody.Mid(ValueStart, ValueEnd - ValueStart);
		}

		// Unquoted value: read to the next whitespace.
		int32 ValueEnd = ValueStart;
		while (ValueEnd < TagBody.Len() && !FChar::IsWhitespace(TagBody[ValueEnd]))
		{
			++ValueEnd;
		}
		return TagBody.Mid(ValueStart, ValueEnd - ValueStart);
	}

	bool IsBlockTag(const FString& Name)
	{
		static const TSet<FString> BlockTags = {
			TEXT("p"), TEXT("div"), TEXT("section"), TEXT("article"),
			TEXT("h1"), TEXT("h2"), TEXT("h3"), TEXT("h4"), TEXT("h5"), TEXT("h6"),
			TEXT("li"), TEXT("ul"), TEXT("ol"), TEXT("table"), TEXT("tr"),
			TEXT("blockquote"), TEXT("pre"),
		};
		return BlockTags.Contains(Name);
	}

	void AppendNewline(FString& Out)
	{
		if (!Out.IsEmpty() && !Out.EndsWith(TEXT("\n")))
		{
			Out.AppendChar(TEXT('\n'));
		}
	}

	/** Trims each line, collapses internal whitespace runs, and limits blank runs to one. */
	FString Tidy(const FString& In)
	{
		TArray<FString> Lines;
		In.ParseIntoArray(Lines, TEXT("\n"), false);

		FString Result;
		Result.Reserve(In.Len());

		int32 ConsecutiveBlank = 0;

		for (const FString& Line : Lines)
		{
			// Preserve leading indentation: it carries list nesting.
			int32 LeadingSpaces = 0;
			while (LeadingSpaces < Line.Len() && (Line[LeadingSpaces] == TEXT(' ') || Line[LeadingSpaces] == TEXT('\t')))
			{
				++LeadingSpaces;
			}

			FString Collapsed;
			bool bPreviousWasSpace = false;
			for (int32 Index = LeadingSpaces; Index < Line.Len(); ++Index)
			{
				const TCHAR Char = Line[Index];
				if (Char == TEXT(' ') || Char == TEXT('\t') || Char == TEXT('\r'))
				{
					if (!bPreviousWasSpace)
					{
						Collapsed.AppendChar(TEXT(' '));
						bPreviousWasSpace = true;
					}
					continue;
				}

				Collapsed.AppendChar(Char);
				bPreviousWasSpace = false;
			}

			Collapsed.TrimEndInline();

			if (Collapsed.IsEmpty())
			{
				if (++ConsecutiveBlank > 1)
				{
					continue;
				}
				Result.AppendChar(TEXT('\n'));
				continue;
			}

			ConsecutiveBlank = 0;
			Result += FString::ChrN(LeadingSpaces, TEXT(' ')) + Collapsed + TEXT("\n");
		}

		return Result.TrimStartAndEnd();
	}
}

namespace ExtendedAtlassianHtmlBlockPrivate
{
	/**
	 * Slate rich text does not nest style runs, so only the innermost applies, and a link wins over
	 * a style. Bold-inside-a-link renders as a link; acceptable, and far safer than emitting nested
	 * tags the markup parser would mishandle.
	 */
	FString StyleInline(const FString& RawText, const TArray<FString>& StyleStack, const TArray<FString>& LinkStack)
	{
		if (RawText.IsEmpty())
		{
			return FString();
		}

		const FString Escaped = FExtendedAtlassianMarkup::Escape(FExtendedAtlassianHtml::DecodeEntities(RawText));

		for (int32 Index = LinkStack.Num() - 1; Index >= 0; --Index)
		{
			const FString& Href = LinkStack[Index];
			if (Href.StartsWith(TEXT("http://")) || Href.StartsWith(TEXT("https://")))
			{
				return FExtendedAtlassianMarkup::Link(Href, Escaped);
			}
		}

		if (StyleStack.Num() > 0)
		{
			return FExtendedAtlassianMarkup::Styled(StyleStack.Last(), Escaped);
		}

		return Escaped;
	}

	/** Maps an inline tag to a text style name, or empty when the tag carries no styling. */
	FString StyleNameForTag(const FString& Tag)
	{
		if (Tag == TEXT("b") || Tag == TEXT("strong")) return TEXT("Bold");
		if (Tag == TEXT("i") || Tag == TEXT("em"))     return TEXT("Italic");
		if (Tag == TEXT("code") || Tag == TEXT("tt"))  return TEXT("Code");
		if (Tag == TEXT("s") || Tag == TEXT("del") || Tag == TEXT("strike")) return TEXT("Strike");
		return FString();
	}
}

TArray<FExtendedAtlassianDocBlock> FExtendedAtlassianHtml::ToBlocks(const FString& Html)
{
	using namespace ExtendedAtlassianHtmlPrivate;
	using namespace ExtendedAtlassianHtmlBlockPrivate;

	TArray<FExtendedAtlassianDocBlock> Blocks;
	if (Html.IsEmpty())
	{
		return Blocks;
	}

	FString Inline;
	FString PlainRun;

	EExtendedAtlassianBlockKind PendingKind = EExtendedAtlassianBlockKind::Paragraph;
	int32 PendingLevel = 0;
	int32 PendingIndent = 0;
	int32 PendingOrderedIndex = 0;

	TArray<FString> StyleStack;
	TArray<FString> LinkStack;

	/** Ordered/unordered nesting, so a list item knows its depth and marker. */
	TArray<bool> ListIsOrdered;
	TArray<int32> ListCounters;

	TArray<FString> RowCells;
	bool bInTableRow = false;
	bool bRowIsHeader = false;

	bool bInPre = false;
	FString PreBuffer;

	int32 SkipDepth = 0;

	auto FlushPlainRun = [&Inline, &PlainRun, &StyleStack, &LinkStack]()
	{
		if (!PlainRun.IsEmpty())
		{
			Inline += StyleInline(PlainRun, StyleStack, LinkStack);
			PlainRun.Reset();
		}
	};

	auto FlushBlock = [&](EExtendedAtlassianBlockKind NextKind)
	{
		FlushPlainRun();

		if (!Inline.IsEmpty())
		{
			FExtendedAtlassianDocBlock Block;
			Block.Kind = PendingKind;
			Block.Markup = Inline;
			Block.Level = PendingLevel;
			Block.IndentDepth = PendingIndent;
			Block.OrderedIndex = PendingOrderedIndex;
			Blocks.Add(Block);
		}

		Inline.Reset();
		PendingKind = NextKind;
		PendingLevel = 0;
		PendingIndent = 0;
		PendingOrderedIndex = 0;
	};

	const int32 Length = Html.Len();
	int32 Index = 0;

	while (Index < Length)
	{
		if (Html[Index] != TEXT('<'))
		{
			if (SkipDepth == 0)
			{
				if (bInPre)
				{
					PreBuffer.AppendChar(Html[Index]);
				}
				else
				{
					PlainRun.AppendChar(Html[Index]);
				}
			}
			++Index;
			continue;
		}

		if (Html.Mid(Index, 4) == TEXT("<!--"))
		{
			const int32 CommentEnd = Html.Find(TEXT("-->"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);
			Index = CommentEnd == INDEX_NONE ? Length : CommentEnd + 3;
			continue;
		}

		int32 TagEnd = Index + 1;
		while (TagEnd < Length && Html[TagEnd] != TEXT('>'))
		{
			++TagEnd;
		}
		if (TagEnd >= Length)
		{
			break;
		}

		const FString TagBody = Html.Mid(Index + 1, TagEnd - Index - 1);
		Index = TagEnd + 1;

		const bool bClosing = TagBody.StartsWith(TEXT("/"));
		const FString Name = ParseTagName(bClosing ? TagBody.RightChop(1) : TagBody);

		if (Name == TEXT("script") || Name == TEXT("style"))
		{
			SkipDepth = bClosing ? FMath::Max(0, SkipDepth - 1) : SkipDepth + 1;
			continue;
		}
		if (SkipDepth > 0)
		{
			continue;
		}

		// --- Preformatted ---------------------------------------------
		if (Name == TEXT("pre"))
		{
			if (!bClosing)
			{
				FlushBlock(EExtendedAtlassianBlockKind::Paragraph);
				bInPre = true;
				PreBuffer.Reset();
			}
			else
			{
				FExtendedAtlassianDocBlock Block;
				Block.Kind = EExtendedAtlassianBlockKind::CodeBlock;
				Block.RawText = DecodeEntities(PreBuffer).TrimStartAndEnd();
				Blocks.Add(Block);

				bInPre = false;
				PreBuffer.Reset();
			}
			continue;
		}
		if (bInPre)
		{
			// Tags inside <pre> (usually <span> syntax highlighting) contribute no text.
			continue;
		}

		// --- Inline styling -------------------------------------------
		const FString StyleName = StyleNameForTag(Name);
		if (!StyleName.IsEmpty())
		{
			FlushPlainRun();
			if (bClosing)
			{
				if (StyleStack.Num() > 0)
				{
					StyleStack.Pop();
				}
			}
			else
			{
				StyleStack.Add(StyleName);
			}
			continue;
		}

		if (Name == TEXT("a"))
		{
			FlushPlainRun();
			if (bClosing)
			{
				if (LinkStack.Num() > 0)
				{
					LinkStack.Pop();
				}
			}
			else
			{
				LinkStack.Add(GetAttribute(TagBody, TEXT("href")));
			}
			continue;
		}

		if (Name == TEXT("br"))
		{
			FlushPlainRun();
			Inline += TEXT("\n");
			continue;
		}

		// --- Blocks ----------------------------------------------------
		if (Name.Len() == 2 && Name[0] == TEXT('h') && Name[1] >= TEXT('1') && Name[1] <= TEXT('6'))
		{
			FlushBlock(bClosing ? EExtendedAtlassianBlockKind::Paragraph : EExtendedAtlassianBlockKind::Heading);
			if (!bClosing)
			{
				PendingLevel = Name[1] - TEXT('0');
			}
			continue;
		}

		if (Name == TEXT("ul") || Name == TEXT("ol"))
		{
			FlushBlock(EExtendedAtlassianBlockKind::Paragraph);
			if (bClosing)
			{
				if (ListIsOrdered.Num() > 0)
				{
					ListIsOrdered.Pop();
					ListCounters.Pop();
				}
			}
			else
			{
				ListIsOrdered.Add(Name == TEXT("ol"));
				ListCounters.Add(0);
			}
			continue;
		}

		if (Name == TEXT("li"))
		{
			if (bClosing)
			{
				FlushBlock(EExtendedAtlassianBlockKind::Paragraph);
				continue;
			}

			FlushBlock(EExtendedAtlassianBlockKind::BulletItem);

			const bool bOrdered = ListIsOrdered.Num() > 0 && ListIsOrdered.Last();
			PendingKind = bOrdered ? EExtendedAtlassianBlockKind::OrderedItem : EExtendedAtlassianBlockKind::BulletItem;
			PendingIndent = FMath::Max(0, ListIsOrdered.Num() - 1);

			if (bOrdered && ListCounters.Num() > 0)
			{
				PendingOrderedIndex = ++ListCounters.Last();
			}
			continue;
		}

		if (Name == TEXT("blockquote"))
		{
			FlushBlock(bClosing ? EExtendedAtlassianBlockKind::Paragraph : EExtendedAtlassianBlockKind::Quote);
			continue;
		}

		if (Name == TEXT("hr"))
		{
			FlushBlock(EExtendedAtlassianBlockKind::Paragraph);

			FExtendedAtlassianDocBlock Block;
			Block.Kind = EExtendedAtlassianBlockKind::Rule;
			Blocks.Add(Block);
			continue;
		}

		if (Name == TEXT("img"))
		{
			FlushBlock(EExtendedAtlassianBlockKind::Paragraph);

			FExtendedAtlassianDocBlock Block;
			Block.Kind = EExtendedAtlassianBlockKind::Image;
			Block.ImageAlt = GetAttribute(TagBody, TEXT("alt"));
			Block.ImageUrl = GetAttribute(TagBody, TEXT("src"));
			Blocks.Add(Block);
			continue;
		}

		// --- Tables ----------------------------------------------------
		if (Name == TEXT("tr"))
		{
			if (bClosing)
			{
				FlushPlainRun();
				if (!Inline.IsEmpty())
				{
					RowCells.Add(Inline);
					Inline.Reset();
				}

				if (RowCells.Num() > 0)
				{
					FExtendedAtlassianDocBlock Block;
					Block.Kind = EExtendedAtlassianBlockKind::TableRow;
					Block.Cells = RowCells;
					Block.bIsHeaderRow = bRowIsHeader;
					Blocks.Add(Block);
				}

				RowCells.Reset();
				bInTableRow = false;
				bRowIsHeader = false;
			}
			else
			{
				FlushBlock(EExtendedAtlassianBlockKind::Paragraph);
				RowCells.Reset();
				bInTableRow = true;
				bRowIsHeader = false;
			}
			continue;
		}

		if (Name == TEXT("td") || Name == TEXT("th"))
		{
			FlushPlainRun();

			if (bClosing)
			{
				RowCells.Add(Inline);
				Inline.Reset();
			}
			else
			{
				Inline.Reset();
				if (Name == TEXT("th"))
				{
					bRowIsHeader = true;
				}
			}
			continue;
		}

		// p, div, section and anything unrecognised: close the current block so text does not run
		// together, but keep the content.
		if (IsBlockTag(Name))
		{
			FlushBlock(EExtendedAtlassianBlockKind::Paragraph);
		}
	}

	FlushPlainRun();
	if (!Inline.IsEmpty() && !bInTableRow)
	{
		FExtendedAtlassianDocBlock Block;
		Block.Kind = PendingKind;
		Block.Markup = Inline;
		Block.Level = PendingLevel;
		Block.IndentDepth = PendingIndent;
		Block.OrderedIndex = PendingOrderedIndex;
		Blocks.Add(Block);
	}

	// Collapse blocks that ended up holding only whitespace.
	Blocks.RemoveAll([](const FExtendedAtlassianDocBlock& Block)
	{
		const bool bHasContent =
			!Block.Markup.TrimStartAndEnd().IsEmpty() ||
			!Block.RawText.IsEmpty() ||
			Block.Cells.Num() > 0 ||
			Block.Kind == EExtendedAtlassianBlockKind::Rule ||
			Block.Kind == EExtendedAtlassianBlockKind::Image;

		return !bHasContent;
	});

	return Blocks;
}

FString FExtendedAtlassianHtml::DecodeEntities(const FString& In)
{
	if (In.IsEmpty() || !In.Contains(TEXT("&")))
	{
		return In;
	}

	FString Out;
	Out.Reserve(In.Len());

	int32 Index = 0;
	const int32 Length = In.Len();

	while (Index < Length)
	{
		if (In[Index] != TEXT('&'))
		{
			Out.AppendChar(In[Index++]);
			continue;
		}

		const int32 Semicolon = In.Find(TEXT(";"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);

		// Entities are short; anything longer is a stray ampersand rather than markup.
		if (Semicolon == INDEX_NONE || Semicolon - Index > 12)
		{
			Out.AppendChar(In[Index++]);
			continue;
		}

		const FString Entity = In.Mid(Index + 1, Semicolon - Index - 1);
		Index = Semicolon + 1;

		if (Entity.StartsWith(TEXT("#")))
		{
			const FString Digits = Entity.RightChop(1);
			const bool bHex = Digits.StartsWith(TEXT("x")) || Digits.StartsWith(TEXT("X"));

			const uint32 Code = bHex
				? static_cast<uint32>(FCString::Strtoi(*Digits.RightChop(1), nullptr, 16))
				: static_cast<uint32>(FCString::Atoi(*Digits));

			if (Code > 0 && Code < 0x10000)
			{
				Out.AppendChar(static_cast<TCHAR>(Code));
			}
			continue;
		}

		static const TMap<FString, FString> NamedEntities = {
			{ TEXT("lt"),     TEXT("<") },
			{ TEXT("gt"),     TEXT(">") },
			{ TEXT("quot"),   TEXT("\"") },
			{ TEXT("apos"),   TEXT("'") },
			{ TEXT("nbsp"),   TEXT(" ") },
			{ TEXT("mdash"),  TEXT("-") },
			{ TEXT("ndash"),  TEXT("-") },
			{ TEXT("hellip"), TEXT("...") },
			{ TEXT("lsquo"),  TEXT("'") },
			{ TEXT("rsquo"),  TEXT("'") },
			{ TEXT("ldquo"),  TEXT("\"") },
			{ TEXT("rdquo"),  TEXT("\"") },
			{ TEXT("bull"),   TEXT("*") },
			{ TEXT("middot"), TEXT("*") },
			{ TEXT("amp"),    TEXT("&") },
		};

		if (const FString* Replacement = NamedEntities.Find(Entity.ToLower()))
		{
			Out += *Replacement;
			continue;
		}

		// Unrecognised: keep it visible rather than silently deleting content.
		Out += TEXT("&") + Entity + TEXT(";");
	}

	return Out;
}

FString FExtendedAtlassianHtml::ToPlainText(const FString& Html)
{
	using namespace ExtendedAtlassianHtmlPrivate;

	if (Html.IsEmpty())
	{
		return FString();
	}

	FString Out;
	Out.Reserve(Html.Len());

	int32 SkipDepth = 0;
	int32 ListDepth = 0;
	bool bFirstCellInRow = true;
	TArray<FString> LinkHrefStack;

	int32 Index = 0;
	const int32 Length = Html.Len();

	while (Index < Length)
	{
		if (Html[Index] != TEXT('<'))
		{
			if (SkipDepth == 0)
			{
				Out.AppendChar(Html[Index]);
			}
			++Index;
			continue;
		}

		// Comments can contain '>', so they need their own terminator.
		if (Html.Mid(Index, 4) == TEXT("<!--"))
		{
			const int32 CommentEnd = Html.Find(TEXT("-->"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);
			Index = CommentEnd == INDEX_NONE ? Length : CommentEnd + 3;
			continue;
		}

		int32 TagEnd = Index + 1;
		while (TagEnd < Length && Html[TagEnd] != TEXT('>'))
		{
			++TagEnd;
		}

		if (TagEnd >= Length)
		{
			// Unterminated tag at end of input; drop the remainder.
			break;
		}

		const FString TagBody = Html.Mid(Index + 1, TagEnd - Index - 1);
		Index = TagEnd + 1;

		const bool bClosing = TagBody.StartsWith(TEXT("/"));
		const FString Name = ParseTagName(bClosing ? TagBody.RightChop(1) : TagBody);

		if (Name == TEXT("script") || Name == TEXT("style"))
		{
			SkipDepth = bClosing ? FMath::Max(0, SkipDepth - 1) : SkipDepth + 1;
			continue;
		}

		if (SkipDepth > 0)
		{
			continue;
		}

		if (bClosing)
		{
			if (Name == TEXT("a"))
			{
				if (LinkHrefStack.Num() > 0)
				{
					const FString Href = LinkHrefStack.Pop();
					// Only surface absolute links; relative Confluence hrefs are noise in plain text.
					if (Href.StartsWith(TEXT("http://")) || Href.StartsWith(TEXT("https://")))
					{
						Out += FString::Printf(TEXT(" (%s)"), *Href);
					}
				}
				continue;
			}

			if (Name == TEXT("ul") || Name == TEXT("ol"))
			{
				ListDepth = FMath::Max(0, ListDepth - 1);
				AppendNewline(Out);
				continue;
			}

			if (Name == TEXT("pre"))
			{
				AppendNewline(Out);
				Out += TEXT("```\n");
				continue;
			}

			if (IsBlockTag(Name))
			{
				AppendNewline(Out);
			}
			continue;
		}

		// Opening tags
		if (Name == TEXT("br"))
		{
			Out.AppendChar(TEXT('\n'));
			continue;
		}

		if (Name == TEXT("hr"))
		{
			AppendNewline(Out);
			Out += TEXT("---\n");
			continue;
		}

		if (Name.Len() == 2 && Name[0] == TEXT('h') && Name[1] >= TEXT('1') && Name[1] <= TEXT('6'))
		{
			AppendNewline(Out);
			Out += FString::ChrN(Name[1] - TEXT('0'), TEXT('#')) + TEXT(" ");
			continue;
		}

		if (Name == TEXT("ul") || Name == TEXT("ol"))
		{
			++ListDepth;
			AppendNewline(Out);
			continue;
		}

		if (Name == TEXT("li"))
		{
			AppendNewline(Out);
			Out += FString::ChrN(FMath::Max(0, ListDepth - 1) * 2, TEXT(' ')) + TEXT("- ");
			continue;
		}

		if (Name == TEXT("tr"))
		{
			AppendNewline(Out);
			bFirstCellInRow = true;
			continue;
		}

		if (Name == TEXT("td") || Name == TEXT("th"))
		{
			if (!bFirstCellInRow)
			{
				Out += TEXT(" | ");
			}
			bFirstCellInRow = false;
			continue;
		}

		if (Name == TEXT("pre"))
		{
			AppendNewline(Out);
			Out += TEXT("```\n");
			continue;
		}

		if (Name == TEXT("a"))
		{
			LinkHrefStack.Add(GetAttribute(TagBody, TEXT("href")));
			continue;
		}

		if (Name == TEXT("img"))
		{
			const FString Alt = GetAttribute(TagBody, TEXT("alt"));
			Out += Alt.IsEmpty() ? TEXT("[image]") : FString::Printf(TEXT("[image: %s]"), *Alt);
			continue;
		}

		if (IsBlockTag(Name))
		{
			AppendNewline(Out);
		}
	}

	return Tidy(DecodeEntities(Out));
}
