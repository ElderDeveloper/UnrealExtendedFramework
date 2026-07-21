// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Input.h" 
#include "Main.h"
#include "Texture.h"
#include "Model.h"
#include "Game.h"
#include "TextureManager.h"

FModel::FModel(EModelType Type, float Size, const std::wstring& ModelTextureFile) :
	ModelType(Type),
	ModelSize(Size),
	TextureFile(ModelTextureFile)
{
	ModelCol = FColor(1.f, 1.f, 1.f, 1.f);
	Texture = FGame::Get().GetTextureManager()->GetTexture(TextureFile);

#ifdef EOS_DEMO_SDL
	Mesh.Texture = Texture;
	Mesh.MeshColor = ModelCol;
#endif //EOS_DEMO_SDL
}

void FModel::Create()
{
#ifdef DXTK
	std::unique_ptr<DeviceResources> const& DeviceResources = Main->GetDeviceResources();
	ID3D11DeviceContext* Context = DeviceResources->GetD3DDeviceContext();

	switch (ModelType)
	{
		case EModelType::Cube:
		{
			Model = DirectX::GeometricPrimitive::CreateCube(Context, ModelSize, true);
			break;
		}
		default:
			break;
	}
#endif // DXTK

	if (Texture)
	{
		Texture->Create();
	}

#ifdef EOS_DEMO_SDL
	SpriteBatch = std::make_unique<FSDLSpriteBatch>();

	Mesh.Texture = Texture;
	Mesh.MeshColor = ModelCol;
	Mesh.TriangleList.clear();
	Mesh.Vertices.clear();

	GenerateModelMesh();
#endif //EOS_DEMO_SDL
}

void FModel::Release()
{
#ifdef DXTK
	if (Texture)
	{
		Texture.reset();
	}

	if (Model)
	{
		Model.reset();
	}
#endif // DXTK

#ifdef EOS_DEMO_SDL
	SpriteBatch.reset();
	Texture.reset();
#endif //EOS_DEMO_SDL
}

void FModel::Update()
{
	
}

void FModel::Render(FSpriteBatchPtr&)
{
#ifdef DXTK
	FMatrix const& View = Main->GetView();
	FMatrix const& Proj = Main->GetProjection();

	if (Model)
	{
		Model->Draw(WorldMatrix, View, Proj, ModelCol, Texture->GetTexture().Get());
	}
#endif // DXTK

#ifdef EOS_DEMO_SDL
	if (SpriteBatch)
	{
		//To make sure the cube is always rendered on top of everything just clear the depth buffer first
		glClear(GL_DEPTH_BUFFER_BIT);
		SpriteBatch->Begin(FSDLSpriteBatch::Render3D, Transform);
		SpriteBatch->Draw(Mesh);
		SpriteBatch->End();
	}
#endif // EOS_DEMO_SDL
}

#ifdef _DEBUG
void FModel::DebugRender()
{

}
#endif

#ifdef EOS_DEMO_SDL
void FModel::SetPosition(Vector3 Pos)
{
	Transform = Transform * BuildTranslate(Pos.x, Pos.y, Pos.z);
}

void FModel::SetRotation(Vector3 Rot)
{
	Transform = BuildRotation(Rot.x, Vector3(1.0f, 0.0f, 0.0f)) * Transform;
	Transform = BuildRotation(Rot.y, Vector3(0.0f, 1.0f, 0.0f)) * Transform;
	Transform = BuildRotation(Rot.z, Vector3(0.0f, 0.0f, 1.0f)) * Transform;
}

void FModel::SetScale(Vector3 Scale)
{
	Transform = Transform * BuildScale(Scale.x, Scale.y, Scale.z);
}

void FModel::GenerateModelMesh()
{
	switch (ModelType)
	{
		case EModelType::Cube:
		{
			GenerateCube();
			break;
		}
		default:
			break;
	}
}

void FModel::GenerateCube()
{
	Mesh.TriangleList.clear();
	Mesh.Vertices.clear();

	FSimpleVertex Vertex;

	// Front Face
	Vertex.Pos = Vector3(-1.0f, -1.0f, 1.0f); Vertex.Tex = Vector2(0.0f, 1.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(1.0f, -1.0f, 1.0f); Vertex.Tex = Vector2(1.0f, 1.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(1.0f, 1.0f, 1.0f); Vertex.Tex = Vector2(1.0f, 0.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(-1.0f, 1.0f, 1.0f); Vertex.Tex = Vector2(0.0f, 0.0f); Mesh.Vertices.push_back(Vertex);

	Mesh.TriangleList.push_back(0); Mesh.TriangleList.push_back(1);	Mesh.TriangleList.push_back(2);
	Mesh.TriangleList.push_back(2); Mesh.TriangleList.push_back(3);	Mesh.TriangleList.push_back(0);


	// Back Face
	Vertex.Pos = Vector3(-1.0f, -1.0f, -1.0f); Vertex.Tex = Vector2(1.0f, 1.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(-1.0f, 1.0f, -1.0f); Vertex.Tex = Vector2(1.0f, 0.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(1.0f, 1.0f, -1.0f); Vertex.Tex = Vector2(0.0f, 0.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(1.0f, -1.0f, -1.0f); Vertex.Tex = Vector2(0.0f, 1.0f); Mesh.Vertices.push_back(Vertex);

	Mesh.TriangleList.push_back(4); Mesh.TriangleList.push_back(5);	Mesh.TriangleList.push_back(6);
	Mesh.TriangleList.push_back(6); Mesh.TriangleList.push_back(7);	Mesh.TriangleList.push_back(4);

	// Top Face
	Vertex.Pos = Vector3(-1.0f, 1.0f, -1.0f); Vertex.Tex = Vector2(0.0f, 0.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(-1.0f, 1.0f, 1.0f);  Vertex.Tex = Vector2(0.0f, 1.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(1.0f, 1.0f, 1.0f);   Vertex.Tex = Vector2(1.0f, 1.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(1.0f, 1.0f, -1.0f);  Vertex.Tex = Vector2(1.0f, 0.0f); Mesh.Vertices.push_back(Vertex);

	Mesh.TriangleList.push_back(8); Mesh.TriangleList.push_back(9);	Mesh.TriangleList.push_back(10);
	Mesh.TriangleList.push_back(10); Mesh.TriangleList.push_back(11);	Mesh.TriangleList.push_back(8);

	// Bottom Face
	Vertex.Pos = Vector3(-1.0f, -1.0f, -1.0f); Vertex.Tex = Vector2(1.0f, 0.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(1.0f, -1.0f, -1.0f);  Vertex.Tex = Vector2(0.0f, 0.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(1.0f, -1.0f, 1.0f);   Vertex.Tex = Vector2(0.0f, 1.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(-1.0f, -1.0f, 1.0f);  Vertex.Tex = Vector2(1.0f, 1.0f); Mesh.Vertices.push_back(Vertex);

	Mesh.TriangleList.push_back(12); Mesh.TriangleList.push_back(13);	Mesh.TriangleList.push_back(14);
	Mesh.TriangleList.push_back(14); Mesh.TriangleList.push_back(15);	Mesh.TriangleList.push_back(12);

	// Right face
	Vertex.Pos = Vector3(1.0f, -1.0f, -1.0f); Vertex.Tex = Vector2(1.0f, 1.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(1.0f, 1.0f, -1.0f); Vertex.Tex = Vector2(1.0f, 0.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(1.0f, 1.0f, 1.0f); Vertex.Tex = Vector2(0.0f, 0.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(1.0f, -1.0f, 1.0f); Vertex.Tex = Vector2(0.0f, 1.0f); Mesh.Vertices.push_back(Vertex);

	Mesh.TriangleList.push_back(16); Mesh.TriangleList.push_back(17);	Mesh.TriangleList.push_back(18);
	Mesh.TriangleList.push_back(18); Mesh.TriangleList.push_back(19);	Mesh.TriangleList.push_back(16);

	// Left Face
	Vertex.Pos = Vector3(-1.0f, -1.0f, -1.0f); Vertex.Tex = Vector2(0.0f, 1.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(-1.0f, -1.0f, 1.0f); Vertex.Tex = Vector2(1.0f, 1.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(-1.0f, 1.0f, 1.0f); Vertex.Tex = Vector2(1.0f, 0.0f); Mesh.Vertices.push_back(Vertex);
	Vertex.Pos = Vector3(-1.0f, 1.0f, -1.0f); Vertex.Tex = Vector2(0.0f, 0.0f); Mesh.Vertices.push_back(Vertex);

	Mesh.TriangleList.push_back(20); Mesh.TriangleList.push_back(21);	Mesh.TriangleList.push_back(22);
	Mesh.TriangleList.push_back(22); Mesh.TriangleList.push_back(23);	Mesh.TriangleList.push_back(20);
}
#endif // EOS_DEMO_SDL