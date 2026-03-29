// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Main.h"
#include "Game.h"
#include "Font.h"
#include "TextLabel.h"
#include "ProgressBar.h"
#include "Texture.h"
#include "TextureManager.h"

FProgressBar::FProgressBar(Vector2 Pos,
	Vector2 Size,
	UILayer Layer,
	const std::wstring& Text,
	const std::wstring& FinishedTextureFilename,
	const std::wstring& UnfinishedTextureFilename,
	FontPtr Font,
	FColor LabelBackCol,
	FColor LabelTextCol) :
	IWidget(Pos, Size, Layer),
	FinishedTextureFile(FinishedTextureFilename),
	UnfinishedTextureFile(UnfinishedTextureFilename)
{
	if (!FinishedTextureFile.empty())
	{
		FinishedTexture = FGame::Get().GetTextureManager()->GetTexture(FinishedTextureFile);
	}

	if (!UnfinishedTextureFile.empty())
	{
		UnfinishedTexture = FGame::Get().GetTextureManager()->GetTexture(UnfinishedTextureFile);
	}

	Label = std::make_shared<FTextLabelWidget>(Pos, Size, Layer - 1, Text, L"", LabelBackCol, LabelTextCol);
	LabelText = Text;

	CurrentProgress = 0.0f;

	if (Font != nullptr)
	{
		Label->SetFont(Font);
	}
}

void FProgressBar::Create()
{
	if (Label)
	{
		Label->Create();
	}

	if (FinishedTexture)
	{
		FinishedTexture->Create();
	}

	if (UnfinishedTexture)
	{
		UnfinishedTexture->Create();
	}
}

void FProgressBar::Release()
{
	if (Label)
	{
		Label->Release();
	}

	UnfinishedTexture.reset();
	FinishedTexture.reset();
}

void FProgressBar::Update()
{
	//clamp
	CurrentProgress = std::max(0.0f, CurrentProgress);
	CurrentProgress = std::min(CurrentProgress, 1.0f);

	if (Label)
	{
		wchar_t Buffer[128];
		swprintf(Buffer, sizeof(Buffer)/sizeof(wchar_t), L"%ls: %d%%", LabelText.c_str(), (int)(CurrentProgress * 100.0f));
		std::wstring ProgressText = Buffer;
		Label->SetText(ProgressText);
	}
}

void FProgressBar::Render(FSpriteBatchPtr& Batch)
{
	if (!IsShown())
		return;

	IWidget::Render(Batch);

	if (Batch &&
		(UnfinishedTexture && UnfinishedTexture->GetTexture()) &&
		(FinishedTexture && FinishedTexture->GetTexture()) )
	{

		//calculate sizes
		float FinishedWidth = Size.x * CurrentProgress;
		float UnfinishedWidth = Size.x - FinishedWidth;

#ifdef DXTK
		// Rect for sprite
		RECT FinishedRect = { LONG(Position.x), LONG(Position.y), LONG(Position.x + FinishedWidth), LONG(Position.y + Size.y) };
		RECT UnfinishedRect = { LONG(Position.x + FinishedWidth), LONG(Position.y), LONG(Position.x + Size.x), LONG(Position.y + Size.y) };

		RECT TextureFinishedRect = { LONG(0), LONG(0), LONG(FinishedWidth), LONG(Size.y) };
		RECT TextureUnfinishedRect = { LONG(FinishedWidth), LONG(0), LONG(Size.x), LONG(Size.y) };

		Batch->Draw(FinishedTexture->GetTexture().Get(), FinishedRect, &TextureFinishedRect, Color::White);
		Batch->Draw(UnfinishedTexture->GetTexture().Get(), UnfinishedRect, &TextureUnfinishedRect, Color::White);
#endif // DXTK

#ifdef EOS_DEMO_SDL
		FSimpleMesh FinishedPartMesh;
		FinishedPartMesh.Texture = FinishedTexture;
		FinishedPartMesh.Add2DQuad(Position, Vector2(FinishedWidth, Size.y), Vector2(0.0f, 0.0f), Vector2(CurrentProgress, 1.0f), Layer);

		FSimpleMesh UnfinishedPartMesh;
		UnfinishedPartMesh.Texture = UnfinishedTexture;
		UnfinishedPartMesh.Add2DQuad(Position + Vector2(FinishedWidth, 0.0f), Vector2(UnfinishedWidth, Size.y), Vector2(CurrentProgress, 0.0f), Vector2(1 - CurrentProgress, 1.0f), Layer);

		Batch->Draw(FinishedPartMesh);
		Batch->Draw(UnfinishedPartMesh);
#endif
	}

	if (Label)
	{
		Label->Render(Batch);
	}
}

#ifdef _DEBUG
void FProgressBar::DebugRender()
{
	IWidget::DebugRender();
}
#endif

void FProgressBar::OnUIEvent(const FUIEvent& Event)
{
}

void FProgressBar::SetPosition(Vector2 Pos)
{
	IWidget::SetPosition(Pos);

	if (Label)
	{
		Label->SetPosition(Pos);
	}


}

void FProgressBar::SetSize(Vector2 NewSize)
{
	IWidget::SetSize(NewSize);

	if (Label)
	{
		Label->SetSize(NewSize);
	}
}

void FProgressBar::SetProgress(float Progress)
{
	CurrentProgress = Progress;
}
