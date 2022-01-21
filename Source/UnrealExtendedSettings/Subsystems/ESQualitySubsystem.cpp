// Fill out your copyright notice in the Description page of Project Settings.


#include "ESQualitySubsystem.h"

#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"


void UESQualitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CheckQualitySettingsConfigChanges();
}



TEnumAsByte<EWindowMode::Type> UESQualitySubsystem::GetDisplayMode() const
{
	if (const auto GUS = GEngine->GameUserSettings)
		 return  GUS->GetFullscreenMode();
	return EWindowMode::Type::Fullscreen;
}



void UESQualitySubsystem::SetDisplayMode(TEnumAsByte<EWindowMode::Type> Value, bool ApplyImmediately)
{
	DisplayMode = Value; SaveConfig();
	
	if (const auto GUS = GEngine->GameUserSettings)
		return GUS->SetFullscreenMode(Value);

	ApplyAllQualitySettings(ApplyImmediately);
}

void UESQualitySubsystem::SetDisplayMode(uint8 Value, bool ApplyImmediately)
{
	TEnumAsByte<EWindowMode::Type> ScreenValue = EWindowMode::Fullscreen;
	switch (Value)
	{
		case 0: ScreenValue = EWindowMode::Fullscreen;break;
		case 1: ScreenValue = EWindowMode::WindowedFullscreen;break;
		case 2: ScreenValue = EWindowMode::Windowed;break;
		default: break;
	}

	DisplayMode = ScreenValue; SaveConfig();
	
	if (const auto GUS = GEngine->GameUserSettings)
		return GUS->SetFullscreenMode(ScreenValue);

	ApplyAllQualitySettings(ApplyImmediately);
	
}


FIntPoint UESQualitySubsystem::GetScreenResolution() const
{
	if (const auto GUS = GEngine->GameUserSettings)
		return GUS->GetScreenResolution();
	return -1;
}



void UESQualitySubsystem::SetScreenResolution(FIntPoint Value, bool ApplyImmediately)
{
	ScreenResolution = Value; SaveConfig();
	
	if (const auto GUS = GEngine->GameUserSettings)
		return GUS->SetScreenResolution(Value);

	ApplyAllQualitySettings(ApplyImmediately);
}




int32 UESQualitySubsystem::GetOverallScalabilityLevel() const
{
	if (const auto GUS = GEngine->GameUserSettings)
		return GUS->GetOverallScalabilityLevel();
	return -1;
}



void UESQualitySubsystem::SetOverallScalabilityLevel(int32 Value , bool ApplyImmediately)
{
	OverallScalabilityLevel = Value; SaveConfig();
	
	if (const auto GUS = GEngine->GameUserSettings)
		GUS->SetOverallScalabilityLevel(Value);
	
	ApplyAllQualitySettings(ApplyImmediately);
}






int32 UESQualitySubsystem::GetTextureQuality() const
{
	if (const auto GUS = GEngine->GameUserSettings)
		return GUS->GetTextureQuality();
	return -1;
}



void UESQualitySubsystem::SetTexturesQuality(int32 Value, bool ApplyImmediately)
{
	TextureQuality = Value; SaveConfig();
	
	if (const auto GUS = GEngine->GameUserSettings)
		GUS->SetTextureQuality(Value);	
	
	ApplyAllQualitySettings(ApplyImmediately);
}






int32 UESQualitySubsystem::GetShadowQuality() const
{
	if (const auto GUS = GEngine->GameUserSettings)
		return GUS->GetShadowQuality();
	return -1;
}



void UESQualitySubsystem::SetShadowsQuality(int32 Value, bool ApplyImmediately)
{
	ShadowQuality = Value; SaveConfig();
	
	if (const auto GUS = GEngine->GameUserSettings)
		GUS->SetShadowQuality(Value);
	
	ApplyAllQualitySettings(ApplyImmediately);
}






int32 UESQualitySubsystem::GetFoliageQuality() const
{
	if (const auto GS = GEngine->GameUserSettings)
		return GS->GetFoliageQuality();
	return -1;
}



void UESQualitySubsystem::SetFoliageQuality(int32 Value, bool ApplyImmediately)
{
	FoliageQuality = Value; SaveConfig();
	if (const auto GS = GEngine->GameUserSettings)
		GS->SetFoliageQuality(Value);
		
	ApplyAllQualitySettings(ApplyImmediately);
}






int32 UESQualitySubsystem::GetAAQuality() const 
{
	if (const auto GUS = GEngine->GameUserSettings)
		return GUS->GetAntiAliasingQuality();
	return -1;
}



void UESQualitySubsystem::SetAAQuality(int32 Value, bool ApplyImmediately)
{
	ScreenSpaceAmbientOcclusion = Value; SaveConfig();
	if (const auto GUS = GEngine->GameUserSettings)
		GUS->SetAntiAliasingQuality(Value);
	
	ApplyAllQualitySettings(ApplyImmediately);
}



int32 UESQualitySubsystem::GetViewDistance() const
{
	if (const auto GUS = GEngine->GameUserSettings)
		return GUS->GetViewDistanceQuality();
	return -1;
}



void UESQualitySubsystem::SetViewDistance(int32 Value, bool ApplyImmediately)
{
	ViewDistance = Value; SaveConfig();
	
	if (const auto GUS = GEngine->GameUserSettings)
		GUS->SetViewDistanceQuality(Value);
	
	ApplyAllQualitySettings(ApplyImmediately);
}






int32 UESQualitySubsystem::GetVisualEffectQuality() const
{
	if (const auto GUS = GEngine->GameUserSettings)
		return GUS->GetVisualEffectQuality();
	return -1;
}



void UESQualitySubsystem::SetVisualEffectQuality(int32 Value, bool ApplyImmediately)
{
	VisualEffectsQuality = Value; SaveConfig();
	
	if (const auto GUS = GEngine->GameUserSettings)
		GUS->SetVisualEffectQuality(Value);
	
	ApplyAllQualitySettings(ApplyImmediately);
}






int32 UESQualitySubsystem::GetPostProcessingQuality() const
{
	if (const auto GUS = GEngine->GameUserSettings)
		return GUS->GetPostProcessingQuality();
	return 4;
}

void UESQualitySubsystem::SetPostProcessingQuality(int32 Value, bool ApplyImmediately)
{
	PostProcessingQuality = Value; SaveConfig();
	
	if (const auto GUS = GEngine->GameUserSettings)
		GUS->SetPostProcessingQuality(Value);
	
	ApplyAllQualitySettings(ApplyImmediately);
}






int32 UESQualitySubsystem::GetMotionBlurQuality() const
{
	if(const auto config = GetConsoleVariable(TEXT("r.MotionBlurQuality")) ) return config->GetInt();
	return 3;
}



void UESQualitySubsystem::SetMotionBlurQuality(int32 Value, bool ApplyImmediately)
{
	MotionBlurQuality = Value; SaveConfig();
	ExecuteConsoleCommand("r.MotionBlurQuality",FString::FromInt(Value));
}






uint8 UESQualitySubsystem::GetLensFlareQuality() const
{
	if(const auto config = GetConsoleVariable(TEXT("r.LensFlareQuality")) ) return config->GetInt();
	return 3;
}



void UESQualitySubsystem::SetLensFlareQuality(uint8 Value, bool ApplyImmediately)
{
	LensFlareQuality = Value ; SaveConfig();
	ExecuteConsoleCommand("r.LensFlareQuality",FString::FromInt(Value));
}






uint8 UESQualitySubsystem::GetSSReflections() const
{
	if(const auto config = GetConsoleVariable(TEXT("r.SSR.Quality")) ) return config->GetInt();
	return 3;
}



void UESQualitySubsystem::SetSSReflections(uint8 Value, bool ApplyImmediately)
{
	ScreenSpaceReflectionsQuality = Value ; SaveConfig();
	ExecuteConsoleCommand("r.LensFlareQuality",FString::FromInt(Value));;
}






int32 UESQualitySubsystem::GetBloomQuality() const
{
	if(const auto config =  GetConsoleVariable(TEXT("r.BloomQuality"))) return config->GetInt();
	return 3;
}



void UESQualitySubsystem::SetBloomQuality(int32 Value, bool ApplyImmediately)
{
	BloomQuality = Value ; SaveConfig();
	ExecuteConsoleCommand("r.BloomQuality",FString::FromInt(Value));
}






float UESQualitySubsystem::GetResolutionScale() const
{
	if (const auto GUS = GEngine->GameUserSettings)
		return GUS->GetResolutionScaleNormalized();
	if(const auto config = GetConsoleVariable(TEXT("r.ScreenPercentage"))) return config->GetInt();
	return 3;
}



void UESQualitySubsystem::SetResolutionScale(float Value, bool ApplyImmediately)
{
	ResolutionScale = Value ; SaveConfig();
	if (const auto GUS = GEngine->GameUserSettings)
		return GUS->SetResolutionScaleNormalized(Value);
}






bool UESQualitySubsystem::GetVSynch() const
{
	if (const auto GUS = GEngine->GameUserSettings)
		return GUS->IsVSyncEnabled();
	return false;
}



void UESQualitySubsystem::SetVSynch(bool Value, bool ApplyImmediately)
{
	Vsync = Value; SaveConfig();
	
	if (const auto GUS = GEngine->GameUserSettings)
		GUS->SetVSyncEnabled(Value);
	
	ApplyAllQualitySettings(ApplyImmediately);
}






float UESQualitySubsystem::GetGamma() const
{
	if(const auto config = GetConsoleVariable(TEXT("Gamma"))) return config->GetInt();
	return -1;
}



void UESQualitySubsystem::SetGamma(float Value, bool ApplyImmediately)
{
	Gamma = Value ; SaveConfig();
	ExecuteConsoleCommand("Gamma",FString::FromInt(Value));
}











void UESQualitySubsystem::ExecuteConsoleCommand(FString Str, FString Value,FString Prefix)
{
	UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(),Str+Prefix+Value);
}



IConsoleVariable* UESQualitySubsystem::GetConsoleVariable(const TCHAR* Name) const
{
	return IConsoleManager::Get().FindConsoleVariable(Name);
}





void UESQualitySubsystem::ApplyAllQualitySettings(bool ApplyImmediately)
{
	if (ApplyImmediately)
	{
		if (const auto GS = GEngine->GameUserSettings)
		{
			GS->ApplySettings(true);
			GS->SaveSettings();
			OnSettingsQualityUpdated.Broadcast();
		}
	}
}




void UESQualitySubsystem::CheckQualitySettingsConfigChanges()
{
	
	if(GetDisplayMode()  != DisplayMode )  { SetDisplayMode( DisplayMode , true) ; } 
	
	if(GetOverallScalabilityLevel() != OverallScalabilityLevel )  {	 SetOverallScalabilityLevel(OverallScalabilityLevel ,true); } 
	
	if(GetTextureQuality() != TextureQuality )  { SetTexturesQuality(TextureQuality,true); }  
	
	if(  GetShadowQuality() != ShadowQuality )  { SetShadowsQuality(ShadowQuality ,true) ;	 }   
	
	if( GetFoliageQuality()  != FoliageQuality)  { SetFoliageQuality(FoliageQuality,true);	}  
	
	if( GetAAQuality() != AntiAliasingQuality )  {	SetAAQuality(AntiAliasingQuality ,true) ; }  
	
	if( GetViewDistance() != ViewDistance)  {	 SetViewDistance(ViewDistance ,true)  ; }  
	
	if( GetVisualEffectQuality() !=  VisualEffectsQuality)  { SetVisualEffectQuality(VisualEffectsQuality ,true) ;	 }

	if( GetPostProcessingQuality() !=  PostProcessingQuality)  { SetPostProcessingQuality(PostProcessingQuality ,true) ;	 }
	
	if( GetMotionBlurQuality() != MotionBlurQuality )  { SetMotionBlurQuality(MotionBlurQuality ,true)  ;	 }  
	
	if( GetLensFlareQuality() != LensFlareQuality )  { SetLensFlareQuality(LensFlareQuality ,true);	 }  
	
	if( GetSSReflections() != ScreenSpaceReflectionsQuality)  {	 SetSSReflections(ScreenSpaceReflectionsQuality ,true); }  
	
	if( GetBloomQuality() != BloomQuality)  {	SetBloomQuality(BloomQuality,true); }
	
	if( GetResolutionScale() != ResolutionScale)  { SetResolutionScale(ResolutionScale ,true) ;	 }  
	
	if( GetScreenResolution() != ScreenResolution)  { SetScreenResolution(ScreenResolution ,true);	 }  
	
	if( GetVSynch() != Vsync)  {	SetVSynch(Vsync ,true) ; }  
	
	if( GetGamma() != Gamma)  {	SetGamma(Gamma ,true) ; }  

	 
	
}