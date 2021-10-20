// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IImageWrapper.h"
#include "Engine.h"
#include "UEExtendedConfigLibrary.generated.h"


UENUM()
enum class EExtendedFilesDirList : uint8 {
	Game 			UMETA( DisplayName = "Game" ),
	Plugin			UMETA( DisplayName = "Plugin" ),
	Config			UMETA( DisplayName = "Config" ),
	User			UMETA( DisplayName = "User" )
};


UENUM()
enum class EExtendedFilesList : uint8 {
	GGameIni 				UMETA( DisplayName = "Game" ),
	GGameUserSettingsIni	UMETA( DisplayName = "User Settings" ),
	GScalabilityIni			UMETA( DisplayName = "Scalability" ),
	GInputIni				UMETA( DisplayName = "Input" ),
	GEngineIni				UMETA( DisplayName = "Engine" ),
	GameSettingsConfig		UMETA( DisplayName = "Game Settings" ),
	PlayerSettingsConfig	UMETA( DisplayName = "Player Settings" )
};


UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedConfigLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UUEExtendedConfigLibrary( const FObjectInitializer& ObjectInitializer );

	public:

	
	// Returns the value of the selected bool from the selected ini config file 
	UFUNCTION( BlueprintPure, Category = "HevLib|IO" )
		static bool GetConfigBool( const FString SectionName, const FString VariableName, const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError );
	
	// Returns the value of the selected uint8 from the selected ini config file 
	UFUNCTION( BlueprintPure, Category = "HevLib|IO" )
		static uint8 GetConfigByte( const FString SectionName, const FString VariableName, const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError );
	
	// Returns the value of the selected int32 from the selected ini config file 
	UFUNCTION( BlueprintPure, Category = "HevLib|IO" )
		static int32 GetConfigInteger( const FString SectionName, const FString VariableName, const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError );

	// Returns the value of the selected float from the selected ini config file 
	UFUNCTION( BlueprintPure, Category = "HevLib|IO" )
		static float GetConfigFloat( const FString SectionName, const FString VariableName, const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError );

	// Returns the value of the selected FVector2D from the selected ini config file 
	UFUNCTION( BlueprintPure, Category = "HevLib|IO" )
		static FVector2D GetConfigVector2D( const FString SectionName, const FString VariableName, const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError );

	// Returns the value of the selected FVector from the selected ini config file 
	UFUNCTION( BlueprintPure, Category = "HevLib|IO" )
		static FVector GetConfigVector( const FString SectionName, const FString VariableName, const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError );
	
	// Returns the value of the selected FVector from the selected ini config file 
	UFUNCTION( BlueprintPure, Category = "HevLib|IO" )
		static FVector4 GetConfigVector4( const FString SectionName, const FString VariableName, const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError );

	// Returns the value of the selected FRotator from the selected ini config file 
	UFUNCTION( BlueprintPure, Category = "HevLib|IO" )
		static FRotator GetConfigRotator( const FString SectionName, const FString VariableName, const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError );

	// Returns the value of the selected FLinearColor from the selected ini config file
	UFUNCTION( BlueprintPure, Category = "HevLib|IO" )
		static FLinearColor GetConfigColor( const FString SectionName, const FString VariableName, const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError );

	// Returns the value of the selected FString from the selected ini config file 
	UFUNCTION( BlueprintPure, Category = "HevLib|IO" )
		static FString GetConfigString( const FString SectionName, const FString VariableName, const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError );

	// Returns the value of the selected FText from the selected ini config file 
	UFUNCTION( BlueprintPure, Category = "HevLib|IO" )
		static FText GetConfigText( const FString SectionName, const FString VariableName, const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError );
	
	// Returns the list of files by extension on the given folder 
	UFUNCTION( BlueprintPure, Category = "HevLib|IO" )
		static TArray<FString> GetFilesByExtension( const FString _Extension, const EExtendedFilesDirList _Directory, const FString _SubDirectory, const bool _Recursive, bool &_Empty );
	
	// Set the value of the selected bool from the selected ini config file 
	UFUNCTION( BlueprintCallable, Category = "HevLib|IO" )
		static void SetConfigBool( const FString SectionName, const FString VariableName, const bool BoolValue, const EExtendedFilesList INIFile );

	// Set the value of the selected uint8 from the selected ini config file 
	UFUNCTION( BlueprintCallable, Category = "HevLib|IO" )
		static void SetConfigByte( const FString SectionName, const FString VariableName, const uint8 ByteValue, const EExtendedFilesList INIFile );
	
	// Set the value of the selected int32 from the selected ini config file 
	UFUNCTION( BlueprintCallable, Category = "HevLib|IO" )
		static void SetConfigInteger( const FString SectionName, const FString VariableName, const int32 IntValue, const EExtendedFilesList INIFile );
	
	// Set the value of the selected float from the selected ini config file 
	UFUNCTION( BlueprintCallable, Category = "HevLib|IO" )
		static void SetConfigFloat( const FString SectionName, const FString VariableName, const float FloatValue, const EExtendedFilesList INIFile );
	
	// Set the value of the selected FVector2D from the selected ini config file 
	UFUNCTION( BlueprintCallable, Category = "HevLib|IO" )
		static void SetConfigVector2D( const FString SectionName, const FString VariableName, const FVector2D Vec2Value, const EExtendedFilesList INIFile );

	// Set the value of the selected FVector from the selected ini config file 
	UFUNCTION( BlueprintCallable, Category = "HevLib|IO" )
		static void SetConfigVector( const FString SectionName, const FString VariableName, const FVector VecValue, const EExtendedFilesList INIFile );
	
	// Set the value of the selected FVector from the selected ini config file 
	UFUNCTION( BlueprintCallable, Category = "HevLib|IO" )
		static void SetConfigVector4( const FString SectionName, const FString VariableName, const FVector4 &Vec4Value, const EExtendedFilesList INIFile );


	
	// Set the value of the selected FRotator from the selected ini config file 
	UFUNCTION( BlueprintCallable, Category = "HevLib|IO" )
		static void SetConfigRotator( const FString SectionName, const FString VariableName, const FRotator RotValue, const EExtendedFilesList INIFile );

	// Set the value of the selected FLinearColor from the selected ini config file 
	UFUNCTION( BlueprintCallable, Category = "HevLib|IO" )
		static void SetConfigColor( const FString SectionName, const FString VariableName, const FLinearColor ColorValue, const EExtendedFilesList INIFile );

	

	// Set the value of the selected FString from the selected ini config file 
	UFUNCTION( BlueprintCallable, Category = "HevLib|IO" )
		static void SetConfigString( const FString SectionName, const FString VariableName, const FString StrValue, const EExtendedFilesList INIFile );
	
	/*
	// Set the value of the selected FText from the selected ini config file 
	UFUNCTION( BlueprintCallable, Category = "HevLib|IO" )
		static void SetConfigText( const FString SectionName, const FString VariableName, const FText TextValue, const EExtendedFilesList INIFile );

	*/
		

};
