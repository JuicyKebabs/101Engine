#pragma once
#define _WIN32_WINNT 0x0A00
#include <sdkddkver.h>
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <d3dcompiler.h>
#include <unordered_map>
#include "d3dx12.h"
#include "ComPtr.h"
#include "RenderData.h"

// Shader stage enumeration
enum class SHADER_STAGE
{
    STAGE_VS,
    STAGE_PS,
	STAGE_COUNT
};

// Shader key type definition(file path, entry point, profile, stage defines, common defines, compile flags)
using ShaderKey = std::tuple<std::wstring, std::string, std::string, uint64_t, uint64_t, uint32_t>;

// Shader key hash function
struct ShaderKeyHash
{
    size_t operator()(const ShaderKey& k) const noexcept
    {
        size_t h1 = std::hash<std::wstring>{}(std::get<0>(k)); // file path
        size_t h2 = std::hash<std::string>{}(std::get<1>(k));  // entry point
        size_t h3 = std::hash<std::string>{}(std::get<2>(k));  // profile
        size_t h4 = std::hash<uint64_t>{}(std::get<3>(k));     // stage defines
		size_t h5 = std::hash<uint32_t>{}(std::get<4>(k));     // common defines
		size_t h6 = std::hash<uint32_t>{}(std::get<5>(k));     // compile flags
        return (((h1 ^ (h2 << 1)) ^ (h3 << 2)) ^ (h4 << 3)) ^ (h5 << 4);
	}
};

// Shader description structure
struct ShaderDesc
{
    std::wstring filePath;	// Shader file path
    std::string entryPoint;	// Shader entry point
    std::string profile;	// Shader profile (e.g., "vs_5_0", "ps_5_0")
};

// Vertex shader file tables
static const std::wstring VS_FILE_TABLE[] = {
    L"VertexShader.hlsl"
};
// Pixel shader file tables
static const std::wstring PS_FILE_TABLE[] = {
    L"PixelShader.hlsl"
};
//  Vertex shader entry point tables
static const std::string VS_ENTRY_TABLE[] = {
    "BasicVS",
	"PostEffectVS"
};
//  Pixel shader entry point tables
static const std::string PS_ENTRY_TABLE[] = {
    "BasicPS",
	"PostEffectPS"
};
// Shader profiles(Solid shader model 5.0)
static const std::string VS_PROFILE = "vs_5_0";
// Shader profiles(Solid shader model 5.0)
static const std::string PS_PROFILE = "ps_5_0";

// Shader macro definition structure
struct MacroDefinition
{
	uint64_t bit;       // Bit flag for the define
	const char* name;   // Macro name to be used in shader compilation
	const char* value;  // Macro value (optional, can be nullptr for simple defines)
};
// Common shader macros
static constexpr MacroDefinition COMMON_SHADER_MACROS[] = {
    { 1ull << 0, "TEST", nullptr },
};
// Vertex shader macros
static constexpr MacroDefinition VS_SHADER_MACROS[] = {
    { 1ull << 0, "TEST", nullptr },
};
// Pixel shader macros
static constexpr MacroDefinition PS_SHADER_MACROS[] = {
	{ 1ull << 0, "NONE", nullptr },
	{ 1ull << 1, "USE_MASK", nullptr },
    { 1ull << 2, "MULTIPLY_ALPHA_CONTROL", nullptr },
    { 1ull << 3, "USE_LIGHTING", nullptr },
};
// Macro counts
static constexpr size_t COMMON_SHADER_MACROS_COUNT = sizeof(COMMON_SHADER_MACROS) / sizeof(MacroDefinition);
static constexpr size_t VS_SHADER_MACROS_COUNT = sizeof(VS_SHADER_MACROS) / sizeof(MacroDefinition);
static constexpr size_t PS_SHADER_MACROS_COUNT = sizeof(PS_SHADER_MACROS) / sizeof(MacroDefinition);

// ShaderLibrary class
class ShaderLibrary
{
public:
    ShaderLibrary();
    ~ShaderLibrary() { m_shaderCache.clear(); };

	// Get vertex/pixel shader
    Microsoft::WRL::ComPtr<ID3DBlob> GetVS(VS_FILE_ID shaderID, VS_ENTRY_ID entryID, uint64_t stageDefines = 0, uint64_t commonDefines = 0);
    Microsoft::WRL::ComPtr<ID3DBlob> GetPS(PS_FILE_ID shaderID, PS_ENTRY_ID entryID, uint64_t stageDefines = 0, uint64_t commonDefines = 0);

private:
    std::unordered_map<ShaderKey, Microsoft::WRL::ComPtr<ID3DBlob>, ShaderKeyHash> m_shaderCache{};

private:
	// Get or compile shader
    Microsoft::WRL::ComPtr<ID3DBlob> GetOrCompileShader(SHADER_STAGE stage, const ShaderDesc& desc, uint64_t stageDefines, uint64_t commonDefines);

    static std::vector<D3D_SHADER_MACRO> BuildMacros(SHADER_STAGE stage, uint64_t stageDefines, uint64_t commonDefines);
    static void AppendMacros(
        std::vector<D3D_SHADER_MACRO>& out,
        uint64_t defines,
        const MacroDefinition* table,
        size_t count
    );
};