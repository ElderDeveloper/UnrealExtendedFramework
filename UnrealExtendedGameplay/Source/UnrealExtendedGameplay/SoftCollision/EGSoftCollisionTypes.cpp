// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSoftCollisionTypes.h"

DEFINE_LOG_CATEGORY(LogEGSoftCollision);

int32 FEGSoftCollisionModifierStack::Push(const FEGSoftCollisionModifier& Modifier)
{
	NextId = NextId == MAX_int32 ? 1 : NextId + 1;
	Modifiers.Emplace(NextId, Modifier);
	return NextId;
}

bool FEGSoftCollisionModifierStack::Remove(int32 Id)
{
	return Id != 0 && Modifiers.RemoveAll([Id](const TPair<int32, FEGSoftCollisionModifier>& Entry) { return Entry.Key == Id; }) > 0;
}

FEGSoftCollisionModifier FEGSoftCollisionModifierStack::GetCombined() const
{
	FEGSoftCollisionModifier Combined;
	for (const TPair<int32, FEGSoftCollisionModifier>& Entry : Modifiers)
	{
		Combined.WeightScale *= FMath::Max(Entry.Value.WeightScale, 0.01f);
		Combined.PushPowerScale *= FMath::Max(Entry.Value.PushPowerScale, 0.0f);
		Combined.bImmovable |= Entry.Value.bImmovable;
		Combined.bPushesNobody |= Entry.Value.bPushesNobody;
	}
	return Combined;
}
