// Copyright Moon Punch Games. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Backend/EFLegacyJsonLibrary.h"
#include "Json/EFJsonLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFLegacyJsonNodesTest, "UnrealExtendedFramework.Backend.LegacyJsonNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFLegacyJsonNodesTest::RunTest(const FString& Parameters)
{
	FEFJsonObject Object;
	if (!TestTrue(TEXT("the sample parses"), UEFJsonLibrary::ParseJsonObject(
		TEXT("{\"age\": 36, \"profile\": {}, \"items\": [{\"name\": \"first\"}, {\"name\": \"second\"}]}"), Object)))
	{
		return false;
	}

	// The deprecated nodes must keep working, and must not crash on an empty handle (the old
	// array getter dereferenced it).
	bool bSuccess = true;
	TArray<FEFJsonObject> Elements;
	UEFLegacyJsonLibrary::GetExtendedArrayField(bSuccess, FEFJsonObject(), TEXT("items"), Elements);
	TestFalse(TEXT("legacy array getter on an empty handle fails"), bSuccess);

	UEFLegacyJsonLibrary::GetExtendedArrayField(bSuccess, Object, TEXT("items"), Elements);
	TestTrue(TEXT("legacy array getter reads objects"), bSuccess);
	TestEqual(TEXT("legacy array getter count"), Elements.Num(), 2);

	int32 Age = 0;
	UEFLegacyJsonLibrary::GetExtendedNumberField(bSuccess, Object, TEXT("age"), Age);
	TestTrue(TEXT("legacy number getter"), bSuccess && Age == 36);

	TArray<FString> Keys;
	UEFLegacyJsonLibrary::GetExtendedJsonValueKeys(bSuccess, Object, Keys);
	TestTrue(TEXT("legacy key list"), bSuccess && Keys.Contains(TEXT("profile")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
