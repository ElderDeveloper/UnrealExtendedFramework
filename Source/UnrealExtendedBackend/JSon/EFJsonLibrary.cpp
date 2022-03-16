// Fill out your copyright notice in the Description page of Project Settings.


#include "EFJsonLibrary.h"
#include "EFJsonObject.h"
#include "UnrealExtendedFramework/Libraries/File/EFFileLibrary.h"

UEFJsonObject* UEFJsonLibrary::LoadJSonFile(TEnumAsByte<EFProjectDirectory> DirectoryType , FString Directory)
{
	const FString JsonFilePath = UEFFileLibrary::ExtendedProjectDirectory(DirectoryType) + Directory;
	//FPaths::ProjectContentDir() + "/JsonFiles/randomgenerated.json";
	
	FString JsonString; //Json converted to FString
	FFileHelper::LoadFileToString(JsonString,*JsonFilePath);

	if (const auto JObject = NewObject<UEFJsonObject>())
	{
		JObject->SetupJSonObject(JsonString);
		return JObject;
	}
	return nullptr;
}
