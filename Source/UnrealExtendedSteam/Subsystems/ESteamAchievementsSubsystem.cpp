// Fill out your copyright notice in the Description page of Project Settings.


#include "ESteamAchievementsSubsystem.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineAchievementsInterface.h"
#include "Online.h"

/*
#define ACH_WIN_ONE_GAME FString("ACH_WIN_ONE_GAME")
#define AC_WIN_100_GAMES FString("ACH_WIN_100_GAMES")
#define AC_TRAVEL_FAR_SINGLE FString("ACH_TRAVEL_FAR_SINGLE")
*/


void UESteamAchievementsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	QueryAchievements();
}




void UESteamAchievementsSubsystem::QueryAchievements()
{
	//Get the online sub system
	if(const auto OnlineSub = IOnlineSubsystem::Get())
	{
		//Get the Identity from the sub system 
		//Meaning our player's profile and various online services
		if (const auto Identity = OnlineSub->GetIdentityInterface())
		{
			//Get a thread-safe pointer (for more info check out this link: https://docs.unrealengine.com/latest/INT/API/Runtime/Core/Templates/TSharedPtr/index.html )
			TSharedPtr<const FUniqueNetId> UserID = Identity->GetUniquePlayerId(0);

			//Get the achievements interface for this platform
			if(IOnlineAchievementsPtr Achievements = OnlineSub->GetAchievementsInterface())
			{
				Achievements->QueryAchievements(*UserID.Get() ,  FOnQueryAchievementsCompleteDelegate::CreateUObject(this,&UESteamAchievementsSubsystem::OnQueryAchievementsComplete));  
			}
		}
	}
}




void UESteamAchievementsSubsystem::OnQueryAchievementsComplete(const FUniqueNetId& PlayerId, const bool bWasSuccessful)
{
	bWasSuccessful ? GLog->Log("Achievements were cached") : GLog->Log("Achievements were failed") ;
}




void UESteamAchievementsSubsystem::UpdateAchievementProgress(const FName& Id, float Percent)
{
	//Get the online sub system
	if(const auto OnlineSub = IOnlineSubsystem::Get())
	{
		//Get the Identity from the sub system 
		//Meaning our player's profile and various online services
		if (const auto Identity = OnlineSub->GetIdentityInterface())
		{
			//Get a thread-safe pointer (for more info check out this link: https://docs.unrealengine.com/latest/INT/API/Runtime/Core/Templates/TSharedPtr/index.html )
			TSharedPtr<const FUniqueNetId> UserID = Identity->GetUniquePlayerId(0);

			//Get the achievements interface for this platform
			if(IOnlineAchievementsPtr Achievements = OnlineSub->GetAchievementsInterface())
			{
				//Make a shared pointer for achievement writing
				if(!AchievementsWriteObjectPtr.IsValid())
					AchievementsWriteObjectPtr = MakeShareable(new FOnlineAchievementsWrite());
				
				if (AchievementsWriteObjectPtr->WriteState != EOnlineAsyncTaskState::InProgress)
				{
					//Sets the progress of the desired achievement - does nothing if the id is not valid
					AchievementsWriteObjectPtr->SetFloatStat(Id,Percent);

					//Write the achievements progress
					FOnlineAchievementsWriteRef AchievementsWriteObjectRef = AchievementsWriteObjectPtr.ToSharedRef();
					Achievements->WriteAchievements(*UserID , AchievementsWriteObjectRef);
				}
			}
		}
	}
}

