// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Input.h" 
#include "Main.h"
#include "GameEvent.h"
#include "Model.h"
#include "BaseLevel.h"

#ifdef DXTK
#include "SpriteBatch.h"
#endif

constexpr FColor FBaseLevel::DefaultCubeCol;
constexpr FColor FBaseLevel::GoodCubeCol;
constexpr FColor FBaseLevel::BadCubeCol;

FBaseLevel::FBaseLevel() :
	CubeRotationTimer(0.f),
	bRotateCube(true),
	CubeColTimer(0.f),
	CubeCol(DefaultCubeCol)
{
	CubeModel = std::make_shared<FModel>(EModelType::Cube, 5.f, L"Assets/Logo.dds");

#ifdef DXTK
	CubePosition = Vector3(0.f, 5.f, -70.f);
	CubeRotation = Vector3(0.f, 0.f, 0.f);
	CubeScale = Vector3(1.f, 1.f, 1.f);
#endif // DXTK

#ifdef EOS_DEMO_SDL
	CubePosition = Vector3(0.f, 0.f, 5.f);
	CubeRotation = Vector3(0.f, 0.f, 0.f);
	CubeScale = Vector3(1.f, 1.f, 1.f);

	CubeModel->SetPosition(CubePosition);
	CubeModel->SetScale(CubeScale);
#endif // EOS_DEMO_SDL
}

void FBaseLevel::Create()
{
	CubeModel->Create();
}

void FBaseLevel::Release()
{
	CubeModel->Release();
}

void FBaseLevel::Update()
{
	if (bRotateCube)
	{
		CubeRotationTimer += static_cast<float>(Main->GetTimer().GetElapsedSeconds());
	}

#ifdef DXTK
	CubeRotation = Vector3(0.f, CubeRotationTimer * 0.78f, 0.f);

	World = FMatrix::CreateRotationY(CubeRotation.y);

	FMatrix Local = World * FMatrix::CreateTranslation(CubePosition);
	CubeModel->SetWorldMatrix(Local);
#endif // DXTK

#ifdef EOS_DEMO_SDL
	// Update model
	CubeRotation = Vector3(CubeRotation.x, static_cast<float>(Main->GetTimer().GetElapsedSeconds()) * 100.0f, CubeRotation.z);
	CubeModel->SetRotation(CubeRotation);
#endif // EOS_DEMO_SDL

	UpdateCubeColTimer();
}

void FBaseLevel::Render(FSpriteBatchPtr&)
{
	FSpriteBatchPtr EmptyBatchPtr;
	CubeModel->Render(EmptyBatchPtr);
}

#ifdef _DEBUG
void FBaseLevel::DebugRender()
{
	CubeModel->DebugRender();
}
#endif

void FBaseLevel::OnGameEvent(const FGameEvent& Event)
{
	auto EventType = Event.GetType();

	// Good events
	if (EventType == EGameEventType::UserLoggedIn ||
		EventType == EGameEventType::UserLoggedOut ||
		EventType == EGameEventType::UserInfoRetrieved ||
		EventType == EGameEventType::PlayerSessionBegin ||
		EventType == EGameEventType::PlayerSessionEnd)
	{
		SetCubeColor(GoodCubeCol, DefaultCubeColTime);
	}
	// Bad events
	else if (EventType == EGameEventType::UserLoginFailed)
	{
		SetCubeColor(BadCubeCol, DefaultCubeColTime);
	}
}

void FBaseLevel::SetCubeColor(FColor Col)
{
	CubeCol = Col;
	CubeColTimer = 0.f;

	if (CubeModel)
	{
		CubeModel->SetColor(CubeCol);
	}
}

void FBaseLevel::SetCubeColor(FColor Col, float TimeToKeepCol)
{
	CubeCol = Col;
	CubeColTimer = TimeToKeepCol;

	if (CubeModel)
	{
		CubeModel->SetColor(CubeCol);
	}
}

void FBaseLevel::UpdateCubeColTimer()
{
	if (CubeColTimer > 0.f)
	{
		CubeColTimer -= static_cast<float>(Main->GetTimer().GetElapsedSeconds());

		if (CubeColTimer <= 0.f)
		{
			CubeColTimer = 0.f;

			SetCubeColor(DefaultCubeCol);
		}
	}
}