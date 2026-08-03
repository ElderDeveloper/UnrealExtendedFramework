// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianAdf.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace ExtendedAtlassianAdfPrivate
{
	/** Guards against a malformed or hostile document sending the walk into deep recursion. */
	constexpr int32 MaxDepth = 32;

	void AppendNode(const TSharedPtr<FJsonObject>& Node, int32 Depth, int32 ListDepth, FString& Out);

	void AppendChildren(const TSharedPtr<FJsonObject>& Node, int32 Depth, int32 ListDepth, FString& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
		if (!Node->TryGetArrayField(TEXT("content"), Content))
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Content)
		{
			const TSharedPtr<FJsonObject>* Child = nullptr;
			if (Value->TryGetObject(Child) && Child->IsValid())
			{
				AppendNode(*Child, Depth + 1, ListDepth, Out);
			}
		}
	}

	/** Returns the href of a link mark on a text node, if any. */
	FString FindLinkHref(const TSharedPtr<FJsonObject>& TextNode)
	{
		const TArray<TSharedPtr<FJsonValue>>* Marks = nullptr;
		if (!TextNode->TryGetArrayField(TEXT("marks"), Marks))
		{
			return FString();
		}

		for (const TSharedPtr<FJsonValue>& Value : *Marks)
		{
			const TSharedPtr<FJsonObject>* Mark = nullptr;
			if (!Value->TryGetObject(Mark) || !Mark->IsValid())
			{
				continue;
			}

			FString MarkType;
			if (!(*Mark)->TryGetStringField(TEXT("type"), MarkType) || MarkType != TEXT("link"))
			{
				continue;
			}

			const TSharedPtr<FJsonObject>* Attrs = nullptr;
			if ((*Mark)->TryGetObjectField(TEXT("attrs"), Attrs) && Attrs->IsValid())
			{
				FString Href;
				(*Attrs)->TryGetStringField(TEXT("href"), Href);
				return Href;
			}
		}

		return FString();
	}

	FString GetAttrString(const TSharedPtr<FJsonObject>& Node, const TCHAR* AttrName)
	{
		const TSharedPtr<FJsonObject>* Attrs = nullptr;
		if (Node->TryGetObjectField(TEXT("attrs"), Attrs) && Attrs->IsValid())
		{
			FString Value;
			(*Attrs)->TryGetStringField(AttrName, Value);
			return Value;
		}
		return FString();
	}

	int32 GetAttrInt(
		const TSharedPtr<FJsonObject>& Node,
		const TCHAR* AttrName,
		int32 DefaultValue)
	{
		const TSharedPtr<FJsonObject>* Attrs = nullptr;
		int32 Value = DefaultValue;
		if (Node->TryGetObjectField(TEXT("attrs"), Attrs) && Attrs->IsValid())
		{
			(*Attrs)->TryGetNumberField(AttrName, Value);
		}
		return Value;
	}

	void AppendInlineNode(
		const TSharedPtr<FJsonObject>& Node,
		int32 Depth,
		FString& OutMarkup);

	void AppendInlineChildren(
		const TSharedPtr<FJsonObject>& Node,
		int32 Depth,
		FString& OutMarkup)
	{
		if (!Node.IsValid() || Depth > MaxDepth)
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
		if (!Node->TryGetArrayField(TEXT("content"), Content))
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Content)
		{
			const TSharedPtr<FJsonObject>* Child = nullptr;
			if (Value->TryGetObject(Child) && Child->IsValid())
			{
				AppendInlineNode(*Child, Depth + 1, OutMarkup);
			}
		}
	}

	/** Converts ADF inline nodes into the escaped markup consumed by the shared document viewer. */
	void AppendInlineNode(
		const TSharedPtr<FJsonObject>& Node,
		int32 Depth,
		FString& OutMarkup)
	{
		if (!Node.IsValid() || Depth > MaxDepth)
		{
			return;
		}

		FString Type;
		Node->TryGetStringField(TEXT("type"), Type);
		if (Type == TEXT("text"))
		{
			FString Text;
			Node->TryGetStringField(TEXT("text"), Text);
			FString Escaped = FExtendedAtlassianMarkup::Escape(Text);

			FString LinkHref;
			FString StyleName;
			const TArray<TSharedPtr<FJsonValue>>* Marks = nullptr;
			if (Node->TryGetArrayField(TEXT("marks"), Marks))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Marks)
				{
					const TSharedPtr<FJsonObject>* Mark = nullptr;
					if (!Value->TryGetObject(Mark) || !Mark->IsValid())
					{
						continue;
					}

					FString MarkType;
					(*Mark)->TryGetStringField(TEXT("type"), MarkType);
					if (MarkType == TEXT("link"))
					{
						LinkHref = GetAttrString(*Mark, TEXT("href"));
					}
					else if (MarkType == TEXT("strong"))
					{
						StyleName = TEXT("Bold");
					}
					else if (MarkType == TEXT("em"))
					{
						StyleName = TEXT("Italic");
					}
					else if (MarkType == TEXT("code"))
					{
						StyleName = TEXT("Code");
					}
					else if (MarkType == TEXT("strike"))
					{
						StyleName = TEXT("Strike");
					}
				}
			}

			// Slate's rich-text parser does not reliably preserve nested decorators. Match the
			// Confluence adapter: links win over cosmetic marks, otherwise keep the last style.
			if (!LinkHref.IsEmpty())
			{
				Escaped = FExtendedAtlassianMarkup::Link(LinkHref, Escaped);
			}
			else if (!StyleName.IsEmpty())
			{
				Escaped = FExtendedAtlassianMarkup::Styled(StyleName, Escaped);
			}
			OutMarkup += Escaped;
			return;
		}

		if (Type == TEXT("hardBreak"))
		{
			OutMarkup += TEXT("\n");
			return;
		}

		if (Type == TEXT("mention"))
		{
			const FString Text = GetAttrString(Node, TEXT("text"));
			OutMarkup += FExtendedAtlassianMarkup::Escape(
				Text.IsEmpty() ? FString(TEXT("@unknown")) : Text);
			return;
		}

		if (Type == TEXT("emoji"))
		{
			FString Text = GetAttrString(Node, TEXT("text"));
			if (Text.IsEmpty())
			{
				Text = GetAttrString(Node, TEXT("shortName"));
			}
			OutMarkup += FExtendedAtlassianMarkup::Escape(Text);
			return;
		}

		if (Type == TEXT("inlineCard") || Type == TEXT("blockCard"))
		{
			const FString Url = GetAttrString(Node, TEXT("url"));
			OutMarkup += FExtendedAtlassianMarkup::Link(
				Url,
				FExtendedAtlassianMarkup::Escape(Url));
			return;
		}

		if (Type == TEXT("status"))
		{
			OutMarkup += FExtendedAtlassianMarkup::Escape(
				GetAttrString(Node, TEXT("text")));
			return;
		}

		if (Type == TEXT("media") || Type == TEXT("mediaInline"))
		{
			FString Alt = GetAttrString(Node, TEXT("alt"));
			if (Alt.IsEmpty())
			{
				Alt = TEXT("attachment");
			}
			OutMarkup += FExtendedAtlassianMarkup::Escape(
				FString::Printf(TEXT("[%s]"), *Alt));
			return;
		}

		AppendInlineChildren(Node, Depth, OutMarkup);
	}

	FString CollectContainerMarkup(
		const TSharedPtr<FJsonObject>& Node,
		int32 Depth,
		bool bSkipNestedLists = true)
	{
		FString Markup;
		if (!Node.IsValid() || Depth > MaxDepth)
		{
			return Markup;
		}

		const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
		if (!Node->TryGetArrayField(TEXT("content"), Content))
		{
			AppendInlineNode(Node, Depth, Markup);
			return Markup;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Content)
		{
			const TSharedPtr<FJsonObject>* Child = nullptr;
			if (!Value->TryGetObject(Child) || !Child->IsValid())
			{
				continue;
			}

			FString Type;
			(*Child)->TryGetStringField(TEXT("type"), Type);
			if (bSkipNestedLists
				&& (Type == TEXT("bulletList")
					|| Type == TEXT("orderedList")
					|| Type == TEXT("taskList")))
			{
				continue;
			}

			FString ChildMarkup;
			if (Type == TEXT("paragraph") || Type == TEXT("heading"))
			{
				AppendInlineChildren(*Child, Depth + 1, ChildMarkup);
			}
			else
			{
				ChildMarkup = CollectContainerMarkup(
					*Child,
					Depth + 1,
					bSkipNestedLists);
			}

			ChildMarkup.TrimStartAndEndInline();
			if (!ChildMarkup.IsEmpty())
			{
				if (!Markup.IsEmpty())
				{
					Markup += TEXT("\n");
				}
				Markup += ChildMarkup;
			}
		}
		return Markup;
	}

	void AppendBlockNode(
		const TSharedPtr<FJsonObject>& Node,
		int32 Depth,
		int32 ListDepth,
		TArray<FExtendedAtlassianDocBlock>& OutBlocks);

	void AppendBlockChildren(
		const TSharedPtr<FJsonObject>& Node,
		int32 Depth,
		int32 ListDepth,
		TArray<FExtendedAtlassianDocBlock>& OutBlocks)
	{
		const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
		if (!Node.IsValid()
			|| Depth > MaxDepth
			|| !Node->TryGetArrayField(TEXT("content"), Content))
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Content)
		{
			const TSharedPtr<FJsonObject>* Child = nullptr;
			if (Value->TryGetObject(Child) && Child->IsValid())
			{
				AppendBlockNode(*Child, Depth + 1, ListDepth, OutBlocks);
			}
		}
	}

	void AppendList(
		const TSharedPtr<FJsonObject>& Node,
		int32 Depth,
		int32 ListDepth,
		bool bOrdered,
		TArray<FExtendedAtlassianDocBlock>& OutBlocks)
	{
		const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
		if (!Node->TryGetArrayField(TEXT("content"), Content))
		{
			return;
		}

		int32 OrderedIndex = bOrdered ? GetAttrInt(Node, TEXT("order"), 1) : 0;
		for (const TSharedPtr<FJsonValue>& Value : *Content)
		{
			const TSharedPtr<FJsonObject>* Item = nullptr;
			if (!Value->TryGetObject(Item) || !Item->IsValid())
			{
				continue;
			}

			FString ItemType;
			(*Item)->TryGetStringField(TEXT("type"), ItemType);
			if (ItemType != TEXT("listItem"))
			{
				AppendBlockNode(*Item, Depth + 1, ListDepth, OutBlocks);
				continue;
			}

			FExtendedAtlassianDocBlock Block;
			Block.Kind = bOrdered
				? EExtendedAtlassianBlockKind::OrderedItem
				: EExtendedAtlassianBlockKind::BulletItem;
			Block.IndentDepth = ListDepth;
			Block.OrderedIndex = OrderedIndex++;
			Block.Markup = CollectContainerMarkup(*Item, Depth + 1);
			if (!Block.Markup.IsEmpty())
			{
				OutBlocks.Add(MoveTemp(Block));
			}

			const TArray<TSharedPtr<FJsonValue>>* ItemContent = nullptr;
			if ((*Item)->TryGetArrayField(TEXT("content"), ItemContent))
			{
				for (const TSharedPtr<FJsonValue>& ChildValue : *ItemContent)
				{
					const TSharedPtr<FJsonObject>* Child = nullptr;
					if (!ChildValue->TryGetObject(Child) || !Child->IsValid())
					{
						continue;
					}
					FString ChildType;
					(*Child)->TryGetStringField(TEXT("type"), ChildType);
					if (ChildType == TEXT("bulletList")
						|| ChildType == TEXT("orderedList")
						|| ChildType == TEXT("taskList"))
					{
						AppendBlockNode(
							*Child,
							Depth + 1,
							ListDepth + 1,
							OutBlocks);
					}
				}
			}
		}
	}

	void AppendTaskItem(
		const TSharedPtr<FJsonObject>& Node,
		int32 Depth,
		int32 ListDepth,
		TArray<FExtendedAtlassianDocBlock>& OutBlocks)
	{
		FExtendedAtlassianDocBlock Block;
		Block.Kind = EExtendedAtlassianBlockKind::TaskItem;
		Block.IndentDepth = ListDepth;
		const FString State = GetAttrString(Node, TEXT("state"));
		Block.bChecked =
			State.Equals(TEXT("DONE"), ESearchCase::IgnoreCase)
			|| State.Equals(TEXT("COMPLETE"), ESearchCase::IgnoreCase)
			|| State.Equals(TEXT("CHECKED"), ESearchCase::IgnoreCase);
		Block.Markup = CollectContainerMarkup(Node, Depth + 1);
		if (!Block.Markup.IsEmpty())
		{
			OutBlocks.Add(MoveTemp(Block));
		}

		const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
		if (Node->TryGetArrayField(TEXT("content"), Content))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Content)
			{
				const TSharedPtr<FJsonObject>* Child = nullptr;
				if (!Value->TryGetObject(Child) || !Child->IsValid())
				{
					continue;
				}
				FString ChildType;
				(*Child)->TryGetStringField(TEXT("type"), ChildType);
				if (ChildType == TEXT("taskList"))
				{
					AppendBlockNode(
						*Child,
						Depth + 1,
						ListDepth + 1,
						OutBlocks);
				}
			}
		}
	}

	void AppendBlockNode(
		const TSharedPtr<FJsonObject>& Node,
		int32 Depth,
		int32 ListDepth,
		TArray<FExtendedAtlassianDocBlock>& OutBlocks)
	{
		if (!Node.IsValid() || Depth > MaxDepth)
		{
			return;
		}

		FString Type;
		Node->TryGetStringField(TEXT("type"), Type);
		if (Type == TEXT("doc"))
		{
			AppendBlockChildren(Node, Depth, ListDepth, OutBlocks);
			return;
		}

		if (Type == TEXT("paragraph") || Type == TEXT("heading"))
		{
			FExtendedAtlassianDocBlock Block;
			Block.Kind = Type == TEXT("heading")
				? EExtendedAtlassianBlockKind::Heading
				: EExtendedAtlassianBlockKind::Paragraph;
			Block.Level = Type == TEXT("heading")
				? FMath::Clamp(GetAttrInt(Node, TEXT("level"), 1), 1, 6)
				: 0;
			AppendInlineChildren(Node, Depth, Block.Markup);
			if (!Block.Markup.IsEmpty())
			{
				OutBlocks.Add(MoveTemp(Block));
			}
			return;
		}

		if (Type == TEXT("bulletList") || Type == TEXT("orderedList"))
		{
			AppendList(
				Node,
				Depth,
				ListDepth,
				Type == TEXT("orderedList"),
				OutBlocks);
			return;
		}

		if (Type == TEXT("taskList"))
		{
			const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
			if (Node->TryGetArrayField(TEXT("content"), Content))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Content)
				{
					const TSharedPtr<FJsonObject>* Item = nullptr;
					if (Value->TryGetObject(Item) && Item->IsValid())
					{
						AppendTaskItem(*Item, Depth + 1, ListDepth, OutBlocks);
					}
				}
			}
			return;
		}

		if (Type == TEXT("taskItem") || Type == TEXT("blockTaskItem"))
		{
			AppendTaskItem(Node, Depth, ListDepth, OutBlocks);
			return;
		}

		if (Type == TEXT("codeBlock"))
		{
			FExtendedAtlassianDocBlock Block;
			Block.Kind = EExtendedAtlassianBlockKind::CodeBlock;
			Block.CodeLanguage = GetAttrString(Node, TEXT("language"));
			AppendChildren(Node, Depth, ListDepth, Block.RawText);
			OutBlocks.Add(MoveTemp(Block));
			return;
		}

		if (Type == TEXT("blockquote") || Type == TEXT("panel"))
		{
			FExtendedAtlassianDocBlock Block;
			Block.Kind = EExtendedAtlassianBlockKind::Quote;
			Block.Markup = CollectContainerMarkup(Node, Depth + 1, false);
			if (!Block.Markup.IsEmpty())
			{
				OutBlocks.Add(MoveTemp(Block));
			}
			return;
		}

		if (Type == TEXT("table"))
		{
			AppendBlockChildren(Node, Depth, ListDepth, OutBlocks);
			return;
		}

		if (Type == TEXT("tableRow"))
		{
			FExtendedAtlassianDocBlock Row;
			Row.Kind = EExtendedAtlassianBlockKind::TableRow;
			const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
			if (Node->TryGetArrayField(TEXT("content"), Cells))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Cells)
				{
					const TSharedPtr<FJsonObject>* Cell = nullptr;
					if (!Value->TryGetObject(Cell) || !Cell->IsValid())
					{
						continue;
					}
					FString CellType;
					(*Cell)->TryGetStringField(TEXT("type"), CellType);
					Row.bIsHeaderRow |= CellType == TEXT("tableHeader");
					Row.Cells.Add(CollectContainerMarkup(*Cell, Depth + 1, false));
				}
			}
			if (!Row.Cells.IsEmpty())
			{
				OutBlocks.Add(MoveTemp(Row));
			}
			return;
		}

		if (Type == TEXT("rule"))
		{
			FExtendedAtlassianDocBlock Rule;
			Rule.Kind = EExtendedAtlassianBlockKind::Rule;
			OutBlocks.Add(MoveTemp(Rule));
			return;
		}

		if (Type == TEXT("blockCard"))
		{
			const FString Url = GetAttrString(Node, TEXT("url"));
			if (!Url.IsEmpty())
			{
				FExtendedAtlassianDocBlock Block;
				Block.Kind = EExtendedAtlassianBlockKind::Paragraph;
				Block.Markup = FExtendedAtlassianMarkup::Link(
					Url,
					FExtendedAtlassianMarkup::Escape(Url));
				OutBlocks.Add(MoveTemp(Block));
			}
			return;
		}

		if (Type == TEXT("media"))
		{
			FExtendedAtlassianDocBlock Block;
			Block.Kind = EExtendedAtlassianBlockKind::Image;
			Block.ImageAlt = GetAttrString(Node, TEXT("alt"));
			if (Block.ImageAlt.IsEmpty())
			{
				Block.ImageAlt = TEXT("Attachment");
			}
			Block.ImageUrl = GetAttrString(Node, TEXT("url"));
			Block.ImageMeta = GetAttrString(Node, TEXT("type"));
			OutBlocks.Add(MoveTemp(Block));
			return;
		}

		// expand, mediaSingle/mediaGroup and unknown containers remain readable by forwarding
		// their children. If the node only contains inline leaves, preserve them as a paragraph.
		const int32 Before = OutBlocks.Num();
		AppendBlockChildren(Node, Depth, ListDepth, OutBlocks);
		if (OutBlocks.Num() == Before)
		{
			FExtendedAtlassianDocBlock Block;
			Block.Kind = EExtendedAtlassianBlockKind::Paragraph;
			AppendInlineChildren(Node, Depth, Block.Markup);
			if (!Block.Markup.IsEmpty())
			{
				OutBlocks.Add(MoveTemp(Block));
			}
		}
	}

	/** Appends a newline unless the buffer already ends with one. */
	void EnsureNewline(FString& Out)
	{
		if (!Out.IsEmpty() && !Out.EndsWith(TEXT("\n")))
		{
			Out += TEXT("\n");
		}
	}

	void AppendNode(const TSharedPtr<FJsonObject>& Node, int32 Depth, int32 ListDepth, FString& Out)
	{
		if (!Node.IsValid() || Depth > MaxDepth)
		{
			return;
		}

		FString Type;
		Node->TryGetStringField(TEXT("type"), Type);

		// --- Inline leaves -------------------------------------------------
		if (Type == TEXT("text"))
		{
			FString Text;
			Node->TryGetStringField(TEXT("text"), Text);

			const FString Href = FindLinkHref(Node);
			Out += Href.IsEmpty() || Href == Text
				? Text
				: FString::Printf(TEXT("%s (%s)"), *Text, *Href);
			return;
		}

		if (Type == TEXT("hardBreak"))
		{
			Out += TEXT("\n");
			return;
		}

		if (Type == TEXT("mention"))
		{
			const FString Text = GetAttrString(Node, TEXT("text"));
			Out += Text.IsEmpty() ? TEXT("@unknown") : Text;
			return;
		}

		if (Type == TEXT("emoji"))
		{
			FString Text = GetAttrString(Node, TEXT("text"));
			if (Text.IsEmpty())
			{
				Text = GetAttrString(Node, TEXT("shortName"));
			}
			Out += Text;
			return;
		}

		if (Type == TEXT("inlineCard") || Type == TEXT("blockCard"))
		{
			Out += GetAttrString(Node, TEXT("url"));
			return;
		}

		if (Type == TEXT("media"))
		{
			const FString Alt = GetAttrString(Node, TEXT("alt"));
			Out += Alt.IsEmpty() ? TEXT("[attachment]") : FString::Printf(TEXT("[attachment: %s]"), *Alt);
			return;
		}

		if (Type == TEXT("rule"))
		{
			EnsureNewline(Out);
			Out += TEXT("---\n");
			return;
		}

		// --- Blocks --------------------------------------------------------
		if (Type == TEXT("heading"))
		{
			EnsureNewline(Out);

			int32 Level = 1;
			const TSharedPtr<FJsonObject>* Attrs = nullptr;
			if (Node->TryGetObjectField(TEXT("attrs"), Attrs) && Attrs->IsValid())
			{
				(*Attrs)->TryGetNumberField(TEXT("level"), Level);
			}

			Out += FString::ChrN(FMath::Clamp(Level, 1, 6), TEXT('#')) + TEXT(" ");
			AppendChildren(Node, Depth, ListDepth, Out);
			Out += TEXT("\n");
			return;
		}

		if (Type == TEXT("paragraph"))
		{
			// A blank line, not a single newline: a paragraph break has to stay distinguishable
			// from a hardBreak, otherwise flattening is lossy and descriptions read as one block.
			AppendChildren(Node, Depth, ListDepth, Out);
			Out += TEXT("\n\n");
			return;
		}

		if (Type == TEXT("codeBlock"))
		{
			EnsureNewline(Out);
			Out += TEXT("```\n");
			AppendChildren(Node, Depth, ListDepth, Out);
			EnsureNewline(Out);
			Out += TEXT("```\n");
			return;
		}

		if (Type == TEXT("blockquote"))
		{
			FString Inner;
			AppendChildren(Node, Depth, ListDepth, Inner);

			// Drop the trailing paragraph break so the quote does not end in empty "> " lines.
			Inner.TrimEndInline();

			TArray<FString> Lines;
			Inner.ParseIntoArrayLines(Lines, false);
			for (const FString& Line : Lines)
			{
				Out += TEXT("> ") + Line + TEXT("\n");
			}
			return;
		}

		if (Type == TEXT("bulletList") || Type == TEXT("orderedList"))
		{
			AppendChildren(Node, Depth, ListDepth + 1, Out);
			return;
		}

		if (Type == TEXT("taskList"))
		{
			AppendChildren(Node, Depth, ListDepth + 1, Out);
			return;
		}

		if (Type == TEXT("taskItem") || Type == TEXT("blockTaskItem"))
		{
			FString Inner;
			AppendChildren(Node, Depth, ListDepth, Inner);
			Inner.TrimStartAndEndInline();
			const FString State = GetAttrString(Node, TEXT("state"));
			const bool bChecked =
				State.Equals(TEXT("DONE"), ESearchCase::IgnoreCase)
				|| State.Equals(TEXT("COMPLETE"), ESearchCase::IgnoreCase)
				|| State.Equals(TEXT("CHECKED"), ESearchCase::IgnoreCase);
			Out += FString::ChrN(FMath::Max(0, ListDepth - 1) * 2, TEXT(' '));
			Out += bChecked ? TEXT("- [x] ") : TEXT("- [ ] ");
			Out += Inner + TEXT("\n");
			return;
		}

		if (Type == TEXT("listItem"))
		{
			FString Inner;
			AppendChildren(Node, Depth, ListDepth, Inner);
			Inner.TrimEndInline();

			const FString Indent = FString::ChrN(FMath::Max(0, ListDepth - 1) * 2, TEXT(' '));

			// A nested list inside the item already carries its own bullets; only prefix the first line.
			TArray<FString> Lines;
			Inner.ParseIntoArrayLines(Lines, false);
			for (int32 Index = 0; Index < Lines.Num(); ++Index)
			{
				Out += Index == 0
					? Indent + TEXT("- ") + Lines[Index] + TEXT("\n")
					: Indent + TEXT("  ") + Lines[Index] + TEXT("\n");
			}
			return;
		}

		if (Type == TEXT("tableRow"))
		{
			TArray<FString> Cells;

			const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
			if (Node->TryGetArrayField(TEXT("content"), Content))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Content)
				{
					const TSharedPtr<FJsonObject>* Cell = nullptr;
					if (Value->TryGetObject(Cell) && Cell->IsValid())
					{
						FString CellText;
						AppendChildren(*Cell, Depth + 1, ListDepth, CellText);
						CellText.TrimStartAndEndInline();
						CellText.ReplaceInline(TEXT("\n"), TEXT(" "));
						Cells.Add(CellText);
					}
				}
			}

			Out += FString::Join(Cells, TEXT(" | ")) + TEXT("\n");
			return;
		}

		// doc, table, panel, expand and anything unrecognised: fall through to the children so
		// unsupported constructs degrade to their text rather than disappearing.
		AppendChildren(Node, Depth, ListDepth, Out);
	}

	/** Builds a paragraph node, turning single newlines into hardBreak nodes. */
	TSharedPtr<FJsonValue> MakeParagraph(const FString& Text)
	{
		TSharedPtr<FJsonObject> Paragraph = MakeShared<FJsonObject>();
		Paragraph->SetStringField(TEXT("type"), TEXT("paragraph"));

		if (Text.IsEmpty())
		{
			// ADF rejects a text node with an empty string, so an empty paragraph carries no content.
			return MakeShared<FJsonValueObject>(Paragraph);
		}

		TArray<FString> Lines;
		Text.ParseIntoArray(Lines, TEXT("\n"), false);

		TArray<TSharedPtr<FJsonValue>> Content;
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (Index > 0)
			{
				TSharedPtr<FJsonObject> Break = MakeShared<FJsonObject>();
				Break->SetStringField(TEXT("type"), TEXT("hardBreak"));
				Content.Add(MakeShared<FJsonValueObject>(Break));
			}

			if (Lines[Index].IsEmpty())
			{
				continue;
			}

			TSharedPtr<FJsonObject> TextNode = MakeShared<FJsonObject>();
			TextNode->SetStringField(TEXT("type"), TEXT("text"));
			TextNode->SetStringField(TEXT("text"), Lines[Index]);
			Content.Add(MakeShared<FJsonValueObject>(TextNode));
		}

		if (Content.Num() > 0)
		{
			Paragraph->SetArrayField(TEXT("content"), Content);
		}

		return MakeShared<FJsonValueObject>(Paragraph);
	}

	TSharedPtr<FJsonValue> MakeCodeBlock(const FString& Code)
	{
		TSharedPtr<FJsonObject> Block = MakeShared<FJsonObject>();
		Block->SetStringField(TEXT("type"), TEXT("codeBlock"));

		TSharedPtr<FJsonObject> Attrs = MakeShared<FJsonObject>();
		Attrs->SetStringField(TEXT("language"), TEXT("text"));
		Block->SetObjectField(TEXT("attrs"), Attrs);

		if (!Code.IsEmpty())
		{
			TSharedPtr<FJsonObject> TextNode = MakeShared<FJsonObject>();
			TextNode->SetStringField(TEXT("type"), TEXT("text"));
			TextNode->SetStringField(TEXT("text"), Code);

			TArray<TSharedPtr<FJsonValue>> Content;
			Content.Add(MakeShared<FJsonValueObject>(TextNode));
			Block->SetArrayField(TEXT("content"), Content);
		}

		return MakeShared<FJsonValueObject>(Block);
	}

	TSharedPtr<FJsonObject> MakeEmptyDoc()
	{
		TSharedPtr<FJsonObject> Doc = MakeShared<FJsonObject>();
		Doc->SetStringField(TEXT("type"), TEXT("doc"));
		Doc->SetNumberField(TEXT("version"), 1);
		return Doc;
	}

	void AppendParagraphs(const FString& PlainText, TArray<TSharedPtr<FJsonValue>>& OutContent)
	{
		FString Normalized = PlainText;
		Normalized.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		Normalized.ReplaceInline(TEXT("\r"), TEXT("\n"));

		TArray<FString> Paragraphs;
		Normalized.ParseIntoArray(Paragraphs, TEXT("\n\n"), false);

		for (const FString& Paragraph : Paragraphs)
		{
			OutContent.Add(MakeParagraph(Paragraph));
		}
	}
}

TArray<FExtendedAtlassianDocBlock> FExtendedAtlassianAdf::ToBlocks(
	const TSharedPtr<FJsonObject>& DocumentNode)
{
	TArray<FExtendedAtlassianDocBlock> Blocks;
	if (!DocumentNode.IsValid())
	{
		return Blocks;
	}

	ExtendedAtlassianAdfPrivate::AppendBlockNode(
		DocumentNode,
		0,
		0,
		Blocks);
	return Blocks;
}

FString FExtendedAtlassianAdf::ToPlainText(const TSharedPtr<FJsonObject>& DocumentNode)
{
	if (!DocumentNode.IsValid())
	{
		return FString();
	}

	FString Out;
	ExtendedAtlassianAdfPrivate::AppendNode(DocumentNode, 0, 0, Out);
	return Out.TrimStartAndEnd();
}

TSharedPtr<FJsonObject> FExtendedAtlassianAdf::MakeDoc(const FString& PlainText)
{
	using namespace ExtendedAtlassianAdfPrivate;

	TSharedPtr<FJsonObject> Doc = MakeEmptyDoc();

	TArray<TSharedPtr<FJsonValue>> Content;
	AppendParagraphs(PlainText, Content);

	if (Content.Num() == 0)
	{
		Content.Add(MakeParagraph(FString()));
	}

	Doc->SetArrayField(TEXT("content"), Content);
	return Doc;
}

TSharedPtr<FJsonObject> FExtendedAtlassianAdf::MakeDocWithCodeBlock(const FString& PlainText, const FString& CodeBlockContent)
{
	using namespace ExtendedAtlassianAdfPrivate;

	TSharedPtr<FJsonObject> Doc = MakeEmptyDoc();

	TArray<TSharedPtr<FJsonValue>> Content;
	AppendParagraphs(PlainText, Content);

	if (!CodeBlockContent.IsEmpty())
	{
		Content.Add(MakeCodeBlock(CodeBlockContent));
	}

	if (Content.Num() == 0)
	{
		Content.Add(MakeParagraph(FString()));
	}

	Doc->SetArrayField(TEXT("content"), Content);
	return Doc;
}

FString FExtendedAtlassianAdf::ToJsonString(const TSharedPtr<FJsonObject>& DocumentNode)
{
	if (!DocumentNode.IsValid())
	{
		return FString();
	}

	FString Output;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);

	FJsonSerializer::Serialize(DocumentNode.ToSharedRef(), Writer);
	return Output;
}
