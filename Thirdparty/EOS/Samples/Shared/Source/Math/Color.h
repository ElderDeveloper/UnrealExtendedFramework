// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Simple Color struct
struct FColor
{
	FColor() noexcept : FColor(0, 0, 0, 1.f) {}
	constexpr FColor(float _r, float _g, float _b) : FColor(_r, _g, _b, 1.f) {}
	constexpr FColor(float _r, float _g, float _b, float _a) : R(_r), G(_g), B(_b), A(_a) {}

	FColor(const FColor&) = default;
	FColor& operator=(const FColor&) = default;

	FColor(FColor&&) = default;
	FColor& operator=(FColor&&) = default;

	// Comparison operators
	bool operator == (const FColor& Other) const { return R == Other.R && G == Other.G && B == Other.B && A == Other.A; }
	bool operator != (const FColor& Other) const { return R != Other.R || G != Other.G || B != Other.B || A != Other.A; }

#ifdef DXTK
	operator DirectX::XMVECTOR() const { DirectX::XMFLOAT4 Float4(R, G, B, A); return XMLoadFloat4(&Float4); }
	operator DirectX::XMFLOAT4() const {	return DirectX::XMFLOAT4(R, G, B, A); }
	operator const float*() const { return reinterpret_cast<const float*>(this); }
#endif //DXTK

	// Properties
	float R;
	float G;
	float B;
	float A;
};

namespace Color
{
	// Standard colors (Red/Green/Blue/Alpha)
	 const FColor AliceBlue = { 0.941176534f, 0.972549081f, 1.000000000f, 1.000000000f };
	 const FColor AntiqueWhite = { 0.980392218f, 0.921568692f, 0.843137324f, 1.000000000f };
	 const FColor Aqua = { 0.000000000f, 1.000000000f, 1.000000000f, 1.000000000f };
	 const FColor Aquamarine = { 0.498039246f, 1.000000000f, 0.831372619f, 1.000000000f };
	 const FColor Azure = { 0.941176534f, 1.000000000f, 1.000000000f, 1.000000000f };
	 const FColor Beige = { 0.960784376f, 0.960784376f, 0.862745166f, 1.000000000f };
	 const FColor Bisque = { 1.000000000f, 0.894117713f, 0.768627524f, 1.000000000f };
	 const FColor Black = { 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f };
	 const FColor BlanchedAlmond = { 1.000000000f, 0.921568692f, 0.803921640f, 1.000000000f };
	 const FColor Blue = { 0.000000000f, 0.000000000f, 1.000000000f, 1.000000000f };
	 const FColor BlueViolet = { 0.541176498f, 0.168627456f, 0.886274576f, 1.000000000f };
	 const FColor Brown = { 0.647058845f, 0.164705887f, 0.164705887f, 1.000000000f };
	 const FColor BurlyWood = { 0.870588303f, 0.721568644f, 0.529411793f, 1.000000000f };
	 const FColor CadetBlue = { 0.372549027f, 0.619607866f, 0.627451003f, 1.000000000f };
	 const FColor Chartreuse = { 0.498039246f, 1.000000000f, 0.000000000f, 1.000000000f };
	 const FColor Chocolate = { 0.823529482f, 0.411764741f, 0.117647067f, 1.000000000f };
	 const FColor Coral = { 1.000000000f, 0.498039246f, 0.313725501f, 1.000000000f };
	 const FColor CornflowerBlue = { 0.392156899f, 0.584313750f, 0.929411829f, 1.000000000f };
	 const FColor Cornsilk = { 1.000000000f, 0.972549081f, 0.862745166f, 1.000000000f };
	 const FColor Crimson = { 0.862745166f, 0.078431375f, 0.235294133f, 1.000000000f };
	 const FColor Cyan = { 0.000000000f, 1.000000000f, 1.000000000f, 1.000000000f };
	 const FColor DarkBlue = { 0.000000000f, 0.000000000f, 0.545098066f, 1.000000000f };
	 const FColor DarkCyan = { 0.000000000f, 0.545098066f, 0.545098066f, 1.000000000f };
	 const FColor DarkGoldenrod = { 0.721568644f, 0.525490224f, 0.043137256f, 1.000000000f };
	 const FColor DarkGray = { 0.662745118f, 0.662745118f, 0.662745118f, 1.000000000f };
	 const FColor DarkGreen = { 0.000000000f, 0.392156899f, 0.000000000f, 1.000000000f };
	 const FColor DarkKhaki = { 0.741176486f, 0.717647076f, 0.419607878f, 1.000000000f };
	 const FColor DarkMagenta = { 0.545098066f, 0.000000000f, 0.545098066f, 1.000000000f };
	 const FColor DarkOliveGreen = { 0.333333343f, 0.419607878f, 0.184313729f, 1.000000000f };
	 const FColor DarkOrange = { 1.000000000f, 0.549019635f, 0.000000000f, 1.000000000f };
	 const FColor DarkOrchid = { 0.600000024f, 0.196078449f, 0.800000072f, 1.000000000f };
	 const FColor DarkRed = { 0.545098066f, 0.000000000f, 0.000000000f, 1.000000000f };
	 const FColor DarkSalmon = { 0.913725555f, 0.588235319f, 0.478431404f, 1.000000000f };
	 const FColor DarkSeaGreen = { 0.560784340f, 0.737254918f, 0.545098066f, 1.000000000f };
	 const FColor DarkSlateBlue = { 0.282352954f, 0.239215702f, 0.545098066f, 1.000000000f };
	 const FColor DarkSlateGray = { 0.184313729f, 0.309803933f, 0.309803933f, 1.000000000f };
	 const FColor DarkTurquoise = { 0.000000000f, 0.807843208f, 0.819607913f, 1.000000000f };
	 const FColor DarkViolet = { 0.580392182f, 0.000000000f, 0.827451050f, 1.000000000f };
	 const FColor DeepPink = { 1.000000000f, 0.078431375f, 0.576470613f, 1.000000000f };
	 const FColor DeepSkyBlue = { 0.000000000f, 0.749019623f, 1.000000000f, 1.000000000f };
	 const FColor DimGray = { 0.411764741f, 0.411764741f, 0.411764741f, 1.000000000f };
	 const FColor DodgerBlue = { 0.117647067f, 0.564705908f, 1.000000000f, 1.000000000f };
	 const FColor Firebrick = { 0.698039234f, 0.133333340f, 0.133333340f, 1.000000000f };
	 const FColor FloralWhite = { 1.000000000f, 0.980392218f, 0.941176534f, 1.000000000f };
	 const FColor ForestGreen = { 0.133333340f, 0.545098066f, 0.133333340f, 1.000000000f };
	 const FColor Fuchsia = { 1.000000000f, 0.000000000f, 1.000000000f, 1.000000000f };
	 const FColor Gainsboro = { 0.862745166f, 0.862745166f, 0.862745166f, 1.000000000f };
	 const FColor GhostWhite = { 0.972549081f, 0.972549081f, 1.000000000f, 1.000000000f };
	 const FColor Gold = { 1.000000000f, 0.843137324f, 0.000000000f, 1.000000000f };
	 const FColor Goldenrod = { 0.854902029f, 0.647058845f, 0.125490203f, 1.000000000f };
	 const FColor Gray = { 0.501960814f, 0.501960814f, 0.501960814f, 1.000000000f };
	 const FColor Green = { 0.000000000f, 0.501960814f, 0.000000000f, 1.000000000f };
	 const FColor GreenYellow = { 0.678431392f, 1.000000000f, 0.184313729f, 1.000000000f };
	 const FColor Honeydew = { 0.941176534f, 1.000000000f, 0.941176534f, 1.000000000f };
	 const FColor HotPink = { 1.000000000f, 0.411764741f, 0.705882370f, 1.000000000f };
	 const FColor IndianRed = { 0.803921640f, 0.360784322f, 0.360784322f, 1.000000000f };
	 const FColor Indigo = { 0.294117659f, 0.000000000f, 0.509803951f, 1.000000000f };
	 const FColor Ivory = { 1.000000000f, 1.000000000f, 0.941176534f, 1.000000000f };
	 const FColor Khaki = { 0.941176534f, 0.901960850f, 0.549019635f, 1.000000000f };
	 const FColor Lavender = { 0.901960850f, 0.901960850f, 0.980392218f, 1.000000000f };
	 const FColor LavenderBlush = { 1.000000000f, 0.941176534f, 0.960784376f, 1.000000000f };
	 const FColor LawnGreen = { 0.486274540f, 0.988235354f, 0.000000000f, 1.000000000f };
	 const FColor LemonChiffon = { 1.000000000f, 0.980392218f, 0.803921640f, 1.000000000f };
	 const FColor LightBlue = { 0.678431392f, 0.847058892f, 0.901960850f, 1.000000000f };
	 const FColor LightCoral = { 0.941176534f, 0.501960814f, 0.501960814f, 1.000000000f };
	 const FColor LightCyan = { 0.878431439f, 1.000000000f, 1.000000000f, 1.000000000f };
	 const FColor LightGoldenrodYellow = { 0.980392218f, 0.980392218f, 0.823529482f, 1.000000000f };
	 const FColor LightGreen = { 0.564705908f, 0.933333397f, 0.564705908f, 1.000000000f };
	 const FColor LightGray = { 0.827451050f, 0.827451050f, 0.827451050f, 1.000000000f };
	 const FColor LightPink = { 1.000000000f, 0.713725507f, 0.756862819f, 1.000000000f };
	 const FColor LightSalmon = { 1.000000000f, 0.627451003f, 0.478431404f, 1.000000000f };
	 const FColor LightSeaGreen = { 0.125490203f, 0.698039234f, 0.666666687f, 1.000000000f };
	 const FColor LightSkyBlue = { 0.529411793f, 0.807843208f, 0.980392218f, 1.000000000f };
	 const FColor LightSlateGray = { 0.466666698f, 0.533333361f, 0.600000024f, 1.000000000f };
	 const FColor LightSteelBlue = { 0.690196097f, 0.768627524f, 0.870588303f, 1.000000000f };
	 const FColor LightYellow = { 1.000000000f, 1.000000000f, 0.878431439f, 1.000000000f };
	 const FColor Lime = { 0.000000000f, 1.000000000f, 0.000000000f, 1.000000000f };
	 const FColor LimeGreen = { 0.196078449f, 0.803921640f, 0.196078449f, 1.000000000f };
	 const FColor Linen = { 0.980392218f, 0.941176534f, 0.901960850f, 1.000000000f };
	 const FColor Magenta = { 1.000000000f, 0.000000000f, 1.000000000f, 1.000000000f };
	 const FColor Maroon = { 0.501960814f, 0.000000000f, 0.000000000f, 1.000000000f };
	 const FColor MediumAquamarine = { 0.400000036f, 0.803921640f, 0.666666687f, 1.000000000f };
	 const FColor MediumBlue = { 0.000000000f, 0.000000000f, 0.803921640f, 1.000000000f };
	 const FColor MediumOrchid = { 0.729411781f, 0.333333343f, 0.827451050f, 1.000000000f };
	 const FColor MediumPurple = { 0.576470613f, 0.439215720f, 0.858823597f, 1.000000000f };
	 const FColor MediumSeaGreen = { 0.235294133f, 0.701960802f, 0.443137288f, 1.000000000f };
	 const FColor MediumSlateBlue = { 0.482352972f, 0.407843173f, 0.933333397f, 1.000000000f };
	 const FColor MediumSpringGreen = { 0.000000000f, 0.980392218f, 0.603921592f, 1.000000000f };
	 const FColor MediumTurquoise = { 0.282352954f, 0.819607913f, 0.800000072f, 1.000000000f };
	 const FColor MediumVioletRed = { 0.780392230f, 0.082352944f, 0.521568656f, 1.000000000f };
	 const FColor MidnightBlue = { 0.098039225f, 0.098039225f, 0.439215720f, 1.000000000f };
	 const FColor MintCream = { 0.960784376f, 1.000000000f, 0.980392218f, 1.000000000f };
	 const FColor MistyRose = { 1.000000000f, 0.894117713f, 0.882353008f, 1.000000000f };
	 const FColor Moccasin = { 1.000000000f, 0.894117713f, 0.709803939f, 1.000000000f };
	 const FColor NavajoWhite = { 1.000000000f, 0.870588303f, 0.678431392f, 1.000000000f };
	 const FColor Navy = { 0.000000000f, 0.000000000f, 0.501960814f, 1.000000000f };
	 const FColor OldLace = { 0.992156923f, 0.960784376f, 0.901960850f, 1.000000000f };
	 const FColor Olive = { 0.501960814f, 0.501960814f, 0.000000000f, 1.000000000f };
	 const FColor OliveDrab = { 0.419607878f, 0.556862772f, 0.137254909f, 1.000000000f };
	 const FColor Orange = { 1.000000000f, 0.647058845f, 0.000000000f, 1.000000000f };
	 const FColor OrangeRed = { 1.000000000f, 0.270588249f, 0.000000000f, 1.000000000f };
	 const FColor Orchid = { 0.854902029f, 0.439215720f, 0.839215755f, 1.000000000f };
	 const FColor PaleGoldenrod = { 0.933333397f, 0.909803987f, 0.666666687f, 1.000000000f };
	 const FColor PaleGreen = { 0.596078455f, 0.984313786f, 0.596078455f, 1.000000000f };
	 const FColor PaleTurquoise = { 0.686274529f, 0.933333397f, 0.933333397f, 1.000000000f };
	 const FColor PaleVioletRed = { 0.858823597f, 0.439215720f, 0.576470613f, 1.000000000f };
	 const FColor PapayaWhip = { 1.000000000f, 0.937254965f, 0.835294187f, 1.000000000f };
	 const FColor PeachPuff = { 1.000000000f, 0.854902029f, 0.725490212f, 1.000000000f };
	 const FColor Peru = { 0.803921640f, 0.521568656f, 0.247058839f, 1.000000000f };
	 const FColor Pink = { 1.000000000f, 0.752941251f, 0.796078503f, 1.000000000f };
	 const FColor Plum = { 0.866666734f, 0.627451003f, 0.866666734f, 1.000000000f };
	 const FColor PowderBlue = { 0.690196097f, 0.878431439f, 0.901960850f, 1.000000000f };
	 const FColor Purple = { 0.501960814f, 0.000000000f, 0.501960814f, 1.000000000f };
	 const FColor Red = { 1.000000000f, 0.000000000f, 0.000000000f, 1.000000000f };
	 const FColor RosyBrown = { 0.737254918f, 0.560784340f, 0.560784340f, 1.000000000f };
	 const FColor RoyalBlue = { 0.254901975f, 0.411764741f, 0.882353008f, 1.000000000f };
	 const FColor SaddleBrown = { 0.545098066f, 0.270588249f, 0.074509807f, 1.000000000f };
	 const FColor Salmon = { 0.980392218f, 0.501960814f, 0.447058856f, 1.000000000f };
	 const FColor SandyBrown = { 0.956862807f, 0.643137276f, 0.376470625f, 1.000000000f };
	 const FColor SeaGreen = { 0.180392161f, 0.545098066f, 0.341176480f, 1.000000000f };
	 const FColor SeaShell = { 1.000000000f, 0.960784376f, 0.933333397f, 1.000000000f };
	 const FColor Sienna = { 0.627451003f, 0.321568638f, 0.176470593f, 1.000000000f };
	 const FColor Silver = { 0.752941251f, 0.752941251f, 0.752941251f, 1.000000000f };
	 const FColor SkyBlue = { 0.529411793f, 0.807843208f, 0.921568692f, 1.000000000f };
	 const FColor SlateBlue = { 0.415686309f, 0.352941185f, 0.803921640f, 1.000000000f };
	 const FColor SlateGray = { 0.439215720f, 0.501960814f, 0.564705908f, 1.000000000f };
	 const FColor Snow = { 1.000000000f, 0.980392218f, 0.980392218f, 1.000000000f };
	 const FColor SpringGreen = { 0.000000000f, 1.000000000f, 0.498039246f, 1.000000000f };
	 const FColor SteelBlue = { 0.274509817f, 0.509803951f, 0.705882370f, 1.000000000f };
	 const FColor Tan = { 0.823529482f, 0.705882370f, 0.549019635f, 1.000000000f };
	 const FColor Teal = { 0.000000000f, 0.501960814f, 0.501960814f, 1.000000000f };
	 const FColor Thistle = { 0.847058892f, 0.749019623f, 0.847058892f, 1.000000000f };
	 const FColor Tomato = { 1.000000000f, 0.388235331f, 0.278431386f, 1.000000000f };
	 const FColor Transparent = { 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f };
	 const FColor Turquoise = { 0.250980407f, 0.878431439f, 0.815686345f, 1.000000000f };
	 const FColor Violet = { 0.933333397f, 0.509803951f, 0.933333397f, 1.000000000f };
	 const FColor Wheat = { 0.960784376f, 0.870588303f, 0.701960802f, 1.000000000f };
	 const FColor White = { 1.000000000f, 1.000000000f, 1.000000000f, 1.000000000f };
	 const FColor WhiteSmoke = { 0.960784376f, 0.960784376f, 0.960784376f, 1.000000000f };
	 const FColor Yellow = { 1.000000000f, 1.000000000f, 0.000000000f, 1.000000000f };
	 const FColor YellowGreen = { 0.603921592f, 0.803921640f, 0.196078449f, 1.000000000f };

	 const FColor UIBorderGrey = { 0.192f, 0.192f, 0.192f, 1.0f };
	 const FColor UIDarkGrey = { 0.38f, 0.38f, 0.38f, 1.0f };
	 const FColor UIBackgroundGrey = { 0.08f, 0.08f, 0.08f, 1.0f };
	 const FColor UIButtonBlue = { 0.196f, 0.371f, 0.73f };
	 const FColor UIHeaderGrey = { 0.13f, 0.13f, 0.13f, 1.0f };

} // namespace Color
