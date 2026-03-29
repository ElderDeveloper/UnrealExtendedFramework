// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef _WIN32
#include <WinSDKVer.h>
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <SDKDDKVer.h>

// Use the C++ standard templated min/max
#define NOMINMAX

// DirectX apps don't need GDI
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP

// Include <mcx.h> if you need this
#define NOMCX

// Include <winsvc.h> if you need this
#define NOSERVICE

// WinHelp is deprecated
#define NOHELP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif //_WIN32

// Linux does not have _DEBUG in debug builds
#if !defined(NDEBUG) && !defined(_DEBUG)
#define _DEBUG
#endif

#if ALLOW_RESERVED_OPTIONS
#define DEV_BUILD
#endif

#if defined (_DEBUG) && defined (DUMP_MEM_LEAKS)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif // _DEBUG


#ifdef DXTK
#include <wrl/client.h>
#include <d3d11_1.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#endif //DXTK

#ifdef EOS_DEMO_SDL
#define GLEW_STATIC
#include <GL/glew.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_ttf.h>
#endif //EOS_DEMO_SDL

#include <algorithm>
#include <exception>
#include <memory>
#include <stdexcept>
#include <mutex>
#include <vector>
#include <array>
#include <map>
#include <queue>
#include <functional>
#include <cmath>
#include <exception>
#include <fstream>
#include <unordered_map>
#include <set>
#include <list>
#include <sstream>
#include <iterator>
#include <cassert>
#include <future>
#include <chrono>
#include <random>
#include <cctype>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <time.h>
#include <stdarg.h>

#ifdef _WIN32
#include <wincodec.h>
#include <shellapi.h>
#endif //_WIN32

#ifdef DXTK
#include "SimpleMath.h"
#include "DeviceResources.h"
#endif //DXTK

#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Color.h"
#include "Math/Matrix.h"

#ifdef EOS_DEMO_SDL
#include "SDLSpriteBatch.h"
#endif //EOS_DEMO_SDL

#ifndef _WIN32

typedef wchar_t WCHAR;
typedef wchar_t* LPWSTR, *PWSTR;
#define sprintf_s snprintf
#define __cdecl
#define wsprintf(BUF, FMT, ...) swprintf(BUF, sizeof(BUF)/sizeof(wchar_t), FMT, __VA_ARGS__)
#define OutputDebugStringW(STRING) wprintf(STRING)
#endif //!_Win32

#include "StepTimer.h"

#ifdef DXTK
// Helper class for COM exceptions
class com_exception : public std::exception
{
public:
    com_exception(HRESULT hr) : result(hr) {}

    virtual const char* what() const override
    {
        static char s_str[64] = {};
        sprintf_s(s_str, "Failure with HRESULT of %08X", static_cast<unsigned int>(result));
        return s_str;
    }

private:
    HRESULT result;
};

// Helper utility converts D3D API failures into exceptions.
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw com_exception(hr);
    }
}
#endif //DXTK

// Not definition, fall-back to Assets residing in executable folder
#ifndef EOS_ASSETS_PATH_PREFIX
#define EOS_ASSETS_PATH_PREFIX L""
#endif
