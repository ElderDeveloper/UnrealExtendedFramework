// Fill out your copyright notice in the Description page of Project Settings.


#include "ESSubtitleSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "UnrealExtendedBackend/JSon/Library/EBJsonLibrary.h"
#include "UnrealExtendedFramework/Data/EFMacro.h"
#include "UnrealExtendedSettings/Subtitles/Asset/ESSubtitleLanguageSave.h"


DEFINE_LOG_CATEGORY(LogExtendedSubtitle);
DEFINE_LOG_CATEGORY(LogExtendedSubtitleError);
DEFINE_LOG_CATEGORY(LogExtendedSubtitleWarning);


void UESSubtitleSubsystem::SaveExtendedLanguage(int32 LanguageIndex)
{
	if(const auto SubtitleSave = UGameplayStatics::LoadGameFromSlot(SESubtitleSaveSlot,0))
	{
		if (const auto Save = Cast<UESSubtitleLanguageSave>(SubtitleSave))
		{
			if (GetSubtitleSettingsFromIndex(LanguageIndex , ESLanguage))
			{
				Save->CurrentLanguage = ESLanguage;
				UE_LOG(LogExtendedSubtitle , Log , TEXT("UESSubtitleSubsystem: Save Compleate , New Direction = %s ") , *Save->CurrentLanguage.AssetDirectory)
				UGameplayStatics::SaveGameToSlot(Save,SESubtitleSaveSlot,0);
			}
			ELSE_LOG(LogBlueprint,Error,TEXT("Extended Subtitle Subystem: Save Requested With Invalid Key Index"));
		}
	}
}





void UESSubtitleSubsystem::LoadLanguage()
{
	if(UGameplayStatics::DoesSaveGameExist(SESubtitleSaveSlot,0))
	{
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle,this,&UESSubtitleSubsystem::SaveExist,0.3,false);
	}
	else
		SaveNotExist();
}




void UESSubtitleSubsystem::SaveExist()
{
	UE_LOG(LogExtendedSubtitle , Log , TEXT("UESSubtitleSubsystem: Save Exist"));
		
	if(const auto SubtitleSave = UGameplayStatics::LoadGameFromSlot(SESubtitleSaveSlot,0))
	{
		if (const auto Save = Cast<UESSubtitleLanguageSave>(SubtitleSave))
		{
			ESLanguage = Save->CurrentLanguage;
			UE_LOG(LogExtendedSubtitle , Log , TEXT("UESSubtitleSubsystem: Load Compleate : New Direction = %s") , *Save->CurrentLanguage.AssetDirectory);
		}
	}
}




void UESSubtitleSubsystem::SaveNotExist()
{
	UE_LOG(LogExtendedSubtitle , Log , TEXT("UESSubtitleSubsystem: Save Not Exist"));
	
	if (const auto SubtitleSave = UGameplayStatics::CreateSaveGameObject(UESSubtitleLanguageSave::StaticClass()))
	{
		if (const auto Save = Cast<UESSubtitleLanguageSave>(SubtitleSave))
		{
			if (GetSubtitleSettingsFromIndex(0,ESLanguage))
			{
				Save->CurrentLanguage = ESLanguage;
				UE_LOG(LogExtendedSubtitle , Log , TEXT("UESSubtitleSubsystem: Load Type Create Save : New Direction = %s") , *Save->CurrentLanguage.AssetDirectory);
				UGameplayStatics::SaveGameToSlot(Save,SESubtitleSaveSlot,0);
			}
		}
	}
}






bool UESSubtitleSubsystem::GetSubtitleSettingsFromIndex(const int32 Index , FExtendedSubtitleLanguageSettings& Settings)
{
	TArray<FGameplayTag> Keys;
	ExtendedSubtitleLanguages.GetKeys(Keys);
	if (Keys.IsValidIndex(Index))
	{
		Settings = ExtendedSubtitleLanguages[Keys[Index]];
		return true;
	}
	return false;
}




void UESSubtitleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	if (ExtendedSubtitleLanguages.Num() == 0)
		ExtendedSubtitleLanguages.Add(FGameplayTag::EmptyTag ,FExtendedSubtitleLanguageSettings());
	
	LoadLanguage();
}



void UESSubtitleSubsystem::ExecuteExtendedSubtitle(const UObject* WorldContextObject ,const FString SubtitleKey)
{
	FExtendedSubtitle SubtitleStruct ;
	GetSubtitleJSon(WorldContextObject,SubtitleKey , SubtitleStruct);
	
	
	if (GetExtendedSubtitleSound(WorldContextObject,SubtitleKey,SubtitleStruct) && WorldContextObject)
		UGameplayStatics::PlaySound2D(WorldContextObject,SubtitleStruct.SubtitleSound,1);

	
	if (WorldContextObject)
	{
		if (WorldContextObject->GetWorld())
		{
			if (const auto Subsystem = WorldContextObject->GetWorld()->GetGameInstance()->GetSubsystem<UESSubtitleSubsystem>())
				Subsystem->OnExecuteSubtitle.Broadcast(SubtitleStruct.Subtitle,SubtitleStruct.Duration , Subsystem->UseLetterCountAsDuration , Subsystem->TimeForEachLetterCount , Subsystem->TimeForAfterLetterCount , Subsystem->AnimateSubtitleLetters);
		}
	}
	
}




void UESSubtitleSubsystem::ExecuteExtendedSubtitleLocation(const UObject* WorldContextObject ,const FString SubtitleKey, const FVector Location)
{
	FExtendedSubtitle SubtitleStruct ;
	GetSubtitleJSon(WorldContextObject,SubtitleKey , SubtitleStruct);
	
	if (GetExtendedSubtitleSound(WorldContextObject,SubtitleKey,SubtitleStruct) && WorldContextObject)
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContextObject,SubtitleStruct.SubtitleSound,Location);
	}
	if (WorldContextObject)
	{
		if (WorldContextObject->GetWorld())
		{
			if (const auto Subsystem = WorldContextObject->GetWorld()->GetGameInstance()->GetSubsystem<UESSubtitleSubsystem>())
				Subsystem->OnExecuteSubtitle.Broadcast(SubtitleStruct.Subtitle,SubtitleStruct.Duration , Subsystem->UseLetterCountAsDuration , Subsystem->TimeForEachLetterCount , Subsystem->TimeForAfterLetterCount , Subsystem->AnimateSubtitleLetters);
		}
	}
}



void UESSubtitleSubsystem::ExecuteExtendedSubtitleAttachedComponent(const UObject* WorldContextObject,const FString SubtitleKey,  USceneComponent* SceneComponent)
{
	FExtendedSubtitle SubtitleStruct ;
	GetSubtitleJSon(WorldContextObject,SubtitleKey , SubtitleStruct);
	
	if (GetExtendedSubtitleSound(WorldContextObject,SubtitleKey,SubtitleStruct) && WorldContextObject)
	{
		UGameplayStatics::SpawnSoundAttached(SubtitleStruct.SubtitleSound,SceneComponent);
	}
	
	if (WorldContextObject)
	{
		if (WorldContextObject->GetWorld())
		{
			if (const auto Subsystem = WorldContextObject->GetWorld()->GetGameInstance()->GetSubsystem<UESSubtitleSubsystem>())
				Subsystem->OnExecuteSubtitle.Broadcast(SubtitleStruct.Subtitle,SubtitleStruct.Duration , Subsystem->UseLetterCountAsDuration , Subsystem->TimeForEachLetterCount , Subsystem->TimeForAfterLetterCount , Subsystem->AnimateSubtitleLetters);
		}
	}
}






bool UESSubtitleSubsystem::GetExtendedSubtitleSound(const UObject* WorldContextObject,const FString SubtitleKey, FExtendedSubtitle& SubtitleStruct)
{
	
	if (WorldContextObject)
	{
		if (WorldContextObject->GetWorld())
		{
			if (const auto Subsystem = WorldContextObject->GetWorld()->GetGameInstance()->GetSubsystem<UESSubtitleSubsystem>())
			{
				if (Subsystem->ESLanguage.LanguageSubtitleDataTable)
				{
					if (auto i = Subsystem->ESLanguage.LanguageSubtitleDataTable->FindRow<FExtendedSubtitle>(*SubtitleKey,""))
					{
						SubtitleStruct.SubtitleSound =  i->SubtitleSound;
						return true;
					}
				}
			}
		}
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
					if(row->SubtitleSound)
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



void UESSubtitleSubsystem::GetSubtitleJSon(const UObject* WorldContextObject ,FString FieldName, FExtendedSubtitle& SubtitleStruct)
{
	bool IsSuccess;

	if (WorldContextObject)
	{
		if (const auto Subsystem = WorldContextObject->GetWorld()->GetGameInstance()->GetSubsystem<UESSubtitleSubsystem>())
		{
			const FExtendedJson ExtendedJson = UEBJsonLibrary::ReadJsonFile(Subsystem->ESLanguage.LanguageAssetProjectDirectory , Subsystem->ESLanguage.AssetDirectory  ,IsSuccess);
	
			if (IsSuccess)
			{
				FExtendedJson ExtendedSubtitleJson;
				UEBJsonLibrary::GetExtendedObjectField(IsSuccess , ExtendedJson , FieldName , ExtendedSubtitleJson);

				if (IsSuccess)
				{
					UEBJsonLibrary::GetExtendedStringField(IsSuccess ,ExtendedSubtitleJson , "Subtitle" , SubtitleStruct.Subtitle );
					UEBJsonLibrary::GetExtendedFloatField(IsSuccess ,ExtendedSubtitleJson,"Duration",SubtitleStruct.Duration );
				}
			}
			else
			{
				UE_LOG(LogBlueprint , Error,TEXT("Read JSon File Failed"));
			}
		}
	}
}




