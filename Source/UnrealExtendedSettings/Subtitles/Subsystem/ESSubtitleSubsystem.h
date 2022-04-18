// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedSettings/Subtitles/ESSubtitleData.h"
#include "ESSubtitleSubsystem.generated.h"

class AESSubtitleAsset;





DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExecuteSubtitle , FString , Subtitle , float , Duration );

DECLARE_LOG_CATEGORY_EXTERN(LogExtendedSubtitle, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogExtendedSubtitleError, Error, All);
DECLARE_LOG_CATEGORY_EXTERN(LogExtendedSubtitleWarning, Warning, All);

UCLASS(Config = "DefaultSubtitlePlugin")
class UNREALEXTENDEDSETTINGS_API UESSubtitleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	
	UPROPERTY(Config , EditAnywhere ,  Category="Subtitles")
	TMap<FGameplayTag , FExtendedSubtitleLanguageSettings > ExtendedSubtitleLanguages;

	FExtendedSubtitleLanguageSettings ESLanguage;
	
	
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


	UPROPERTY(BlueprintAssignable)
	FOnExecuteSubtitle OnExecuteSubtitle;


	

private:

	const FString SESubtitleSaveSlot = "ExtendedSubtitleSave";

	
	static bool GetExtendedSubtitleSound(const UObject* WorldContextObject,const FString SubtitleKey, FExtendedSubtitle& SubtitleStruct);
	static void GetSubtitleJSon(const UObject* WorldContextObject , FString FieldName , FExtendedSubtitle& SubtitleStruct);
	
	
	void LoadLanguage();
	bool GetSubtitleSettingsFromIndex(const int32 Index, FExtendedSubtitleLanguageSettings& Settings);

	void SaveExist();
	void SaveNotExist();	
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	
public: //Access

	UFUNCTION(BlueprintPure)
	FExtendedSubtitleLanguageSettings GetExtendedSubtitleLanguage() { return ESLanguage; }

	UFUNCTION(BlueprintPure)
	TMap<FGameplayTag , FExtendedSubtitleLanguageSettings> GetExtendedSubtitleLanguages() const { return ExtendedSubtitleLanguages; }


	
};

