// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.

#include "CoreTypes.h"
#include "EGQuestIOTesterTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/AutomationTest.h"

#include "UnrealExtendedQuest/IO/EGQuestJsonParser.h"
#include "UnrealExtendedQuest/IO/EGQuestJsonWriter.h"

DECLARE_LOG_CATEGORY_EXTERN(LogQuestIOTester, All, All);
DEFINE_LOG_CATEGORY(LogQuestIOTester);

#if WITH_DEV_AUTOMATION_TESTS

class FEGQuestIOTester
{
public:
	// Tests Parser and Writer
	template <typename ConfigWriterType, typename ConfigParserType>
	static bool TestParser(
			FAutomationTestBase& Test,
			const FEGQuestIOTesterOptions& Options,
			const FString NameWriterType = FString(),
			const FString NameParserType = FString()
		);

	// Test all parsers/writers
	static bool TestAllParsers(FAutomationTestBase& Test);

	template <typename ConfigWriterType, typename ConfigParserType, typename StructType>
	static bool TestStruct(
		FAutomationTestBase& Test,
		const FString& StructDescription,
		const FEGQuestIOTesterOptions& Options,
		const FString NameWriterType = FString(),
		const FString NameParserType = FString()
	);
};


template <typename ConfigWriterType, typename ConfigParserType>
bool FEGQuestIOTester::TestParser(
	FAutomationTestBase& Test,
	const FEGQuestIOTesterOptions& Options,
	const FString NameWriterType,
	const FString NameParserType
)
{
	bool bAllSucceeded = true;

	bAllSucceeded &= TestStruct<ConfigWriterType, ConfigParserType, FEGQuestTestStructPrimitives>(Test, "Struct of Primitives", Options, NameWriterType, NameParserType);
	bAllSucceeded &= TestStruct<ConfigWriterType, ConfigParserType, FEGQuestTestStructComplex>(Test, "Struct of Complex types", Options, NameWriterType, NameParserType);

	bAllSucceeded &= TestStruct<ConfigWriterType, ConfigParserType, FEGQuestTestArrayPrimitive>(Test, "Array of Primitives", Options, NameWriterType, NameParserType);
	bAllSucceeded &= TestStruct<ConfigWriterType, ConfigParserType, FEGQuestTestArrayComplex>(Test, "Array of Complex types", Options, NameWriterType, NameParserType);

	bAllSucceeded &= TestStruct<ConfigWriterType, ConfigParserType, FEGQuestTestSetPrimitive>(Test, "Set of Primitives", Options, NameWriterType, NameParserType);
	bAllSucceeded &= TestStruct<ConfigWriterType, ConfigParserType, FEGQuestTestSetComplex>(Test, "Set of Complex types", Options, NameWriterType, NameParserType);

	bAllSucceeded &= TestStruct<ConfigWriterType, ConfigParserType, FEGQuestTestMapPrimitive>(Test, "Map with Primitives", Options, NameWriterType, NameParserType);
	bAllSucceeded &= TestStruct<ConfigWriterType, ConfigParserType, FEGQuestTestMapComplex>(Test, "Map with Complex types", Options, NameWriterType, NameParserType);

	return bAllSucceeded;
}


template <typename ConfigWriterType, typename ConfigParserType, typename StructType>
bool FEGQuestIOTester::TestStruct(
	FAutomationTestBase& Test,
	const FString& StructDescription,
	const FEGQuestIOTesterOptions& Options,
	const FString NameWriterType,
	const FString NameParserType
)
{
	StructType ExportedStruct;
	StructType ImportedStruct;
	ExportedStruct.GenerateRandomData(Options);
	ImportedStruct.GenerateRandomData(Options);

	// Write struct
	ConfigWriterType Writer;
	//Writer.SetLogVerbose(true);
	Writer.Write(StructType::StaticStruct(), &ExportedStruct);
	const FString WriterString = Writer.GetAsString();

	// Read struct
	ConfigParserType Parser;
	//Parser.SetLogVerbose(true);
	Parser.InitializeParserFromString(WriterString);
	Parser.ReadAllProperty(StructType::StaticStruct(), &ImportedStruct);

	// Should be the same
	FString ErrorMessage;
	if (ExportedStruct.IsEqual(ImportedStruct, ErrorMessage))
	{
		return true;
	}

	if (NameWriterType.IsEmpty() || NameParserType.IsEmpty())
	{
		UE_LOG(LogQuestIOTester, Warning, TEXT("TestStruct: Test Failed (both empty) = %s"), *StructDescription);
	}
	else
	{
		// Used only for debugging
		ConfigWriterType DebugParser;
		DebugParser.Write(StructType::StaticStruct(), &ImportedStruct);
		const FString ParserString = DebugParser.GetAsString();

		UE_LOG(LogQuestIOTester, Warning, TEXT("This = ExportedStruct, Other = ImportedStruct"));
		UE_LOG(LogQuestIOTester, Warning, TEXT("Writer = %s, Parser = %s. Test Failed = %s"), *NameWriterType, *NameParserType, *StructDescription);
		UE_LOG(LogQuestIOTester, Warning, TEXT("ErrorMessage = %s"), *ErrorMessage);
		UE_LOG(LogQuestIOTester, Warning, TEXT("ExportedStruct.GetAsString() = |%s|\n"), *WriterString);
		UE_LOG(LogQuestIOTester, Warning, TEXT("ImportedStruct.GetAsString() = |%s|\n"), *ParserString);
		UE_LOG(LogQuestIOTester, Warning, TEXT(""));
	}

	return false;
}

bool FEGQuestIOTester::TestAllParsers(FAutomationTestBase& Test)
{
	bool bAllSucceeded = true;

	FEGQuestIOTesterOptions Options;
	Options.bSupportsDatePrimitive = false;
	Options.bSupportsUObjectValueInMap = false;
	bAllSucceeded &= TestParser<FEGQuestJsonWriter, FEGQuestJsonParser>(Test, Options, TEXT("FEGQuestJsonWriter"), TEXT("FEGQuestJsonParser"));

	return bAllSucceeded;
}

// NOTE: to run this test, first remove the EAutomationTestFlags::Disabled flag
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEGQuestIOAutomationTest,
	"QuestPlugin.IO.Tests",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter
)
	//EAutomationTestFlags::Disabled |
	// EAutomationTestFlags::RequiresUser |
	// EAutomationTestFlags::ApplicationContextMask |
	// EAutomationTestFlags::ProductFilter)

bool FEGQuestIOAutomationTest::RunTest(const FString& Parameters)
{
	// UE_LOG(LogTemp, Error, TEXT("FEGQuestIOAutomationTest::RunTest"));
	TestEqual(TEXT("true == true"), true, true);
	TestTrue(TEXT("Testing all parsers"), FEGQuestIOTester::TestAllParsers(*this));

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
