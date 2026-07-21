// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestTextArgument.h"

#include "EGQuestContext.h"
#include "EGQuestHelper.h"

FFormatArgumentValue FEGQuestTextArgument::ConstructFormatArgumentValue(const UEGQuestContext& Context) const
{
	const FString ArgumentName = DisplayString;
	if (ArgumentName.StartsWith(TEXT("Role.")))
	{
		return FFormatArgumentValue(Context.ResolveRoleText(FName(*ArgumentName.RightChop(5))));
	}
	return FFormatArgumentValue(CustomTextArgument ? CustomTextArgument->GetText(&Context, DisplayString) : FText::GetEmpty());
}

void FEGQuestTextArgument::UpdateTextArgumentArray(const FText& Text, TArray<FEGQuestTextArgument>& InOutArgumentArray)
{
	TArray<FString> Parameters;
	FText::GetFormatPatternParameters(Text, Parameters);
	const TArray<FEGQuestTextArgument> Existing = InOutArgumentArray;
	InOutArgumentArray.Reset();
	for (const FString& Parameter : Parameters)
	{
		FEGQuestTextArgument& Argument = InOutArgumentArray.AddDefaulted_GetRef();
		Argument.DisplayString = Parameter;
		if (const FEGQuestTextArgument* Previous = Existing.FindByPredicate([&Parameter](const FEGQuestTextArgument& Item) { return Item.DisplayString == Parameter; }))
		{
			Argument = *Previous;
		}
	}
}
