// Copyright Moon Punch Games. All Rights Reserved.


#include "EFPrintScreenLibrary.h"

#include "Kismet/KismetSystemLibrary.h"


void UEFPrintScreenLibrary::PrintZeroString(const UObject* WorldContextObject, FString String, float Time, FLinearColor Color, const bool bPrintToLog)
{
	if (!WorldContextObject) return;
	UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(), String, true, bPrintToLog, Color, Time);
}
