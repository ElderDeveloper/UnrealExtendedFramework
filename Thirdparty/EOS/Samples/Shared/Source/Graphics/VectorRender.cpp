// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Main.h"
#include "VectorRender.h"

FVectorRender::FVectorRender() :
	bEnabled(false)
{

}

void FVectorRender::Create()
{
#ifdef DXTK
	VectorRenderEffect = std::make_unique<DirectX::BasicEffect>(Main->GetDeviceResources()->GetD3DDevice());
	VectorRenderEffect->SetVertexColorEnabled(true);
	auto Size = Main->GetDeviceResources()->GetOutputSize();
	
	FMatrix ProjectionMatrix = FMatrix::CreateScale(
		2.f / float(Size.right),
		-2.f / float(Size.bottom), 1.f)
		* FMatrix::CreateTranslation(-1.f, 1.f, 0.f);
	VectorRenderEffect->SetProjection(ProjectionMatrix);

	void const* ShaderByteCode;
	size_t ByteCodeLength;

	VectorRenderEffect->GetVertexShaderBytecode(&ShaderByteCode, &ByteCodeLength);

	ThrowIfFailed(
		Main->GetDeviceResources()->GetD3DDevice()->CreateInputLayout(DirectX::VertexPositionColor::InputElements,
			DirectX::VertexPositionColor::InputElementCount,
			ShaderByteCode, ByteCodeLength,
			VectorRenderInputLayout.ReleaseAndGetAddressOf()));

	VectorRenderBatch = std::make_unique<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>(Main->GetDeviceResources()->GetD3DDeviceContext());
#endif // DXTK

#ifdef EOS_DEMO_SDL
	VectorRenderBatch = std::make_unique<FSDLSpriteBatch>();
#endif // EOS_DEMO_SDL
}

void FVectorRender::Begin()
{
#ifdef DXTK
	VectorRenderEffect->Apply(Main->GetDeviceResources()->GetD3DDeviceContext());
	Main->GetDeviceResources()->GetD3DDeviceContext()->IASetInputLayout(VectorRenderInputLayout.Get());
	VectorRenderBatch->Begin();
#endif // DXTK

#ifdef EOS_DEMO_SDL
	VectorRenderBatch->Begin(FSDLSpriteBatch::Render2DWires);
#endif
}

void FVectorRender::End()
{
#ifdef DXTK
	VectorRenderBatch->End();
#endif // DXTK

#ifdef EOS_DEMO_SDL
	VectorRenderBatch->End();
#endif // EOS_DEMO_SDL
}

void FVectorRender::AddLine(const FColoredVertex& Vertex1, const FColoredVertex& Vertex2, size_t LineWidth /* = 1 */)
{
	if (LineWidth == 1)
	{
		QueueVertices.push_back(Vertex1);
		QueueVertices.push_back(Vertex2);

		QueueIndices.push_back(static_cast<unsigned int>(QueueVertices.size() - 2));
		QueueIndices.push_back(static_cast<unsigned int>(QueueVertices.size() - 1));
	}
	else if (LineWidth > 1)
	{
		Vector3 Dir = (Vertex1.Position - Vertex2.Position);
		Dir.Normalize();

		Vector3 OffsetDir = Dir.Cross(Vector3(0.0f, 0.0f, 1.0f));
		OffsetDir.Normalize();

		for (size_t LineIndex = 0; LineIndex < LineWidth; ++LineIndex)
		{
			Vector3 Offset = OffsetDir * (float(LineIndex) - (LineWidth / 2));

			FColoredVertex V1 = Vertex1;
			FColoredVertex V2 = Vertex2;

			V1.Position += Offset;
			V2.Position += Offset;

			QueueVertices.push_back(V1);
			QueueVertices.push_back(V2);

			QueueIndices.push_back(static_cast<unsigned int>(QueueVertices.size() - 2));
			QueueIndices.push_back(static_cast<unsigned int>(QueueVertices.size() - 1));
		}
	}
}

void FVectorRender::AddPolygon(const std::vector<FColoredVertex> Vertices, size_t LineWidth /* = 1 */)
{
	const size_t NumVertices = Vertices.size();

	if (NumVertices < 2)
	{
		return;
	}

	if (LineWidth == 1)
	{
		const size_t IndexOffset = QueueVertices.size();

		QueueVertices.push_back(Vertices[0]);

		for (size_t VertexIndex = 1; VertexIndex < NumVertices; ++VertexIndex)
		{
			QueueVertices.push_back(Vertices[VertexIndex]);

			QueueIndices.push_back(static_cast<unsigned int>(IndexOffset + VertexIndex - 1));
			QueueIndices.push_back(static_cast<unsigned int>(IndexOffset + VertexIndex));
		}

		QueueIndices.push_back(static_cast<unsigned int>(IndexOffset + NumVertices - 1));
		QueueIndices.push_back(static_cast<unsigned int>(IndexOffset));
	}
	else if (LineWidth > 1)
	{
		std::vector<Vector3> OffsetDirs;
		for (size_t VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			Vector3 CurVertex = Vertices[VertexIndex].Position;
			Vector3 PrevVertex = (VertexIndex == 0) ? Vertices[NumVertices - 1].Position : Vertices[VertexIndex - 1].Position;
			Vector3 NextVertex = (VertexIndex == NumVertices - 1) ? Vertices[0].Position : Vertices[VertexIndex + 1].Position;
			
			Vector3 ToNextDir = NextVertex - CurVertex;
			ToNextDir.Normalize();

			Vector3 ToPrevDir = PrevVertex - CurVertex;
			ToPrevDir.Normalize();

			Vector3 OffsetDir = ToPrevDir + ToNextDir;
			OffsetDir.Normalize();

			OffsetDirs.push_back(OffsetDir);
		}

		for (size_t LineIndex = 0; LineIndex < LineWidth; ++LineIndex)
		{
			float OffsetMagnitude = (float(LineIndex) - (LineWidth / 2));

			std::vector<FColoredVertex> ModifiedVertices;
			for (size_t VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
			{
				FColoredVertex Vertex = Vertices[VertexIndex];
				Vertex.Position += OffsetDirs[VertexIndex] * OffsetMagnitude;

				ModifiedVertices.push_back(Vertex);
			}

			const size_t IndexOffset = QueueVertices.size();

			QueueVertices.push_back(ModifiedVertices[0]);

			for (size_t VertexIndex = 1; VertexIndex < NumVertices; ++VertexIndex)
			{
				QueueVertices.push_back(ModifiedVertices[VertexIndex]);

				QueueIndices.push_back(static_cast<unsigned int>(IndexOffset + VertexIndex - 1));
				QueueIndices.push_back(static_cast<unsigned int>(IndexOffset + VertexIndex));
			}

			QueueIndices.push_back(static_cast<unsigned int>(IndexOffset + NumVertices - 1));
			QueueIndices.push_back(static_cast<unsigned int>(IndexOffset));
		}
	}
}

void FVectorRender::DrawQueue()
{
#ifdef DXTK
	for (size_t LineIndex = 0; LineIndex < QueueIndices.size() - 1; LineIndex += 2)
	{
		const FColoredVertex& V1 = QueueVertices[QueueIndices[LineIndex]];
		const FColoredVertex& V2 = QueueVertices[QueueIndices[LineIndex + 1]];
	
		VectorRenderBatch->DrawLine(DirectX::VertexPositionColor(DirectX::XMFLOAT3(V1.Position.x, V1.Position.y, 0.0f), V1.Color),
			DirectX::VertexPositionColor(DirectX::XMFLOAT3(V2.Position.x, V2.Position.y, 0.0f), V2.Color));
	}
#endif //DXTK

#ifdef EOS_DEMO_SDL
	VectorRenderBatch->DrawLines(QueueVertices, QueueIndices);
#endif //EOS_DEMO_SDL

	QueueVertices.clear();
	QueueIndices.clear();
}
