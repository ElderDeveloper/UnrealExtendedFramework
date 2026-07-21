// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "EGQuestTargetRegistry.h"
#include "EGQuestRole.generated.h"

class UEGQuestComponent;

UENUM(BlueprintType)
enum class EEGQuestRoleSelection : uint8 { Nearest, Random, All };

UENUM(BlueprintType)
enum class EEGQuestRoleLossPolicy : uint8 { ReResolve, Suspend, Notify };

USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestRoleBinding
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Roles") FName RoleName = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Roles") TArray<FEGQuestEntityHandle> Handles;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Roles") bool bStageScoped = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Roles") FName ScopeTrackName = NAME_None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Roles") FGuid ScopeStageGuid;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Roles") EEGQuestRoleLossPolicy LossPolicy = EEGQuestRoleLossPolicy::ReResolve;
	/** Prevents Notify loss policies from spamming until the binding resolves again. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Roles") bool bLossReported = false;
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestRoleMarker
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Roles") FName RoleName = NAME_None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Roles") FEGQuestEntityHandle Handle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Roles") FTransform Transform = FTransform::Identity;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Roles") bool bResolved = false;
};

struct FEGQuestRoleResolveContext
{
	UWorld* World = nullptr;
	UEGQuestComponent* QuestComponent = nullptr;
	FGuid RunId;
	FTransform Origin = FTransform::Identity;
	const TArray<FEGQuestRoleBinding>* ExistingBindings = nullptr;
};

UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class UNREALEXTENDEDQUEST_API UEGQuestRoleResolver : public UObject
{
	GENERATED_BODY()
public:
	virtual bool Resolve(const FEGQuestRoleResolveContext& Context, FName RoleName, TArray<FEGQuestEntityHandle>& OutHandles) const;
};

UCLASS(DisplayName = "Registry Query")
class UNREALEXTENDEDQUEST_API UEGQuestRoleResolver_RegistryQuery : public UEGQuestRoleResolver
{
	GENERATED_BODY()
public:
	bool Resolve(const FEGQuestRoleResolveContext& Context, FName RoleName, TArray<FEGQuestEntityHandle>& OutHandles) const override;
	UPROPERTY(EditAnywhere, Category = "Quest|Roles") FGameplayTagQuery TargetQuery;
	UPROPERTY(EditAnywhere, Category = "Quest|Roles") EEGQuestRoleSelection Selection = EEGQuestRoleSelection::Nearest;
	UPROPERTY(EditAnywhere, Category = "Quest|Roles", Meta = (ClampMin = "1")) int32 MaxResults = 1;
};

UCLASS(DisplayName = "Players")
class UNREALEXTENDEDQUEST_API UEGQuestRoleResolver_Players : public UEGQuestRoleResolver
{
	GENERATED_BODY()
public:
	bool Resolve(const FEGQuestRoleResolveContext& Context, FName RoleName, TArray<FEGQuestEntityHandle>& OutHandles) const override;
};

UCLASS(DisplayName = "Explicit Actor")
class UNREALEXTENDEDQUEST_API UEGQuestRoleResolver_Explicit : public UEGQuestRoleResolver
{
	GENERATED_BODY()
public:
	bool Resolve(const FEGQuestRoleResolveContext& Context, FName RoleName, TArray<FEGQuestEntityHandle>& OutHandles) const override;
	UPROPERTY(EditAnywhere, Category = "Quest|Roles") TSoftObjectPtr<AActor> Actor;
};

UCLASS(DisplayName = "From Fact Or Role")
class UNREALEXTENDEDQUEST_API UEGQuestRoleResolver_FromFact : public UEGQuestRoleResolver
{
	GENERATED_BODY()
public:
	bool Resolve(const FEGQuestRoleResolveContext& Context, FName RoleName, TArray<FEGQuestEntityHandle>& OutHandles) const override;
	UPROPERTY(EditAnywhere, Category = "Quest|Roles") FName SourceRole = NAME_None;
	UPROPERTY(EditAnywhere, Category = "Quest|Roles") FGameplayTag FactTag;
	UPROPERTY(EditAnywhere, Category = "Quest|Roles") TMap<int32, FGameplayTag> FactValueTargetTags;
};

UCLASS(Abstract, Blueprintable, DisplayName = "Scripted")
class UNREALEXTENDEDQUEST_API UEGQuestRoleResolver_Scripted : public UEGQuestRoleResolver
{
	GENERATED_BODY()
public:
	bool Resolve(const FEGQuestRoleResolveContext& Context, FName RoleName, TArray<FEGQuestEntityHandle>& OutHandles) const override;
	UFUNCTION(BlueprintNativeEvent, Category = "Quest|Roles")
	bool ResolveRole(UEGQuestComponent* QuestComponent, FGuid RunId, FName RoleName, UPARAM(ref) TArray<FEGQuestEntityHandle>& OutHandles) const;
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestRoleDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Roles") FName RoleName = NAME_None;
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Quest|Roles") TObjectPtr<UEGQuestRoleResolver> Resolver = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Roles") bool bRequired = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Roles") EEGQuestRoleLossPolicy LossPolicy = EEGQuestRoleLossPolicy::ReResolve;
};
