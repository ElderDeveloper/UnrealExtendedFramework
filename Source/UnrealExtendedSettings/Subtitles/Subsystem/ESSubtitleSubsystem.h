// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedSettings/Subtitles/Data/ESSubtitleData.h"
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

	UPROPERTY(Config , EditAnywhere ,  Category="Subtitles")
	bool bUseBorder = true;

	UPROPERTY(Config , EditAnywhere ,  Category="Subtitles")
	bool UseLetterCountAsDuration = false;

	UPROPERTY(Config , EditAnywhere ,  Category="Subtitles")
	float TimeForEachLetterCount = 0.1;

	UPROPERTY(Config , EditAnywhere ,  Category="Subtitles")
	float TimeForAfterLetterCount = 1;

	UPROPERTY(Config , EditAnywhere ,  Category="Subtitles")
	bool AnimateSubtitleLetters = false;
	
	UFUNCTION(BlueprintCallable)
	void SaveExtendedLanguageWithIndex(int32 LanguageIndex);

	UFUNCTION(BlueprintCallable)
	void SaveExtendedLanguageWithTag(FGameplayTag LanguageTag);

	// Triggers the Subtitle Event , OnExecuteSubtitle will be triggered , Audio Will Be Played On 2D Sound 
	UFUNCTION(BlueprintCallable ,meta=(WorldContext = WorldContextObject) , Category="Extended Settings | Subtitle")
	static void ExecuteExtendedSubtitle(const UObject* WorldContextObject , const FString SubtitleKey);

	// Triggers the Subtitle Event , OnExecuteSubtitle will be triggered , Audio Will Be Played On The Given Location
	UFUNCTION(BlueprintCallable ,meta=(WorldContext = WorldContextObject) , Category="Extended Settings | Subtitle")
	static void ExecuteExtendedSubtitleLocation(const UObject* WorldContextObject ,const FString SubtitleKey , const FVector Location);

	// Triggers the Subtitle Event , OnExecuteSubtitle will be triggered , Audio Will Be Played On The Given Component
	UFUNCTION(BlueprintCallable ,meta=(WorldContext = WorldContextObject) , Category="Extended Settings | Subtitle")
	static void ExecuteExtendedSubtitleAttachedComponent(const UObject* WorldContextObject ,const FString SubtitleKey , USceneComponent* SceneComponent);
	
	UFUNCTION(BlueprintCallable, Category="Extended Settings | Subtitle")
	static void FillExtendedSubtitleDataTable(UDataTable* DataTable ,const EFProjectDirectory LanguageAssetProjectDirectory = EFProjectDirectory::ProjectContentDir , const FString AssetDirectory = "" );
	
	UPROPERTY(BlueprintAssignable)
	FOnExecuteSubtitle OnExecuteSubtitle;

private:
	
	FExtendedSubtitleLanguageSettings ESLanguage;
	const FString SESubtitleSaveSlot = "ExtendedSubtitleSave";
	
	static bool GetExtendedSubtitleSound(const UObject* WorldContextObject,const FString SubtitleKey, FExtendedSubtitle& SubtitleStruct);
	static void GetSubtitleJSon(const UObject* WorldContextObject , FString FieldName , FExtendedSubtitle& SubtitleStruct);
	
	void LoadLanguage();
	bool GetSubtitleSettingsFromIndex(const int32 Index, FExtendedSubtitleLanguageSettings& Settings);
	
	void SaveExist();
	void SaveNotExist();
	
	bool CheckSavesTheSame(FExtendedSubtitleLanguageSettings CheckLanguage) const;
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	
public: //Access

	UFUNCTION(BlueprintPure)
	FExtendedSubtitleLanguageSettings GetExtendedSubtitleLanguage() { return ESLanguage; }

	UFUNCTION(BlueprintPure)
	TMap<FGameplayTag , FExtendedSubtitleLanguageSettings> GetExtendedSubtitleLanguages() const { return ExtendedSubtitleLanguages; }
	
};

