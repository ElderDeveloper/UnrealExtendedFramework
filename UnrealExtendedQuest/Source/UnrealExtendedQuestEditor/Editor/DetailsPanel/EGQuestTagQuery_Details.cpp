// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestTagQuery_Details.h"

#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "SGameplayTagQueryEntryBox.h"

TSharedRef<IPropertyTypeCustomization> FEGQuestTagQuery_Details::MakeInstance()
{
	return MakeShared<FEGQuestTagQuery_Details>();
}

void FEGQuestTagQuery_Details::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow
		.NameContent()
		[
			InStructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.VAlign(VAlign_Center)
		[
			SNew(SGameplayTagQueryEntryBox)
			.DescriptionMaxWidth(400.0f)
			.PropertyHandle(InStructPropertyHandle)
		];
}
