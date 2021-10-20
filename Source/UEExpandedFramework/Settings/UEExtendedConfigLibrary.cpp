// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedConfigLibrary.h"




UUEExtendedConfigLibrary::UUEExtendedConfigLibrary( const class FObjectInitializer& ObjectInitializer ) {

}

static FDateTime GetFileTimeStamp( const FString& File ) {
	return FPlatformFileManager::Get().GetPlatformFile().GetTimeStamp( *File );
}
static void SetTimeStamp( const FString& File, const FDateTime& TimeStamp ) {
	FPlatformFileManager::Get().GetPlatformFile().SetTimeStamp( *File, TimeStamp );
}

static bool FileExists( const FString& File ) {
	return FPlatformFileManager::Get().GetPlatformFile().FileExists( *File );
}
static bool FolderExists( const FString& Dir ) {
	return FPlatformFileManager::Get().GetPlatformFile().DirectoryExists( *Dir );
}


bool UUEExtendedConfigLibrary::GetConfigBool( const FString SectionName, const FString VariableName, 
									  const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError ) {
	if ( !GConfig ) return false;

	bool value = false;
	const bool error = false;
	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			GConfig->GetBool( *SectionName, *VariableName, value, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			GConfig->GetBool( *SectionName, *VariableName, value, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			GConfig->GetBool( *SectionName, *VariableName, value, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			GConfig->GetBool( *SectionName, *VariableName, value, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			GConfig->GetBool( *SectionName, *VariableName, value, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			GConfig->GetBool( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			GConfig->GetBool( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			break;
	}
	ReadError = error;
	return value;
}


uint8 UUEExtendedConfigLibrary::GetConfigByte( const FString SectionName, const FString VariableName, 
									   const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError ) {
	if ( !GConfig ) return 0;

	// PR GetByte
	int32 value = 0;
	bool error = false;
	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			error = GConfig->GetInt( *SectionName, *VariableName, value, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			error = GConfig->GetInt( *SectionName, *VariableName, value, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			error = GConfig->GetInt( *SectionName, *VariableName, value, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			error = GConfig->GetInt( *SectionName, *VariableName, value, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			error = GConfig->GetInt( *SectionName, *VariableName, value, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			error = GConfig->GetInt( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			error = GConfig->GetInt( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			break;
	}
	ReadError = error;
	return ( value < 0 ) ? 0 : ( value > sizeof( uint8 ) ) ? sizeof( uint8 ) : value;
}

int32 UUEExtendedConfigLibrary::GetConfigInteger( const FString SectionName, const FString VariableName, 
										  const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError ) {
	if ( !GConfig ) return 0;

	int32 value = 0;
	bool error = false;
	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			error = GConfig->GetInt( *SectionName, *VariableName, value, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			error = GConfig->GetInt( *SectionName, *VariableName, value, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			error = GConfig->GetInt( *SectionName, *VariableName, value, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			error = GConfig->GetInt( *SectionName, *VariableName, value, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			error = GConfig->GetInt( *SectionName, *VariableName, value, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			error = GConfig->GetInt( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			error = GConfig->GetInt( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			break;
	}
	ReadError = error;
	return value;
}

float UUEExtendedConfigLibrary::GetConfigFloat( const FString SectionName, const FString VariableName, 
										const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError ) {
	if ( !GConfig ) return 0;

	float value = 0.0f;
	bool error = false;
	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			error = GConfig->GetFloat( *SectionName, *VariableName, value, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			error = GConfig->GetFloat( *SectionName, *VariableName, value, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			error = GConfig->GetFloat( *SectionName, *VariableName, value, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			error = GConfig->GetFloat( *SectionName, *VariableName, value, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			error = GConfig->GetFloat( *SectionName, *VariableName, value, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			error = GConfig->GetFloat( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			error = GConfig->GetFloat( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			break;
	}
	ReadError = error;
	return value;
}

FVector2D UUEExtendedConfigLibrary::GetConfigVector2D( const FString SectionName, const FString VariableName, 
											   const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError ) {
	if ( !GConfig ) return FVector2D::ZeroVector;

	FVector2D value = FVector2D::ZeroVector;
	bool error = false;
	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			error = GConfig->GetVector2D( *SectionName, *VariableName, value, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			error = GConfig->GetVector2D( *SectionName, *VariableName, value, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			error = GConfig->GetVector2D( *SectionName, *VariableName, value, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			error = GConfig->GetVector2D( *SectionName, *VariableName, value, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			error = GConfig->GetVector2D( *SectionName, *VariableName, value, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			error = GConfig->GetVector2D( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			error = GConfig->GetVector2D( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			break;
	}
	ReadError = error;
	return FVector2D( value.X, value.Y );
}

FVector UUEExtendedConfigLibrary::GetConfigVector( const FString SectionName, const FString VariableName, 
										   const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError ) {
	if ( !GConfig ) return FVector::ZeroVector;

	FVector value = FVector::ZeroVector;
	bool error = false;
	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			error = GConfig->GetVector( *SectionName, *VariableName, value, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			error = GConfig->GetVector( *SectionName, *VariableName, value, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			error = GConfig->GetVector( *SectionName, *VariableName, value, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			error = GConfig->GetVector( *SectionName, *VariableName, value, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			error = GConfig->GetVector( *SectionName, *VariableName, value, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			error = GConfig->GetVector( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			error = GConfig->GetVector( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			break;

	}
	ReadError = error;
	return value;
}


FVector4 UUEExtendedConfigLibrary::GetConfigVector4( const FString SectionName, const FString VariableName, 
											 const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError ) {
	if ( !GConfig ) return FVector4( 0.f, 0.f, 0.f, 0.f );

	// PR FVector4::ZeroVector
	FVector4 value = FVector4( 0.f, 0.f, 0.f, 0.f );
	bool error = false;
	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			error = GConfig->GetVector4( *SectionName, *VariableName, value, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			error = GConfig->GetVector4( *SectionName, *VariableName, value, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			error = GConfig->GetVector4( *SectionName, *VariableName, value, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			error = GConfig->GetVector4( *SectionName, *VariableName, value, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			error = GConfig->GetVector4( *SectionName, *VariableName, value, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			error = GConfig->GetVector4( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			error = GConfig->GetVector4( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			break;
	}
	ReadError = error;
	return value;
}

FRotator UUEExtendedConfigLibrary::GetConfigRotator( const FString SectionName, const FString VariableName, 
											 const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError ) {
	if ( !GConfig ) return FRotator::ZeroRotator;

	FRotator value = FRotator::ZeroRotator;
	bool error = false;
	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			error = GConfig->GetRotator( *SectionName, *VariableName, value, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			error = GConfig->GetRotator( *SectionName, *VariableName, value, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			error = GConfig->GetRotator( *SectionName, *VariableName, value, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			error = GConfig->GetRotator( *SectionName, *VariableName, value, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			error = GConfig->GetRotator( *SectionName, *VariableName, value, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			error = GConfig->GetRotator( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			error = GConfig->GetRotator( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			break;
	}
	ReadError = error;
	return value;
}

FLinearColor UUEExtendedConfigLibrary::GetConfigColor( const FString SectionName, const FString VariableName, 
											   const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError ) {
	if ( !GConfig ) return FColor::Black;

	FColor value = FColor::Black;
	bool error = false;
	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			error = GConfig->GetColor( *SectionName, *VariableName, value, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			error = GConfig->GetColor( *SectionName, *VariableName, value, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			error = GConfig->GetColor( *SectionName, *VariableName, value, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			error = GConfig->GetColor( *SectionName, *VariableName, value, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			GConfig->GetColor( *SectionName, *VariableName, value, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			error = GConfig->GetColor( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			error = GConfig->GetColor( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			break;
	}
	ReadError = error;
	return FLinearColor( value );
}

FString UUEExtendedConfigLibrary::GetConfigString( const FString SectionName, const FString VariableName, 
										   const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError ) {
	if ( !GConfig ) return "";

	FString value = "";
	bool error = false;
	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			error = GConfig->GetString( *SectionName, *VariableName, value, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			error = GConfig->GetString( *SectionName, *VariableName, value, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			error = GConfig->GetString( *SectionName, *VariableName, value, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			error = GConfig->GetString( *SectionName, *VariableName, value, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			error = GConfig->GetString( *SectionName, *VariableName, value, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			error = GConfig->GetString( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			error = GConfig->GetString( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			break;
	}
	ReadError = error;
	return value;
}

FText UUEExtendedConfigLibrary::GetConfigText( const FString SectionName, const FString VariableName,
										   const EExtendedFilesList INIFile, const int32 ProfileIndex, bool &ReadError ) {
	if ( !GConfig ) return FText::GetEmpty();

	FText value = FText::GetEmpty();
	bool error = false;
	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			error = GConfig->GetText( *SectionName, *VariableName, value, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			error = GConfig->GetText( *SectionName, *VariableName, value, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			error = GConfig->GetText( *SectionName, *VariableName, value, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			error = GConfig->GetText( *SectionName, *VariableName, value, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			error = GConfig->GetText( *SectionName, *VariableName, value, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			error = GConfig->GetText( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->Flush( true, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			error = GConfig->GetText( *SectionName, *VariableName, value, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings" + FString::FromInt( ProfileIndex ) + ".cfg" ) ) );
			break;
	}
	ReadError = error;
	return value;
}



TArray<FString> UUEExtendedConfigLibrary::GetFilesByExtension( const FString _Extension, const EExtendedFilesDirList _Directory, const FString _SubDirectory, const bool _Recursive, bool &_Empty ) {
	TArray<FString> save = TArray<FString>();
	TArray<FString> save_Slots = TArray<FString>();

	const bool error = false;
	FString dir = "";

	switch ( _Directory ) {
		case EExtendedFilesDirList::Game:
			dir = FPaths::ConvertRelativePathToFull( FPaths::ProjectDir() );
			break;
		case EExtendedFilesDirList::Plugin:
			dir = FPaths::ConvertRelativePathToFull( FPaths::ProjectPluginsDir() );
			break;
		case EExtendedFilesDirList::Config:
			dir = FPaths::ConvertRelativePathToFull( FPaths::ProjectConfigDir() );
			break;
		case EExtendedFilesDirList::User:
			dir = FPaths::ConvertRelativePathToFull( FPaths::ProjectUserDir() );
			break;
	}
	dir = dir + _SubDirectory;

	FPaths::NormalizeDirectoryName( dir );
	const FString ext = _Extension == "" ? "*" : _Extension;
	const FString extComp = "*." + ext;
	const TCHAR* charExt = *extComp;
	//FText textVariable = FText::AsCultureInvariant( "*." + _Extension );
	if ( _Recursive ) {
		IFileManager::Get().FindFilesRecursive( save, *dir, charExt, true, false, true );
	} else {
		IFileManager::Get().FindFiles( save, *dir, charExt );
	}
	//IFileManager::Get().FindFilesRecursive( save, *dir, TEXT( textVariable ), true, true, true );

	const int filesCount = save.Num();
	if ( filesCount <= 0 ) {
		_Empty = true;
		return save_Slots;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	for ( int32 i = 0; i < filesCount; ++i ) {
		save_Slots.Add( FPaths::GetBaseFilename( save[i] ) );
	}

#else

	for ( int32 i = 0; i < filesCount; ++i ) {
		save_Slots.Add( FPaths::GetBaseFilename( save[i] ) );
	}

#endif

	_Empty = error;
	return save_Slots;
}



void UUEExtendedConfigLibrary::SetConfigBool( const FString SectionName, const FString VariableName, const bool BoolValue, const EExtendedFilesList INIFile ) {
	if ( !GConfig ) return;
	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			GConfig->SetBool( *SectionName, *VariableName, BoolValue, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			GConfig->SetBool( *SectionName, *VariableName, BoolValue, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			GConfig->SetBool( *SectionName, *VariableName, BoolValue, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			GConfig->SetBool( *SectionName, *VariableName, BoolValue, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			GConfig->SetBool( *SectionName, *VariableName, BoolValue, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->SetBool( *SectionName, *VariableName, BoolValue, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->SetBool( *SectionName, *VariableName, BoolValue, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			break;
	}

}



void UUEExtendedConfigLibrary::SetConfigByte( const FString SectionName, const FString VariableName, const uint8 ByteValue, const EExtendedFilesList INIFile ) {
	if ( !GConfig ) return;

	// PR SetByte
	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			GConfig->SetInt( *SectionName, *VariableName, ByteValue, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			GConfig->SetInt( *SectionName, *VariableName, ByteValue, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			GConfig->SetInt( *SectionName, *VariableName, ByteValue, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			GConfig->SetInt( *SectionName, *VariableName, ByteValue, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			GConfig->SetInt( *SectionName, *VariableName, ByteValue, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->SetInt( *SectionName, *VariableName, ByteValue, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->SetInt( *SectionName, *VariableName, ByteValue, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			break;
	}
}

void UUEExtendedConfigLibrary::SetConfigInteger( const FString SectionName, const FString VariableName, const int32 IntValue, const EExtendedFilesList INIFile ) {
	if ( !GConfig ) return;

	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			GConfig->SetInt( *SectionName, *VariableName, IntValue, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			GConfig->SetInt( *SectionName, *VariableName, IntValue, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			GConfig->SetInt( *SectionName, *VariableName, IntValue, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			GConfig->SetInt( *SectionName, *VariableName, IntValue, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			GConfig->SetInt( *SectionName, *VariableName, IntValue, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->SetInt( *SectionName, *VariableName, IntValue, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->SetInt( *SectionName, *VariableName, IntValue, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			break;
	}
}

void UUEExtendedConfigLibrary::SetConfigFloat( const FString SectionName, const FString VariableName, const float FloatValue, const EExtendedFilesList INIFile ) {
	if ( !GConfig ) return;

	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			GConfig->SetFloat( *SectionName, *VariableName, FloatValue, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			GConfig->SetFloat( *SectionName, *VariableName, FloatValue, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			GConfig->SetFloat( *SectionName, *VariableName, FloatValue, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			GConfig->SetFloat( *SectionName, *VariableName, FloatValue, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			GConfig->SetFloat( *SectionName, *VariableName, FloatValue, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->SetFloat( *SectionName, *VariableName, FloatValue, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->SetFloat( *SectionName, *VariableName, FloatValue, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			break;
	}
}

void UUEExtendedConfigLibrary::SetConfigVector2D( const FString SectionName, const FString VariableName, const FVector2D Vec2Value, const EExtendedFilesList INIFile ) {
	if ( !GConfig ) return;

	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			GConfig->SetVector2D( *SectionName, *VariableName, Vec2Value, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			GConfig->SetVector2D( *SectionName, *VariableName, Vec2Value, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			GConfig->SetVector2D( *SectionName, *VariableName, Vec2Value, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			GConfig->SetVector2D( *SectionName, *VariableName, Vec2Value, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			GConfig->SetVector2D( *SectionName, *VariableName, Vec2Value, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->SetVector2D( *SectionName, *VariableName, Vec2Value, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->SetVector2D( *SectionName, *VariableName, Vec2Value, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			break;
	}
}

void UUEExtendedConfigLibrary::SetConfigVector( const FString SectionName, const FString VariableName, const FVector VecValue, const EExtendedFilesList INIFile ) {
	if ( !GConfig ) return;

	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			GConfig->SetVector( *SectionName, *VariableName, VecValue, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			GConfig->SetVector( *SectionName, *VariableName, VecValue, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			GConfig->SetVector( *SectionName, *VariableName, VecValue, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			GConfig->SetVector( *SectionName, *VariableName, VecValue, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			GConfig->SetVector( *SectionName, *VariableName, VecValue, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->SetVector( *SectionName, *VariableName, VecValue, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->SetVector( *SectionName, *VariableName, VecValue, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			break;
	}
}

void UUEExtendedConfigLibrary::SetConfigVector4( const FString SectionName, const FString VariableName, const FVector4& Vec4Value, const EExtendedFilesList INIFile ) {
	if ( !GConfig ) return;

	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			GConfig->SetVector4( *SectionName, *VariableName, Vec4Value, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			GConfig->SetVector4( *SectionName, *VariableName, Vec4Value, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			GConfig->SetVector4( *SectionName, *VariableName, Vec4Value, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			GConfig->SetVector4( *SectionName, *VariableName, Vec4Value, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			GConfig->SetVector4( *SectionName, *VariableName, Vec4Value, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->SetVector4( *SectionName, *VariableName, Vec4Value, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->SetVector4( *SectionName, *VariableName, Vec4Value, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			break;
	}
}

void UUEExtendedConfigLibrary::SetConfigRotator( const FString SectionName, const FString VariableName, const FRotator RotValue, const EExtendedFilesList INIFile ) {
	if ( !GConfig ) return;

	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			GConfig->SetRotator( *SectionName, *VariableName, RotValue, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			GConfig->SetRotator( *SectionName, *VariableName, RotValue, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			GConfig->SetRotator( *SectionName, *VariableName, RotValue, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			GConfig->SetRotator( *SectionName, *VariableName, RotValue, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			GConfig->SetRotator( *SectionName, *VariableName, RotValue, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->SetRotator( *SectionName, *VariableName, RotValue, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->SetRotator( *SectionName, *VariableName, RotValue, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			break;
	}
}

void UUEExtendedConfigLibrary::SetConfigColor( const FString SectionName, const FString VariableName, const FLinearColor ColorValue, const EExtendedFilesList INIFile ) {
	if ( !GConfig ) return;

	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			GConfig->SetColor( *SectionName, *VariableName, ColorValue.ToFColor( true ), GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			GConfig->SetColor( *SectionName, *VariableName, ColorValue.ToFColor( true ), GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			GConfig->SetColor( *SectionName, *VariableName, ColorValue.ToFColor( true ), GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			GConfig->SetColor( *SectionName, *VariableName, ColorValue.ToFColor( true ), GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			GConfig->SetColor( *SectionName, *VariableName, ColorValue.ToFColor( true ), GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->SetColor( *SectionName, *VariableName, ColorValue.ToFColor( true ), FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->SetColor( *SectionName, *VariableName, ColorValue.ToFColor( true ), FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			break;
	}
}


void UUEExtendedConfigLibrary::SetConfigString( const FString SectionName, const FString VariableName, const FString StrValue, const EExtendedFilesList INIFile ) {
	if ( !GConfig ) return;

	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			GConfig->SetString( *SectionName, *VariableName, *StrValue, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			GConfig->SetString( *SectionName, *VariableName, *StrValue, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			GConfig->SetString( *SectionName, *VariableName, *StrValue, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			GConfig->SetString( *SectionName, *VariableName, *StrValue, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			GConfig->SetString( *SectionName, *VariableName, *StrValue, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->SetString( *SectionName, *VariableName, *StrValue, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->SetString( *SectionName, *VariableName, *StrValue, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			break;
	}
}


/*
void UUEExtendedConfigLibrary::SetConfigText( const FString SectionName, const FString VariableName, const FText TextValue, const EExtendedFilesList INIFile ) {
	if ( !GConfig ) return;

	switch ( INIFile ) {
		case EExtendedFilesList::GGameIni:
			GConfig->SetText( *SectionName, *VariableName, TextValue, GGameIni );
			break;
		case EExtendedFilesList::GGameUserSettingsIni:
			GConfig->SetText( *SectionName, *VariableName, TextValue, GGameUserSettingsIni );
			break;
		case EExtendedFilesList::GScalabilityIni:
			GConfig->SetText( *SectionName, *VariableName, TextValue, GScalabilityIni );
			break;
		case EExtendedFilesList::GInputIni:
			GConfig->SetText( *SectionName, *VariableName, TextValue, GInputIni );
			break;
		case EExtendedFilesList::GEngineIni:
			GConfig->SetText( *SectionName, *VariableName, TextValue, GEngineIni );
			break;
		case EExtendedFilesList::GameSettingsConfig:
			GConfig->SetText( *SectionName, *VariableName, TextValue, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + TEXT( "GameSettings.cfg" ) ) );
			break;
		case EExtendedFilesList::PlayerSettingsConfig:
			GConfig->SetText( *SectionName, *VariableName, TextValue, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			GConfig->Flush( false, FString( FPaths::GeneratedConfigDir() + FString( "PlayerSettings.cfg" ) ) );
			break;
	}

*/