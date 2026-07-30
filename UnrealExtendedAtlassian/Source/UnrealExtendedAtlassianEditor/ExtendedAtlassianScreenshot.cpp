// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianScreenshot.h"

#include "ExtendedAtlassianLog.h"

#include "Editor.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace ExtendedAtlassianScreenshotPrivate
{
	const FColor AnnotationColors[] = {
		FColor(227, 165, 74),
		FColor(88, 166, 255),
		FColor(87, 204, 138),
		FColor(240, 102, 95),
		FColor(182, 169, 255)
	};

	void PutPixel(
		TArray<FColor>& Pixels,
		const FIntPoint& Size,
		int32 X,
		int32 Y,
		const FColor& Color)
	{
		if (X >= 0 && X < Size.X && Y >= 0 && Y < Size.Y)
		{
			Pixels[Y * Size.X + X] = Color;
		}
	}

	void DrawCircle(
		TArray<FColor>& Pixels,
		const FIntPoint& Size,
		int32 CenterX,
		int32 CenterY,
		int32 Radius,
		const FColor& Color)
	{
		const int32 RadiusSquared = Radius * Radius;
		for (int32 Y = -Radius; Y <= Radius; ++Y)
		{
			for (int32 X = -Radius; X <= Radius; ++X)
			{
				if (X * X + Y * Y <= RadiusSquared)
				{
					PutPixel(Pixels, Size, CenterX + X, CenterY + Y, Color);
				}
			}
		}
	}

	const TCHAR* DigitRows(int32 Digit)
	{
		static const TCHAR* Digits[] = {
			TEXT("111101101101111"),
			TEXT("010110010010111"),
			TEXT("111001111100111"),
			TEXT("111001111001111"),
			TEXT("101101111001001"),
			TEXT("111100111001111"),
			TEXT("111100111101111"),
			TEXT("111001001001001"),
			TEXT("111101111101111"),
			TEXT("111101111001111")
		};
		return Digits[FMath::Clamp(Digit, 0, 9)];
	}

	void DrawNumber(
		TArray<FColor>& Pixels,
		const FIntPoint& Size,
		int32 CenterX,
		int32 CenterY,
		int32 Number,
		int32 Scale)
	{
		const FString Text = FString::FromInt(Number);
		const int32 DigitWidth = 3 * Scale;
		const int32 Gap = Scale;
		const int32 TotalWidth =
			Text.Len() * DigitWidth + FMath::Max(0, Text.Len() - 1) * Gap;
		int32 StartX = CenterX - TotalWidth / 2;
		const int32 StartY = CenterY - (5 * Scale) / 2;
		for (int32 TextIndex = 0; TextIndex < Text.Len(); ++TextIndex)
		{
			const int32 Digit = Text[TextIndex] - TEXT('0');
			const TCHAR* Rows = DigitRows(Digit);
			for (int32 Row = 0; Row < 5; ++Row)
			{
				for (int32 Column = 0; Column < 3; ++Column)
				{
					if (Rows[Row * 3 + Column] != TEXT('1'))
					{
						continue;
					}
					for (int32 DY = 0; DY < Scale; ++DY)
					{
						for (int32 DX = 0; DX < Scale; ++DX)
						{
							PutPixel(
								Pixels,
								Size,
								StartX + Column * Scale + DX,
								StartY + Row * Scale + DY,
								FColor(15, 17, 20));
						}
					}
				}
			}
			StartX += DigitWidth + Gap;
		}
	}

	void DrawRectBorder(
		TArray<FColor>& Pixels,
		const FIntPoint& Size,
		const FIntRect& Rect,
		int32 Thickness,
		const FColor& Color)
	{
		for (int32 T = 0; T < Thickness; ++T)
		{
			for (int32 X = Rect.Min.X + T; X < Rect.Max.X - T; ++X)
			{
				PutPixel(Pixels, Size, X, Rect.Min.Y + T, Color);
				PutPixel(Pixels, Size, X, Rect.Max.Y - 1 - T, Color);
			}
			for (int32 Y = Rect.Min.Y + T; Y < Rect.Max.Y - T; ++Y)
			{
				PutPixel(Pixels, Size, Rect.Min.X + T, Y, Color);
				PutPixel(Pixels, Size, Rect.Max.X - 1 - T, Y, Color);
			}
		}
	}

	void BlurRect(
		TArray<FColor>& Pixels,
		const FIntPoint& Size,
		const FIntRect& Rect)
	{
		const TArray<FColor> Source = Pixels;
		for (int32 Y = Rect.Min.Y; Y < Rect.Max.Y; ++Y)
		{
			for (int32 X = Rect.Min.X; X < Rect.Max.X; ++X)
			{
				uint32 R = 0;
				uint32 G = 0;
				uint32 B = 0;
				uint32 Count = 0;
				for (int32 DY = -4; DY <= 4; ++DY)
				{
					for (int32 DX = -4; DX <= 4; ++DX)
					{
						const int32 SampleX = FMath::Clamp(X + DX, 0, Size.X - 1);
						const int32 SampleY = FMath::Clamp(Y + DY, 0, Size.Y - 1);
						const FColor& Sample = Source[SampleY * Size.X + SampleX];
						R += Sample.R;
						G += Sample.G;
						B += Sample.B;
						++Count;
					}
				}
				Pixels[Y * Size.X + X] =
					FColor(R / Count, G / Count, B / Count, 255);
			}
		}
	}
}

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

bool FExtendedAtlassianScreenshot::BurnAnnotations(
	const TArray<uint8>& SourcePng,
	const FIntPoint& SourceSize,
	const TArray<FExtendedAtlassianAnnotation>& Annotations,
	TArray<uint8>& OutAnnotatedPng)
{
	using namespace ExtendedAtlassianScreenshotPrivate;
	OutAnnotatedPng.Reset();
	if (SourcePng.IsEmpty() || SourceSize.X <= 0 || SourceSize.Y <= 0)
	{
		return false;
	}

	IImageWrapperModule& ImageWrapperModule =
		FModuleManager::LoadModuleChecked<IImageWrapperModule>(
			TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> Decoder =
		ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!Decoder.IsValid()
		|| !Decoder->SetCompressed(SourcePng.GetData(), SourcePng.Num()))
	{
		return false;
	}
	TArray64<uint8> Raw;
	if (!Decoder->GetRaw(ERGBFormat::BGRA, 8, Raw)
		|| Raw.Num()
			< static_cast<int64>(SourceSize.X)
				* static_cast<int64>(SourceSize.Y)
				* static_cast<int64>(sizeof(FColor)))
	{
		return false;
	}
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(SourceSize.X * SourceSize.Y);
	FMemory::Memcpy(
		Pixels.GetData(),
		Raw.GetData(),
		Pixels.Num() * sizeof(FColor));

	const float ScaleX = SourceSize.X / 528.0f;
	const float ScaleY = SourceSize.Y / 288.0f;
	const int32 PinRadius =
		FMath::Max(6, FMath::RoundToInt(11.0f * FMath::Min(ScaleX, ScaleY)));
	const int32 BorderThickness =
		FMath::Max(2, FMath::RoundToInt(2.0f * FMath::Min(ScaleX, ScaleY)));
	int32 PinNumber = 0;
	for (const FExtendedAtlassianAnnotation& Annotation : Annotations)
	{
		const int32 CenterX = FMath::Clamp(
			FMath::RoundToInt(Annotation.NormalizedPosition.X * SourceSize.X),
			0,
			SourceSize.X - 1);
		const int32 CenterY = FMath::Clamp(
			FMath::RoundToInt(Annotation.NormalizedPosition.Y * SourceSize.Y),
			0,
			SourceSize.Y - 1);
		const FColor Color = AnnotationColors[
			FMath::Abs(Annotation.ColorIndex)
				% UE_ARRAY_COUNT(AnnotationColors)];
		if (Annotation.Kind == EExtendedAtlassianAnnotationKind::Pin)
		{
			++PinNumber;
			DrawCircle(
				Pixels,
				SourceSize,
				CenterX,
				CenterY,
				PinRadius,
				Color);
			DrawNumber(
				Pixels,
				SourceSize,
				CenterX,
				CenterY,
				PinNumber,
				FMath::Max(1, PinRadius / 8));
			continue;
		}

		const int32 HalfWidth =
			FMath::Max(8, FMath::RoundToInt(46.0f * ScaleX));
		const int32 HalfHeight =
			FMath::Max(8, FMath::RoundToInt(31.0f * ScaleY));
		const FIntRect Rect(
			FMath::Clamp(CenterX - HalfWidth, 0, SourceSize.X - 1),
			FMath::Clamp(CenterY - HalfHeight, 0, SourceSize.Y - 1),
			FMath::Clamp(CenterX + HalfWidth, 1, SourceSize.X),
			FMath::Clamp(CenterY + HalfHeight, 1, SourceSize.Y));
		if (Annotation.Kind == EExtendedAtlassianAnnotationKind::Blur)
		{
			BlurRect(Pixels, SourceSize, Rect);
		}
		DrawRectBorder(
			Pixels,
			SourceSize,
			Rect,
			BorderThickness,
			Color);
	}

	const TSharedPtr<IImageWrapper> Encoder =
		ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!Encoder.IsValid()
		|| !Encoder->SetRaw(
			Pixels.GetData(),
			Pixels.Num() * sizeof(FColor),
			SourceSize.X,
			SourceSize.Y,
			ERGBFormat::BGRA,
			8))
	{
		return false;
	}
	const TArray64<uint8>& Compressed = Encoder->GetCompressed(100);
	if (Compressed.IsEmpty())
	{
		return false;
	}
	OutAnnotatedPng.Append(Compressed.GetData(), Compressed.Num());
	return true;
}

TSharedPtr<FSlateDynamicImageBrush>
FExtendedAtlassianScreenshot::CreatePreviewBrush(
	const TArray<uint8>& SourcePng,
	const FVector2D& PreviewSize)
{
	if (SourcePng.IsEmpty())
	{
		return nullptr;
	}
	IImageWrapperModule& ImageWrapperModule =
		FModuleManager::LoadModuleChecked<IImageWrapperModule>(
			TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> Decoder =
		ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!Decoder.IsValid()
		|| !Decoder->SetCompressed(SourcePng.GetData(), SourcePng.Num()))
	{
		return nullptr;
	}
	TArray64<uint8> Raw64;
	if (!Decoder->GetRaw(ERGBFormat::BGRA, 8, Raw64))
	{
		return nullptr;
	}
	TArray<uint8> Raw;
	Raw.Append(Raw64.GetData(), Raw64.Num());
	return FSlateDynamicImageBrush::CreateWithImageData(
		FName(*FString::Printf(
			TEXT("Backlot.CapturePreview.%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits))),
		PreviewSize,
		Raw);
}

FString FExtendedAtlassianScreenshot::SerializeAnnotations(
	const TArray<FExtendedAtlassianAnnotation>& Annotations)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	Values.Reserve(Annotations.Num());
	for (const FExtendedAtlassianAnnotation& Annotation : Annotations)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("id"), Annotation.Id);
		Object->SetStringField(
			TEXT("kind"),
			Annotation.Kind == EExtendedAtlassianAnnotationKind::Pin
				? TEXT("pin")
				: Annotation.Kind == EExtendedAtlassianAnnotationKind::Box
					? TEXT("box")
					: TEXT("blur"));
		Object->SetNumberField(TEXT("x"), Annotation.NormalizedPosition.X);
		Object->SetNumberField(TEXT("y"), Annotation.NormalizedPosition.Y);
		Object->SetNumberField(TEXT("w"), Annotation.NormalizedSize.X);
		Object->SetNumberField(TEXT("h"), Annotation.NormalizedSize.Y);
		Object->SetNumberField(TEXT("color"), Annotation.ColorIndex);
		Values.Add(MakeShared<FJsonValueObject>(Object));
	}
	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	FJsonSerializer::Serialize(Values, Writer);
	return Json;
}

bool FExtendedAtlassianScreenshot::DeserializeAnnotations(
	const FString& Json,
	TArray<FExtendedAtlassianAnnotation>& OutAnnotations,
	FString& OutError)
{
	OutAnnotations.Reset();
	OutError.Reset();
	TArray<TSharedPtr<FJsonValue>> Values;
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Values))
	{
		OutError = TEXT("Annotation payload is not a JSON array.");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : Values)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object)
			|| !Object->IsValid())
		{
			OutError = TEXT("Annotation payload contains an invalid row.");
			OutAnnotations.Reset();
			return false;
		}
		FExtendedAtlassianAnnotation Annotation;
		(*Object)->TryGetStringField(TEXT("id"), Annotation.Id);
		FString Kind;
		(*Object)->TryGetStringField(TEXT("kind"), Kind);
		Annotation.Kind = Kind == TEXT("box")
			? EExtendedAtlassianAnnotationKind::Box
			: Kind == TEXT("blur")
				? EExtendedAtlassianAnnotationKind::Blur
				: EExtendedAtlassianAnnotationKind::Pin;
		double Number = 0.0;
		(*Object)->TryGetNumberField(TEXT("x"), Number);
		Annotation.NormalizedPosition.X = FMath::Clamp(
			static_cast<float>(Number),
			0.0f,
			1.0f);
		(*Object)->TryGetNumberField(TEXT("y"), Number);
		Annotation.NormalizedPosition.Y = FMath::Clamp(
			static_cast<float>(Number),
			0.0f,
			1.0f);
		(*Object)->TryGetNumberField(TEXT("w"), Number);
		Annotation.NormalizedSize.X = FMath::Clamp(
			static_cast<float>(Number),
			0.0f,
			1.0f);
		(*Object)->TryGetNumberField(TEXT("h"), Number);
		Annotation.NormalizedSize.Y = FMath::Clamp(
			static_cast<float>(Number),
			0.0f,
			1.0f);
		(*Object)->TryGetNumberField(TEXT("color"), Number);
		Annotation.ColorIndex = FMath::Max(0, FMath::RoundToInt(Number));
		OutAnnotations.Add(MoveTemp(Annotation));
	}
	return true;
}
