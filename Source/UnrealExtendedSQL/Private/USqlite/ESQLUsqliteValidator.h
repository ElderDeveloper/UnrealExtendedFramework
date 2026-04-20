// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Private/USqlite/ESQLUsqliteTypes.h"

class FESQLUsqliteValidator
{
public:
	static FESQLUsqliteValidationResult ValidateProject(const FESQLUsqliteProject& Project);
	static FESQLUsqliteValidationResult ValidateAgainstLock(const FESQLUsqliteProject& Project, const TMap<FString, FString>& GeneratedFiles);
	static FESQLUsqliteValidationResult ValidateForSave(const FESQLUsqliteProject& Project, const TMap<FString, FString>& GeneratedFiles);
};