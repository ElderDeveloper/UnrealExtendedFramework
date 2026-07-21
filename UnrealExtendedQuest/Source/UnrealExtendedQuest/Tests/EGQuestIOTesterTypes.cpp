// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestIOTesterTypes.h"
#include "UnrealExtendedQuest/EGQuestComponent.h"

void UEGQuestTestReentrySink::Configure(UEGQuestComponent* InComponent, FGuid InRunId, FGuid InObjectiveGuid)
{
	Component = InComponent;
	RunId = InRunId;
	ObjectiveGuid = InObjectiveGuid;
}

void UEGQuestTestReentrySink::HandleAcceptedEvent(const FEGQuestGameplayEvent& Event)
{
	FEGQuestViewSnapshot Projection;
	if (Component && Component->FindQuestSnapshot(RunId, Projection))
	{
		ProjectionRevisionSeenByCallback = Projection.Revision;
	}
	if (Component)
	{
		CallbackStatus = Component->SetObjectiveRequiredCount(RunId, ObjectiveGuid, 7).Status;
	}
}

TMap<const UEGQuestContext*, FText> UEGQuestTestContextTextArgument::ContextValues;

#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

#include "EGQuestTesterHelper.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UEGQuestTestObjectPrimitivesBase
void UEGQuestTestObjectPrimitivesBase::GenerateRandomData(const FEGQuestIOTesterOptions& InOptions)
{
	Options = InOptions;
	SetToDefaults();

	Integer = FMath::Rand();
	String = FString::SanitizeFloat(FMath::SRand());
}

void UEGQuestTestObjectPrimitivesBase::SetToDefaults()
{
	Integer = 0;
	String.Empty();
}

bool UEGQuestTestObjectPrimitivesBase::IsEqual(const Self* Other, FString& OutError) const
{
	if (Other == nullptr)
	{
		OutError += FString::Printf(TEXT("\tOther is nullptr.\n"));
		return false;
	}

	bool bIsEqual = true;
	if (Integer != Other->Integer)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Integer (%d) != Other.Integer (%d)\n"), Integer, Other->Integer);
	}

	if (String != Other->String)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.String (%s) != Other.String (%s)\n"), *String, *Other->String);
	}

	return bIsEqual;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UEGQuestTestObjectPrimitives_DefaultToInstanced
void UEGQuestTestObjectPrimitives_DefaultToInstanced::GenerateRandomData(const FEGQuestIOTesterOptions& InOptions)
{
	Super::GenerateRandomData(InOptions);
	InstancedChild = FMath::Rand();
}

void UEGQuestTestObjectPrimitives_DefaultToInstanced::SetToDefaults()
{
	Super::SetToDefaults();
	InstancedChild = 0;
}

bool UEGQuestTestObjectPrimitives_DefaultToInstanced::IsEqual(const Super* Other, FString& OutError) const
{
	const Self* SelfOther = Cast<Self>(Other);
	if (SelfOther == nullptr)
	{
		OutError += FString::Printf(TEXT("This.Class (%s) != Other.Class (%s)\n"), *FEGQuestHelper::GetClassNameFromObject(this), *FEGQuestHelper::GetClassNameFromObject(Other));
		return false;
	}

	bool bIsEqual = Super::IsEqual(Other, OutError);
	if (InstancedChild != SelfOther->InstancedChild)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.IntegerChildA (%d) != Other.IntegerChildA (%d)\n"), InstancedChild, SelfOther->InstancedChild);
	}

	return bIsEqual;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UEGQuestTestObjectPrimitives_ChildA
void UEGQuestTestObjectPrimitives_ChildA::GenerateRandomData(const FEGQuestIOTesterOptions& InOptions)
{
	Super::GenerateRandomData(InOptions);
	IntegerChildA = FMath::Rand();
}

void UEGQuestTestObjectPrimitives_ChildA::SetToDefaults()
{
	Super::SetToDefaults();
	IntegerChildA = 0;
}

bool UEGQuestTestObjectPrimitives_ChildA::IsEqual(const Super* Other, FString& OutError) const
{
	const Self* SelfOther = Cast<Self>(Other);
	if (SelfOther == nullptr)
	{
		OutError += FString::Printf(TEXT("This.Class (%s) != Other.Class (%s)\n"), *FEGQuestHelper::GetClassNameFromObject(this), *FEGQuestHelper::GetClassNameFromObject(Other));
		return false;
	}

	bool bIsEqual = Super::IsEqual(Other, OutError);
	if (IntegerChildA != SelfOther->IntegerChildA)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.IntegerChildA (%d) != Other.IntegerChildA (%d)\n"), IntegerChildA, SelfOther->IntegerChildA);
	}

	return bIsEqual;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UEGQuestTestObjectPrimitives_ChildB
void UEGQuestTestObjectPrimitives_ChildB::GenerateRandomData(const FEGQuestIOTesterOptions& InOptions)
{
	Super::GenerateRandomData(InOptions);
	StringChildB = FString::SanitizeFloat(FMath::SRand());
}

void UEGQuestTestObjectPrimitives_ChildB::SetToDefaults()
{
	Super::SetToDefaults();
	StringChildB.Empty();
}

bool UEGQuestTestObjectPrimitives_ChildB::IsEqual(const Super* Other, FString& OutError) const
{
	const Self* SelfOther = Cast<Self>(Other);
	if (SelfOther == nullptr)
	{
		OutError += FString::Printf(TEXT("This.Class (%s) != Other.Class (%s)\n"), *FEGQuestHelper::GetClassNameFromObject(this), *FEGQuestHelper::GetClassNameFromObject(Other));
		return false;
	}

	bool bIsEqual = Super::IsEqual(Other, OutError);
	if (StringChildB != SelfOther->StringChildB)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.StringChildB (%s) != Other.StringChildB (%s)\n"), *StringChildB, *SelfOther->StringChildB);
	}

	return bIsEqual;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UEGQuestTestObjectPrimitives_GrandChildA_Of_ChildA
void UEGQuestTestObjectPrimitives_GrandChildA_Of_ChildA::GenerateRandomData(const FEGQuestIOTesterOptions& InOptions)
{
	Super::GenerateRandomData(InOptions);
	IntegerGrandChildA_Of_ChildA = FMath::Rand();
}

void UEGQuestTestObjectPrimitives_GrandChildA_Of_ChildA::SetToDefaults()
{
	Super::SetToDefaults();
	IntegerGrandChildA_Of_ChildA = 0;
}

bool UEGQuestTestObjectPrimitives_GrandChildA_Of_ChildA::IsEqual(const SuperBase* Other, FString& OutError) const
{
	const Self* SelfOther = Cast<Self>(Other);
	if (SelfOther == nullptr)
	{
		OutError += FString::Printf(TEXT("This.Class (%s) != Other.Class (%s)\n"), *FEGQuestHelper::GetClassNameFromObject(this), *FEGQuestHelper::GetClassNameFromObject(Other));
		return false;
	}

	bool bIsEqual = Super::IsEqual(Other, OutError);
	if (IntegerGrandChildA_Of_ChildA != SelfOther->IntegerGrandChildA_Of_ChildA)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.IntegerGrandChildA_Of_ChildA (%d) != Other.IntegerGrandChildA_Of_ChildA (%d)\n"), IntegerGrandChildA_Of_ChildA, IntegerGrandChildA_Of_ChildA);
	}

	return bIsEqual;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestTestStructPrimitives
void FEGQuestTestStructPrimitives::GenerateRandomData(const FEGQuestIOTesterOptions& InOptions)
{
	Options = InOptions;
	SetToDefaults();

	bBoolean = FMath::RandBool();
	// Random negative too
	Integer32 = FMath::Rand() * (FMath::RandBool() ? 1 : -1);
	Integer64 = FEGQuestHelper::RandomInt64() * (FMath::RandBool() ? 1 : -1);
	Float = FMath::SRand();
	String = FString::SanitizeFloat(Float);
	Name = FName(*String);
	Enum = static_cast<EEGQuestTestEnum>(FMath::RandHelper(static_cast<int32>(EEGQuestTestEnum::NumOf)));
	Text = FText::FromString(String);

	if (Options.bSupportsColorPrimitives)
	{
		LinearColor = FLinearColor(FMath::SRand(), FMath::SRand(), FMath::SRand());
		Color = LinearColor.ToFColor(false);
	}
	if (Options.bSupportsDatePrimitive)
	{
		DateTime = FDateTime(FMath::Rand());
	}
	IntPoint = FIntPoint(FMath::Rand(), FMath::Rand());
	Vector3 = FVector(FMath::SRand(), FMath::SRand(), FMath::SRand());
	Vector2 = FVector2D(FMath::SRand(), FMath::SRand());
	Vector4 = FVector4(FMath::SRand(), FMath::SRand(), FMath::SRand(), FMath::SRand());
	Rotator = FRotator(FMath::SRand(), FMath::SRand(), FMath::SRand());
	Matrix = FMatrix(Vector3, FVector(IntPoint), FVector(Vector4), Vector3);
	Transform = FTransform(Vector3);
	GUID = FGuid::NewGuid();

	const TArray<UClass*> ClassesPool = {
		UField::StaticClass(), UStruct::StaticClass(), nullptr, UObject::StaticClass(),
		AActor::StaticClass(), APawn::StaticClass(), nullptr
	};
	Class = ClassesPool[FMath::RandHelper(ClassesPool.Num())];

	check(GEngine);
	const TArray<UTexture2D*> TexturesPool = {
		GEngine->DefaultTexture, nullptr, GEngine->DefaultBokehTexture, GEngine->DefaultBloomKernelTexture, GEngine->LightMapDensityTexture,
		GEngine->HighFrequencyNoiseTexture, GEngine->MiniFontTexture, GEngine->PreIntegratedSkinBRDFTexture, nullptr
	};
	Texture2DReference = TexturesPool[FMath::RandHelper(TexturesPool.Num())];
	ConstTexture2D = GEngine->DefaultTexture;

	ObjectPrimitivesBase = NewObject<UEGQuestTestObjectPrimitivesBase>();
	ObjectPrimitivesBase->GenerateRandomData(Options);

	ObjectDefaultToInstanced = NewObject<UEGQuestTestObjectPrimitives_DefaultToInstanced>();
	ObjectDefaultToInstanced->GenerateRandomData(Options);

	ObjectPrimitivesChildA = NewObject<UEGQuestTestObjectPrimitives_ChildA>();
	ObjectPrimitivesChildA->GenerateRandomData(Options);

	ObjectPrimitivesPolymorphismChildA = NewObject<UEGQuestTestObjectPrimitives_ChildA>();
	ObjectPrimitivesPolymorphismChildA->GenerateRandomData(Options);

	ObjectPrimitivesPolymorphismChildB = NewObject<UEGQuestTestObjectPrimitives_ChildB>();
	ObjectPrimitivesPolymorphismChildB->GenerateRandomData(Options);

	ObjectPrimitivesGrandChildA = NewObject<UEGQuestTestObjectPrimitives_GrandChildA_Of_ChildA>();
	ObjectPrimitivesGrandChildA->GenerateRandomData(Options);

	ObjectPrimitivesPolymorphismBaseGrandChildA = NewObject<UEGQuestTestObjectPrimitives_GrandChildA_Of_ChildA>();
	ObjectPrimitivesPolymorphismBaseGrandChildA->GenerateRandomData(Options);

	ObjectPrimitivesPolymorphismChildGrandChildA = NewObject<UEGQuestTestObjectPrimitives_GrandChildA_Of_ChildA>();
	ObjectPrimitivesPolymorphismChildGrandChildA->GenerateRandomData(Options);

	if (FMath::RandBool())
	{
		ObjectSwitch = nullptr;
	}
	else
	{
		ObjectSwitch = NewObject<UEGQuestTestObjectPrimitivesBase>();
		ObjectSwitch->GenerateRandomData(Options);
	}
}

bool FEGQuestTestStructPrimitives::IsEqual(const Self& Other, FString& OutError) const
{
	//if (Options != Other.Options)
	//{
	//	OutError += FString::Printf(TEXT("This.Options(%s) != Other.Options(%s) This will make the test fail most likely:\n"), *Options.ToString(), *Other.Options.ToString());
	//	return false;
	//}

	bool bIsEqual = true;
	OutError += TEXT("FEGQuestTestStructPrimitives::IsEqual:\n");

	if ((ObjectSwitch == nullptr && Other.ObjectSwitch != nullptr) || (ObjectSwitch != nullptr && Other.ObjectSwitch == nullptr))
	{
		OutError += FString::Printf(TEXT("\tThis.ObjectSwitch (%d) != Other.ObjectSwitch (%d). Null values are different. Writer supports writing null values?\n"), ObjectSwitch != nullptr, Other.ObjectSwitch != nullptr);
		return false;
	}

	if (bBoolean != Other.bBoolean)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.bBoolean (%d) != Other.bBoolean (%d)\n"), bBoolean, Other.bBoolean);
	}

	if (Integer32 != Other.Integer32)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Integer32 (%d) != Other.Integer32 (%d)\n"), Integer32, Other.Integer32);
	}

	if (Integer64 != Other.Integer64)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Integer64 (%lld) != Other.Integer64 (%lld)\n"), Integer64, Other.Integer64);
	}

	if (!FEGQuestHelper::IsFloatEqual(Float, Other.Float))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Float (%f) != Other.Float (%f)\n"), Float, Other.Float);
	}

	if (String != Other.String)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.String (%s) != Other.String (%s)\n"), *String, *Other.String);
	}

	if (Name != Other.Name)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Name (%s) != Other.Name (%s)\n"), *Name.ToString(), *Other.Name.ToString());
	}

	if (!Text.EqualTo(Other.Text))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Text (%s) != Other.Text (%s)\n"), *Text.ToString(), *Other.Text.ToString());
	}

	if (Enum != Other.Enum)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Enum (%d) != Other.Enum (%d)\n"), static_cast<int32>(Enum), static_cast<int32>(Other.Enum));
	}

	if (Color != Other.Color)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Color (%s) != Other.Color (%s)\n"), *Color.ToString(), *Other.Color.ToString());
	}

	if (!LinearColor.Equals(Other.LinearColor))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.LinearColor (%s) != Other.LinearColor (%s)\n"), *LinearColor.ToString(), *Other.LinearColor.ToString());
	}

	if (DateTime != Other.DateTime)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.DateTime.Ticks (%lld) != Other.DateTime.Ticks (%lld)\n"), DateTime.GetTicks(), Other.DateTime.GetTicks());
	}

	if (IntPoint != Other.IntPoint)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.IntPoint (%s) != Other.IntPoint (%s)\n"), *IntPoint.ToString(), *Other.IntPoint.ToString());
	}

	if (!Vector3.Equals(Other.Vector3))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Vector3 (%s) != Other.Vector3 (%s)\n"), *Vector3.ToString(), *Other.Vector3.ToString());
	}

	if (!Vector2.Equals(Other.Vector2))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Vector2 (%s) != Other.Vector2 (%s)\n"), *Vector2.ToString(), *Other.Vector2.ToString());
	}

	if (!Vector4.Equals(Other.Vector4))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Vector4 (%s) != Other.Vector4 (%s)\n"), *Vector4.ToString(), *Other.Vector4.ToString());
	}

	if (!Rotator.Equals(Other.Rotator))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Rotator (%s) != Other.Rotator (%s)\n"), *Rotator.ToString(), *Other.Rotator.ToString());
	}

	if (!Matrix.Equals(Other.Matrix))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Matrix (%s) != Other.Matrix (%s)\n"), *Matrix.ToString(), *Other.Matrix.ToString());
	}

	if (!Transform.Equals(Other.Transform))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Transform (%s) != Other.Transform (%s)\n"), *Transform.ToString(), *Other.Transform.ToString());
	}

	if (GUID != Other.GUID)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Guid (%s) != Other.Guid (%s)\n"), *GUID.ToString(), *Other.GUID.ToString());
	}

	if (Class != Other.Class)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Class (%s) != Other.Class (%s)\n"), *FEGQuestHelper::GetFullNameFromObject(Class), *FEGQuestHelper::GetFullNameFromObject(Other.Class));
	}

	if (EmptyObjectInitialized != Other.EmptyObjectInitialized || EmptyObjectInitialized != nullptr || Other.EmptyObjectInitialized != nullptr)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.EmptyObjectInitialized OR Other.EmptyObjectInitialized are not empty :O\n"));
	}

	if (EmptyObjectInitializedReference != Other.EmptyObjectInitializedReference || EmptyObjectInitializedReference != nullptr || Other.EmptyObjectInitializedReference != nullptr)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.EmptyObjectInitializedReference OR Other.EmptyObjectInitializedReference are not empty :O\n"));
	}

	if (Texture2DReference != Other.Texture2DReference)
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.Texture2D (%s) != Other.Texture2D (%s)\n"), *FEGQuestHelper::GetFullNameFromObject(Texture2DReference), *FEGQuestHelper::GetFullNameFromObject(Other.Texture2DReference));
	}

	if (!ObjectPrimitivesBase->IsEqual(Other.ObjectPrimitivesBase, OutError))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.ObjectPrimitives (%s) != Other.ObjectPrimitives (%s)\n"), *FEGQuestHelper::GetFullNameFromObject(ObjectPrimitivesBase), *FEGQuestHelper::GetFullNameFromObject(Other.ObjectPrimitivesBase));
	}

	if (!ObjectDefaultToInstanced->IsEqual(Other.ObjectDefaultToInstanced, OutError))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.ObjectDefaultToInstanced (%s) != Other.ObjectDefaultToInstanced (%s)\n"), *FEGQuestHelper::GetFullNameFromObject(ObjectDefaultToInstanced), *FEGQuestHelper::GetFullNameFromObject(Other.ObjectDefaultToInstanced));
	}

	if (!ObjectPrimitivesChildA->IsEqual(Other.ObjectPrimitivesChildA, OutError))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.ObjectPrimitivesChildA (%s) != Other.ObjectPrimitivesChildA (%s)\n"), *FEGQuestHelper::GetFullNameFromObject(ObjectPrimitivesChildA), *FEGQuestHelper::GetFullNameFromObject(Other.ObjectPrimitivesChildA));
	}

	if (!ObjectPrimitivesPolymorphismChildA->IsEqual(Other.ObjectPrimitivesPolymorphismChildA, OutError))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.ObjectPrimitivesPolymorphismChildA (%s) != Other.ObjectPrimitivesPolymorphismChildA (%s)\n"), *FEGQuestHelper::GetFullNameFromObject(ObjectPrimitivesPolymorphismChildA), *FEGQuestHelper::GetFullNameFromObject(Other.ObjectPrimitivesPolymorphismChildA));
	}

	if (!ObjectPrimitivesPolymorphismChildB->IsEqual(Other.ObjectPrimitivesPolymorphismChildB, OutError))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.ObjectPrimitivesPolymorphismChildB (%s) != Other.ObjectPrimitivesPolymorphismChildB (%s)\n"), *FEGQuestHelper::GetFullNameFromObject(ObjectPrimitivesPolymorphismChildB), *FEGQuestHelper::GetFullNameFromObject(Other.ObjectPrimitivesPolymorphismChildB));
	}

	if (!ObjectPrimitivesGrandChildA->IsEqual(Other.ObjectPrimitivesGrandChildA, OutError))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.ObjectPrimitivesGrandChildA (%s) != Other.ObjectPrimitivesGrandChildA (%s)\n"), *FEGQuestHelper::GetFullNameFromObject(ObjectPrimitivesGrandChildA), *FEGQuestHelper::GetFullNameFromObject(Other.ObjectPrimitivesGrandChildA));
	}

	if (!ObjectPrimitivesPolymorphismBaseGrandChildA->IsEqual(Other.ObjectPrimitivesPolymorphismBaseGrandChildA, OutError))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.ObjectPrimitivesPolymorphismBaseGrandChildA (%s) != Other.ObjectPrimitivesPolymorphismBaseGrandChildA (%s)\n"), *FEGQuestHelper::GetFullNameFromObject(ObjectPrimitivesPolymorphismBaseGrandChildA), *FEGQuestHelper::GetFullNameFromObject(Other.ObjectPrimitivesPolymorphismBaseGrandChildA));
	}

	if (!ObjectPrimitivesPolymorphismChildGrandChildA->IsEqual(Other.ObjectPrimitivesPolymorphismChildGrandChildA, OutError))
	{
		bIsEqual = false;
		OutError += FString::Printf(TEXT("\tThis.ObjectPrimitivesPolymorphismChildGrandChildA (%s) != Other.ObjectPrimitivesPolymorphismChildGrandChildA (%s)\n"), *FEGQuestHelper::GetFullNameFromObject(ObjectPrimitivesPolymorphismChildGrandChildA), *FEGQuestHelper::GetFullNameFromObject(Other.ObjectPrimitivesPolymorphismChildGrandChildA));
	}

	// Clear error message
	if (bIsEqual)
	{
		OutError.Empty();
	}
	return bIsEqual;
}

void FEGQuestTestStructPrimitives::SetToDefaults()
{
	Integer32 = 42;
	Integer64 = 223372036854775807;
	bBoolean = true;
	Enum = EEGQuestTestEnum::Second;
	Float = -23.549f;
	Name = NAME_None;
	String.Empty();
	EmptyString.Empty();
	Text = FText::GetEmpty();
	LinearColor = FLinearColor(ForceInitToZero);
	Color = FColor(ForceInitToZero);
	DateTime = FDateTime(0);
	IntPoint = FIntPoint(ForceInitToZero);
	Vector3 = FVector(ForceInitToZero);
	Vector2 = FVector2D(ForceInitToZero);
	Vector4 = FVector4(ForceInitToZero);
	Rotator = FRotator(ForceInitToZero);
	Matrix = FMatrix(ForceInitToZero);
	Transform = FTransform();
	GUID = FGuid();
	Class = nullptr;
	EmptyObjectInitialized = nullptr;
	EmptyObjectInitializedReference = nullptr;
	Texture2DReference = nullptr;
	ConstTexture2D = nullptr;
	ObjectPrimitivesBase = nullptr;
	ObjectDefaultToInstanced = nullptr;
	ObjectPrimitivesChildA = nullptr;
	ObjectPrimitivesPolymorphismChildA = nullptr;
	ObjectPrimitivesPolymorphismChildB = nullptr;
	ObjectPrimitivesGrandChildA = nullptr;
	ObjectPrimitivesPolymorphismBaseGrandChildA = nullptr;
	ObjectPrimitivesPolymorphismChildGrandChildA = nullptr;
	ObjectSwitch = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestTestStructPrimitives
void FEGQuestTestStructComplex::GenerateRandomData(const FEGQuestIOTesterOptions& InOptions)
{
	Options = InOptions;
	SetToDefaults();
	const int32 Num = FMath::RandHelper(10) + 2;
	static const TArray<UClass*> ObjectClassPool = {
		UEGQuestTestObjectPrimitivesBase::StaticClass(), UEGQuestTestObjectPrimitives_ChildA::StaticClass(),
		UEGQuestTestObjectPrimitives_ChildB::StaticClass(), UEGQuestTestObjectPrimitives_GrandChildA_Of_ChildA::StaticClass(), nullptr
	};

	for (int32 i = 0; i < Num; ++i)
	{
		// StructArrayPrimitives
		FEGQuestTestStructPrimitives StructPrimitives;
		StructPrimitives.GenerateRandomData(Options);
		StructArrayPrimitives.Add(StructPrimitives);

		// ArrayOfObjects
		{
			UClass* ChosenClass = ObjectClassPool[FMath::RandHelper(ObjectClassPool.Num())];
			if (ChosenClass == nullptr)
			{
				ArrayOfObjects.Add(nullptr);
			}
			else
			{
				UEGQuestTestObjectPrimitivesBase* ObjectPrimitives = NewObject<UEGQuestTestObjectPrimitivesBase>((UObject*)GetTransientPackage(), ChosenClass);
				ObjectPrimitives->GenerateRandomData(Options);
				ArrayOfObjects.Add(ObjectPrimitives);
			}
		}
	}
}

bool FEGQuestTestStructComplex::IsEqual(const Self& Other, FString& OutError) const
{
	FString PropertyName;
	bool bIsEqual = true;
	OutError += TEXT("FEGQuestTestStructComplex::IsEqual:\n");

	PropertyName = TEXT("StructArrayPrimitives");
	bIsEqual &= FEGQuestTestHelper::IsComplexArrayEqual<FEGQuestTestStructPrimitives>(StructArrayPrimitives, Other.StructArrayPrimitives, PropertyName, OutError);

	PropertyName = TEXT("ArrayOfObjects");
	bIsEqual &= FEGQuestTestHelper::IsComplexPointerArrayEqual<UEGQuestTestObjectPrimitivesBase>(ArrayOfObjects, Other.ArrayOfObjects, PropertyName, OutError);


	// Clear error message
	if (bIsEqual)
	{
		OutError.Empty();
	}
	return bIsEqual;
}


void FEGQuestTestStructComplex::SetToDefaults()
{
	StructArrayPrimitives = {};
	ArrayOfObjects = {};
	ArrayOfObjectsAsReference = {};
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestTestArrayPrimitive
void FEGQuestTestArrayPrimitive::GenerateRandomData(const FEGQuestIOTesterOptions& InOptions)
{
	Options = InOptions;
	Num1_Array = { FMath::Rand() };
	EmptyArray.Empty();
	Int32Array.Empty();
	Int64Array.Empty();
	BoolArray.Empty();
	FloatArray.Empty();
	EnumArray.Empty();
	NameArray.Empty();
	StringArray.Empty();
	ObjectArrayConstantNulls.Empty();

	const int32 Num = FMath::RandHelper(10) + 2;
	Int32Array.SetNum(Num);
	Int64Array.SetNum(Num);
	BoolArray.SetNum(Num);
	FloatArray.SetNum(Num);
	if (Options.bSupportsPureEnumContainer)
	{
		EnumArray.SetNum(Num);
	}
	NameArray.SetNum(Num);
	StringArray.SetNum(Num);
	ObjectArrayConstantNulls.SetNum(Num);

	for (int32 i = 0; i < Num; ++i)
	{
		Int32Array[i] = FMath::Rand();
		Int64Array[i] = FEGQuestHelper::RandomInt64();
		BoolArray[i] = FMath::RandBool();
		if (Options.bSupportsPureEnumContainer)
		{
			EnumArray[i] = static_cast<EEGQuestTestEnum>(FMath::RandHelper(static_cast<int32>(EEGQuestTestEnum::NumOf)));
		}
		FloatArray[i] = FMath::SRand();
		StringArray[i] = FString::SanitizeFloat(FloatArray[i]);
		NameArray[i] = FName(*StringArray[i]);

		// ObjectArrayConstantNulls
		ObjectArrayConstantNulls[i] = nullptr;
	}
}

bool FEGQuestTestArrayPrimitive::IsEqual(const Self& Other, FString& OutError) const
{
	FString PropertyName;
	bool bIsEqual = true;
	OutError += TEXT("FEGQuestTestArrayPrimitive::IsEqual:\n");

	PropertyName = TEXT("ObjectArrayConstantNulls");
	if (ObjectArrayConstantNulls.Num() != Other.ObjectArrayConstantNulls.Num())
	{
		OutError = FString::Printf(TEXT("This.ObjectArrayConstantNulls.Num(%d) != Other.ObjectArrayConstantNulls.Num(%d). Most likely because the writer does not support writing nulls."),
			ObjectArrayConstantNulls.Num(), Other.ObjectArrayConstantNulls.Num());
		return false;
	}

	PropertyName = TEXT("EmptyArray");
	if (EmptyArray.Num() != Other.EmptyArray.Num())
	{
		OutError = FString::Printf(TEXT("This.EmptyArray.Num(%d) != Other.EmptyArray.Num(%d). but why :O"),
			EmptyArray.Num(), Other.EmptyArray.Num());
		return false;
	}

	PropertyName = TEXT("Num1_Array");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveArrayEqual<int32>(Num1_Array, Other.Num1_Array, PropertyName, OutError,
		FEGQuestTestHelper::Int32ToString);

	PropertyName = TEXT("Int32Array");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveArrayEqual<int32>(Int32Array, Other.Int32Array, PropertyName, OutError,
		FEGQuestTestHelper::Int32ToString);

	PropertyName = TEXT("Int64Array");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveArrayEqual<int64>(Int64Array, Other.Int64Array, PropertyName, OutError,
		FEGQuestTestHelper::Int64ToString);

	PropertyName = TEXT("BoolArray");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveArrayEqual<bool>(BoolArray, Other.BoolArray, PropertyName, OutError,
		[](const bool& Value) -> FString { return FString::FromInt(Value); });

	PropertyName = TEXT("FloatArray");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveArrayEqual<float>(FloatArray, Other.FloatArray, PropertyName, OutError,
		FEGQuestTestHelper::FloatToString);

	PropertyName = TEXT("EnumArray");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveArrayEqual<EEGQuestTestEnum>(EnumArray, Other.EnumArray, PropertyName, OutError,
		[](const EEGQuestTestEnum& Value) -> FString { return FString::FromInt(static_cast<int32>(Value)); });

	PropertyName = TEXT("NameArray");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveArrayEqual<FName>(NameArray, Other.NameArray, PropertyName, OutError,
		FEGQuestTestHelper::NameToString);

	PropertyName = TEXT("StringArray");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveArrayEqual<FString>(StringArray, Other.StringArray, PropertyName, OutError,
		FEGQuestTestHelper::StringToString);

	// Clear error message
	if (bIsEqual)
	{
		OutError.Empty();
	}
	return bIsEqual;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestTestArrayStruct
void FEGQuestTestArrayComplex::GenerateRandomData(const FEGQuestIOTesterOptions& InOptions)
{
	Options = InOptions;
	StructArrayPrimitives.Empty();
	StructArrayOfArrayPrimitives.Empty();
	ObjectArrayPrimitivesBase.Empty();
	ObjectArrayPrimitivesAll.Empty();

	const int32 Num = FMath::RandHelper(20) + 3;
	static const TArray<UClass*> ObjectPrimitiveClassPool = {
		UEGQuestTestObjectPrimitivesBase::StaticClass(), UEGQuestTestObjectPrimitives_ChildA::StaticClass(),
		UEGQuestTestObjectPrimitives_ChildB::StaticClass(), UEGQuestTestObjectPrimitives_GrandChildA_Of_ChildA::StaticClass(), nullptr
	};
	for (int32 Index = 0; Index < Num; Index++)
	{
		// StructArrayPrimitives
		FEGQuestTestStructPrimitives StructPrimitives;
		StructPrimitives.GenerateRandomData(Options);
		StructArrayPrimitives.Add(StructPrimitives);

		//StructArrayOfArrayPrimitives
		FEGQuestTestArrayPrimitive ArrayOfArrayPrimitive;
		ArrayOfArrayPrimitive.GenerateRandomData(Options);
		StructArrayOfArrayPrimitives.Add(ArrayOfArrayPrimitive);

		// ObjectArrayPrimitivesBase
		{
			UEGQuestTestObjectPrimitivesBase* ObjectPrimitives = NewObject<UEGQuestTestObjectPrimitivesBase>();
			ObjectPrimitives->GenerateRandomData(Options);
			ObjectArrayPrimitivesBase.Add(ObjectPrimitives);
		}

		// ObjectArrayPrimitivesAll
		{
			UClass* ChosenClass = ObjectPrimitiveClassPool[FMath::RandHelper(ObjectPrimitiveClassPool.Num())];
			if (ChosenClass == nullptr)
			{
				ObjectArrayPrimitivesAll.Add(nullptr);
			}
			else
			{
				UEGQuestTestObjectPrimitivesBase* ObjectPrimitives = NewObject<UEGQuestTestObjectPrimitivesBase>((UObject*)GetTransientPackage(), ChosenClass);
				ObjectPrimitives->GenerateRandomData(Options);
				ObjectArrayPrimitivesAll.Add(ObjectPrimitives);
			}
		}

		// ObjectArrayFrequentsNulls
		{
			const bool bNull = FMath::RandBool();
			if (bNull)
			{
				ObjectArrayFrequentsNulls.Add(nullptr);
			}
			else
			{
				UEGQuestTestObjectPrimitivesBase* ObjectPrimitives = NewObject<UEGQuestTestObjectPrimitivesBase>();
				ObjectPrimitives->GenerateRandomData(Options);
				ObjectArrayFrequentsNulls.Add(ObjectPrimitives);
			}
		}
	}
}

bool FEGQuestTestArrayComplex::IsEqual(const Self& Other, FString& OutError) const
{
	FString PropertyName;
	bool bIsEqual = true;
	OutError += TEXT("FEGQuestTestArrayStruct::IsEqual:\n");

	PropertyName = TEXT("StructArrayPrimitives");
	bIsEqual &= FEGQuestTestHelper::IsComplexArrayEqual<FEGQuestTestStructPrimitives>(StructArrayPrimitives, Other.StructArrayPrimitives, PropertyName, OutError);

	PropertyName = TEXT("StructArrayOfArrayPrimitives");
	bIsEqual &= FEGQuestTestHelper::IsComplexArrayEqual<FEGQuestTestArrayPrimitive>(StructArrayOfArrayPrimitives, Other.StructArrayOfArrayPrimitives, PropertyName, OutError);

	PropertyName = TEXT("ObjectArrayPrimitivesBase");
	bIsEqual &= FEGQuestTestHelper::IsComplexPointerArrayEqual<UEGQuestTestObjectPrimitivesBase>(ObjectArrayPrimitivesBase, Other.ObjectArrayPrimitivesBase, PropertyName, OutError);

	PropertyName = TEXT("ObjectArrayPrimitivesAll");
	bIsEqual &= FEGQuestTestHelper::IsComplexPointerArrayEqual<UEGQuestTestObjectPrimitivesBase>(ObjectArrayPrimitivesAll, Other.ObjectArrayPrimitivesAll, PropertyName, OutError);

	PropertyName = TEXT("ObjectArrayFrequentsNulls");
	bIsEqual &= FEGQuestTestHelper::IsComplexPointerArrayEqual<UEGQuestTestObjectPrimitivesBase>(ObjectArrayFrequentsNulls, Other.ObjectArrayFrequentsNulls, PropertyName, OutError);

	// Clear error message
	if (bIsEqual)
	{
		OutError.Empty();
	}
	return bIsEqual;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestTestSetPrimitive
void FEGQuestTestSetPrimitive::GenerateRandomData(const FEGQuestIOTesterOptions& InOptions)
{
	Options = InOptions;
	Num1_Set = { FMath::Rand() };
	EmptySet.Empty();
	Int32Set.Empty();
	Int64Set.Empty();
	EnumSet.Empty();
	NameSet.Empty();
	StringSet.Empty();

	const int32 Num = FMath::RandHelper(10) + 2;
	for (int32 i = 0; i < Num; ++i)
	{
		Int32Set.Add(FMath::Rand());
		Int64Set.Add(FEGQuestHelper::RandomInt64());
		if (Options.bSupportsPureEnumContainer)
		{
			EnumSet.Add(static_cast<EEGQuestTestEnum>(FMath::RandHelper(static_cast<int32>(EEGQuestTestEnum::NumOf))));
		}
		const float Float = FMath::SRand();
		const FString String = FString::SanitizeFloat(Float);
		StringSet.Add(String);
		NameSet.Add(FName(*String));
	}
}

bool FEGQuestTestSetPrimitive::IsEqual(const Self& Other, FString& OutError) const
{
	FString PropertyName;
	bool bIsEqual = true;
	OutError += TEXT("FEGQuestTestSetPrimitive::IsEqual:\n");

	PropertyName = TEXT("EmptySet");
	if (EmptySet.Num() != Other.EmptySet.Num())
	{
		OutError = FString::Printf(TEXT("This.EmptySet.Num(%d) != Other.EmptySet.Num(%d). but why :O"),
			EmptySet.Num(), Other.EmptySet.Num());
		return false;
	}

	PropertyName = TEXT("Num1_Set");
	bIsEqual &= FEGQuestTestHelper::IsSetEqual<int32>(Num1_Set, Other.Num1_Set, PropertyName, OutError, FEGQuestTestHelper::Int32ToString);

	PropertyName = TEXT("Int32Set");
	bIsEqual &= FEGQuestTestHelper::IsSetEqual<int32>(Int32Set, Other.Int32Set, PropertyName, OutError, FEGQuestTestHelper::Int32ToString);

	PropertyName = TEXT("Int64Set");
	bIsEqual &= FEGQuestTestHelper::IsSetEqual<int64>(Int64Set, Other.Int64Set, PropertyName, OutError, FEGQuestTestHelper::Int64ToString);

	PropertyName = TEXT("EnumSet");
	bIsEqual &= FEGQuestTestHelper::IsSetEqual<EEGQuestTestEnum>(EnumSet, Other.EnumSet, PropertyName, OutError,
		[](const EEGQuestTestEnum& Value) -> FString { return FString::FromInt(static_cast<int32>(Value)); });

	PropertyName = TEXT("NameSet");
	bIsEqual &= FEGQuestTestHelper::IsSetEqual<FName>(NameSet, Other.NameSet, PropertyName, OutError, FEGQuestTestHelper::NameToString);

	PropertyName = TEXT("StringSet");
	bIsEqual &= FEGQuestTestHelper::IsSetEqual<FString>(StringSet, Other.StringSet, PropertyName, OutError, FEGQuestTestHelper::StringToString);

	if (bIsEqual)
	{
		OutError.Empty();
	}
	return bIsEqual;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestTestSetStruct
void FEGQuestTestSetComplex::GenerateRandomData(const FEGQuestIOTesterOptions& InOptions)
{
	Options = InOptions;
	StructSetPrimitives.Empty();

	if (!Options.bSupportsNonPrimitiveInSet)
	{
		return;
	}

	const int32 Num = FMath::RandHelper(10) + 2;
	for (int32 i = 0; i < Num; ++i)
	{
		FEGQuestTestStructPrimitives StructPrimitives;
		StructPrimitives.GenerateRandomData(Options);
		StructSetPrimitives.Add(StructPrimitives);
	}
}

bool FEGQuestTestSetComplex::IsEqual(const Self& Other, FString& OutError) const
{
	if (!Options.bSupportsNonPrimitiveInSet)
	{
		return true;
	}

	FString PropertyName;
	bool bIsEqual = true;
	OutError += TEXT("FEGQuestTestSetStruct::IsEqual:\n");

	PropertyName = TEXT("StructSetPrimitives");
	bIsEqual = bIsEqual && FEGQuestTestHelper::IsSetEqual<FEGQuestTestStructPrimitives>(
		StructSetPrimitives,
		Other.StructSetPrimitives,
		PropertyName,
		OutError,
		[](const FEGQuestTestStructPrimitives& Value) -> FString
		{
			return FString::Printf(TEXT("Struct(%s)"), *Value.ToString());
		}
	);

	// Clear error message
	if (bIsEqual)
	{
		OutError.Empty();
	}
	return bIsEqual;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestTestMapPrimitive
void FEGQuestTestMapPrimitive::GenerateRandomData(const FEGQuestIOTesterOptions& InOptions)
{
	Options = InOptions;
	EmptyMap.Empty();
	Int32ToInt32Map.Empty();
	Int64ToInt64Map.Empty();
	Int32ToStringMap.Empty();
	Int32ToNameMap.Empty();
	StringToInt32Map.Empty();
	StringToStringMap.Empty();
	NameToInt32Map.Empty();
	NameToNameMap.Empty();
	StringToFloatMap.Empty();
	Int32ToFloatMap.Empty();
	NameToColorMap.Empty();
	ObjectConstantNullMap.Empty();
	ObjectFrequentsNullsMap.Empty();
	ObjectPrimitivesAllMap.Empty();

	static TArray<UClass*> ObjectPrimitiveClassPool = {
		UEGQuestTestObjectPrimitivesBase::StaticClass(), UEGQuestTestObjectPrimitives_ChildA::StaticClass(),
		UEGQuestTestObjectPrimitives_ChildB::StaticClass(), UEGQuestTestObjectPrimitives_GrandChildA_Of_ChildA::StaticClass(), nullptr
	};
	const int32 Num = FMath::RandHelper(10) + 2;
	for (int32 i = 0; i < Num; ++i)
	{
		Int32ToInt32Map.Add(FMath::Rand(), FMath::Rand());
		Int64ToInt64Map.Add(FEGQuestHelper::RandomInt64(), FEGQuestHelper::RandomInt64());
		Int32ToStringMap.Add(FMath::Rand(), FString::SanitizeFloat(FMath::SRand()));
		Int32ToNameMap.Add(FMath::Rand(), FName(*FString::SanitizeFloat(FMath::SRand())));

		StringToInt32Map.Add(FString::FromInt(FMath::Rand()), FMath::Rand());
		StringToStringMap.Add(FString::FromInt(FMath::Rand()), FString::FromInt(FMath::Rand()));
		NameToInt32Map.Add(FName(*FString::FromInt(FMath::Rand())), FMath::Rand());
		NameToNameMap.Add(FName(*FString::FromInt(FMath::Rand())), FName(*FString::FromInt(FMath::Rand())));
		StringToFloatMap.Add(FString::FromInt(FMath::Rand()), FMath::SRand());
		Int32ToFloatMap.Add(FMath::Rand(), FMath::SRand());

		ObjectConstantNullMap.Add(FString::FromInt(FMath::Rand()), nullptr);

		if (Options.bSupportsColorPrimitives)
		{
			const FColor Color(FMath::Rand());
			NameToColorMap.Add(FName(*FString::FromInt(FMath::Rand())), Color);
		}

		// ObjectArrayPrimitivesAll
		if (Options.bSupportsUObjectValueInMap)
		{
			UClass* ChosenClass = ObjectPrimitiveClassPool[FMath::RandHelper(ObjectPrimitiveClassPool.Num())];
			if (ChosenClass == nullptr)
			{
				ObjectPrimitivesAllMap.Add(FString::FromInt(FMath::Rand()), nullptr);
			}
			else
			{
				UEGQuestTestObjectPrimitivesBase* ObjectPrimitives = NewObject<UEGQuestTestObjectPrimitivesBase>((UObject*)GetTransientPackage(), ChosenClass);
				ObjectPrimitives->GenerateRandomData(Options);
				ObjectPrimitivesAllMap.Add(FString::FromInt(FMath::Rand()), ObjectPrimitives);
			}
		}

		// ObjectFrequentsNullsMap
		if (Options.bSupportsUObjectValueInMap)
		{
			const bool bNull = FMath::RandBool();
			if (bNull)
			{
				ObjectFrequentsNullsMap.Add(FString::FromInt(FMath::Rand()), nullptr);
			}
			else
			{
				UEGQuestTestObjectPrimitivesBase* ObjectPrimitives = NewObject<UEGQuestTestObjectPrimitivesBase>();
				ObjectPrimitives->GenerateRandomData(Options);
				ObjectFrequentsNullsMap.Add(FString::FromInt(FMath::Rand()), ObjectPrimitives);
			}
		}
	}
	CheckInvariants();
}

void FEGQuestTestMapPrimitive::CheckInvariants() const
{
	FEGQuestTestHelper::CheckMapStringKeyInvariants(StringToInt32Map);
	FEGQuestTestHelper::CheckMapStringKeyInvariants(StringToStringMap);
	FEGQuestTestHelper::CheckMapStringKeyInvariants(StringToFloatMap);
	FEGQuestTestHelper::CheckMapStringKeyInvariants(ObjectConstantNullMap);
}

bool FEGQuestTestMapPrimitive::IsEqual(const Self& Other, FString& OutError) const
{
	CheckInvariants();
	Other.CheckInvariants();

	FString PropertyName;
	bool bIsEqual = true;
	OutError += TEXT("FEGQuestTestMapPrimitive::IsEqual:\n");

	PropertyName = TEXT("EmptyMap");
	if (EmptyMap.Num() != Other.EmptyMap.Num())
	{
		OutError = FString::Printf(TEXT("This.EmptyMap.Num(%d) != Other.EmptyMap.Num(%d). but why :O"),
			EmptyMap.Num(), Other.EmptyMap.Num());
		return false;
	}

	PropertyName = TEXT("Int32ToInt32Map");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveMapEqual<int32, int32>(
		Int32ToInt32Map,
		Other.Int32ToInt32Map,
		PropertyName,
		OutError,
		FEGQuestTestHelper::Int32ToString,
		FEGQuestTestHelper::Int32ToString
	);

	PropertyName = TEXT("Int64ToInt64Map");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveMapEqual<int64, int64>(
		Int64ToInt64Map,
		Other.Int64ToInt64Map,
		PropertyName,
		OutError,
		FEGQuestTestHelper::Int64ToString,
		FEGQuestTestHelper::Int64ToString
	);

	PropertyName = TEXT("Int32ToStringMap");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveMapEqual<int32, FString>(
		Int32ToStringMap,
		Other.Int32ToStringMap,
		PropertyName,
		OutError,
		FEGQuestTestHelper::Int32ToString,
		FEGQuestTestHelper::StringToString
	);

	PropertyName = TEXT("Int32ToNameMap");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveMapEqual<int32, FName>(
		Int32ToNameMap,
		Other.Int32ToNameMap,
		PropertyName,
		OutError,
		FEGQuestTestHelper::Int32ToString,
		FEGQuestTestHelper::NameToString
	);

	PropertyName = TEXT("NameToInt32Map");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveMapEqual<FName, int32>(
		NameToInt32Map,
		Other.NameToInt32Map,
		PropertyName,
		OutError,
		FEGQuestTestHelper::NameToString,
		FEGQuestTestHelper::Int32ToString
	);

	PropertyName = TEXT("NameToNameMap");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveMapEqual<FName, FName>(
		NameToNameMap,
		Other.NameToNameMap,
		PropertyName,
		OutError,
		FEGQuestTestHelper::NameToString,
		FEGQuestTestHelper::NameToString
	);

	PropertyName = TEXT("StringToInt32Map");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveMapEqual<FString, int32>(
		StringToInt32Map,
		Other.StringToInt32Map,
		PropertyName,
		OutError,
		FEGQuestTestHelper::StringToString,
		FEGQuestTestHelper::Int32ToString
	);

	PropertyName = TEXT("StringToStringMap");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveMapEqual<FString, FString>(
		StringToStringMap,
		Other.StringToStringMap,
		PropertyName,
		OutError,
		FEGQuestTestHelper::StringToString,
		FEGQuestTestHelper::StringToString
	);

	PropertyName = TEXT("StringToFloatMap");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveMapEqual<FString, float>(
		StringToFloatMap,
		Other.StringToFloatMap,
		PropertyName,
		OutError,
		FEGQuestTestHelper::StringToString,
		FEGQuestTestHelper::FloatToString
	);

	PropertyName = TEXT("IntToFloatMap");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveMapEqual<int32, float>(
		Int32ToFloatMap,
		Other.Int32ToFloatMap,
		PropertyName,
		OutError,
		FEGQuestTestHelper::Int32ToString,
		FEGQuestTestHelper::FloatToString
	);

	PropertyName = TEXT("NameToColorMap");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveMapEqual<FName, FColor>(
		NameToColorMap,
		Other.NameToColorMap,
		PropertyName,
		OutError,
		FEGQuestTestHelper::NameToString,
		FEGQuestTestHelper::ColorToString
	);

	PropertyName = TEXT("ObjectConstantNullMap");
	bIsEqual &= FEGQuestTestHelper::IsComplexPointersMapEqual<FString, UEGQuestTestObjectPrimitivesBase>(
		ObjectConstantNullMap,
		Other.ObjectConstantNullMap,
		PropertyName,
		OutError,
		FEGQuestTestHelper::StringToString
	);

	PropertyName = TEXT("ObjectFrequentsNullsMap");
	bIsEqual &= FEGQuestTestHelper::IsComplexPointersMapEqual<FString, UEGQuestTestObjectPrimitivesBase>(
		ObjectFrequentsNullsMap,
		Other.ObjectFrequentsNullsMap,
		PropertyName,
		OutError,
		FEGQuestTestHelper::StringToString
	);

	PropertyName = TEXT("ObjectPrimitivesAllMap");
	bIsEqual &= FEGQuestTestHelper::IsComplexPointersMapEqual<FString, UEGQuestTestObjectPrimitivesBase>(
		ObjectPrimitivesAllMap,
		Other.ObjectPrimitivesAllMap,
		PropertyName,
		OutError,
		FEGQuestTestHelper::StringToString
	);

	// Clear error message
	if (bIsEqual)
	{
		OutError.Empty();
	}
	return bIsEqual;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestTestMapStruct
void FEGQuestTestMapComplex::GenerateRandomData(const FEGQuestIOTesterOptions& InOptions)
{
	Options = InOptions;
	Int32ToStructPrimitiveMap.Empty();
	StructPrimitiveToIntMap.Empty();
	NameToStructPrimitiveMap.Empty();
	NameToStructOfMapPrimitives.Empty();
	NameToStructOfArrayPrimitives.Empty();
	NameToStructOfSetPrimitives.Empty();
	NameToStructOfArrayComplex.Empty();
	NameToStructOfSetComplex.Empty();

	const int32 Num = FMath::RandHelper(10) + 2;
	for (int32 i = 0; i < Num; ++i)
	{
		const FString StringKey = FString::FromInt(FMath::Rand());
		FEGQuestTestStructPrimitives StructPrimitives;
		StructPrimitives.GenerateRandomData(Options);

		Int32ToStructPrimitiveMap.Add(FMath::Rand(), StructPrimitives);
		NameToStructPrimitiveMap.Add(FName(*StringKey), StructPrimitives);
		StructPrimitiveToIntMap.Add(StructPrimitives, FMath::Rand());

		FEGQuestTestMapPrimitive MapPrimitive;
		MapPrimitive.GenerateRandomData(Options);
		NameToStructOfMapPrimitives.Add(FName(*StringKey), MapPrimitive);

		FEGQuestTestArrayPrimitive ArrayPrimitive;
		ArrayPrimitive.GenerateRandomData(Options);
		NameToStructOfArrayPrimitives.Add(FName(*StringKey), ArrayPrimitive);

		FEGQuestTestSetPrimitive SetPrimitive;
		SetPrimitive.GenerateRandomData(Options);
		NameToStructOfSetPrimitives.Add(FName(*StringKey), SetPrimitive);

		FEGQuestTestArrayComplex ArrayComplex;
		ArrayComplex.GenerateRandomData(Options);
		NameToStructOfArrayComplex.Add(FName(*StringKey), ArrayComplex);

		FEGQuestTestSetComplex SetComplex;
		SetComplex.GenerateRandomData(Options);
		NameToStructOfSetComplex.Add(FName(*StringKey), SetComplex);
	}
}

bool FEGQuestTestMapComplex::IsEqual(const Self& Other, FString& OutError) const
{
	FString PropertyName;
	bool bIsEqual = true;
	OutError += TEXT("FEGQuestTestMapStruct::IsEqual:\n");

	auto StructPrimitiveToString = [](const FEGQuestTestStructPrimitives& Value) -> FString
	{
		return FString::Printf(TEXT("Struct(%s)"), *Value.ToString());
	};
	auto StructMapPrimitiveToString = [](const FEGQuestTestMapPrimitive& Value) -> FString
	{
		return TEXT("FEGQuestTestMapPrimitive");
	};
	auto StructArrayPrimitiveToString = [](const FEGQuestTestArrayPrimitive& Value) -> FString
	{
		return TEXT("FEGQuestTestArrayPrimitive");
	};
	auto StructSetPrimitiveToString = [](const FEGQuestTestSetPrimitive& Value) -> FString
	{
		return TEXT("FEGQuestTestSetPrimitive");
	};
	auto StructArrayComplexToString = [](const FEGQuestTestArrayComplex& Value) -> FString
	{
		return TEXT("FEGQuestTestArrayComplex");
	};
	auto StructSetComplexToString = [](const FEGQuestTestSetComplex& Value) -> FString
	{
		return TEXT("FEGQuestTestSetComplex");
	};

	PropertyName = TEXT("Int32ToStructPrimitiveMap");
	bIsEqual &= FEGQuestTestHelper::IsComplexMapValueEqual<int32, FEGQuestTestStructPrimitives>(
		Int32ToStructPrimitiveMap,
		Other.Int32ToStructPrimitiveMap,
		PropertyName,
		OutError,
		FEGQuestTestHelper::Int32ToString,
		StructPrimitiveToString
	);

	PropertyName = TEXT("NameToStructPrimitiveMap");
	bIsEqual &= FEGQuestTestHelper::IsComplexMapValueEqual<FName, FEGQuestTestStructPrimitives>(
		NameToStructPrimitiveMap,
		Other.NameToStructPrimitiveMap,
		PropertyName,
		OutError,
		FEGQuestTestHelper::NameToString,
		StructPrimitiveToString
	);

	PropertyName = TEXT("StructPrimitiveToIntMap");
	bIsEqual &= FEGQuestTestHelper::IsPrimitiveMapEqual<FEGQuestTestStructPrimitives, int32>(
		StructPrimitiveToIntMap,
		Other.StructPrimitiveToIntMap,
		PropertyName,
		OutError,
		StructPrimitiveToString,
		FEGQuestTestHelper::Int32ToString
	);

	PropertyName = TEXT("NameToStructOfMapPrimitives");
	bIsEqual &= FEGQuestTestHelper::IsComplexMapValueEqual<FName, FEGQuestTestMapPrimitive>(
		NameToStructOfMapPrimitives,
		Other.NameToStructOfMapPrimitives,
		PropertyName,
		OutError,
		FEGQuestTestHelper::NameToString,
		StructMapPrimitiveToString
	);

	PropertyName = TEXT("NameToStructOfArrayPrimitives");
	bIsEqual &= FEGQuestTestHelper::IsComplexMapValueEqual<FName, FEGQuestTestArrayPrimitive>(
		NameToStructOfArrayPrimitives,
		Other.NameToStructOfArrayPrimitives,
		PropertyName,
		OutError,
		FEGQuestTestHelper::NameToString,
		StructArrayPrimitiveToString
	);

	PropertyName = TEXT("NameToStructOfSetPrimitives");
	bIsEqual &= FEGQuestTestHelper::IsComplexMapValueEqual<FName, FEGQuestTestSetPrimitive>(
		NameToStructOfSetPrimitives,
		Other.NameToStructOfSetPrimitives,
		PropertyName,
		OutError,
		FEGQuestTestHelper::NameToString,
		StructSetPrimitiveToString
	);

	PropertyName = TEXT("NameToStructOfArrayComplex");
	bIsEqual &= FEGQuestTestHelper::IsComplexMapValueEqual<FName, FEGQuestTestArrayComplex>(
		NameToStructOfArrayComplex,
		Other.NameToStructOfArrayComplex,
		PropertyName,
		OutError,
		FEGQuestTestHelper::NameToString,
		StructArrayComplexToString
	);

	PropertyName = TEXT("NameToStructOfSetComplex");
	bIsEqual &= FEGQuestTestHelper::IsComplexMapValueEqual<FName, FEGQuestTestSetComplex>(
		NameToStructOfSetComplex,
		Other.NameToStructOfSetComplex,
		PropertyName,
		OutError,
		FEGQuestTestHelper::NameToString,
		StructSetComplexToString
	);

	// Clear error message
	if (bIsEqual)
	{
		OutError.Empty();
	}
	return bIsEqual;
}
