// Copyright Moon Punch Games. All Rights Reserved.

#include "EFUIFixtureBasicProviders.h"

#include "Blueprint/UserWidget.h"

void UEFUIFixtureSetPropertiesConfig::ApplyToWidget(UUserWidget& Widget) const
{
	for (const TPair<FName, FString>& Pair : PropertyValues)
	{
		const FProperty* Property = Widget.GetClass()->FindPropertyByName(Pair.Key);
		if (!Property)
		{
			UE_LOG(LogTemp, Warning, TEXT("UI Lab fixture: property '%s' not found on '%s'."),
				*Pair.Key.ToString(), *Widget.GetClass()->GetName());
			continue;
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(&Widget);
		Property->ImportText_Direct(*Pair.Value, ValuePtr, &Widget, PPF_None);
	}
}

void UEFUIFixtureCallFunctionConfig::ApplyToWidget(UUserWidget& Widget) const
{
	if (FunctionName.IsNone())
	{
		return;
	}

	UFunction* Function = Widget.FindFunction(FunctionName);
	if (!Function || Function->NumParms != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UI Lab fixture: parameterless function '%s' not found on '%s'."),
			*FunctionName.ToString(), *Widget.GetClass()->GetName());
		return;
	}

	Widget.ProcessEvent(Function, nullptr);
}
