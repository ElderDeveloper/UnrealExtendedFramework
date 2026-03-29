// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"

#ifdef EOS_DEMO_SDL

#include "DebugLog.h"
#include "StringUtils.h"
#include "SDLSpriteBatch.h"
#include "Main.h"
#include "Game.h"
#include "TextureManager.h"

struct FSDLSpriteBatch::FImpl
{
	std::vector<FSimpleMesh> Meshes;

	static GLuint ShaderProgramID;
	static GLint VertexPosLocation;
	static GLint TextureCoordsLocation;
	static GLint SamplerLocation;
	static GLint ProjectionMatrixLocation;
	static GLint ColorLocation;

	//Shader params/data for Wire mode
	struct Wires
	{
		static GLuint ShaderProgramID;
		static GLint VertexPosLocation;
		static GLint ColorLocation;
		static GLint ProjectionMatrixLocation;
	};

	static FTexturePtr DummyTexture;

	static bool LoadTexturedShaders();
	static void GetShaderParamsLocations();

	static bool LoadWireShaders();
	static void GetWireShaderParamsLocations();
};

GLuint FSDLSpriteBatch::FImpl::ShaderProgramID = 0;
GLint FSDLSpriteBatch::FImpl::TextureCoordsLocation = -1;
GLint FSDLSpriteBatch::FImpl::VertexPosLocation = -1;
GLint FSDLSpriteBatch::FImpl::SamplerLocation = -1;
GLint FSDLSpriteBatch::FImpl::ProjectionMatrixLocation = -1;
GLint FSDLSpriteBatch::FImpl::ColorLocation = -1;

GLuint FSDLSpriteBatch::FImpl::Wires::ShaderProgramID = 0;
GLint FSDLSpriteBatch::FImpl::Wires::VertexPosLocation = -1;
GLint FSDLSpriteBatch::FImpl::Wires::ColorLocation = -1;
GLint FSDLSpriteBatch::FImpl::Wires::ProjectionMatrixLocation = -1;

FTexturePtr FSDLSpriteBatch::FImpl::DummyTexture = nullptr;

static void LogShaderErrors(GLuint Shader)
{
	if (glIsShader(Shader))
	{
		int LogLength = 0;
		int MaxLength = LogLength;

		//Get info string length
		glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &MaxLength);

		std::vector<char> LogData;
		LogData.resize(MaxLength);

		//Get info log
		glGetShaderInfoLog(Shader, MaxLength, &LogLength, &LogData[0]);
		if (LogLength > 0)
		{
			FDebugLog::LogError(FStringUtils::Widen(std::string(&LogData[0])).c_str());
		}
	}
}

bool FSDLSpriteBatch::FImpl::LoadTexturedShaders()
{
	//Generate program
	ShaderProgramID = glCreateProgram();

	//Create vertex shader
	GLuint VertexShader = glCreateShader(GL_VERTEX_SHADER);

	//Get vertex source
	const GLchar* VertexShaderSource[] =
	{
		"#version 140\n"
		"in vec3 VertexPosition;\n"
		"in vec2 TextureCoords;\n"
		"uniform mat4 ProjectionMatrix;\n"
		"out vec2 UV;\n"
		"void main() "
		"{ "
		"gl_Position = ProjectionMatrix * vec4(VertexPosition.x, VertexPosition.y, VertexPosition.z, 1 ); "
		"UV = TextureCoords;"
		"}"
	};

	//Set vertex source
	glShaderSource(VertexShader, 1, VertexShaderSource, NULL);

	//Compile vertex source
	glCompileShader(VertexShader);

	//Check vertex shader for errors
	GLint VertexShaderCompiled = GL_FALSE;
	glGetShaderiv(VertexShader, GL_COMPILE_STATUS, &VertexShaderCompiled);
	if (VertexShaderCompiled != GL_TRUE)
	{
		FDebugLog::LogError(L"Unable to compile vertex shader!\n");
		LogShaderErrors(VertexShader);
		return false;
	}

	//Attach vertex shader to program
	glAttachShader(ShaderProgramID, VertexShader);


	//Create fragment shader
	GLuint FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	//Get fragment source
	const GLchar* FragmentShaderSource[] =
	{
		"#version 140\n"
		"out vec4 Fragment;\n"
		"in vec2 UV;\n"
		"uniform sampler2D TextureSampler;\n"
		"uniform vec4 Color;\n"
		"void main()"
		"{"
		"Fragment = Color * texture(TextureSampler, UV);\n"
		"}"
	};

	//Set fragment source
	glShaderSource(FragmentShader, 1, FragmentShaderSource, NULL);

	//Compile fragment source
	glCompileShader(FragmentShader);

	//Check fragment shader for errors
	GLint fShaderCompiled = GL_FALSE;
	glGetShaderiv(FragmentShader, GL_COMPILE_STATUS, &fShaderCompiled);
	if (fShaderCompiled != GL_TRUE)
	{
		FDebugLog::LogError(L"Unable to compile fragment shader!\n");
		LogShaderErrors(FragmentShader);
		return false;
	}

	//Attach fragment shader to program
	glAttachShader(ShaderProgramID, FragmentShader);


	//Link program
	glLinkProgram(ShaderProgramID);

	//Check for errors
	GLint LinkSuccess = GL_TRUE;
	glGetProgramiv(ShaderProgramID, GL_LINK_STATUS, &LinkSuccess);
	if (LinkSuccess != GL_TRUE)
	{
		FDebugLog::LogError(L"Error linking program!\n");
		LogShaderErrors(ShaderProgramID);
		return false;
	}

	return true;
}

void FSDLSpriteBatch::FImpl::GetShaderParamsLocations()
{
	//Get vertex attribute location
	VertexPosLocation = glGetAttribLocation(ShaderProgramID, "VertexPosition");
	TextureCoordsLocation = glGetAttribLocation(ShaderProgramID, "TextureCoords");
	SamplerLocation = glGetUniformLocation(ShaderProgramID, "TextureSampler");
	ProjectionMatrixLocation = glGetUniformLocation(ShaderProgramID, "ProjectionMatrix");
	ColorLocation = glGetUniformLocation(ShaderProgramID, "Color");

	if (VertexPosLocation == -1 || TextureCoordsLocation == -1 || 
		SamplerLocation == -1 || ProjectionMatrixLocation == -1 || ColorLocation == -1)
	{
		FDebugLog::LogError(L"Could not find locations of some shader parameters!");
		assert(false);
	}
}

bool FSDLSpriteBatch::FImpl::LoadWireShaders()
{
	//Generate program
	Wires::ShaderProgramID = glCreateProgram();

	//Create vertex shader
	GLuint VertexShader = glCreateShader(GL_VERTEX_SHADER);

	//Get vertex source
	const GLchar* VertexShaderSource[] =
	{
		"#version 140\n"
		"in vec3 VertexPosition;\n"
		"in vec4 Color;\n"
		"uniform mat4 ProjectionMatrix;\n"
		"out vec4 ColorV;\n"
		"void main() "
		"{ "
		"ColorV = Color; "
		"gl_Position = ProjectionMatrix * vec4(VertexPosition.x, VertexPosition.y, VertexPosition.z, 1 ); "
		"}"
	};

	//Set vertex source
	glShaderSource(VertexShader, 1, VertexShaderSource, NULL);

	//Compile vertex source
	glCompileShader(VertexShader);

	//Check vertex shader for errors
	GLint VertexShaderCompiled = GL_FALSE;
	glGetShaderiv(VertexShader, GL_COMPILE_STATUS, &VertexShaderCompiled);
	if (VertexShaderCompiled != GL_TRUE)
	{
		FDebugLog::LogError(L"Unable to compile wire vertex shader!\n");
		LogShaderErrors(VertexShader);
		return false;
	}

	//Attach vertex shader to program
	glAttachShader(Wires::ShaderProgramID, VertexShader);


	//Create fragment shader
	GLuint FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	//Get fragment source
	const GLchar* FragmentShaderSource[] =
	{
		"#version 140\n"
		"out vec4 Fragment;\n"
		"in vec4 ColorV;\n"
		"void main()"
		"{"
		"Fragment = ColorV;\n"
		"}"
	};

	//Set fragment source
	glShaderSource(FragmentShader, 1, FragmentShaderSource, NULL);

	//Compile fragment source
	glCompileShader(FragmentShader);

	//Check fragment shader for errors
	GLint fShaderCompiled = GL_FALSE;
	glGetShaderiv(FragmentShader, GL_COMPILE_STATUS, &fShaderCompiled);
	if (fShaderCompiled != GL_TRUE)
	{
		FDebugLog::LogError(L"Unable to compile wire fragment shader!\n");
		LogShaderErrors(FragmentShader);
		return false;
	}

	//Attach fragment shader to program
	glAttachShader(Wires::ShaderProgramID, FragmentShader);


	//Link program
	glLinkProgram(Wires::ShaderProgramID);

	//Check for errors
	GLint LinkSuccess = GL_TRUE;
	glGetProgramiv(Wires::ShaderProgramID, GL_LINK_STATUS, &LinkSuccess);
	if (LinkSuccess != GL_TRUE)
	{
		FDebugLog::LogError(L"Error linking wire program!\n");
		LogShaderErrors(Wires::ShaderProgramID);
		return false;
	}

	return true;
}

void FSDLSpriteBatch::FImpl::GetWireShaderParamsLocations()
{
	//Get vertex attribute location
	Wires::VertexPosLocation = glGetAttribLocation(Wires::ShaderProgramID, "VertexPosition");
	Wires::ColorLocation = glGetAttribLocation(Wires::ShaderProgramID, "Color");
	Wires::ProjectionMatrixLocation = glGetUniformLocation(Wires::ShaderProgramID, "ProjectionMatrix");

	if (Wires::VertexPosLocation == -1 || Wires::ProjectionMatrixLocation == -1 || Wires::ColorLocation == -1)
	{
		FDebugLog::LogError(L"Could not find locations of some wire shader parameters!");
		assert(false);
	}
}


FSDLSpriteBatch::FSDLSpriteBatch(): Impl(new FImpl())
{

}

void FSDLSpriteBatch::Init()
{
	if (FSDLSpriteBatch::FImpl::LoadTexturedShaders())
	{
		FSDLSpriteBatch::FImpl::GetShaderParamsLocations();
	}

	if (FSDLSpriteBatch::FImpl::LoadWireShaders())
	{
		FSDLSpriteBatch::FImpl::GetWireShaderParamsLocations();
	}

	FImpl::DummyTexture = FGame::Get().GetTextureManager()->GetTexture(L"Assets/solid_white.dds");
	FImpl::DummyTexture->Create();
}

void FSDLSpriteBatch::Begin(EMode Mode, FMatrix Model)
{
	Impl->Meshes.clear();
	CurrentMode = Mode;

	glUseProgram((Mode == Render2DWires) ? FImpl::Wires::ShaderProgramID : FImpl::ShaderProgramID);

	int Width = 0, Height = 0;
	SDL_GetWindowSize(Main->GetWindow(), &Width, &Height);

	float AspectRatio = float(Width) / Height;

	if (Mode == Render2D || Mode == Render2DWires)
	{
		FMatrix OrthoMatrix = BuildOrtho(0.0f, float(Width), float(Height), 0.0f, -1.0f, 10.0f);
		glUniformMatrix4fv((Mode == Render2DWires) ? FImpl::Wires::ProjectionMatrixLocation : FImpl::ProjectionMatrixLocation, 1, GL_FALSE, (const GLfloat*)(&OrthoMatrix.data[0][0]));
	}
	else if (Mode == Render3D)
	{
		FMatrix Proj = BuildPerspProjMat(70.0f, AspectRatio, 0.01f, 100.0f);
		FMatrix View = BuildLookAtMatrix(0, 0, -20.0, 0, -12.0, 0, 0, 1, 0);
		FMatrix MVP = Model * View * Proj;

		glUniformMatrix4fv(FImpl::ProjectionMatrixLocation, 1, GL_FALSE, (const GLfloat*)(&MVP.data[0][0]));
	}

	Main->GLError(L"FSDLSpriteBatch::Begin");
}

void FSDLSpriteBatch::Draw(FSimpleMesh Mesh)
{
	if (CurrentMode == Render2DWires)
	{
		Mesh.Texture = FImpl::DummyTexture;
	}

	//try to merge the mesh with existing ones
	for (size_t i = 0; i < Impl->Meshes.size(); ++i)
	{
		if (Impl->Meshes[i].AreMaterialsEqual(Mesh))
		{
			//merging in
			FSimpleMesh& OldMesh = Impl->Meshes[i];
			size_t IndexOffset = OldMesh.Vertices.size();

			std::copy(Mesh.Vertices.begin(), Mesh.Vertices.end(), std::back_inserter(OldMesh.Vertices));

			for (size_t Index : Mesh.TriangleList)
			{
				OldMesh.TriangleList.push_back((unsigned int)(Index + IndexOffset));
			}

			//Done
			return;
		}
	}

	//could not merge in, just add to the back
	Impl->Meshes.emplace_back(std::move(Mesh));
}

void FSDLSpriteBatch::DrawLines(const std::vector<FColoredVertex>& Vertices, const std::vector<unsigned int>& Indices)
{
	if (CurrentMode != Render2DWires || Indices.empty())
	{
		return;
	}

	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glEnable(GL_BLEND);

	GLuint VBO = 0;
	GLuint IBO = 0;

	//Create VBO
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, Vertices.size() * sizeof(FColoredVertex), &Vertices[0], GL_STATIC_DRAW);

	// Bind VAO
	GLuint VAO = 0;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glEnableVertexAttribArray(FImpl::Wires::VertexPosLocation);
	glEnableVertexAttribArray(FImpl::Wires::ColorLocation);

	//Create IBO
	glGenBuffers(1, &IBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, Indices.size() * sizeof(GLuint), &Indices[0], GL_STATIC_DRAW);

	static_assert(sizeof(GLuint) == sizeof(unsigned int), "We assume we can use unsigned int as triangle index");

	//Fill VBO with data
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glVertexAttribPointer(FImpl::Wires::VertexPosLocation, 3, GL_FLOAT, GL_FALSE, sizeof(FColoredVertex), NULL);
	glVertexAttribPointer(FImpl::Wires::ColorLocation, 4, GL_FLOAT, GL_FALSE, sizeof(FColoredVertex), (void*)(offsetof(FColoredVertex, Color)));

	//Set index data and render
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
	glDrawElements(GL_LINES, (GLsizei)Indices.size(), GL_UNSIGNED_INT, NULL);

	//Disable vertex position
	glDisableVertexAttribArray(FImpl::Wires::VertexPosLocation);
	glDisableVertexAttribArray(FImpl::Wires::ColorLocation);

	//We don't cache VAO/VBO/IBOs
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &IBO);
	glDeleteVertexArrays(1, &VAO);

	glUseProgram(0);

	glDisable(GL_BLEND);

	Main->GLError(L"FSDLSpriteBatch::DrawLines");
}

void FSDLSpriteBatch::End()
{
	if (CurrentMode == Render2DWires)
	{
		glDisable(GL_CULL_FACE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	else
	{
		glEnable(GL_CULL_FACE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	if (CurrentMode == Render2D)
	{
		glEnable(GL_BLEND);
	}

	//Render
	for (const auto& Mesh : Impl->Meshes)
	{
		GLuint TextureIDToRender = 0;
		if (Mesh.Texture)
		{
			TextureIDToRender = Mesh.Texture->GetTexture();
		}
		if (Mesh.AnimatedTexture)
		{
			TextureIDToRender = Mesh.AnimatedTexture->GetCurrentTexture();
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, TextureIDToRender);

		glUniform4f(FImpl::ColorLocation, Mesh.MeshColor.R, Mesh.MeshColor.G, Mesh.MeshColor.B, Mesh.MeshColor.A);

		GLuint VBO = 0;
		GLuint IBO = 0;

		//Create VBO
		glGenBuffers(1, &VBO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, Mesh.Vertices.size() * sizeof(FSimpleVertex), &Mesh.Vertices[0], GL_STATIC_DRAW);

		// Bind VAO
		GLuint VAO = 0;
		glGenVertexArrays(1, &VAO);
		glBindVertexArray(VAO);

		glEnableVertexAttribArray(FImpl::VertexPosLocation);
		glEnableVertexAttribArray(FImpl::TextureCoordsLocation);
		glUniform1i(FImpl::SamplerLocation, 0);

		//Create IBO
		glGenBuffers(1, &IBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, Mesh.TriangleList.size() * sizeof(GLuint), &Mesh.TriangleList[0], GL_STATIC_DRAW);

		static_assert(sizeof(GLuint) == sizeof(unsigned int), "We assume we can use unsigned int as triangle index");
		
		//Fill VBO with data
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glVertexAttribPointer(FImpl::VertexPosLocation, 3, GL_FLOAT, GL_FALSE, sizeof(FSimpleVertex), NULL);
		glVertexAttribPointer(FImpl::TextureCoordsLocation, 2, GL_FLOAT, GL_FALSE, sizeof(FSimpleVertex), (void*)(offsetof(FSimpleVertex, Tex)));

		//Set index data and render
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
		glDrawElements(GL_TRIANGLES, (GLsizei)Mesh.TriangleList.size(), GL_UNSIGNED_INT, NULL);

		//Disable vertex position
		glDisableVertexAttribArray(FImpl::VertexPosLocation);
		glDisableVertexAttribArray(FImpl::TextureCoordsLocation);

		//We don't cache VAO/VBO/IBOs
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &IBO);
		glDeleteVertexArrays(1, &VAO);
	}

	glUseProgram(0);
	
	glDisable(GL_BLEND);

	Main->GLError(L"FSDLSpriteBatch::End");

	Impl->Meshes.clear();
}

FSDLSpriteBatch::~FSDLSpriteBatch()
{

}


void FSimpleMesh::Add2DQuad(Vector2 Corner, Vector2 Size, UILayer Layer)
{
	Add2DQuad(Corner, Size, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f), Layer);
}

void FSimpleMesh::Add2DQuad(Vector2 Corner, Vector2 Size, Vector2 TextureCoordCorner, Vector2 TextureCoordSize, UILayer Layer)
{
	const float Depth = UILayerToDepth(Layer);

	FSimpleVertex Vertex;
	Vertex.Pos = Vector3(Corner.x, Corner.y, Depth);
	Vertex.Tex = Vector2(TextureCoordCorner.x, TextureCoordCorner.y);

	Vertices.push_back(Vertex);

	unsigned int FirstIndex = (unsigned int)Vertices.size() - 1;

	Vertex.Pos = Vector3(Corner.x, Corner.y + Size.y, Depth);
	Vertex.Tex = Vector2(TextureCoordCorner.x, TextureCoordCorner.y + TextureCoordSize.y);

	Vertices.push_back(Vertex);

	Vertex.Pos = Vector3(Corner.x + Size.x, Corner.y + Size.y, Depth);
	Vertex.Tex = Vector2(TextureCoordCorner.x + TextureCoordSize.x, TextureCoordCorner.y + TextureCoordSize.y);

	Vertices.push_back(Vertex);

	Vertex.Pos = Vector3(Corner.x + Size.x, Corner.y, Depth);
	Vertex.Tex = Vector2(TextureCoordCorner.x + TextureCoordSize.x, TextureCoordCorner.y);

	Vertices.push_back(Vertex);

	//Add 2 triangles
	TriangleList.push_back(FirstIndex);
	TriangleList.push_back(FirstIndex + 1);
	TriangleList.push_back(FirstIndex + 2);

	TriangleList.push_back(FirstIndex + 2);
	TriangleList.push_back(FirstIndex + 3);
	TriangleList.push_back(FirstIndex);
}

void FSimpleMesh::Add2DLine(Vector2 Pos1, Vector2 Pos2, UILayer Layer)
{
	const float Depth = UILayerToDepth(Layer);

	FSimpleVertex Vertex;
	Vertex.Pos = Vector3(Pos1.x, Pos1.y, Depth);
	Vertex.Tex = Vector2(0.0f, 0.0f);

	Vertices.push_back(Vertex);

	unsigned int FirstIndex = (unsigned int)Vertices.size() - 1;

	Vertex.Pos = Vector3(Pos2.x, Pos2.y, Depth);
	Vertex.Tex = Vector2(1.0f, 1.0f);

	Vertices.push_back(Vertex);

	//Add 1 triangle
	TriangleList.push_back(FirstIndex);
	TriangleList.push_back(FirstIndex + 1);
	TriangleList.push_back(FirstIndex + 1);
}

void FSimpleMesh::Add2DHollowRect(Vector2 Corner, Vector2 Size, UILayer Layer)
{
	Add2DLine(Corner, Corner + Vector2(Size.x, 0.0f), Layer);
	Add2DLine(Corner + Vector2(Size.x, 0.0f), Corner + Size,  Layer);
	Add2DLine(Corner + Size, Corner + Vector2(0.0f, Size.y), Layer);
	Add2DLine(Corner + Vector2(0.0f, Size.y), Corner, Layer);
}

bool FSimpleMesh::AreMaterialsEqual(const FSimpleMesh& Other) const
{
	//compare colors
	if (MeshColor != Other.MeshColor)
	{
		return false;
	}

	//compare textures
	bool bHasTexture = bool(Texture);
	bool bOtherHasTexture = bool(Other.Texture);

	if (bHasTexture != bOtherHasTexture)
	{
		return false;
	}

	if (bHasTexture)
	{
		//we assume that we don't create duplicate texture object for the same asset
		if(Texture != Other.Texture)
		{
			return false;
		}
	}

	//compare animated textures
	bool bHasAnimatedTexture = bool(AnimatedTexture);
	bool bOtherHasAnimatedTexture = bool(Other.AnimatedTexture);

	if (bHasAnimatedTexture != bOtherHasAnimatedTexture)
	{
		return false;
	}

	if (bHasAnimatedTexture)
	{
		//we assume that we don't create duplicate texture object for the same asset
		if (AnimatedTexture->GetCurrentTexturePtr() != Other.AnimatedTexture->GetCurrentTexturePtr())
		{
			return false;
		}
	}

	return true;
}

#endif //EOS_DEMO_SDL

