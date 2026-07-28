// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianScreenshot.h"

#include "ExtendedAtlassianLog.h"

#include "Editor.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "UnrealClient.h"

bool FExtendedAtlassianScreenshot::CaptureActiveViewport(TArray<uint8>& OutPngData, FIntPoint& OutSize)
{
	OutPngData.Reset();
	OutSize = FIntPoint::ZeroValue;

	if (!GEditor)
	{
		return false;
	}

	FViewport* Viewport = GEditor->GetPIEViewport();
	if (!Viewport)
	{
		Viewport = GEditor->GetActiveViewport();
	}

	if (!Viewport)
	{
		UE_LOG(LogExtendedAtlassian, Warning, TEXT("No active viewport to capture."));
		return false;
	}

	const FIntPoint Size = Viewport->GetSizeXY();
	if (Size.X <= 0 || Size.Y <= 0)
	{
		return false;
	}

	TArray<FColor> Pixels;
	if (!Viewport->ReadPixels(Pixels, FReadSurfaceDataFlags(), FIntRect(0, 0, Size.X, Size.Y)))
	{
		UE_LOG(LogExtendedAtlassian, Warning, TEXT("Viewport ReadPixels failed."));
		return false;
	}

	if (Pixels.Num() < Size.X * Size.Y)
	{
		UE_LOG(LogExtendedAtlassian, Warning, TEXT("Viewport returned fewer pixels than its reported size."));
		return false;
	}

	// Scene alpha is not meaningful here; a PNG that honoured it would come out largely transparent.
	for (FColor& Pixel : Pixels)
	{
		Pixel.A = 255;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid())
	{
		return false;
	}

	if (!ImageWrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Size.X, Size.Y, ERGBFormat::BGRA, 8))
	{
		UE_LOG(LogExtendedAtlassian, Warning, TEXT("Could not hand the captured pixels to the PNG wrapper."));
		return false;
	}

	const TArray64<uint8>& Compressed = ImageWrapper->GetCompressed(100);
	if (Compressed.Num() == 0)
	{
		return false;
	}

	OutPngData.Append(Compressed.GetData(), Compressed.Num());
	OutSize = Size;

	UE_LOG(LogExtendedAtlassian, Verbose, TEXT("Captured %dx%d viewport into %d KB of PNG."),
		Size.X, Size.Y, OutPngData.Num() / 1024);

	return true;
}
