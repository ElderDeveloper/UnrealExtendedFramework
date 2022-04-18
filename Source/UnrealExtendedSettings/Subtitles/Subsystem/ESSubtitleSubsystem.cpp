// Fill out your copyright notice in the Description page of Project Settings.


#include "ESSubtitleSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "UnrealExtendedBackend/JSon/Library/EBJsonLibrary.h"
#include "UnrealExtendedFramework/Libraries/File/EFFileLibrary.h"
#include "UnrealExtendedSettings/Subtitles/Asset/ESSubtitleLanguageSave.h"



void UESSubtitleSubsystem::SaveExtendedLanguage(int32 LanguageIndex)
{
	if(const auto SubtitleSave = UGameplayStatics::LoadGameFromSlot(SESubtitleSaveSlot,0))
	{
		if (const auto Save = Cast<UESSubtitleLanguageSave>(SubtitleSave))
		{
			Save->CurrentLanguage = SESubtitleLanguage;
			Save->SelectedLanguageIndex = SESubtitleIndex;
			UGameplayStatics::SaveGameToSlot(SubtitleSave,SESubtitleSaveSlot,0);
		}
	}
}





void UESSubtitleSubsystem::LoadLanguage()
{
	if(UGameplayStatics::DoesSaveGameExist(SESubtitleSaveSlot,0))
	{
		if(const auto SubtitleSave = UGameplayStatics::LoadGameFromSlot(SESubtitleSaveSlot,0))
		{
			if (const auto Save = Cast<UESSubtitleLanguageSave>(SubtitleSave))
			{
				SESubtitleLanguage = Save->CurrentLanguage;
				SESubtitleIndex = Save->SelectedLanguageIndex;
				UGameplayStatics::SaveGameToSlot(SubtitleSave,SESubtitleSaveSlot,0);
			}
		}
	}
	else if (const auto SubtitleSave = UGameplayStatics::CreateSaveGameObject(UESSubtitleLanguageSave::StaticClass()))
	{
		if (const auto Save = Cast<UESSubtitleLanguageSave>(SubtitleSave))
		{
			Save->CurrentLanguage = SESubtitleLanguage;
			Save->SelectedLanguageIndex = SESubtitleIndex;
			UGameplayStatics::SaveGameToSlot(SubtitleSave,SESubtitleSaveSlot,0);
		}
	}
}





void UESSubtitleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ExtendedSubtitleLanguages.Num() == 0)
	{
		ExtendedSubtitleLanguages.Add(FGameplayTag::EmptyTag ,FExtendedSubtitleLanguageSettings());
	}
	
	LoadLanguage();
	SESubtitleLanguages = ExtendedSubtitleLanguages;
}









void UESSubtitleSubsystem::ExecuteExtendedSubtitle(const UObject* WorldContextObject ,const FString SubtitleKey)
{
	FExtendedSubtitle SubtitleStruct = GetSubtitleJSon(SubtitleKey);
	
	if (GetExtendedSubtitleSound(SubtitleKey,SubtitleStruct) && WorldContextObject)
	{
		UGameplayStatics::PlaySound2D(WorldContextObject,SubtitleStruct.SubtitleSound,1);
	}
	OnExecuteSubtitle.Broadcast(SubtitleStruct.Subtitle,SubtitleStruct.Duration);
}




void UESSubtitleSubsystem::ExecuteExtendedSubtitleLocation(const UObject* WorldContextObject ,const FString SubtitleKey, const FVector Location)
{
	FExtendedSubtitle SubtitleStruct = GetSubtitleJSon(SubtitleKey);
	if (GetExtendedSubtitleSound(SubtitleKey,SubtitleStruct) && WorldContextObject)
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContextObject,SubtitleStruct.SubtitleSound,Location);
	}
	OnExecuteSubtitle.Broadcast(SubtitleStruct.Subtitle,SubtitleStruct.Duration);
}

void UESSubtitleSubsystem::ExecuteExtendedSubtitleAttachedComponent(const UObject* WorldContextObject,const FString SubtitleKey,  USceneComponent* SceneComponent)
{
	FExtendedSubtitle SubtitleStruct = GetSubtitleJSon(SubtitleKey);
	if (GetExtendedSubtitleSound(SubtitleKey,SubtitleStruct) && WorldContextObject)
	{
		UGameplayStatics::SpawnSoundAttached(SubtitleStruct.SubtitleSound,SceneComponent);
	}
	OnExecuteSubtitle.Broadcast(SubtitleStruct.Subtitle,SubtitleStruct.Duration);
}






bool UESSubtitleSubsystem::GetExtendedSubtitleSound(const FString SubtitleKey, FExtendedSubtitle& SubtitleStruct)
{
	if (SESubtitleLanguage.LanguageSubtitleDataTable)
		if (auto i = SESubtitleLanguage.LanguageSubtitleDataTable->FindRow<FExtendedSubtitle>(*SubtitleKey,""))
		{
			SubtitleStruct.SubtitleSound =  i->SubtitleSound;
			return true;
		}
	return false;
}





void UESSubtitleSubsystem::FillExtendedSubtitleDataTable(UDataTable* DataTable , const TEnumAsByte<EFProjectDirectory> LanguageAssetProjectDirectory , const FString AssetDirectory )
{
	bool IsSuccess;
	const FExtendedJson ExtendedJson = UEBJsonLibrary::ReadJsonFile(LanguageAssetProjectDirectory , AssetDirectory ,IsSuccess);

	if (IsSuccess && DataTable)
	{
		TArray<FName> AllRows = DataTable->GetRowNames();

		for (const auto i : ExtendedJson.JsonObject->Values)
		{
			const TSharedPtr<FJsonObject>* OutObjectField;
			if (ExtendedJson.GetExtendedObjectField(i.Key,OutObjectField))
			{
				FExtendedSubtitle SubtitleStruct;
				SubtitleStruct.Subtitle = OutObjectField->Get()->GetStringField("Subtitle");
				SubtitleStruct.Duration = OutObjectField->Get()->GetNumberField("Duration");
				SubtitleStruct.SubtitleSound = nullptr;
	
				if (const auto row = DataTable->FindRow<FExtendedSubtitle>(*i.Key,""))
				{
					SubtitleStruct.SubtitleSound = row->SubtitleSound;
					DataTable->RemoveRow(*i.Key);
					AllRows.Remove(*i.Key);
				}
				DataTable->AddRow(*i.Key,SubtitleStruct);
			}
		}
		for (const auto i : AllRows)
		{
			DataTable->RemoveRow(i);
		}
	}
	
}




FExtendedSubtitle UESSubtitleSubsystem::GetSubtitleJSon(FString FieldName)
{
	bool IsSuccess;
	FExtendedSubtitle OutSubtitle = FExtendedSubtitle();
	const FExtendedJson ExtendedJson = UEBJsonLibrary::ReadJsonFile(SESubtitleLanguage.LanguageAssetProjectDirectory , SESubtitleLanguage.AssetDirectory  ,IsSuccess);
	if (IsSuccess)
	{
		FExtendedJson ExtendedSubtitleJson;
		UEBJsonLibrary::GetExtendedObjectField(IsSuccess , ExtendedJson , FieldName , ExtendedSubtitleJson);

		if (IsSuccess)
		{
			UEBJsonLibrary::GetExtendedStringField(IsSuccess ,ExtendedSubtitleJson , "Subtitle" , OutSubtitle.Subtitle );
			UEBJsonLibrary::GetExtendedFloatField(IsSuccess ,ExtendedSubtitleJson,"Duration",OutSubtitle.Duration );
			return OutSubtitle;
		}
	}
	return OutSubtitle;
}






