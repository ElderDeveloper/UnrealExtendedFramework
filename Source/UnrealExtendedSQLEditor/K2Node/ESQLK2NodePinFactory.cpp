// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "K2Node/ESQLK2NodePinFactory.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node/ESQLAsyncTypedK2Nodes.h"
#include "K2Node/ESQLK2FieldFilterUtils.h"
#include "K2Node/ESQLPlayerTypedK2Nodes.h"
#include "K2Node/ESQLQueryBuilderK2Nodes.h"
#include "K2Node/ESQLRowQueryK2Nodes.h"
#include "K2Node/SGraphPinESQLId.h"
#include "K2Node/ESQLTypedRowK2Nodes.h"
#include "Misc/OutputDeviceNull.h"
#include "Shared/ESQLId.h"
#include "SGraphPinNameList.h"
#include "UObject/Class.h"

namespace
{
bool IsESQLIdStructPin(const UEdGraphPin* Pin)
{
	if (!Pin || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Struct)
	{
		return false;
	}

	const UScriptStruct* PinStruct = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get());
	const UScriptStruct* SqlIdStruct = FESQLId::StaticStruct();
	if (!PinStruct || !SqlIdStruct)
	{
		return false;
	}

	if (PinStruct == SqlIdStruct)
	{
		return true;
	}

	const FString PinStructName = PinStruct->GetName();
	const FString SqlIdStructName = SqlIdStruct->GetName();
	return PinStructName.Equals(SqlIdStructName, ESearchCase::IgnoreCase)
		|| PinStructName.Contains(SqlIdStructName, ESearchCase::IgnoreCase);
}
}

TSharedPtr<SGraphPin> FESQLK2NodePinFactory::CreatePin(UEdGraphPin* Pin) const
{
	if (Pin
		&& Pin->Direction == EGPD_Input
		&& Pin->ParentPin == nullptr
		&& IsESQLIdStructPin(Pin))
	{
		return SNew(SGraphPinESQLId, Pin);
	}

	if (!Pin
		|| Pin->Direction != EGPD_Input
		|| Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Name)
	{
		return nullptr;
	}

	UEdGraphNode* Node = Pin->GetOwningNode();
	if (!Node
		|| (!Node->IsA<UK2Node_ESQLFindRows>()
			&& !Node->IsA<UK2Node_ESQLQueryRows>()
			&& !Node->IsA<UK2Node_ESQLCountRowsQuery>()
			&& !Node->IsA<UK2Node_ESQLFindRowsByField>()
			&& !Node->IsA<UK2Node_ESQLLoadPage>()
			&& !Node->IsA<UK2Node_ESQLCountRows>()
			&& !Node->IsA<UK2Node_ESQLFindFirstRowByField>()
			&& !Node->IsA<UK2Node_ESQLFindPlayerRows>()
			&& !Node->IsA<UK2Node_ESQLAsyncFindRows>()
			&& !Node->IsA<UK2Node_ESQLMakeQuerySpec>()))
	{
		return nullptr;
	}

	if (Pin->PinName != TEXT("FieldName") && !FESQLQueryClauseUiBase::IsFieldNamePinName(Pin->PinName))
	{
		return nullptr;
	}

	TArray<TSharedPtr<FName>> NameOptions;
	ESQLK2FieldFilterUtils::CollectFieldNameOptions(Node, TEXT("TableAsset"), NameOptions);
	if (NameOptions.Num() == 0)
	{
		return nullptr;
	}

	return SNew(SGraphPinNameList, Pin, NameOptions);
}