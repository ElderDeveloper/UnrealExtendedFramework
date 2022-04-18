// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedSettings/Subtitles/ESSubtitleData.h"
#include "ESSubtitleSubsystem.generated.h"

class AESSubtitleAsset;

static FExtendedSubtitleLanguageSettings SESubtitleLanguage;
static int32 SESubtitleIndex = 0;
static const FString SESubtitleSaveSlot = "ExtendedSubtitleSave";
static TMap<FGameplayTag , FExtendedSubtitleLanguageSettings > SESubtitleLanguages;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExecuteSubtitle , FString , Subtitle , float , Duration );


static FOnExecuteSubtitle OnExecuteSubtitle;

UCLASS(Config = DefaultSubtitlePlugin)
class UNREALEXTENDEDSETTINGS_API UESSubtitleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	
	UPROPERTY(Config , EditAnywhere ,  Category="Subtitles")
	TMap<FGameplayTag , FExtendedSubtitleLanguageSettings > ExtendedSubtitleLanguages;
	
	
	
	UFUNCTION(BlueprintCallable)
	void SaveExtendedLanguage(int32 LanguageIndex);
	


	UFUNCTION(BlueprintCallable ,meta=(WorldContext = WorldContextObject) , Category="Extended Settings | Subtitle")
	static void ExecuteExtendedSubtitle(const UObject* WorldContextObject , const FString SubtitleKey);

	UFUNCTION(BlueprintCallable ,meta=(WorldContext = WorldContextObject) , Category="Extended Settings | Subtitle")
	static void ExecuteExtendedSubtitleLocation(const UObject* WorldContextObject ,const FString SubtitleKey , const FVector Location);

	UFUNCTION(BlueprintCallable ,meta=(WorldContext = WorldContextObject) , Category="Extended Settings | Subtitle")
	static void ExecuteExtendedSubtitleAttachedComponent(const UObject* WorldContextObject ,const FString SubtitleKey , USceneComponent* SceneComponent);

	

	UFUNCTION(BlueprintCallable, Category="Extended Settings | Subtitle")
	static void FillExtendedSubtitleDataTable(UDataTable* DataTable ,const TEnumAsByte<EFProjectDirectory> LanguageAssetProjectDirectory = EFProjectDirectory::UEF_ProjectContentDir , const FString AssetDirectory = "" );
	
	
	static bool GetExtendedSubtitleSound(const FString SubtitleKey, FExtendedSubtitle& SubtitleStruct);
	
	static FExtendedSubtitle GetSubtitleJSon( FString FieldName);

	
private:
	
	static void LoadLanguage();


	
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	
public: //Access

	UFUNCTION(BlueprintPure)
	static FExtendedSubtitleLanguageSettings GetExtendedSubtitleLanguage() { return SESubtitleLanguage; }

	UFUNCTION(BlueprintPure)
	TMap<FGameplayTag , FExtendedSubtitleLanguageSettings> GetExtendedSubtitleLanguages() const { return ExtendedSubtitleLanguages; }
	
	
};

