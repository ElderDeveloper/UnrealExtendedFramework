// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedSQLSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"



TSharedPtr<FJsonObject> JsonReturn(FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		if (FJsonSerializer::Deserialize(Reader, JsonObject))
		{
			return JsonObject;
		}	return nullptr;
	}
	GEngine->AddOnScreenDebugMessage(-1, 4.0, FColor::Red, "Json Error");
	return nullptr;

}




void UUEExtendedSQLSubsystem::CreatePostRequest(FString Data, FString Api, TMemFunPtrType<false, UUEExtendedSQLSubsystem, void(FHttpRequestPtr, FHttpResponsePtr, bool)>::Type InFunc)
{
	const auto Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, InFunc);
	Request->SetURL("http://alkogames.com/brawl/Apii.php?data=" + Api);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	Request->SetContentAsString(Data);
	Request->ProcessRequest();
}




void UUEExtendedSQLSubsystem::CreateGetRequest(FString Api, TMemFunPtrType<false, UUEExtendedSQLSubsystem, void(FHttpRequestPtr, FHttpResponsePtr, bool)>::Type InFunc)
{
	const auto Request =FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, InFunc);
	FString Url = "http://alkogames.com/brawl/Apii.php?data=" + Api;
	Request->SetURL(Url);
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("User-Agent"), "X-UnrealEngine-Agent");
	Request->SetContentAsString("Request");
	Request->ProcessRequest();
}




FString UUEExtendedSQLSubsystem::JsonString(TArray<FString> StringArray)
{
	//FString data = "{\"id\":" + s_userid + ",\"userGold\":" + s_goldamount + "}";
	FString Result = "{";
	bool even = false;
	for (int32 i=0 ; i<StringArray.Max(); i++ )
	{
		if (!even)
		{
			Result = Result + "\"" + StringArray[i] + "\":";	
		}
		if (even)
		{
			if (i!=StringArray.Max()-1)
			{
				Result = Result +"\""+ StringArray[i] +"\""+ ",";
			}
			else
			{
				Result = Result + "\"" + StringArray[i] + "\"";
			}
		}
		even = !even;
	}
	Result = Result + "}";
	return Result;

}