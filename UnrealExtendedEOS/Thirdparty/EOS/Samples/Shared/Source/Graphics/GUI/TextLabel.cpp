// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Main.h"
#include "Font.h"
#include "Sprite.h"
#include "TextLabel.h"

FTextLabelWidget::FTextLabelWidget(Vector2 LabelPos,
								   Vector2 LabelSize,
								   UILayer LabelLayer,
								   const std::wstring& LabelText,
								   const std::wstring& LabelAssetFile,
								   FColor LabelBackCol,
								   FColor LabelTextCol,
								   EAlignmentType LabelAlignmentType) :
	IWidget(LabelPos, LabelSize, LabelLayer),
	Text(LabelText),
	TextCol(LabelTextCol),
	AlignmentType(LabelAlignmentType),
	bBackgroundVisible(false)
{
	if (!LabelAssetFile.empty())
	{
		bBackgroundVisible = true;
	}

	BackgroundImage = std::make_shared<FSpriteWidget>(Position, Size, LabelLayer, LabelAssetFile, LabelBackCol);
}

FTextLabelWidget::FTextLabelWidget(Vector2 LabelPos,
									Vector2 LabelSize,
									UILayer LabelLayer,
									const std::wstring& LabelText,
									const std::vector<std::wstring>& LabelAssetFiles,
									FColor LabelBackCol,
									FColor LabelTextCol,
									EAlignmentType LabelAlignmentType) :
	IWidget(LabelPos, LabelSize, LabelLayer),
	Text(LabelText),
	TextCol(LabelTextCol),
	AlignmentType(LabelAlignmentType),
	bBackgroundVisible(false)
{
	if (!LabelAssetFiles.empty())
	{
		bBackgroundVisible = true;
	}

	BackgroundImage = std::make_shared<FSpriteWidget>(Position, Size, LabelLayer, LabelAssetFiles, LabelBackCol);
}

void FTextLabelWidget::Create()
{
	BackgroundImage->Create();
}

void FTextLabelWidget::Release()
{
	BackgroundImage->Release();
	Sprites.reset();
	MarkDirty();
}

void FTextLabelWidget::Update()
{
	BackgroundImage->Update();
}

void FTextLabelWidget::Render(FSpriteBatchPtr& Batch)
{
	IWidget::Render(Batch);

	if (bBackgroundVisible)
	{
		BackgroundImage->Render(Batch);
	}

	if (Font && Batch)
	{
#ifdef DXTK
		Font->DrawString(Batch.get(), Text.c_str(), TextPosition, TextCol);
#endif

#ifdef EOS_DEMO_SDL
		if (!FontTexture && !Text.empty())
		{
			FontTexture = Font->RenderString(Text, TextCol);
		}

		if (FontTexture && !Text.empty())
		{
			FSimpleMesh Mesh;
			Mesh.Texture = FontTexture;
			Mesh.Add2DQuad(TextPosition, Vector2(Font->MeasureString(Text)), Layer - 1);
			Batch->Draw(Mesh);
		}
#endif
	}

	bDirtyFlag = false;
}

#ifdef _DEBUG
void FTextLabelWidget::DebugRender()
{
	IWidget::DebugRender();

	BackgroundImage->DebugRender();
}
#endif

void FTextLabelWidget::OnUIEvent(const FUIEvent& Event)
{
	BackgroundImage->OnUIEvent(Event);
}

void FTextLabelWidget::SetPosition(Vector2 Pos)
{
	IWidget::SetPosition(Pos);

	BackgroundImage->SetPosition(Pos);

	UpdateTextPosition();
}

void FTextLabelWidget::SetSize(Vector2 NewSize)
{
	IWidget::SetSize(NewSize);

	BackgroundImage->SetSize(NewSize);

	UpdateTextPosition();
}

void FTextLabelWidget::SetFont(FontPtr NewFont)
{
	Font = NewFont;

	UpdateTextPosition();

	MarkDirty();
}

FontPtr FTextLabelWidget::GetFont()
{
	return Font;
}

void FTextLabelWidget::SetText(const std::wstring& LabelText)
{
	Text = LabelText;

	UpdateTextPosition();

	MarkDirty();
}

void FTextLabelWidget::ClearText()
{
	Text = L"";

	MarkDirty();
}

void FTextLabelWidget::UpdateTextPosition()
{
	if (!Font)
	{
		return;
	}

	Vector2 TextSize = Font->MeasureString(Text.c_str());
	float TextWidth = TextSize.x;
	float TextHeight = TextSize.y;
	float TextFieldWidth = Size.x - HorizontalOffset;

	TextPosition = Position;

	if ((TextWidth) < TextFieldWidth)
	{
		if (AlignmentType == EAlignmentType::Center)
		{
			TextPosition.x = Position.x + (Size.x / 2.0f) - (TextWidth / 2.0f);
		}
		else if (AlignmentType == EAlignmentType::Right)
		{
			TextPosition.x = Position.x + Size.x - TextWidth;
		}
		else if (AlignmentType == EAlignmentType::Left)
		{
			TextPosition.x = Position.x + HorizontalOffset;
		}
	}

	TextPosition.y = Position.y + (Size.y / 2.0f) - (TextHeight / 2.f);

	TextPosition.x = ceilf(TextPosition.x);
	TextPosition.y = ceilf(TextPosition.y);
}

void FTextLabelWidget::SetTextColor(FColor Col)
{
	TextCol = Col;

	MarkDirty();
}

void FTextLabelWidget::SetBackgroundColor(FColor Col)
{
	BackgroundImage->SetBackgroundColor(Col);
}

FColor FTextLabelWidget::GetBackgroundColor()
{
	return BackgroundImage->GetBackgroundColor();
}

void FTextLabelWidget::MarkDirty()
{
	bDirtyFlag = true;

#ifdef EOS_DEMO_SDL
	FontTexture.reset();
#endif
}
