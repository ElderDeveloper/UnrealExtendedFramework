// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFEntityFilesSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Auth/EPFAuthSubsystem.h"
#include "Dom/JsonObject.h"

void UEPFEntityFilesSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UEPFEntityFilesSubsystem::Deinitialize() { CachedObjects.Empty(); Super::Deinitialize(); }

// ── Get Objects ──────────────────────────────────────────────────────────────

void UEPFEntityFilesSubsystem::GetObjects(const FString& EntityId, const FString& EntityType)
{
	if (EntityId.IsEmpty()) { OnObjectsReceived.Broadcast(FEPFResult::Failure(TEXT("EntityId cannot be empty")), TArray<FEPFEntityObject>()); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Entity = MakeShared<FJsonObject>();
	Entity->SetStringField(TEXT("Id"), EntityId);
	Entity->SetStringField(TEXT("Type"), EntityType);
	Body->SetObjectField(TEXT("Entity"), Entity);

	SendPlayFabRequestDetailed(TEXT("/Object/GetObjects"), Body, EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			CachedObjects.Empty();
			if (Result.bSuccess && Response.IsValid())
			{
				const TSharedPtr<FJsonObject>* ObjectsObj = nullptr;
				if (Response->TryGetObjectField(TEXT("Objects"), ObjectsObj) && ObjectsObj)
				{
					for (const auto& Pair : (*ObjectsObj)->Values)
					{
						const TSharedPtr<FJsonObject>* ObjData = nullptr;
						if (Pair.Value->TryGetObject(ObjData) && ObjData)
						{
							FEPFEntityObject Obj;
							Obj.ObjectName = (*ObjData)->GetStringField(TEXT("ObjectName"));
							Obj.LastUpdated = (*ObjData)->GetStringField(TEXT("LastUpdated"));
							Obj.EscapedDataValue = (*ObjData)->GetStringField(TEXT("EscapedDataValue"));

							// Try to get DataAsJson
							const TSharedPtr<FJsonObject>* DataObj = nullptr;
							if ((*ObjData)->TryGetObjectField(TEXT("DataObject"), DataObj) && DataObj)
							{
								FString JsonStr;
								TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
								FJsonSerializer::Serialize((*DataObj).ToSharedRef(), Writer);
								Obj.DataAsJson = JsonStr;
							}
							else
							{
								Obj.DataAsJson = Obj.EscapedDataValue;
							}

							CachedObjects.Add(Obj);
						}
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFEntityFiles — %d objects received"), CachedObjects.Num());
			}
			OnObjectsReceived.Broadcast(Result, CachedObjects);
		}));
}

// ── Set Objects ──────────────────────────────────────────────────────────────

void UEPFEntityFilesSubsystem::SetObjects(const FString& EntityId, const TMap<FString, FString>& Objects, const FString& EntityType)
{
	if (EntityId.IsEmpty() || Objects.Num() == 0) { OnObjectsUpdated.Broadcast(FEPFResult::Failure(TEXT("EntityId and Objects are required"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Entity = MakeShared<FJsonObject>();
	Entity->SetStringField(TEXT("Id"), EntityId);
	Entity->SetStringField(TEXT("Type"), EntityType);
	Body->SetObjectField(TEXT("Entity"), Entity);

	TArray<TSharedPtr<FJsonValue>> ObjectsArr;
	for (const auto& Pair : Objects)
	{
		TSharedPtr<FJsonObject> SetObj = MakeShared<FJsonObject>();
		SetObj->SetStringField(TEXT("ObjectName"), Pair.Key);

		// Try to parse as JSON, if it fails set as raw string
		TSharedPtr<FJsonObject> DataObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Pair.Value);
		if (FJsonSerializer::Deserialize(Reader, DataObj) && DataObj.IsValid())
		{
			SetObj->SetObjectField(TEXT("DataObject"), DataObj);
		}
		else
		{
			SetObj->SetStringField(TEXT("EscapedDataValue"), Pair.Value);
		}

		ObjectsArr.Add(MakeShared<FJsonValueObject>(SetObj));
	}
	Body->SetArrayField(TEXT("Objects"), ObjectsArr);

	SendPlayFabRequestDetailed(TEXT("/Object/SetObjects"), Body, EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFEntityFiles — Objects updated"));
			OnObjectsUpdated.Broadcast(Result);
		}));
}

// ── Queries ──────────────────────────────────────────────────────────────────

TArray<FEPFEntityObject> UEPFEntityFilesSubsystem::GetCachedObjects() const { return CachedObjects; }

bool UEPFEntityFilesSubsystem::FindObject(const FString& ObjectName, FEPFEntityObject& OutObject) const
{
	for (const auto& O : CachedObjects) { if (O.ObjectName == ObjectName) { OutObject = O; return true; } }
	return false;
}
