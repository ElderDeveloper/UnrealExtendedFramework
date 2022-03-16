// Fill out your copyright notice in the Description page of Project Settings.


#include "EBHttpSubsystem.h"
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




void UEBHttpSubsystem::CreatePostRequest(FString Data, FString Api, TMemFunPtrType<false, UEBHttpSubsystem, void(FHttpRequestPtr, FHttpResponsePtr, bool)>::Type InFunc)
{
	const auto Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, InFunc);
	Request->SetURL("http://alkogames.com/brawl/Apii.php?data=" + Api);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	Request->SetContentAsString(Data);
	Request->ProcessRequest();
}




void UEBHttpSubsystem::CreateGetRequest(FString Api, TMemFunPtrType<false, UEBHttpSubsystem, void(FHttpRequestPtr, FHttpResponsePtr, bool)>::Type InFunc)
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

void UEBHttpSubsystem::SendHttpRequest(const FString& Url, const FString& RequestContent)
{
	//Creating a request using UE4's Http Module
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();

	//Binding a function that will fire when we receive a response from our request
	Request->OnProcessRequestComplete().BindUObject(this, &UEBHttpSubsystem::OnResponseReceived);

	//This is the url on which to process the request
	Request->SetURL(Url);
	//We're sending data so set the Http method to POST
	Request->SetVerb("POST");

	//Tell the host that we're an unreal agent
	Request->SetHeader(TEXT("User-Agent"), "X-UnrealEngine-Agent");

	//In case you're sending json you can also make use of headers using the following command
	//Request->SetHeader("Content-Type", TEXT("application/json"));
	//Use the following command in case you want to send a string value to the server
	//Request->SetContentAsString("Hello kind server.");

	//Send the request
	Request->ProcessRequest();
}

void UEBHttpSubsystem::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if(bWasSuccessful)
	{
		GLog->Log("Hey we received the following response!");
		GLog->Log(Response->GetContentAsString());
	}
}


FString UEBHttpSubsystem::JsonString(TArray<FString> StringArray)
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
