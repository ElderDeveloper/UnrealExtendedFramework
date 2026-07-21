// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestTesterHelper.h"


std::function<FString(const int32&)> FEGQuestTestHelper::Int32ToString = [](const int32& Value) -> FString
{
	return FString::FromInt(Value);
};

std::function<FString(const int64&)> FEGQuestTestHelper::Int64ToString = [](const int64& Value) -> FString
{
	return FString::Printf(TEXT("%lld"), Value);
};


std::function<FString(const FName&)> FEGQuestTestHelper::NameToString = [](const FName& Value) -> FString
{
	return Value.ToString();
};

std::function<FString(const FString&)> FEGQuestTestHelper::StringToString = [](const FString& Value) -> FString
{
	return Value;
};

std::function<FString(const float&)> FEGQuestTestHelper::FloatToString = [](const float& Value) -> FString
{
	return FString::SanitizeFloat(Value);
};

std::function<FString(const FVector&)> FEGQuestTestHelper::VectorToString = [](const FVector& Value) -> FString
{
	return Value.ToString();
};

std::function<FString(const FColor&)> FEGQuestTestHelper::ColorToString = [](const FColor& Value) -> FString
{
	return Value.ToString();
};
