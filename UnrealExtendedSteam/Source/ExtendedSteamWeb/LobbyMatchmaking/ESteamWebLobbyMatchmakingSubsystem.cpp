// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "LobbyMatchmaking/ESteamWebLobbyMatchmakingSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebLobbyMatchmakingSubsystem::CreateLobby(int32 AppId, int32 MaxMembers, int32 LobbyType, FString LobbyName, FString InputJson, FString SteamIdInvitedMembersCsv, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ILobbyMatchmakingService"), TEXT("CreateLobby"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("max_members"), MaxMembers);
	Request.AddParam(TEXT("lobby_type"), LobbyType);
	if (!LobbyName.IsEmpty())
	{
		Request.AddParam(TEXT("lobby_name"), LobbyName);
	}
	if (!InputJson.IsEmpty())
	{
		Request.AddParam(TEXT("input_json"), InputJson);
	}

	TArray<FString> InvitedMembers;
	SteamIdInvitedMembersCsv.ParseIntoArray(InvitedMembers, TEXT(","), /*bCullEmpty*/ true);
	for (int32 Index = 0; Index < InvitedMembers.Num(); ++Index)
	{
		Request.AddParam(FString::Printf(TEXT("steamid_invited_members[%d]"), Index), InvitedMembers[Index].TrimStartAndEnd());
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebLobbyMatchmakingSubsystem::RemoveUserFromLobby(int32 AppId, FString SteamIdToRemove, FString SteamIdLobby, FString InputJson, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ILobbyMatchmakingService"), TEXT("RemoveUserFromLobby"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("steamid_to_remove"), SteamIdToRemove);
	Request.AddParam(TEXT("steamid_lobby"), SteamIdLobby);
	if (!InputJson.IsEmpty())
	{
		Request.AddParam(TEXT("input_json"), InputJson);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}
