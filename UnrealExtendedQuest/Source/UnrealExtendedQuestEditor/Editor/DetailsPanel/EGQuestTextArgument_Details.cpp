#include "EGQuestTextArgument_Details.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"

void FEGQuestTextArgument_Details::CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row,
	IPropertyTypeCustomizationUtils&)
{
	if (!Handle->GetProperty()->HasMetaData(TEXT("ShowOnlyInnerProperties")))
	{
		Row.NameContent()[Handle->CreatePropertyNameWidget()];
	}
}

void FEGQuestTextArgument_Details::CustomizeChildren(TSharedRef<IPropertyHandle> Handle, IDetailChildrenBuilder& Builder,
	IPropertyTypeCustomizationUtils&)
{
	uint32 Count = 0;
	Handle->GetNumChildren(Count);
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		if (TSharedPtr<IPropertyHandle> Child = Handle->GetChildHandle(Index); Child.IsValid())
		{
			Builder.AddProperty(Child.ToSharedRef());
		}
	}
}
