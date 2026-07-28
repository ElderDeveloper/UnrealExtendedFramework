// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianStorage.h"

#include "ExtendedAtlassianDocBlock.h"
#include "ExtendedAtlassianHtml.h"
#include "ExtendedAtlassianMarkdown.h"

namespace ExtendedAtlassianStoragePrivate
{
	/** XML-escapes text destined for storage format. */
	FString EscapeXml(const FString& In)
	{
		FString Out = In;
		Out.ReplaceInline(TEXT("&"), TEXT("&amp;"));
		Out.ReplaceInline(TEXT("<"), TEXT("&lt;"));
		Out.ReplaceInline(TEXT(">"), TEXT("&gt;"));
		return Out;
	}

	/** Converts inline Markdown to storage-format inline XHTML. */
	FString InlineToStorage(const FString& Inline)
	{
		// Reuse the Markdown inline parser, then translate its Slate markup into XHTML. Keeping one
		// inline parser means bold, links and code cannot drift between the two output formats.
		const FString Markup = FExtendedAtlassianMarkdown::InlineToMarkup(Inline);

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
				Out += FString::Printf(TEXT("<strong>%s</strong>"), *Inner);
			}
			else if (Tag == TEXT("Italic"))
			{
				Out += FString::Printf(TEXT("<em>%s</em>"), *Inner);
			}
			else if (Tag == TEXT("Strike"))
			{
				Out += FString::Printf(TEXT("<s>%s</s>"), *Inner);
			}
			else if (Tag == TEXT("Code"))
			{
				Out += FString::Printf(TEXT("<code>%s</code>"), *Inner);
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

				Out += Href.IsEmpty()
					? Inner
					: FString::Printf(TEXT("<a href=\"%s\">%s</a>"), *Href, *Inner);
			}
			else
			{
				Out += Inner;
			}

			Index = CloseTag == INDEX_NONE ? Length : CloseTag + 3;
		}

		return Out;
	}
}

bool FExtendedAtlassianStorage::CanRoundTrip(const FString& Storage, TArray<FString>& OutReasons)
{
	OutReasons.Reset();

	if (Storage.IsEmpty())
	{
		return true;
	}

	const FString Lowered = Storage.ToLower();

	// Macros, layouts and resource references have no representation in the block model, so
	// rebuilding the page from Markdown would silently delete them.
	if (Lowered.Contains(TEXT("<ac:structured-macro")))
	{
		OutReasons.Add(TEXT("Confluence macros"));
	}
	if (Lowered.Contains(TEXT("<ac:layout")))
	{
		OutReasons.Add(TEXT("multi-column layouts"));
	}
	if (Lowered.Contains(TEXT("<ac:task-list")))
	{
		OutReasons.Add(TEXT("Confluence task lists"));
	}
	if (Lowered.Contains(TEXT("<ri:attachment")) || Lowered.Contains(TEXT("<ac:image")))
	{
		OutReasons.Add(TEXT("embedded images or attachments"));
	}
	if (Lowered.Contains(TEXT("<ac:emoticon")))
	{
		OutReasons.Add(TEXT("emoticons"));
	}

	// Anything else in the Confluence namespaces we have not enumerated.
	if (OutReasons.Num() == 0 && (Lowered.Contains(TEXT("<ac:")) || Lowered.Contains(TEXT("<ri:"))))
	{
		OutReasons.Add(TEXT("Confluence-specific markup"));
	}

	return OutReasons.Num() == 0;
}

FString FExtendedAtlassianStorage::ToMarkdown(const FString& Storage)
{
	// Storage format is XHTML, so the existing tolerant scanner handles it; anything in the ac: or
	// ri: namespaces falls through to its text, which is why CanRoundTrip gates editing separately.
	return FExtendedAtlassianMarkdown::FromBlocks(FExtendedAtlassianHtml::ToBlocks(Storage));
}

FString FExtendedAtlassianStorage::FromMarkdown(const FString& Markdown)
{
	using namespace ExtendedAtlassianStoragePrivate;

	const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianMarkdown::ToBlocks(Markdown);

	FString Out;
	Out.Reserve(Markdown.Len() * 2);

	// Storage format has no list container per item, so runs of items must be wrapped once.
	EExtendedAtlassianBlockKind OpenListKind = EExtendedAtlassianBlockKind::Paragraph;
	bool bTableOpen = false;

	auto CloseList = [&Out, &OpenListKind]()
	{
		if (OpenListKind == EExtendedAtlassianBlockKind::BulletItem || OpenListKind == EExtendedAtlassianBlockKind::TaskItem)
		{
			Out += TEXT("</ul>");
		}
		else if (OpenListKind == EExtendedAtlassianBlockKind::OrderedItem)
		{
			Out += TEXT("</ol>");
		}
		OpenListKind = EExtendedAtlassianBlockKind::Paragraph;
	};

	auto CloseTable = [&Out, &bTableOpen]()
	{
		if (bTableOpen)
		{
			Out += TEXT("</tbody></table>");
			bTableOpen = false;
		}
	};

	for (const FExtendedAtlassianDocBlock& Block : Blocks)
	{
		const bool bIsListItem =
			Block.Kind == EExtendedAtlassianBlockKind::BulletItem ||
			Block.Kind == EExtendedAtlassianBlockKind::OrderedItem ||
			Block.Kind == EExtendedAtlassianBlockKind::TaskItem;

		if (!bIsListItem)
		{
			CloseList();
		}
		if (Block.Kind != EExtendedAtlassianBlockKind::TableRow)
		{
			CloseTable();
		}

		switch (Block.Kind)
		{
		case EExtendedAtlassianBlockKind::Heading:
		{
			const int32 Level = FMath::Clamp(Block.Level, 1, 6);
			const FString Text = InlineToStorage(FExtendedAtlassianMarkdown::MarkupToInline(Block.Markup));
			Out += FString::Printf(TEXT("<h%d>%s</h%d>"), Level, *Text, Level);
			break;
		}

		case EExtendedAtlassianBlockKind::BulletItem:
		case EExtendedAtlassianBlockKind::TaskItem:
		case EExtendedAtlassianBlockKind::OrderedItem:
		{
			const EExtendedAtlassianBlockKind WantedList =
				Block.Kind == EExtendedAtlassianBlockKind::OrderedItem
					? EExtendedAtlassianBlockKind::OrderedItem
					: EExtendedAtlassianBlockKind::BulletItem;

			if (OpenListKind != WantedList)
			{
				CloseList();
				Out += WantedList == EExtendedAtlassianBlockKind::OrderedItem ? TEXT("<ol>") : TEXT("<ul>");
				OpenListKind = WantedList;
			}

			FString Text = FExtendedAtlassianMarkdown::MarkupToInline(Block.Markup);

			// Confluence task lists are a macro we cannot rebuild, so a checkbox is written as text.
			if (Block.Kind == EExtendedAtlassianBlockKind::TaskItem)
			{
				Text = (Block.bChecked ? TEXT("[x] ") : TEXT("[ ] ")) + Text;
			}

			Out += FString::Printf(TEXT("<li>%s</li>"), *InlineToStorage(Text));
			break;
		}

		case EExtendedAtlassianBlockKind::CodeBlock:
			// A plain <pre> avoids the code-block macro, which would not survive a further round trip.
			Out += FString::Printf(TEXT("<pre>%s</pre>"), *EscapeXml(Block.RawText));
			break;

		case EExtendedAtlassianBlockKind::Quote:
			Out += FString::Printf(TEXT("<blockquote><p>%s</p></blockquote>"),
				*InlineToStorage(FExtendedAtlassianMarkdown::MarkupToInline(Block.Markup)));
			break;

		case EExtendedAtlassianBlockKind::Rule:
			Out += TEXT("<hr/>");
			break;

		case EExtendedAtlassianBlockKind::TableRow:
		{
			if (!bTableOpen)
			{
				Out += TEXT("<table><tbody>");
				bTableOpen = true;
			}

			Out += TEXT("<tr>");
			for (const FString& Cell : Block.Cells)
			{
				const FString CellHtml = InlineToStorage(FExtendedAtlassianMarkdown::MarkupToInline(Cell));
				Out += Block.bIsHeaderRow
					? FString::Printf(TEXT("<th>%s</th>"), *CellHtml)
					: FString::Printf(TEXT("<td>%s</td>"), *CellHtml);
			}
			Out += TEXT("</tr>");
			break;
		}

		case EExtendedAtlassianBlockKind::Image:
			// Without attachment upload there is nothing to point at, so keep it as readable text.
			Out += FString::Printf(TEXT("<p>[image: %s]</p>"), *EscapeXml(Block.ImageAlt));
			break;

		default:
			Out += FString::Printf(TEXT("<p>%s</p>"),
				*InlineToStorage(FExtendedAtlassianMarkdown::MarkupToInline(Block.Markup)));
			break;
		}
	}

	CloseList();
	CloseTable();

	return Out;
}
