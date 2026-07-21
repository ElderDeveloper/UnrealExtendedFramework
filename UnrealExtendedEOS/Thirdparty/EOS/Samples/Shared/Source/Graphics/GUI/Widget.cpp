// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Game.h"
#ifdef DXTK
#include "PrimitiveBatch.h"
#include "VertexTypes.h"
#endif //  DXTK
#include "VectorRender.h"
#include "Widget.h"
#include "Dialog.h"

float UILayerToDepth(UILayer Layer)
{
	return 0.7f - Layer * 0.001f;
}

bool IWidget::IsMouseHovered() const
{
	if (!IsEnabled() || !IsShown())
	{
		return false;
	}

	Vector2 MousePosition = FGame::Get().GetInput()->GetMousePosition();
	return (CheckCollision(MousePosition));
}

void IWidget::SetParent(std::weak_ptr<FDialog> InParent)
{
	Parent = InParent;
}

void IWidget::RenderBorders()
{
	std::vector<FColoredVertex> Vertices;
	Vertices.resize(4);

	const float Depth = UILayerToDepth(Layer);
	const size_t BorderWidth = 3;

	Vertices[0].Color = BorderColor;
	Vertices[0].Position = Vector3(Position.x, Position.y, Depth);

	Vertices[1].Color = BorderColor;
	Vertices[1].Position = Vector3(Position.x + Size.x, Position.y, Depth);

	Vertices[2].Color = BorderColor;
	Vertices[2].Position = Vector3(Position.x + Size.x, Position.y + Size.y, Depth);

	Vertices[3].Color = BorderColor;
	Vertices[3].Position = Vector3(Position.x, Position.y + Size.y, Depth);

	FGame::Get().GetVectorRender()->AddPolygon(Vertices, BorderWidth);
}

void IWidget::Render(FSpriteBatchPtr& Batch)
{
	if (!IsShown())
		return;

	if (bBordersEnabled)
	{
		RenderBorders();
	}
}

#ifdef _DEBUG

void IWidget::DebugRender()
{
	FColor BoxColor = Color::Green;

	if (IsFocused())
	{
		BoxColor = Color::Red;
	}
	else if (IsShown())
	{
		BoxColor = Color::Green;
	}
	else if (IsEnabled())
	{
		BoxColor = Color::Yellow;
	}

	const float Depth = 1.00f;

	std::vector<FColoredVertex> Vertices;

	Vertices.resize(4);

	Vertices[0].Position = Vector3(Position.x, Position.y, Depth);
	Vertices[0].Color = BoxColor;
	
	Vertices[1].Position = Vector3(Position.x + Size.x, Position.y, Depth);
	Vertices[1].Color = BoxColor;

	Vertices[2].Position = Vector3(Position.x + Size.x, Position.y + Size.y, Depth);
	Vertices[2].Color = BoxColor;

	Vertices[3].Position = Vector3(Position.x, Position.y + Size.y, Depth);
	Vertices[3].Color = BoxColor;

	FGame::Get().GetVectorRender()->AddPolygon(Vertices);
}

#endif

