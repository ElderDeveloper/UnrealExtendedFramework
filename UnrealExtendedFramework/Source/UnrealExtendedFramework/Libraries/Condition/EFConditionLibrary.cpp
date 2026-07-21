// Fill out your copyright notice in the Description page of Project Settings.


#include "EFConditionLibrary.h"

bool UEFConditionLibrary::IsPlayingInEditor()
{
	#if WITH_EDITOR
		return true;
	#else
		return false;
	#endif
}
