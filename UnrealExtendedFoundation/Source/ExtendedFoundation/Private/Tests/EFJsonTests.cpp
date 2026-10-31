// Copyright Moon Punch Games. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Http/EFHttpTypes.h"
#include "Json/EFJsonLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace EFJsonTestsPrivate
{
	static const TCHAR* SampleJson = TEXT(R"json({
		"name": "Ada",
		"age": 36,
		"ratio": 0.5,
		"admin": true,
		"nothing": null,
		"tags": ["a", "b", 3],
		"profile": { "city": "London" },
		"items": [ { "name": "first" }, { "name": "second" } ]
	})json");

	static FEFJsonObject ParseSample(FAutomationTestBase& Test)
	{
		FEFJsonObject Object;
		Test.TestTrue(TEXT("The sample parses"), UEFJsonLibrary::ParseJsonObject(SampleJson, Object));
		return Object;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFJsonParseReadTest, "ExtendedFoundation.Json.ParseAndRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFJsonParseReadTest::RunTest(const FString& Parameters)
{
	const FEFJsonObject Object = EFJsonTestsPrivate::ParseSample(*this);
	if (!TestTrue(TEXT("Parsed handle is valid"), Object.IsValid()))
	{
		return false;
	}

	FString Name;
	TestTrue(TEXT("name is a string"), UEFJsonLibrary::TryGetStringField(Object, TEXT("name"), Name));
	TestEqual(TEXT("name value"), Name, FString(TEXT("Ada")));

	int64 Age = 0;
	TestTrue(TEXT("age is an integer"), UEFJsonLibrary::TryGetIntegerField(Object, TEXT("age"), Age));
	TestEqual(TEXT("age value"), Age, int64(36));

	double Ratio = 0.0;
	TestTrue(TEXT("ratio is a number"), UEFJsonLibrary::TryGetNumberField(Object, TEXT("ratio"), Ratio));
	TestEqual(TEXT("ratio value"), Ratio, 0.5);

	bool bAdmin = false;
	TestTrue(TEXT("admin is a bool"), UEFJsonLibrary::TryGetBoolField(Object, TEXT("admin"), bAdmin));
	TestTrue(TEXT("admin value"), bAdmin);

	TestTrue(TEXT("nothing is null"), UEFJsonLibrary::GetFieldType(Object, TEXT("nothing")) == EEFJsonType::Null);
	TestTrue(TEXT("a missing field has no type"), UEFJsonLibrary::GetFieldType(Object, TEXT("missing")) == EEFJsonType::None);
	TestFalse(TEXT("a missing field is not found"), UEFJsonLibrary::HasField(Object, TEXT("missing")));

	FString Missing = TEXT("stale");
	TestFalse(TEXT("reading a missing string fails"), UEFJsonLibrary::TryGetStringField(Object, TEXT("missing"), Missing));
	TestTrue(TEXT("a failed read clears the output"), Missing.IsEmpty());

	TArray<FString> Tags;
	TestTrue(TEXT("tags read as strings (numbers convert)"), UEFJsonLibrary::TryGetStringArrayField(Object, TEXT("tags"), Tags));
	TestEqual(TEXT("tags count"), Tags.Num(), 3);

	TArray<FEFJsonObject> NotObjects;
	TestFalse(TEXT("tags are not an object array"), UEFJsonLibrary::TryGetObjectArrayField(Object, TEXT("tags"), NotObjects));
	TestEqual(TEXT("a failed object array read is empty"), NotObjects.Num(), 0);

	TArray<FEFJsonObject> Items;
	TestTrue(TEXT("items is an object array"), UEFJsonLibrary::TryGetObjectArrayField(Object, TEXT("items"), Items));
	TestEqual(TEXT("items count"), Items.Num(), 2);

	FEFJsonObject Profile;
	TestTrue(TEXT("profile is an object"), UEFJsonLibrary::TryGetObjectField(Object, TEXT("profile"), Profile));
	FString City;
	TestTrue(TEXT("profile.city reads"), UEFJsonLibrary::TryGetStringField(Profile, TEXT("city"), City));
	TestEqual(TEXT("profile.city value"), City, FString(TEXT("London")));

	FEFJsonObject Invalid;
	TestFalse(TEXT("malformed text does not parse"), UEFJsonLibrary::ParseJsonObject(TEXT("{ \"a\": "), Invalid));
	TestFalse(TEXT("a failed parse leaves an empty handle"), Invalid.IsValid());
	TestFalse(TEXT("an array root is not an object"), UEFJsonLibrary::ParseJsonObject(TEXT("[1, 2]"), Invalid));

	FEFJsonValue ArrayRoot;
	TestTrue(TEXT("an array root parses as a value"), UEFJsonLibrary::ParseJsonValue(TEXT("[1, 2]"), ArrayRoot));
	TestTrue(TEXT("array root type"), ArrayRoot.GetType() == EEFJsonType::Array);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFJsonWriteTest, "ExtendedFoundation.Json.WriteAndRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFJsonWriteTest::RunTest(const FString& Parameters)
{
	// A setter on an empty handle creates the object and returns it.
	FEFJsonObject Object = UEFJsonLibrary::SetStringField(FEFJsonObject(), TEXT("quote"), TEXT("he said \"hi\"\n"));
	if (!TestTrue(TEXT("setter on an empty handle returns a new object"), Object.IsValid()))
	{
		return false;
	}

	UEFJsonLibrary::SetIntegerField(Object, TEXT("count"), 42);
	UEFJsonLibrary::SetBoolField(Object, TEXT("flag"), false);
	UEFJsonLibrary::SetNullField(Object, TEXT("empty"));
	UEFJsonLibrary::SetStringArrayField(Object, TEXT("names"), { TEXT("x"), TEXT("y") });
	UEFJsonLibrary::SetArrayField(Object, TEXT("mixed"), { UEFJsonLibrary::MakeJsonNumberValue(1.5), FEFJsonValue(), UEFJsonLibrary::MakeJsonStringValue(TEXT("s")) });
	UEFJsonLibrary::SetObjectField(Object, TEXT("child"), UEFJsonLibrary::SetStringField(UEFJsonLibrary::MakeJsonObject(), TEXT("k"), TEXT("v")));
	UEFJsonLibrary::SetObjectField(Object, TEXT("missingChild"), FEFJsonObject());

	TestTrue(TEXT("an empty child handle writes null"), UEFJsonLibrary::GetFieldType(Object, TEXT("missingChild")) == EEFJsonType::Null);

	// Copies of the handle share the object; CopyJsonObject does not.
	const FEFJsonObject SharedCopy = Object;
	const FEFJsonObject DeepCopy = UEFJsonLibrary::CopyJsonObject(Object);
	UEFJsonLibrary::SetStringField(SharedCopy, TEXT("added"), TEXT("later"));
	TestTrue(TEXT("a handle copy sees later writes"), UEFJsonLibrary::HasField(Object, TEXT("added")));
	TestFalse(TEXT("a deep copy does not"), UEFJsonLibrary::HasField(DeepCopy, TEXT("added")));

	FEFJsonObject DeepChild;
	FEFJsonObject OriginalChild;
	UEFJsonLibrary::TryGetObjectField(DeepCopy, TEXT("child"), DeepChild);
	UEFJsonLibrary::TryGetObjectField(Object, TEXT("child"), OriginalChild);
	UEFJsonLibrary::SetStringField(DeepChild, TEXT("k"), TEXT("changed"));
	FString OriginalValue;
	UEFJsonLibrary::TryGetStringField(OriginalChild, TEXT("k"), OriginalValue);
	TestEqual(TEXT("a deep copy owns its nested objects"), OriginalValue, FString(TEXT("v")));

	UEFJsonLibrary::RemoveField(Object, TEXT("added"));
	TestFalse(TEXT("RemoveField removes"), UEFJsonLibrary::HasField(Object, TEXT("added")));

	// Serialize and parse back.
	const FString Text = UEFJsonLibrary::JsonObjectToString(Object);
	FEFJsonObject Reparsed;
	if (!TestTrue(TEXT("serialized text parses"), UEFJsonLibrary::ParseJsonObject(Text, Reparsed)))
	{
		return false;
	}

	FString Quote;
	UEFJsonLibrary::TryGetStringField(Reparsed, TEXT("quote"), Quote);
	TestEqual(TEXT("escaped text survives a round trip"), Quote, FString(TEXT("he said \"hi\"\n")));

	int64 Count = 0;
	UEFJsonLibrary::TryGetIntegerField(Reparsed, TEXT("count"), Count);
	TestEqual(TEXT("integer survives a round trip"), Count, int64(42));

	TArray<FEFJsonValue> Mixed;
	TestTrue(TEXT("mixed array reads"), UEFJsonLibrary::TryGetArrayField(Reparsed, TEXT("mixed"), Mixed));
	if (TestEqual(TEXT("mixed array keeps every slot"), Mixed.Num(), 3))
	{
		TestTrue(TEXT("an empty value handle was written as null"), Mixed[1].GetType() == EEFJsonType::Null);
	}

	TestTrue(TEXT("pretty printing produces line breaks"), UEFJsonLibrary::JsonObjectToString(Object, true).Contains(TEXT("\n")));
	TestTrue(TEXT("an empty handle serializes to an empty string"), UEFJsonLibrary::JsonObjectToString(FEFJsonObject()).IsEmpty());

	// Primitive values serialize to their JSON literal.
	TestEqual(TEXT("string literal"), UEFJsonLibrary::JsonValueToString(UEFJsonLibrary::MakeJsonStringValue(TEXT("a\"b"))), FString(TEXT("\"a\\\"b\"")));
	TestEqual(TEXT("bool literal"), UEFJsonLibrary::JsonValueToString(UEFJsonLibrary::MakeJsonBoolValue(true)), FString(TEXT("true")));
	TestEqual(TEXT("null literal"), UEFJsonLibrary::JsonValueToString(UEFJsonLibrary::MakeJsonNullValue()), FString(TEXT("null")));
	TestEqual(TEXT("integer literal"), UEFJsonLibrary::JsonValueToString(UEFJsonLibrary::MakeJsonIntegerValue(7)), FString(TEXT("7")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFJsonPathTest, "ExtendedFoundation.Json.Path",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFJsonPathTest::RunTest(const FString& Parameters)
{
	const FEFJsonObject Object = EFJsonTestsPrivate::ParseSample(*this);

	FEFJsonValue Value;
	FString Text;

	TestTrue(TEXT("items[1].name resolves"), UEFJsonLibrary::TryGetFieldByPath(Object, TEXT("items[1].name"), Value));
	UEFJsonLibrary::TryGetStringValue(Value, Text);
	TestEqual(TEXT("items[1].name value"), Text, FString(TEXT("second")));

	TestTrue(TEXT("profile.city resolves"), UEFJsonLibrary::TryGetFieldByPath(Object, TEXT("profile.city"), Value));
	TestTrue(TEXT("tags[2] resolves"), UEFJsonLibrary::TryGetFieldByPath(Object, TEXT("tags[2]"), Value));
	TestTrue(TEXT("tags[2] is a number"), Value.GetType() == EEFJsonType::Number);

	TestTrue(TEXT("an empty path is the object itself"), UEFJsonLibrary::TryGetFieldByPath(Object, TEXT(""), Value));
	TestTrue(TEXT("empty path type"), Value.GetType() == EEFJsonType::Object);

	const TCHAR* BadPaths[] = {
		TEXT("items[2].name"),  // out of range
		TEXT("items[-1]"),      // negative
		TEXT("items[x]"),       // not a number
		TEXT("items[0"),        // unclosed
		TEXT("profile..city"),  // empty segment
		TEXT("profile."),       // trailing dot
		TEXT("name.first"),     // descending into a string
		TEXT("profile[0]"),     // indexing an object
		TEXT("missing.key"),
	};
	for (const TCHAR* Path : BadPaths)
	{
		TestFalse(FString::Printf(TEXT("'%s' does not resolve"), Path), UEFJsonLibrary::TryGetFieldByPath(Object, Path, Value));
		TestFalse(FString::Printf(TEXT("'%s' leaves an empty value"), Path), Value.IsValid());
	}

	TestFalse(TEXT("an empty handle resolves nothing"), UEFJsonLibrary::TryGetFieldByPath(FEFJsonObject(), TEXT(""), Value));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFJsonStructTest, "ExtendedFoundation.Json.Structs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFJsonStructTest::RunTest(const FString& Parameters)
{
	FEFHttpRequest Source;
	Source.Url = TEXT("https://example.com/a");
	Source.Verb = EEFHttpVerb::Patch;
	Source.MaxRetries = 3;
	Source.Headers.Add(TEXT("X-Test"), TEXT("1"));

	const FEFJsonObject Object = UEFJsonLibrary::ToJsonObject(Source);
	if (!TestTrue(TEXT("a struct converts to an object"), Object.IsValid()))
	{
		return false;
	}

	FString Url;
	TestTrue(TEXT("field names keep their authored case"), UEFJsonLibrary::TryGetStringField(Object, TEXT("Url"), Url));
	TestEqual(TEXT("Url value"), Url, Source.Url);

	FEFHttpRequest Restored;
	TestTrue(TEXT("the object converts back"), UEFJsonLibrary::FromJsonObject(Object, Restored));
	TestEqual(TEXT("Url restored"), Restored.Url, Source.Url);
	TestTrue(TEXT("Verb restored"), Restored.Verb == Source.Verb);
	TestEqual(TEXT("MaxRetries restored"), Restored.MaxRetries, Source.MaxRetries);
	TestEqual(TEXT("Headers restored"), Restored.Headers.FindRef(TEXT("X-Test")), FString(TEXT("1")));

	FEFHttpRequest Untouched;
	TestFalse(TEXT("an empty handle does not convert"), UEFJsonLibrary::FromJsonObject(FEFJsonObject(), Untouched));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFJsonFileTest, "ExtendedFoundation.Json.Files",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFJsonFileTest::RunTest(const FString& Parameters)
{
	const FEFJsonObject Object = EFJsonTestsPrivate::ParseSample(*this);

	const FString FilePath = FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("EFJson"), FGuid::NewGuid().ToString() + TEXT(".json"));
	TestTrue(TEXT("saves to a new folder"), UEFJsonLibrary::SaveJsonObjectToFile(Object, FilePath));

	FEFJsonObject Loaded;
	TestTrue(TEXT("loads back"), UEFJsonLibrary::LoadJsonObjectFromFile(FilePath, Loaded));
	FString Name;
	UEFJsonLibrary::TryGetStringField(Loaded, TEXT("name"), Name);
	TestEqual(TEXT("loaded content"), Name, FString(TEXT("Ada")));
	IFileManager::Get().Delete(*FilePath);

	TestFalse(TEXT("a missing file fails"), UEFJsonLibrary::LoadJsonObjectFromFile(FilePath, Loaded));
	TestFalse(TEXT("an empty handle is not saved"), UEFJsonLibrary::SaveJsonObjectToFile(FEFJsonObject(), FilePath));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
