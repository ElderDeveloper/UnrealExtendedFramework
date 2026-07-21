#include "BTComposite_SelectRandomCount.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTComposite_SelectRandomCount::UBTComposite_SelectRandomCount(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    NodeName = "Select Random Count";
}

int32 UBTComposite_SelectRandomCount::GetNextChildHandler(FBehaviorTreeSearchData& SearchData, int32 PrevChild, EBTNodeResult::Type LastResult) const
{
    const int32 ChildCount = GetChildrenNum();
    if (ChildCount == 0) return BTSpecialChild::ReturnToParent;

    UBlackboardComponent* BB = SearchData.OwnerComp.GetBlackboardComponent();
    if (!BB) return BTSpecialChild::ReturnToParent;

    int32 CurrentIdx = BB->GetValueAsInt(CurrentChildIndexKey.SelectedKeyName);
    int32 Count = BB->GetValueAsInt(ConsecutiveCountKey.SelectedKeyName);
    int32 Threshold = BB->GetValueAsInt(CurrentThresholdKey.SelectedKeyName);

    // --- DURUM 1: EQS (Çocuk) BÝTTÝ ---
    if (PrevChild != BTSpecialChild::NotInitialized)
    {
        Count++;
        BB->SetValueAsInt(ConsecutiveCountKey.SelectedKeyName, Count);

        UE_LOG(LogTemp, Log, TEXT("BOSS LOGIC: Child Index %d tamamlandi. Tur: %d/%d"), CurrentIdx, Count, Threshold);

        if (Count >= Threshold)
        {
            int32 NextIdx = (CurrentIdx + 1) % ChildCount;
            BB->SetValueAsInt(CurrentChildIndexKey.SelectedKeyName, NextIdx);
            BB->SetValueAsInt(ConsecutiveCountKey.SelectedKeyName, 0);
            BB->SetValueAsInt(CurrentThresholdKey.SelectedKeyName, 0);

            UE_LOG(LogTemp, Warning, TEXT("BOSS LOGIC: Hedef tura ulasildi. Bir sonraki secim Index: %d olacak."), NextIdx);
        }

        return BTSpecialChild::ReturnToParent;
    }

    // --- DURUM 2: DÜÐÜME GÝRÝÞ (ZAR ATMA VE SEÇÝM) ---
    if (Threshold <= 0)
    {
        int32 Min = MinRepeat.IsValidIndex(CurrentIdx) ? MinRepeat[CurrentIdx] : 2;
        int32 Max = MaxRepeat.IsValidIndex(CurrentIdx) ? MaxRepeat[CurrentIdx] : 5;
        Threshold = FMath::RandRange(Min, Max);
        BB->SetValueAsInt(CurrentThresholdKey.SelectedKeyName, Threshold);

        UE_LOG(LogTemp, Log, TEXT("BOSS LOGIC: Yeni dongu basliyor. Secili Index: %d, Belirlenen Tur: %d"), CurrentIdx, Threshold);
    }

    return CurrentIdx;
}

#if WITH_EDITOR
void UBTComposite_SelectRandomCount::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedEvent);
    while (MinRepeat.Num() < GetChildrenNum()) MinRepeat.Add(2);
    while (MaxRepeat.Num() < GetChildrenNum()) MaxRepeat.Add(5);
}

FString UBTComposite_SelectRandomCount::GetStaticDescription() const
{
    FString Description = TEXT("Sequential Random Repeat:\n");

    const int32 Count = GetChildrenNum();

    for (int32 i = 0; i < Count; i++)
    {
        // Fonksiyon yerine doðrudan dizi kontrolü yaparak okuyoruz
        int32 Min = MinRepeat.IsValidIndex(i) ? MinRepeat[i] : 2;
        int32 Max = MaxRepeat.IsValidIndex(i) ? MaxRepeat[i] : 5;

        Description += FString::Printf(TEXT("[%d]: %d-%d times\n"), i, Min, Max);
    }
    return Description;
}
#endif