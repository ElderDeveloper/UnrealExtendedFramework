// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"

#include "EGQuestThumbnailRenderer.generated.h"

// Useful if you want to have fancy thumbnail previews.
UCLASS()
class UNREALEXTENDEDQUESTEDITOR_API UEGQuestThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

public:
	UEGQuestThumbnailRenderer(const FObjectInitializer& ObjectInitializer);

};
