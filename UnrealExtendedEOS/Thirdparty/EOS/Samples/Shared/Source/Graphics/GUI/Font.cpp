// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Main.h"
#include "Font.h"
#include "DebugLog.h"
#include "CommandLine.h"
#include "StringUtils.h"
#include "AssetUtils.h"

#ifdef DXTK
using DirectX::SpriteFont;
#endif

std::wstring FFont::TestString = L"|TEST'`,_";

FFont::FFont(const std::wstring& TexAssetFile, size_t SizeOptional):
	AssetFile(TexAssetFile),
	Size(SizeOptional)
{

}

void FFont::Create()
{
	std::wstring AssetPath = FAssetUtils::Get().GetAssetDir() + AssetFile;

#ifdef DXTK
	std::unique_ptr<DeviceResources> const& DeviceResources = Main->GetDeviceResources();
	ID3D11DeviceContext* Context = DeviceResources->GetD3DDeviceContext();
	ID3D11Device* Device = DeviceResources->GetD3DDevice();

	Font = std::make_shared<SpriteFont>(Device, AssetPath.c_str());
	if (Font != nullptr)
	{
		Font->SetDefaultCharacter(L'?');
	}
#endif

#ifdef EOS_DEMO_SDL
	Font = std::make_shared<FSDLTrueTypeFont>(FStringUtils::Narrow(AssetPath), Size);
#endif //EOS_DEMO_SDL
}

void FFont::Release()
{
#ifdef DXTK
	Font.reset();
#endif
}

FontPtr FFont::GetFont()
{
	return Font;
}
