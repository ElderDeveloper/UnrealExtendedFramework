// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "ExtendedAtlassianAdf.h"
#include "ExtendedAtlassianCredentials.h"
#include "ExtendedAtlassianDocBlock.h"
#include "ExtendedAtlassianDocumentStore.h"
#include "ExtendedAtlassianHtml.h"
#include "ExtendedAtlassianJira.h"
#include "ExtendedAtlassianMarkdown.h"
#include "ExtendedAtlassianModelUtils.h"
#include "ExtendedAtlassianMultipart.h"
#include "ExtendedAtlassianStorage.h"
#include "ExtendedAtlassianUserCache.h"
#include "ExtendedAtlassianWorkspaceData.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/**
 * Coverage for the four pieces most likely to break silently: they are pure functions with exact
 * output requirements, and a regression in any of them produces a server-side error that reads like
 * something else entirely.
 */

namespace ExtendedAtlassianTestsPrivate
{
	FString BytesToString(const TArray<uint8>& Bytes)
	{
		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		return FString(Converted.Length(), Converted.Get());
	}

	TSharedPtr<FJsonObject> ParseJson(const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Object);
		return Object;
	}
}

// --- ADF ---------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianAdfRoundTripTest,
	"ExtendedAtlassian.Adf.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianAdfRoundTripTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace ExtendedAtlassianTestsPrivate;

	{
		const FString Source = TEXT("First paragraph.\n\nSecond paragraph.");
		const FString Json = FExtendedAtlassianAdf::ToJsonString(FExtendedAtlassianAdf::MakeDoc(Source));

		TestTrue(TEXT("Doc declares its type"), Json.Contains(TEXT("\"type\":\"doc\"")));
		TestTrue(TEXT("Doc declares version 1"), Json.Contains(TEXT("\"version\":1")));

		const FString RoundTripped = FExtendedAtlassianAdf::ToPlainText(ParseJson(Json));
		TestEqual(TEXT("Paragraphs survive the round trip"), RoundTripped, Source);
	}

	{
		// A single newline inside a paragraph must become a hardBreak, not a paragraph split.
		const FString Json = FExtendedAtlassianAdf::ToJsonString(FExtendedAtlassianAdf::MakeDoc(TEXT("Line one\nLine two")));
		TestTrue(TEXT("Single newline becomes a hard break"), Json.Contains(TEXT("hardBreak")));
	}

	{
		// ADF rejects a text node holding an empty string, so an empty paragraph must carry no content.
		const FString Json = FExtendedAtlassianAdf::ToJsonString(FExtendedAtlassianAdf::MakeDoc(FString()));
		TestFalse(TEXT("Empty input produces no empty text node"), Json.Contains(TEXT("\"text\":\"\"")));
		TestTrue(TEXT("Empty input still produces a valid doc"), Json.Contains(TEXT("\"type\":\"paragraph\"")));
	}

	{
		const FString Json = FExtendedAtlassianAdf::ToJsonString(
			FExtendedAtlassianAdf::MakeDocWithCodeBlock(TEXT("Description."), TEXT("Level: TestMap")));

		TestTrue(TEXT("Context becomes a code block"), Json.Contains(TEXT("codeBlock")));

		const FString Flattened = FExtendedAtlassianAdf::ToPlainText(ParseJson(Json));
		TestTrue(TEXT("Description survives"), Flattened.Contains(TEXT("Description.")));
		TestTrue(TEXT("Code block content survives"), Flattened.Contains(TEXT("Level: TestMap")));
	}

	{
		const FString Json = TEXT(
			"{\"version\":1,\"type\":\"doc\",\"content\":["
			"{\"type\":\"heading\",\"attrs\":{\"level\":2},\"content\":[{\"type\":\"text\",\"text\":\"Work\"}]},"
			"{\"type\":\"paragraph\",\"content\":[{\"type\":\"text\",\"text\":\"Important\",\"marks\":[{\"type\":\"strong\"}]}]},"
			"{\"type\":\"taskList\",\"attrs\":{\"localId\":\"list-1\"},\"content\":["
			"{\"type\":\"taskItem\",\"attrs\":{\"localId\":\"task-1\",\"state\":\"TODO\"},\"content\":[{\"type\":\"text\",\"text\":\"Open item\"}]},"
			"{\"type\":\"taskItem\",\"attrs\":{\"localId\":\"task-2\",\"state\":\"DONE\"},\"content\":[{\"type\":\"text\",\"text\":\"Finished item\"}]}"
			"]}]}"
		);
		const TSharedPtr<FJsonObject> Adf = ParseJson(Json);
		const TArray<FExtendedAtlassianDocBlock> Blocks =
			FExtendedAtlassianAdf::ToBlocks(Adf);

		TestEqual(TEXT("ADF preserves four structured blocks"), Blocks.Num(), 4);
		if (Blocks.Num() == 4)
		{
			TestTrue(TEXT("Heading kind survives"),
				Blocks[0].Kind == EExtendedAtlassianBlockKind::Heading
				&& Blocks[0].Level == 2);
			TestEqual(TEXT("Strong mark becomes shared rich-text markup"),
				Blocks[1].Markup, FString(TEXT("<Bold>Important</>")));
			TestTrue(TEXT("Open task remains unchecked"),
				Blocks[2].Kind == EExtendedAtlassianBlockKind::TaskItem
				&& !Blocks[2].bChecked
				&& Blocks[2].Markup.Contains(TEXT("Open item")));
			TestTrue(TEXT("Done task remains checked"),
				Blocks[3].Kind == EExtendedAtlassianBlockKind::TaskItem
				&& Blocks[3].bChecked
				&& Blocks[3].Markup.Contains(TEXT("Finished item")));
		}

		const FString Flattened = FExtendedAtlassianAdf::ToPlainText(Adf);
		TestTrue(TEXT("Plain edit fallback keeps unchecked task syntax"),
			Flattened.Contains(TEXT("- [ ] Open item")));
		TestTrue(TEXT("Plain edit fallback keeps checked task syntax"),
			Flattened.Contains(TEXT("- [x] Finished item")));

		const FString IssueJson = FString::Printf(
			TEXT("{\"id\":\"133\",\"key\":\"TOT-133\",\"fields\":{\"summary\":\"Inventory Bugfix Tasklist\",\"description\":%s}}"),
			*Json);
		const FExtendedAtlassianIssue Issue =
			FExtendedAtlassianJira::ParseIssue(ParseJson(IssueJson));
		TestEqual(TEXT("Jira issue keeps the ADF document blocks"),
			Issue.DescriptionBlocks.Num(), 4);
	}

	{
		// Reading must tolerate rubbish rather than crashing on an unexpected payload.
		TestEqual(TEXT("Null node flattens to empty"), FExtendedAtlassianAdf::ToPlainText(nullptr), FString());
		TestEqual(TEXT("Non-ADF object flattens to empty"),
			FExtendedAtlassianAdf::ToPlainText(ParseJson(TEXT("{\"unexpected\":true}"))), FString());
	}

	return true;
}

// --- Confluence HTML ---------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianHtmlTest,
	"ExtendedAtlassian.Html.ToPlainText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianHtmlTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	{
		const FString Text = FExtendedAtlassianHtml::ToPlainText(
			TEXT("<h1>Title</h1><p>Hello <strong>world</strong>.</p>"));

		TestTrue(TEXT("Heading level becomes hashes"), Text.Contains(TEXT("# Title")));
		TestTrue(TEXT("Inline markup is unwrapped"), Text.Contains(TEXT("Hello world.")));
	}

	{
		const FString Text = FExtendedAtlassianHtml::ToPlainText(
			TEXT("<ul><li>One</li><li>Two<ul><li>Nested</li></ul></li></ul>"));

		TestTrue(TEXT("List items become bullets"), Text.Contains(TEXT("- One")));
		TestTrue(TEXT("Nested items are indented"), Text.Contains(TEXT("  - Nested")));
	}

	{
		const FString Text = FExtendedAtlassianHtml::ToPlainText(
			TEXT("<table><tr><th>A</th><th>B</th></tr><tr><td>1</td><td>2</td></tr></table>"));

		TestTrue(TEXT("Header cells are separated"), Text.Contains(TEXT("A | B")));
		TestTrue(TEXT("Body cells are separated"), Text.Contains(TEXT("1 | 2")));
	}

	{
		// Script and style content must not leak into the readable text.
		const FString Text = FExtendedAtlassianHtml::ToPlainText(
			TEXT("<p>Visible</p><script>var secret = 1;</script><style>.x{color:red}</style>"));

		TestTrue(TEXT("Body text survives"), Text.Contains(TEXT("Visible")));
		TestFalse(TEXT("Script content is dropped"), Text.Contains(TEXT("secret")));
		TestFalse(TEXT("Style content is dropped"), Text.Contains(TEXT("color:red")));
	}

	{
		const FString Text = FExtendedAtlassianHtml::ToPlainText(
			TEXT("<p>a &amp; b &lt;tag&gt; &#65; &nbsp;end</p>"));

		TestTrue(TEXT("Named entities decode"), Text.Contains(TEXT("a & b")));
		TestTrue(TEXT("Escaped angle brackets decode"), Text.Contains(TEXT("<tag>")));
		TestTrue(TEXT("Numeric entities decode"), Text.Contains(TEXT("A")));
	}

	{
		// Confluence storage format escapes non-ASCII as named entities. Without the accented
		// Latin-1 block these survive as literal "&uuml;" text and are escaped again on write,
		// permanently corrupting every Turkish word in the document.
		const FString Decoded = FExtendedAtlassianHtml::DecodeEntities(
			TEXT("G&uuml;ncel &ccedil;er&ccedil;eve &Ouml;zet Ama&ccedil; d&ouml;k&uuml;m&uuml;"));

		TestEqual(TEXT("Turkish named entities decode"), Decoded,
			FString(TEXT("Güncel çerçeve Özet Amaç dökümü")));

		// Entity names are case-sensitive: &Ouml; and &ouml; are different letters.
		TestEqual(TEXT("Capital and lowercase umlauts differ"),
			FExtendedAtlassianHtml::DecodeEntities(TEXT("&Ouml;&ouml;")), FString(TEXT("Öö")));

		// Latin Extended-A has no named entities, so Confluence numbers them.
		TestEqual(TEXT("Numeric entities cover the rest of Turkish"),
			FExtendedAtlassianHtml::DecodeEntities(TEXT("&#287;&#305;&#351;&#304;")),
			FString(TEXT("ğışİ")));

		// Emoji live above the BMP. Truncating them into one TCHAR deleted the character outright,
		// so a status marker in a page body vanished instead of merely rendering as a placeholder.
		const FString Astral = FExtendedAtlassianHtml::DecodeEntities(TEXT("a&#x1F7E9;b"));
		TestEqual(TEXT("Astral code point survives as a surrogate pair"), Astral.Len(), sizeof(TCHAR) >= 4 ? 3 : 4);
		TestTrue(TEXT("Text around an astral code point is intact"),
			Astral.StartsWith(TEXT("a")) && Astral.EndsWith(TEXT("b")));

		// Confluence pages lean on dashes heavily, and the viewer reproduces the page rather than an
		// ASCII transliteration of it.
		TestEqual(TEXT("Dashes and ellipsis keep their real characters"),
			FExtendedAtlassianHtml::DecodeEntities(TEXT("a&ndash;b &mdash; c&hellip;")),
			FString(TEXT("a–b — c…")));

		// Unencodable inputs must not leak a broken character into the document.
		TestEqual(TEXT("Lone surrogate dropped"),
			FExtendedAtlassianHtml::DecodeEntities(TEXT("a&#xD800;b")), FString(TEXT("ab")));
		TestEqual(TEXT("Out-of-range code point dropped"),
			FExtendedAtlassianHtml::DecodeEntities(TEXT("a&#x110000;b")), FString(TEXT("ab")));
	}

	{
		// The full path a Turkish page takes: entity-escaped storage through to Markdown.
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianHtml::ToBlocks(
			TEXT("<h2>Ama&ccedil; / &Ouml;zet</h2><p>G&uuml;ncel</p>"));

		bool bHeadingOk = false;
		bool bBodyOk = false;

		for (const FExtendedAtlassianDocBlock& Block : Blocks)
		{
			if (Block.Kind == EExtendedAtlassianBlockKind::Heading && Block.Markup.Contains(TEXT("Amaç")))
			{
				bHeadingOk = true;
			}
			if (Block.Markup.Contains(TEXT("Güncel")))
			{
				bBodyOk = true;
			}
			TestFalse(TEXT("No raw entity survives into a block"), Block.Markup.Contains(TEXT("uml;")));
		}

		TestTrue(TEXT("Accented heading survives"), bHeadingOk);
		TestTrue(TEXT("Accented body survives"), bBodyOk);
	}

	{
		const FString Text = FExtendedAtlassianHtml::ToPlainText(
			TEXT("<p>See <a href=\"https://example.com/doc\">the doc</a>.</p>"));

		TestTrue(TEXT("Link text is kept"), Text.Contains(TEXT("the doc")));
		TestTrue(TEXT("Absolute href is surfaced"), Text.Contains(TEXT("(https://example.com/doc)")));
	}

	{
		// Relative Confluence links are noise in plain text and must not be appended.
		const FString Text = FExtendedAtlassianHtml::ToPlainText(
			TEXT("<p><a href=\"/wiki/spaces/X/pages/1\">internal</a></p>"));

		TestTrue(TEXT("Relative link text is kept"), Text.Contains(TEXT("internal")));
		TestFalse(TEXT("Relative href is not appended"), Text.Contains(TEXT("/wiki/spaces")));
	}

	{
		// Malformed markup must degrade, never crash or hang.
		TestEqual(TEXT("Empty input yields empty output"), FExtendedAtlassianHtml::ToPlainText(FString()), FString());

		const FString Unterminated = FExtendedAtlassianHtml::ToPlainText(TEXT("<p>Text<div class=\"broken"));
		TestTrue(TEXT("Text before an unterminated tag survives"), Unterminated.Contains(TEXT("Text")));

		const FString Comment = FExtendedAtlassianHtml::ToPlainText(TEXT("<p>A<!-- hidden > still hidden -->B</p>"));
		TestFalse(TEXT("Comment content is dropped"), Comment.Contains(TEXT("hidden")));
		TestTrue(TEXT("Text around a comment survives"), Comment.Contains(TEXT("A")) && Comment.Contains(TEXT("B")));
	}

	return true;
}

// --- Multipart ---------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianMultipartTest,
	"ExtendedAtlassian.Multipart.Framing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianMultipartTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace ExtendedAtlassianTestsPrivate;

	const FString Boundary = TEXT("TESTBOUNDARY");

	FExtendedAtlassianMultipartFile File;
	File.FieldName = TEXT("file");
	File.FileName = TEXT("shot.png");
	File.ContentType = TEXT("image/png");

	// Deliberately includes a byte that is invalid UTF-8, to prove the body is assembled as bytes
	// rather than round-tripped through a string.
	const uint8 Payload[] = { 0x89, 'P', 'N', 'G', 0x00, 0xFF };
	File.Data.Append(Payload, UE_ARRAY_COUNT(Payload));

	TArray<FExtendedAtlassianMultipartFile> Files;
	Files.Add(File);

	const TArray<uint8> Body = FExtendedAtlassianMultipart::BuildBody(Files, Boundary);

	// Header framing is CRLF-sensitive; a lone LF produces a 400 that reads like an auth failure.
	const FString Expected =
		TEXT("--TESTBOUNDARY\r\n")
		TEXT("Content-Disposition: form-data; name=\"file\"; filename=\"shot.png\"\r\n")
		TEXT("Content-Type: image/png\r\n")
		TEXT("\r\n");

	const FTCHARToUTF8 ExpectedUtf8(*Expected);
	TestTrue(TEXT("Body is longer than its header"), Body.Num() > ExpectedUtf8.Length());

	bool bHeaderMatches = Body.Num() >= ExpectedUtf8.Length();
	for (int32 Index = 0; bHeaderMatches && Index < ExpectedUtf8.Length(); ++Index)
	{
		bHeaderMatches = Body[Index] == static_cast<uint8>(ExpectedUtf8.Get()[Index]);
	}
	TestTrue(TEXT("Header framing is byte-exact"), bHeaderMatches);

	// Payload must appear verbatim, immediately after the blank line.
	bool bPayloadIntact = true;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Payload); ++Index)
	{
		if (Body[ExpectedUtf8.Length() + Index] != Payload[Index])
		{
			bPayloadIntact = false;
			break;
		}
	}
	TestTrue(TEXT("Binary payload is preserved byte for byte"), bPayloadIntact);

	const FString Trailer = TEXT("\r\n--TESTBOUNDARY--\r\n");
	const FTCHARToUTF8 TrailerUtf8(*Trailer);

	bool bTrailerMatches = Body.Num() >= TrailerUtf8.Length();
	for (int32 Index = 0; bTrailerMatches && Index < TrailerUtf8.Length(); ++Index)
	{
		bTrailerMatches = Body[Body.Num() - TrailerUtf8.Length() + Index] == static_cast<uint8>(TrailerUtf8.Get()[Index]);
	}
	TestTrue(TEXT("Closing boundary is byte-exact"), bTrailerMatches);

	TestEqual(TEXT("Content type header names the boundary"),
		FExtendedAtlassianMultipart::MakeContentTypeHeader(Boundary),
		FString(TEXT("multipart/form-data; boundary=TESTBOUNDARY")));

	{
		// A quote or newline in a filename would otherwise let a caller inject header lines.
		FExtendedAtlassianMultipartFile Hostile;
		Hostile.FileName = TEXT("a\"b\r\nX-Injected: 1");
		Hostile.Data.Add(0x41);

		TArray<FExtendedAtlassianMultipartFile> HostileFiles;
		HostileFiles.Add(Hostile);

		const FString Text = BytesToString(FExtendedAtlassianMultipart::BuildBody(HostileFiles, Boundary));
		TestFalse(TEXT("Filename cannot inject a header"), Text.Contains(TEXT("X-Injected: 1\r\n")));
	}

	TestNotEqual(TEXT("Boundaries are unique per call"),
		FExtendedAtlassianMultipart::MakeBoundary(),
		FExtendedAtlassianMultipart::MakeBoundary());

	return true;
}

// --- Document model ----------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianMarkupTest,
	"ExtendedAtlassian.Document.Markup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianMarkupTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// A stray '<' in page content would otherwise be read as a markup tag and swallow text.
	TestEqual(TEXT("Angle brackets are escaped"),
		FExtendedAtlassianMarkup::Escape(TEXT("a < b > c")), FString(TEXT("a &lt; b &gt; c")));

	// Ampersand must be escaped first or it corrupts the entities the others introduce.
	TestEqual(TEXT("Ampersand escapes without double-encoding"),
		FExtendedAtlassianMarkup::Escape(TEXT("Tom & Jerry")), FString(TEXT("Tom &amp; Jerry")));
	TestEqual(TEXT("Already-escaped input is not re-corrupted into &amp;lt;"),
		FExtendedAtlassianMarkup::Escape(TEXT("<")), FString(TEXT("&lt;")));

	TestEqual(TEXT("Styled wraps in a tag"),
		FExtendedAtlassianMarkup::Styled(TEXT("Bold"), TEXT("hi")), FString(TEXT("<Bold>hi</>")));

	// A quote in an href would otherwise break out of the attribute.
	const FString Link = FExtendedAtlassianMarkup::Link(TEXT("https://x.com/\"onerror=1"), TEXT("text"));
	TestFalse(TEXT("Quotes cannot escape the href attribute"), Link.Contains(TEXT("\"onerror")));

	TestTrue(TEXT("Empty content produces no tag"), FExtendedAtlassianMarkup::Styled(TEXT("Bold"), FString()).IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianMarkdownTest,
	"ExtendedAtlassian.Document.Markdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianMarkdownTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	{
		const FString Markup = FExtendedAtlassianMarkdown::InlineToMarkup(
			TEXT("**bold** and *italic* and `code` and ~~gone~~"));

		TestTrue(TEXT("Bold"), Markup.Contains(TEXT("<Bold>bold</>")));
		TestTrue(TEXT("Italic"), Markup.Contains(TEXT("<Italic>italic</>")));
		TestTrue(TEXT("Inline code"), Markup.Contains(TEXT("<Code>code</>")));
		TestTrue(TEXT("Strikethrough"), Markup.Contains(TEXT("<Strike>gone</>")));
	}

	{
		// Content inside backticks is literal and must not be re-scanned for other markers.
		const FString Markup = FExtendedAtlassianMarkdown::InlineToMarkup(TEXT("`**not bold**`"));
		TestTrue(TEXT("Code content stays literal"), Markup.Contains(TEXT("**not bold**")));
		TestFalse(TEXT("Code content is not styled"), Markup.Contains(TEXT("<Bold>")));
	}

	{
		const FString Markup = FExtendedAtlassianMarkdown::InlineToMarkup(TEXT("see [docs](https://example.com)"));
		TestTrue(TEXT("Link href"), Markup.Contains(TEXT("href=\"https://example.com\"")));
		TestTrue(TEXT("Link text"), Markup.Contains(TEXT("docs")));
	}

	{
		// Text taken from a document must never reach the markup unescaped.
		const FString Markup = FExtendedAtlassianMarkdown::InlineToMarkup(TEXT("a < b & c"));
		TestTrue(TEXT("Inline text is escaped"), Markup.Contains(TEXT("&lt;")) && Markup.Contains(TEXT("&amp;")));
	}

	{
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianMarkdown::ToBlocks(
			TEXT("# Title\n\nBody text.\n\n## Sub\n\n- one\n  - nested\n\n1. first\n\n- [x] done\n- [ ] todo\n\n---\n\n```cpp\nint x = 1;\n```\n"));

		auto CountOf = [&Blocks](EExtendedAtlassianBlockKind Kind)
		{
			int32 Count = 0;
			for (const FExtendedAtlassianDocBlock& Block : Blocks)
			{
				if (Block.Kind == Kind) { ++Count; }
			}
			return Count;
		};

		TestEqual(TEXT("Two headings"), CountOf(EExtendedAtlassianBlockKind::Heading), 2);
		TestEqual(TEXT("Two bullets"), CountOf(EExtendedAtlassianBlockKind::BulletItem), 2);
		TestEqual(TEXT("One ordered item"), CountOf(EExtendedAtlassianBlockKind::OrderedItem), 1);
		TestEqual(TEXT("Two task items"), CountOf(EExtendedAtlassianBlockKind::TaskItem), 2);
		TestEqual(TEXT("One rule"), CountOf(EExtendedAtlassianBlockKind::Rule), 1);
		TestEqual(TEXT("One code block"), CountOf(EExtendedAtlassianBlockKind::CodeBlock), 1);

		for (const FExtendedAtlassianDocBlock& Block : Blocks)
		{
			if (Block.Kind == EExtendedAtlassianBlockKind::Heading && Block.Markup.Contains(TEXT("Sub")))
			{
				TestEqual(TEXT("Heading level is read from the hash count"), Block.Level, 2);
			}
			if (Block.Kind == EExtendedAtlassianBlockKind::CodeBlock)
			{
				TestEqual(TEXT("Code fence language"), Block.CodeLanguage, FString(TEXT("cpp")));
				TestEqual(TEXT("Code body is verbatim"), Block.RawText, FString(TEXT("int x = 1;")));
			}
			if (Block.Kind == EExtendedAtlassianBlockKind::TaskItem && Block.Markup.Contains(TEXT("done")))
			{
				TestTrue(TEXT("Checked task"), Block.bChecked);
			}
		}
	}

	{
		// "#1 priority" is not a heading; requiring the space keeps ordinary text intact.
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianMarkdown::ToBlocks(TEXT("#1 priority"));
		TestEqual(TEXT("Hash without a space stays a paragraph"), Blocks.Num(), 1);
		TestTrue(TEXT("Not treated as a heading"), Blocks[0].Kind == EExtendedAtlassianBlockKind::Paragraph);
	}

	{
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianMarkdown::ToBlocks(
			TEXT("| A | B |\n|---|---|\n| 1 | 2 |\n"));

		TestEqual(TEXT("Header plus one body row"), Blocks.Num(), 2);
		TestTrue(TEXT("First row is a header"), Blocks[0].bIsHeaderRow);
		TestEqual(TEXT("Two columns"), Blocks[0].Cells.Num(), 2);
		TestFalse(TEXT("Body row is not a header"), Blocks[1].bIsHeaderRow);
	}

	TestEqual(TEXT("Empty input yields no blocks"), FExtendedAtlassianMarkdown::ToBlocks(FString()).Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianHtmlBlocksTest,
	"ExtendedAtlassian.Document.HtmlBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianHtmlBlocksTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	{
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianHtml::ToBlocks(
			TEXT("<h2>Heading</h2><p>Body <strong>bold</strong>.</p><ul><li>one</li><li>two</li></ul>"));

		bool bFoundHeading = false;
		int32 BulletCount = 0;
		bool bFoundBold = false;

		for (const FExtendedAtlassianDocBlock& Block : Blocks)
		{
			if (Block.Kind == EExtendedAtlassianBlockKind::Heading && Block.Level == 2)
			{
				bFoundHeading = true;
			}
			if (Block.Kind == EExtendedAtlassianBlockKind::BulletItem)
			{
				++BulletCount;
			}
			if (Block.Markup.Contains(TEXT("<Bold>bold</>")))
			{
				bFoundBold = true;
			}
		}

		TestTrue(TEXT("h2 becomes a level 2 heading"), bFoundHeading);
		TestEqual(TEXT("Two list items"), BulletCount, 2);
		TestTrue(TEXT("strong becomes a Bold run"), bFoundBold);
	}

	{
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianHtml::ToBlocks(
			TEXT("<pre>line one\nline two</pre>"));

		TestEqual(TEXT("One code block"), Blocks.Num(), 1);
		TestTrue(TEXT("Code kind"), Blocks[0].Kind == EExtendedAtlassianBlockKind::CodeBlock);
		TestTrue(TEXT("Newlines preserved verbatim"), Blocks[0].RawText.Contains(TEXT("\n")));
	}

	{
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianHtml::ToBlocks(
			TEXT("<table><tr><th>A</th><th>B</th></tr><tr><td>1</td><td>2</td></tr></table>"));

		TestEqual(TEXT("Two table rows"), Blocks.Num(), 2);
		TestTrue(TEXT("Header row flagged"), Blocks[0].bIsHeaderRow);
		TestEqual(TEXT("Two cells"), Blocks[0].Cells.Num(), 2);
	}

	{
		// The shape Confluence storage actually emits: every cell's content is wrapped in <p>.
		// Treating that <p> as a sibling block used to flush the text out as a loose paragraph and
		// leave the row holding empty cells, so a table rendered as a column of stray lines
		// separated by empty pipe rows.
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianHtml::ToBlocks(
			TEXT("<table data-layout=\"default\"><colgroup><col /></colgroup><tbody>")
			TEXT("<tr><th><p>Hafta</p></th><th><p>Hedef</p></th></tr>")
			TEXT("<tr><td><p>5</p></td><td><p><strong>Kapi</strong>: kat-1</p></td></tr>")
			TEXT("</tbody></table>"));

		TestEqual(TEXT("Paragraph-wrapped cells produce only rows, no loose paragraphs"), Blocks.Num(), 2);

		if (Blocks.Num() == 2)
		{
			TestTrue(TEXT("Header row flagged"), Blocks[0].bIsHeaderRow);
			TestEqual(TEXT("Header keeps both cells"), Blocks[0].Cells.Num(), 2);
			TestEqual(TEXT("Header cell text stays in the cell"), Blocks[0].Cells[0], FString(TEXT("Hafta")));
			TestEqual(TEXT("Second header cell text stays in the cell"), Blocks[0].Cells[1], FString(TEXT("Hedef")));

			TestFalse(TEXT("Body row not flagged as header"), Blocks[1].bIsHeaderRow);
			TestEqual(TEXT("Body keeps both cells"), Blocks[1].Cells.Num(), 2);
			TestEqual(TEXT("Body cell text stays in the cell"), Blocks[1].Cells[0], FString(TEXT("5")));
			TestTrue(TEXT("Inline styling survives inside a cell"),
				Blocks[1].Cells[1].Contains(TEXT("<Bold>Kapi</>")));
		}
	}

	{
		// A cell holding more than one paragraph stays one cell.
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianHtml::ToBlocks(
			TEXT("<table><tbody><tr><td><p>first</p><p>second</p></td></tr></tbody></table>"));

		TestEqual(TEXT("One row"), Blocks.Num(), 1);
		if (Blocks.Num() == 1)
		{
			TestEqual(TEXT("Still one cell"), Blocks[0].Cells.Num(), 1);
			TestEqual(TEXT("Both paragraphs kept, separated by a break"),
				Blocks[0].Cells[0], FString(TEXT("first\nsecond")));
		}
	}

	{
		// Storage wraps list-item content in <p> too, which used to reset the pending kind and turn
		// every bullet into an unmarked paragraph.
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianHtml::ToBlocks(
			TEXT("<ul><li><p><strong>Host-authoritative:</strong> yalniz host.</p></li>")
			TEXT("<li><p>EOS akisi.</p></li></ul>"));

		TestEqual(TEXT("Two blocks for two items"), Blocks.Num(), 2);

		int32 BulletCount = 0;
		for (const FExtendedAtlassianDocBlock& Block : Blocks)
		{
			if (Block.Kind == EExtendedAtlassianBlockKind::BulletItem)
			{
				++BulletCount;
			}
		}

		TestEqual(TEXT("Paragraph-wrapped items stay bullets"), BulletCount, 2);
		if (Blocks.Num() == 2)
		{
			TestTrue(TEXT("Bold survives inside a bulleted item"),
				Blocks[0].Markup.Contains(TEXT("<Bold>Host-authoritative:</>")));
			TestTrue(TEXT("Item text follows its styled run"),
				Blocks[0].Markup.Contains(TEXT("yalniz host.")));
		}
	}

	{
		// Ordered items and nesting keep their numbering and depth through the same wrapper.
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianHtml::ToBlocks(
			TEXT("<ol><li><p>one</p></li><li><p>two</p>")
			TEXT("<ul><li><p>nested</p></li></ul></li></ol>"));

		int32 OrderedCount = 0;
		int32 NestedBullets = 0;
		for (const FExtendedAtlassianDocBlock& Block : Blocks)
		{
			if (Block.Kind == EExtendedAtlassianBlockKind::OrderedItem)
			{
				++OrderedCount;
			}
			if (Block.Kind == EExtendedAtlassianBlockKind::BulletItem && Block.IndentDepth == 1)
			{
				++NestedBullets;
			}
		}

		TestEqual(TEXT("Two ordered items"), OrderedCount, 2);
		TestEqual(TEXT("Nested bullet keeps its depth"), NestedBullets, 1);
	}

	{
		// A Confluence inline task. Its id, UUID and status live in child elements, so dropping the
		// tag while keeping the text printed all three into the document ahead of the real wording.
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianHtml::ToBlocks(
			TEXT("<ac:task-list><ac:task>")
			TEXT("<ac:task-id>3</ac:task-id>")
			TEXT("<ac:task-uuid>12a64e22-1989-460a-b968-b0fb5290e5a0</ac:task-uuid>")
			TEXT("<ac:task-status>incomplete</ac:task-status>")
			TEXT("<ac:task-body>Enchantment geri doner mi</ac:task-body>")
			TEXT("</ac:task></ac:task-list>"));

		TestEqual(TEXT("One block for one task"), Blocks.Num(), 1);
		if (Blocks.Num() == 1)
		{
			TestTrue(TEXT("Task becomes a task item"),
				Blocks[0].Kind == EExtendedAtlassianBlockKind::TaskItem);
			TestFalse(TEXT("incomplete is unchecked"), Blocks[0].bChecked);
			TestTrue(TEXT("Task body survives"),
				Blocks[0].Markup.Contains(TEXT("Enchantment geri doner mi")));

			// The three that used to leak.
			TestFalse(TEXT("Task id does not reach the document"),
				Blocks[0].Markup.Contains(TEXT("3")));
			TestFalse(TEXT("Task UUID does not reach the document"),
				Blocks[0].Markup.Contains(TEXT("12a64e22")));
			TestFalse(TEXT("Task status word does not reach the document"),
				Blocks[0].Markup.Contains(TEXT("incomplete")));
		}
	}

	{
		// A completed task, with the body wrapped in <p> as Confluence often emits it.
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianHtml::ToBlocks(
			TEXT("<ac:task-list><ac:task>")
			TEXT("<ac:task-id>1</ac:task-id>")
			TEXT("<ac:task-status>complete</ac:task-status>")
			TEXT("<ac:task-body><p>Scope <strong>dolduruldu</strong></p></ac:task-body>")
			TEXT("</ac:task></ac:task-list>"));

		TestEqual(TEXT("One block for one completed task"), Blocks.Num(), 1);
		if (Blocks.Num() == 1)
		{
			TestTrue(TEXT("Completed task is a task item"),
				Blocks[0].Kind == EExtendedAtlassianBlockKind::TaskItem);
			TestTrue(TEXT("complete is checked"), Blocks[0].bChecked);
			TestTrue(TEXT("Paragraph-wrapped body stays with its task"),
				Blocks[0].Markup.Contains(TEXT("Scope")));
			TestTrue(TEXT("Inline styling inside a task body survives"),
				Blocks[0].Markup.Contains(TEXT("<Bold>dolduruldu</>")));
		}
	}

	{
		// Macro parameter values are configuration, not prose.
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianHtml::ToBlocks(
			TEXT("<ac:structured-macro ac:name=\"info\">")
			TEXT("<ac:parameter ac:name=\"icon\">false</ac:parameter>")
			TEXT("<ac:rich-text-body><p>Kept</p></ac:rich-text-body>")
			TEXT("</ac:structured-macro>"));

		bool bLeaked = false;
		bool bKept = false;
		for (const FExtendedAtlassianDocBlock& Block : Blocks)
		{
			if (Block.Markup.Contains(TEXT("false"))) { bLeaked = true; }
			if (Block.Markup.Contains(TEXT("Kept"))) { bKept = true; }
		}

		TestFalse(TEXT("Macro parameter value does not reach the document"), bLeaked);
		TestTrue(TEXT("Macro rich text body is kept"), bKept);
	}

	{
		// Content-derived text must be escaped before it becomes markup.
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianHtml::ToBlocks(
			TEXT("<p>if a &lt; b &amp;&amp; c</p>"));

		TestEqual(TEXT("One paragraph"), Blocks.Num(), 1);
		TestTrue(TEXT("Decoded then re-escaped safely"), Blocks[0].Markup.Contains(TEXT("&lt;")));
		TestFalse(TEXT("No raw angle bracket reaches the markup"), Blocks[0].Markup.Contains(TEXT("a < b")));
	}

	{
		const TArray<FExtendedAtlassianDocBlock> Blocks = FExtendedAtlassianHtml::ToBlocks(
			TEXT("<p><a href=\"https://example.com\">link</a></p>"));

		TestEqual(TEXT("One paragraph"), Blocks.Num(), 1);
		TestTrue(TEXT("Absolute link becomes a hyperlink run"), Blocks[0].Markup.Contains(TEXT("href=\"https://example.com\"")));
	}

	TestEqual(TEXT("Empty input yields no blocks"), FExtendedAtlassianHtml::ToBlocks(FString()).Num(), 0);
	TestEqual(TEXT("Whitespace-only input yields no blocks"), FExtendedAtlassianHtml::ToBlocks(TEXT("<p>   </p>")).Num(), 0);

	return true;
}

// --- Editing round trip ------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianRoundTripTest,
	"ExtendedAtlassian.Document.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianRoundTripTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	{
		// Inline conversion must be reversible, or every save churns the file and pollutes diffs.
		const FString Source = TEXT("**bold** then *italic* then `code` then [text](https://x.com)");
		const FString Recovered = FExtendedAtlassianMarkdown::MarkupToInline(
			FExtendedAtlassianMarkdown::InlineToMarkup(Source));

		TestEqual(TEXT("Inline Markdown survives a round trip"), Recovered, Source);
	}

	{
		// Escaped characters must come back as themselves, not as entities.
		const FString Source = TEXT("a < b & c > d");
		const FString Recovered = FExtendedAtlassianMarkdown::MarkupToInline(
			FExtendedAtlassianMarkdown::InlineToMarkup(Source));

		TestEqual(TEXT("Escaped characters are restored"), Recovered, Source);
	}

	{
		// The critical property: converting twice must equal converting once. Without it, every
		// pull-edit-push cycle would slowly rewrite documents that nobody edited.
		const FString Source =
			TEXT("# Title\n\nA paragraph with **bold**.\n\n## Section\n\n- one\n- two\n\n```\ncode\n```\n\n> quoted\n");

		const FString Once = FExtendedAtlassianMarkdown::FromBlocks(FExtendedAtlassianMarkdown::ToBlocks(Source));
		const FString Twice = FExtendedAtlassianMarkdown::FromBlocks(FExtendedAtlassianMarkdown::ToBlocks(Once));

		TestEqual(TEXT("Markdown serialisation is idempotent"), Twice, Once);
		TestTrue(TEXT("Heading survives"), Once.Contains(TEXT("# Title")));
		TestTrue(TEXT("Bold survives"), Once.Contains(TEXT("**bold**")));
		TestTrue(TEXT("List survives"), Once.Contains(TEXT("- one")));
		TestTrue(TEXT("Code fence survives"), Once.Contains(TEXT("```")));
	}

	{
		// Markdown to storage and back must preserve the document.
		const FString Source = TEXT("# Heading\n\nText with **bold** and a [link](https://x.com).\n\n- item one\n- item two\n");

		const FString Storage = FExtendedAtlassianStorage::FromMarkdown(Source);
		TestTrue(TEXT("Storage contains a heading element"), Storage.Contains(TEXT("<h1>")));
		TestTrue(TEXT("Storage contains a list"), Storage.Contains(TEXT("<ul>")) && Storage.Contains(TEXT("<li>")));
		TestTrue(TEXT("Storage contains strong"), Storage.Contains(TEXT("<strong>")));
		TestTrue(TEXT("Storage contains an anchor"), Storage.Contains(TEXT("href=\"https://x.com\"")));

		const FString Recovered = FExtendedAtlassianStorage::ToMarkdown(Storage);
		TestTrue(TEXT("Heading returns"), Recovered.Contains(TEXT("# Heading")));
		TestTrue(TEXT("Bold returns"), Recovered.Contains(TEXT("**bold**")));
		TestTrue(TEXT("Both list items return"), Recovered.Contains(TEXT("- item one")) && Recovered.Contains(TEXT("- item two")));
	}

	{
		// The safety gate. A page whose macros we cannot rebuild must never be editable, because a
		// save would silently delete them.
		TArray<FString> Reasons;

		TestTrue(TEXT("Plain prose is safe to edit"),
			FExtendedAtlassianStorage::CanRoundTrip(TEXT("<p>Just text</p>"), Reasons));
		TestEqual(TEXT("No blockers reported for prose"), Reasons.Num(), 0);

		TestFalse(TEXT("A macro blocks editing"),
			FExtendedAtlassianStorage::CanRoundTrip(
				TEXT("<p>text</p><ac:structured-macro ac:name=\"jira\"/>"), Reasons));
		TestTrue(TEXT("The macro is named as the reason"), Reasons.Num() > 0);

		TestFalse(TEXT("A layout blocks editing"),
			FExtendedAtlassianStorage::CanRoundTrip(TEXT("<ac:layout><ac:layout-section/></ac:layout>"), Reasons));

		TestFalse(TEXT("Unenumerated Confluence markup still blocks editing"),
			FExtendedAtlassianStorage::CanRoundTrip(TEXT("<p><ac:something-new/></p>"), Reasons));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianDocumentStoreTest,
	"ExtendedAtlassian.Document.Store",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianDocumentStoreTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	{
		FExtendedAtlassianDocumentFile File;
		File.PageId = TEXT("12345");
		File.SpaceId = TEXT("98765");
		File.SpaceKey = TEXT("TOT");
		File.Title = TEXT("08 - UX / UI");
		File.Version = 7;

		const FString Contents = FExtendedAtlassianDocumentStore::MakeFrontMatter(File) + TEXT("# Body\n\nText.\n");

		FExtendedAtlassianDocumentFile Parsed;
		TestTrue(TEXT("Front matter is recognised"),
			FExtendedAtlassianDocumentStore::ParseFrontMatter(Contents, Parsed));

		TestEqual(TEXT("Page id round trips"), Parsed.PageId, File.PageId);
		TestEqual(TEXT("Space key round trips"), Parsed.SpaceKey, File.SpaceKey);
		TestEqual(TEXT("Version round trips"), Parsed.Version, File.Version);
		TestEqual(TEXT("Title round trips"), Parsed.Title, File.Title);
		TestTrue(TEXT("Body is separated from front matter"), Parsed.Markdown.StartsWith(TEXT("# Body")));
		TestFalse(TEXT("Body excludes the delimiter"), Parsed.Markdown.Contains(TEXT("confluence-id")));
	}

	{
		// A hand-written Markdown file with no front matter must load as an unlinked document
		// rather than having its first lines eaten.
		FExtendedAtlassianDocumentFile Parsed;
		const bool bHadFrontMatter = FExtendedAtlassianDocumentStore::ParseFrontMatter(
			TEXT("# Just Markdown\n\nNo front matter here.\n"), Parsed);

		TestFalse(TEXT("No front matter reported"), bHadFrontMatter);
		TestFalse(TEXT("Not linked to Confluence"), Parsed.IsLinkedToConfluence());
		TestTrue(TEXT("Whole file is body"), Parsed.Markdown.StartsWith(TEXT("# Just Markdown")));
	}

	{
		// An unterminated block is malformed; it must not swallow the document.
		FExtendedAtlassianDocumentFile Parsed;
		FExtendedAtlassianDocumentStore::ParseFrontMatter(TEXT("---\ntitle: x\n\n# Body\n"), Parsed);
		TestTrue(TEXT("Malformed front matter keeps the content"), Parsed.Markdown.Contains(TEXT("# Body")));
	}

	{
		// The id in the filename is what keeps a renamed page mapped to the same working copy.
		const FString PathA = FExtendedAtlassianDocumentStore::MakeFilePath(TEXT("TOT"), TEXT("Old Title"), TEXT("999"));
		const FString PathB = FExtendedAtlassianDocumentStore::MakeFilePath(TEXT("TOT"), TEXT("New Title"), TEXT("999"));

		TestTrue(TEXT("Both paths carry the page id"), PathA.Contains(TEXT("999")) && PathB.Contains(TEXT("999")));

		const FString Illegal = FExtendedAtlassianDocumentStore::MakeFilePath(TEXT("TOT"), TEXT("a/b:c*d?"), TEXT("1"));
		TestFalse(TEXT("Illegal filename characters are replaced"),
			FPaths::GetCleanFilename(Illegal).Contains(TEXT(":")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianIssueKeyTest,
	"ExtendedAtlassian.Jira.ExtractIssueKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianIssueKeyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TArray<FString> Projects = { TEXT("TOT"), TEXT("ART") };

	{
		const TArray<FString> Keys = FExtendedAtlassianJira::ExtractIssueKeys(
			TEXT("See TOT-123 and ART-9 for details."), Projects);

		TestEqual(TEXT("Both keys found"), Keys.Num(), 2);
		TestTrue(TEXT("First key"), Keys.Contains(TEXT("TOT-123")));
		TestTrue(TEXT("Second key"), Keys.Contains(TEXT("ART-9")));
	}

	{
		// The reason keys are matched against known projects rather than a bare pattern.
		const TArray<FString> Keys = FExtendedAtlassianJira::ExtractIssueKeys(
			TEXT("Encoded as UTF-8 on UE-5 with ISO-9001."), Projects);

		TestEqual(TEXT("Unknown prefixes are not issue keys"), Keys.Num(), 0);
	}

	{
		// A key must not be matched inside a longer token.
		const TArray<FString> Keys = FExtendedAtlassianJira::ExtractIssueKeys(TEXT("XTOT-5 and NOTTOT-7"), Projects);
		TestEqual(TEXT("Embedded matches are rejected"), Keys.Num(), 0);
	}

	{
		const TArray<FString> Keys = FExtendedAtlassianJira::ExtractIssueKeys(
			TEXT("TOT-1 again TOT-1 and TOT-1"), Projects);

		TestEqual(TEXT("Duplicates collapse"), Keys.Num(), 1);
	}

	{
		const TArray<FString> Keys = FExtendedAtlassianJira::ExtractIssueKeys(TEXT("TOT- has no number"), Projects);
		TestEqual(TEXT("A prefix without digits is not a key"), Keys.Num(), 0);
	}

	TestEqual(TEXT("Empty text yields nothing"),
		FExtendedAtlassianJira::ExtractIssueKeys(FString(), Projects).Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianParseLabelsTest,
	"ExtendedAtlassian.Jira.ParseLabels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianParseLabelsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	{
		const TArray<FString> Labels = FExtendedAtlassianJira::ParseLabels(TEXT("ui, audio ,inventory"));

		TestEqual(TEXT("Three labels"), Labels.Num(), 3);
		TestEqual(TEXT("Surrounding whitespace trimmed"), Labels[1], FString(TEXT("audio")));
	}

	{
		// The whole reason this is not a plain split: Jira rejects a label containing a space.
		const TArray<FString> Labels = FExtendedAtlassianJira::ParseLabels(TEXT("needs art pass"));

		TestEqual(TEXT("One label"), Labels.Num(), 1);
		TestEqual(TEXT("Inner spaces become hyphens"), Labels[0], FString(TEXT("needs-art-pass")));
	}

	{
		// A trailing comma is the most common way to send Jira an empty label and fail the create.
		const TArray<FString> Labels = FExtendedAtlassianJira::ParseLabels(TEXT("ui,,audio,   ,"));

		TestEqual(TEXT("Blank entries dropped"), Labels.Num(), 2);
		TestFalse(TEXT("No empty label survives"), Labels.Contains(FString()));
	}

	TestEqual(TEXT("Empty input yields nothing"), FExtendedAtlassianJira::ParseLabels(FString()).Num(), 0);
	TestEqual(TEXT("Whitespace-only input yields nothing"), FExtendedAtlassianJira::ParseLabels(TEXT("   ")).Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianClearParentPayloadTest,
	"ExtendedAtlassian.Jira.ClearParentPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianClearParentPayloadTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FExtendedAtlassianIssueUpdate Update;
	Update.bClearParent = true;
	TestEqual(
		TEXT("Clearing an epic emits Jira's documented parent-none update"),
		FExtendedAtlassianJira::BuildIssueUpdateBody(Update),
		FString(TEXT("{\"fields\":{},\"update\":{\"parent\":[{\"set\":{\"none\":true}}]}}")));
	return true;
}

// --- Credential store --------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianCredentialStoreTest,
	"ExtendedAtlassian.Credentials.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianCredentialStoreTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Never the real store: running the suite must not clobber the user's stored token.
	const FString TempPath = FPaths::Combine(
		FPaths::ProjectIntermediateDir(),
		TEXT("ExtendedAtlassianTests"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".ini"));

	FExtendedAtlassianCredentials Written;
	Written.Email = TEXT("tester@example.com");
	Written.ApiToken = TEXT("token-with-symbols-!@#$%^&*()_+-=[]{}|;:,.<>?");

	TestTrue(TEXT("Credentials save"), FExtendedAtlassianCredentialStore::SaveTo(TempPath, Written));

	FExtendedAtlassianCredentials Read;
	TestTrue(TEXT("Credentials load"), FExtendedAtlassianCredentialStore::LoadFrom(TempPath, Read));
	TestEqual(TEXT("E-mail round trips"), Read.Email, Written.Email);
	TestEqual(TEXT("Token round trips intact"), Read.ApiToken, Written.ApiToken);

	if (FExtendedAtlassianCredentialStore::IsEncryptionAvailable())
	{
		// The point of the store is that the token is not sitting on disk in the clear.
		FString RawFile;
		FFileHelper::LoadFileToString(RawFile, *TempPath);
		TestFalse(TEXT("Token is not stored in plain text"), RawFile.Contains(Written.ApiToken));
	}

	{
		// An incomplete credential must be rejected rather than half-written.
		FExtendedAtlassianCredentials Incomplete;
		Incomplete.Email = TEXT("someone@example.com");
		TestFalse(TEXT("Incomplete credentials are refused"),
			FExtendedAtlassianCredentialStore::SaveTo(TempPath + TEXT(".partial"), Incomplete));
	}

	IFileManager::Get().Delete(*TempPath, false, true, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianUserCacheTest,
	"ExtendedAtlassian.Credentials.VerifiedUserCache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianUserCacheTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString TempPath = FPaths::Combine(
		FPaths::ProjectIntermediateDir(),
		TEXT("ExtendedAtlassianTests"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".user.ini"));
	const FString SiteUrl = TEXT("https://example.atlassian.net");
	const FString CredentialEmail = TEXT("tester@example.com");

	FExtendedAtlassianUser Written;
	Written.AccountId = TEXT("account-123");
	Written.DisplayName = TEXT("Test User");
	Written.EmailAddress = CredentialEmail;
	Written.AvatarUrl = TEXT("https://avatar.example/user.png");
	Written.Initials = TEXT("TU");
	Written.AvatarBackground = TEXT("#123456");
	Written.AvatarForeground = TEXT("#ffffff");

	TestTrue(
		TEXT("Verified user saves"),
		FExtendedAtlassianUserCache::SaveTo(
			TempPath,
			SiteUrl,
			CredentialEmail,
			Written));

	FExtendedAtlassianUser Read;
	TestTrue(
		TEXT("Matching site and credential hydrate the cached user"),
		FExtendedAtlassianUserCache::LoadFrom(
			TempPath,
			SiteUrl,
			CredentialEmail,
			Read));
	TestEqual(TEXT("Account id round trips"), Read.AccountId, Written.AccountId);
	TestEqual(TEXT("Display name round trips"), Read.DisplayName, Written.DisplayName);
	TestEqual(TEXT("Avatar round trips"), Read.AvatarUrl, Written.AvatarUrl);

	FExtendedAtlassianUser Mismatch;
	TestFalse(
		TEXT("A different site cannot reuse the profile"),
		FExtendedAtlassianUserCache::LoadFrom(
			TempPath,
			TEXT("https://other.atlassian.net"),
			CredentialEmail,
			Mismatch));
	TestFalse(
		TEXT("A different credential cannot reuse the profile"),
		FExtendedAtlassianUserCache::LoadFrom(
			TempPath,
			SiteUrl,
			TEXT("other@example.com"),
			Mismatch));

	IFileManager::Get().Delete(*TempPath, false, true, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExtendedAtlassianArchivePageModelTest,
	"ExtendedAtlassian.Workspace.ArchivePageModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FExtendedAtlassianArchivePageModelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FExtendedAtlassianWorkspaceSnapshot Snapshot;
	FExtendedAtlassianPage Page;
	Page.Id = TEXT("12345");
	Page.Title = TEXT("Archive me");
	Snapshot.Pages.Add(Page);

	FExtendedAtlassianDocumentTreeNode Node;
	Node.Id = Page.Id;
	Node.Label = Page.Title;
	Snapshot.DocumentTree.Add(Node);

	FExtendedAtlassianCommentCollection Comments;
	Comments.TargetId = TEXT("page:") + Page.Id;
	Snapshot.CommentCollections.Add(Comments);

	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::ArchivePage;
	Mutation.TargetId = Page.Id;

	TestTrue(
		TEXT("Archive is handled as a document mutation"),
		ExtendedAtlassianModelUtils::ApplyDocumentMutation(Snapshot, Mutation));
	TestEqual(TEXT("Archived page leaves the page model"), Snapshot.Pages.Num(), 0);
	TestEqual(TEXT("Archived page leaves the document tree"), Snapshot.DocumentTree.Num(), 0);
	TestEqual(TEXT("Archived page comments leave the model"), Snapshot.CommentCollections.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
