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
