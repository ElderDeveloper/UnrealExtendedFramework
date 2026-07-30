// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianHtml.h"

#include "Misc/Crc.h"

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

	/**
	 * HTML named entities that map to a single code point.
	 *
	 * Confluence's storage format escapes non-ASCII this way, so without the accented Latin-1 block
	 * every "ü", "ö" and "ç" in a Turkish document arrives as literal "&uuml;" text — and would then
	 * be escaped again on write, permanently corrupting the page. Names are case-sensitive.
	 */
	/**
	 * FString map keys compare case-insensitively in Unreal, which silently makes &Ouml; find the
	 * "ouml" entry and turn every capital accented letter lowercase. Entity names are
	 * case-sensitive, so the map has to be too.
	 */
	struct FCaseSensitiveEntityKeyFuncs : BaseKeyFuncs<TPair<FString, uint32>, FString>
	{
		static FORCEINLINE const FString& GetSetKey(const TPair<FString, uint32>& Element)
		{
			return Element.Key;
		}

		static FORCEINLINE bool Matches(const FString& A, const FString& B)
		{
			return A.Equals(B, ESearchCase::CaseSensitive);
		}

		static FORCEINLINE uint32 GetKeyHash(const FString& Key)
		{
			return FCrc::StrCrc32(*Key);
		}
	};

	using FEntityMap = TMap<FString, uint32, FDefaultSetAllocator, FCaseSensitiveEntityKeyFuncs>;

	const FEntityMap& GetNamedEntities()
	{
		static const FEntityMap Entities = {
			// Core markup
			{ TEXT("quot"), 34 }, { TEXT("amp"), 38 }, { TEXT("apos"), 39 },
			{ TEXT("lt"), 60 }, { TEXT("gt"), 62 },

			// Latin-1 punctuation and symbols
			{ TEXT("nbsp"), 32 }, { TEXT("iexcl"), 161 }, { TEXT("cent"), 162 }, { TEXT("pound"), 163 },
			{ TEXT("curren"), 164 }, { TEXT("yen"), 165 }, { TEXT("brvbar"), 166 }, { TEXT("sect"), 167 },
			{ TEXT("uml"), 168 }, { TEXT("copy"), 169 }, { TEXT("ordf"), 170 }, { TEXT("laquo"), 171 },
			{ TEXT("not"), 172 }, { TEXT("reg"), 174 }, { TEXT("macr"), 175 }, { TEXT("deg"), 176 },
			{ TEXT("plusmn"), 177 }, { TEXT("sup2"), 178 }, { TEXT("sup3"), 179 }, { TEXT("acute"), 180 },
			{ TEXT("micro"), 181 }, { TEXT("para"), 182 }, { TEXT("middot"), 183 }, { TEXT("cedil"), 184 },
			{ TEXT("sup1"), 185 }, { TEXT("ordm"), 186 }, { TEXT("raquo"), 187 }, { TEXT("frac14"), 188 },
			{ TEXT("frac12"), 189 }, { TEXT("frac34"), 190 }, { TEXT("iquest"), 191 },
			{ TEXT("times"), 215 }, { TEXT("divide"), 247 },

			// Latin-1 accented capitals
			{ TEXT("Agrave"), 192 }, { TEXT("Aacute"), 193 }, { TEXT("Acirc"), 194 }, { TEXT("Atilde"), 195 },
			{ TEXT("Auml"), 196 }, { TEXT("Aring"), 197 }, { TEXT("AElig"), 198 }, { TEXT("Ccedil"), 199 },
			{ TEXT("Egrave"), 200 }, { TEXT("Eacute"), 201 }, { TEXT("Ecirc"), 202 }, { TEXT("Euml"), 203 },
			{ TEXT("Igrave"), 204 }, { TEXT("Iacute"), 205 }, { TEXT("Icirc"), 206 }, { TEXT("Iuml"), 207 },
			{ TEXT("ETH"), 208 }, { TEXT("Ntilde"), 209 }, { TEXT("Ograve"), 210 }, { TEXT("Oacute"), 211 },
			{ TEXT("Ocirc"), 212 }, { TEXT("Otilde"), 213 }, { TEXT("Ouml"), 214 }, { TEXT("Oslash"), 216 },
			{ TEXT("Ugrave"), 217 }, { TEXT("Uacute"), 218 }, { TEXT("Ucirc"), 219 }, { TEXT("Uuml"), 220 },
			{ TEXT("Yacute"), 221 }, { TEXT("THORN"), 222 },

			// Latin-1 accented lowercase
			{ TEXT("szlig"), 223 }, { TEXT("agrave"), 224 }, { TEXT("aacute"), 225 }, { TEXT("acirc"), 226 },
			{ TEXT("atilde"), 227 }, { TEXT("auml"), 228 }, { TEXT("aring"), 229 }, { TEXT("aelig"), 230 },
			{ TEXT("ccedil"), 231 }, { TEXT("egrave"), 232 }, { TEXT("eacute"), 233 }, { TEXT("ecirc"), 234 },
			{ TEXT("euml"), 235 }, { TEXT("igrave"), 236 }, { TEXT("iacute"), 237 }, { TEXT("icirc"), 238 },
			{ TEXT("iuml"), 239 }, { TEXT("eth"), 240 }, { TEXT("ntilde"), 241 }, { TEXT("ograve"), 242 },
			{ TEXT("oacute"), 243 }, { TEXT("ocirc"), 244 }, { TEXT("otilde"), 245 }, { TEXT("ouml"), 246 },
			{ TEXT("oslash"), 248 }, { TEXT("ugrave"), 249 }, { TEXT("uacute"), 250 }, { TEXT("ucirc"), 251 },
			{ TEXT("uuml"), 252 }, { TEXT("yacute"), 253 }, { TEXT("thorn"), 254 }, { TEXT("yuml"), 255 },

			// Latin Extended-A that Confluence names rather than numbers
			{ TEXT("OElig"), 338 }, { TEXT("oelig"), 339 }, { TEXT("Scaron"), 352 }, { TEXT("scaron"), 353 },
			{ TEXT("Yuml"), 376 }, { TEXT("fnof"), 402 },

			// General punctuation
			{ TEXT("lsquo"), 8216 }, { TEXT("rsquo"), 8217 }, { TEXT("sbquo"), 8218 },
			{ TEXT("ldquo"), 8220 }, { TEXT("rdquo"), 8221 }, { TEXT("bdquo"), 8222 },
			{ TEXT("dagger"), 8224 }, { TEXT("Dagger"), 8225 }, { TEXT("bull"), 8226 },
			{ TEXT("prime"), 8242 }, { TEXT("Prime"), 8243 }, { TEXT("lsaquo"), 8249 }, { TEXT("rsaquo"), 8250 },

			// Dashes and ellipsis are the punctuation Confluence pages use most, and the bundled
			// IBM Plex faces carry all three. Folding them to ASCII "-" and "..." visibly retyped
			// every heading and aside in a document the viewer is meant to reproduce.
			{ TEXT("ndash"), 8211 }, { TEXT("mdash"), 8212 }, { TEXT("hellip"), 8230 },
			{ TEXT("euro"), 8364 }, { TEXT("trade"), 8482 },
			{ TEXT("larr"), 8592 }, { TEXT("uarr"), 8593 }, { TEXT("rarr"), 8594 }, { TEXT("darr"), 8595 },
		};

		return Entities;
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

	/**
	 * How deep we are inside a table cell and a list item.
	 *
	 * Confluence storage wraps cell and list-item content in <p>, so a block tag seen in here opens a
	 * paragraph *within* the container rather than a sibling block. Treating it as a sibling flushes
	 * the cell text out as a loose paragraph — leaving the row holding empty cells — and resets the
	 * pending kind, which silently turns every list item into an unbulleted paragraph.
	 */
	int32 CellDepth = 0;
	int32 ListItemDepth = 0;

	/** Ticked state of the Confluence inline task currently being read. */
	bool bTaskChecked = false;

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
		Inline.TrimEndInline();

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

	/**
	 * A paragraph boundary inside a table cell or list item.
	 *
	 * Kept inline so the enclosing cell or bullet keeps accumulating, which is what makes a
	 * multi-paragraph cell read as one cell instead of several stray blocks.
	 */
	auto AppendParagraphBreak = [&]()
	{
		FlushPlainRun();
		Inline.TrimEndInline();
		if (!Inline.IsEmpty())
		{
			Inline += TEXT("\n");
		}
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

		/**
		 * Confluence keeps machine metadata in child *elements*, not attributes, so these carry
		 * text that is never document content: a task's numeric id, its UUID, its status word, and
		 * a macro's parameter values. Dropping the tag but keeping the text printed all of it into
		 * the page — an inline task rendered as "3", the UUID, and "incomplete" on three lines
		 * ahead of its actual wording. Suppressed the same way script and style are, which has to
		 * happen before the skip guard below so the closing tag is still seen.
		 *
		 * ac:task-status is handled separately just after: its text is metadata too, but the value
		 * is needed to decide whether the task is ticked.
		 */
		if (Name == TEXT("script") || Name == TEXT("style")
			|| Name == TEXT("ac:task-id")
			|| Name == TEXT("ac:task-uuid")
			|| Name == TEXT("ac:parameter"))
		{
			SkipDepth = bClosing ? FMath::Max(0, SkipDepth - 1) : SkipDepth + 1;
			continue;
		}
		if (SkipDepth > 0)
		{
			continue;
		}

		if (Name == TEXT("ac:task-status"))
		{
			if (bClosing)
			{
				bTaskChecked = PlainRun.TrimStartAndEnd().Equals(
					TEXT("complete"),
					ESearchCase::IgnoreCase);
			}
			else
			{
				// Commit real text that preceded the element, then take the run for the status.
				FlushPlainRun();
			}
			PlainRun.Reset();
			continue;
		}

		if (Name == TEXT("ac:task-list"))
		{
			FlushBlock(EExtendedAtlassianBlockKind::Paragraph);
			continue;
		}

		if (Name == TEXT("ac:task"))
		{
			if (bClosing)
			{
				FlushPlainRun();
				Inline.TrimEndInline();

				if (!Inline.IsEmpty())
				{
					FExtendedAtlassianDocBlock Block;
					Block.Kind = EExtendedAtlassianBlockKind::TaskItem;
					Block.Markup = Inline;
					Block.bChecked = bTaskChecked;
					Blocks.Add(Block);
				}

				Inline.Reset();
				PendingKind = EExtendedAtlassianBlockKind::Paragraph;
				ListItemDepth = FMath::Max(0, ListItemDepth - 1);
			}
			else
			{
				FlushBlock(EExtendedAtlassianBlockKind::Paragraph);
				// Counts as a list item so a <p> inside ac:task-body is a paragraph within the
				// task rather than a sibling block that would tear the wording out of it.
				++ListItemDepth;
			}
			bTaskChecked = false;
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
				ListItemDepth = FMath::Max(0, ListItemDepth - 1);
				continue;
			}

			FlushBlock(EExtendedAtlassianBlockKind::BulletItem);
			++ListItemDepth;

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
				Inline.TrimStartAndEndInline();
				RowCells.Add(Inline);
				Inline.Reset();
				CellDepth = FMath::Max(0, CellDepth - 1);
			}
			else
			{
				Inline.Reset();
				++CellDepth;
				if (Name == TEXT("th"))
				{
					bRowIsHeader = true;
				}
			}
			continue;
		}

		// p, div, section and anything unrecognised: close the current block so text does not run
		// together, but keep the content. Inside a table cell or list item the same tag is a
		// paragraph within that container, so it must not end the container's block.
		if (IsBlockTag(Name))
		{
			if (CellDepth > 0 || ListItemDepth > 0)
			{
				AppendParagraphBreak();
			}
			else
			{
				FlushBlock(EExtendedAtlassianBlockKind::Paragraph);
			}
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

			// Lone surrogates and out-of-range values cannot be encoded at all, so they are dropped.
			if (Code == 0 || Code > 0x10FFFF || (Code >= 0xD800 && Code <= 0xDFFF))
			{
				continue;
			}

			if (Code < 0x10000)
			{
				Out.AppendChar(static_cast<TCHAR>(Code));
			}
			else if constexpr (sizeof(TCHAR) >= 4)
			{
				Out.AppendChar(static_cast<TCHAR>(Code));
			}
			else
			{
				// TCHAR is UTF-16 on this platform, so a code point above the BMP needs a surrogate
				// pair. Truncating it into a single TCHAR silently deleted every emoji and symbol
				// Confluence numeric-escapes, which is a content loss rather than a display issue.
				const uint32 Adjusted = Code - 0x10000;
				Out.AppendChar(static_cast<TCHAR>(0xD800 + (Adjusted >> 10)));
				Out.AppendChar(static_cast<TCHAR>(0xDC00 + (Adjusted & 0x3FF)));
			}
			continue;
		}

		// Entity names are case-sensitive: &Ouml; is O-umlaut, &ouml; is o-umlaut.
		if (const uint32* CodePoint = ExtendedAtlassianHtmlPrivate::GetNamedEntities().Find(Entity))
		{
			Out.AppendChar(static_cast<TCHAR>(*CodePoint));
			continue;
		}

		// A handful expand to more than one character, or to nothing at all.
		static const TMap<FString, FString> MultiCharEntities = {
			{ TEXT("shy"),    TEXT("") },
		};

		if (const FString* Replacement = MultiCharEntities.Find(Entity.ToLower()))
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
