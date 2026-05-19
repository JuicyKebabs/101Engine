#include "ShaderLibrary.h"
#include <cassert>

// Output compile error messages
static inline void OutputCompileError(ID3DBlob* errorBlob, const wchar_t* file)
{
	if (!errorBlob) return;

	const char* msg = static_cast<const char*>(errorBlob->GetBufferPointer());
	if (!msg) return;

	OutputDebugStringW(L"[ShaderLibrary] Compile failed: ");
	OutputDebugStringW(file);
	OutputDebugStringW(L"\n");

	OutputDebugStringA(msg);
	OutputDebugStringA("\n");
}

static UINT GetCompileFlags()
{
	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG)
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	return flags;
}

// Constructor
ShaderLibrary::ShaderLibrary()
{
	m_shaderCache.clear();
}

// Get vertex shader
Microsoft::WRL::ComPtr<ID3DBlob> ShaderLibrary::GetVS(VS_FILE_ID fieldID, VS_ENTRY_ID entryID, uint64_t stageDefines, uint64_t commonDefines)
{
	// Validate vsId
	const size_t fileIndex = static_cast<size_t>(fieldID);
	if (fileIndex >= sizeof(VS_FILE_TABLE) / sizeof(VS_FILE_TABLE[0]))
	{
		OutputDebugStringA("[ShaderLibrary] Invalid VS_FILE_ID\n");
		return {};
	}
	const size_t entryIndex = static_cast<size_t>(entryID);
	if(entryIndex >= sizeof(VS_ENTRY_TABLE) / sizeof(VS_ENTRY_TABLE[0]))
	{
		OutputDebugStringA("[ShaderLibrary] Invalid VS_ID\n");
		return {};
	}

	ShaderDesc desc = {
		VS_FILE_TABLE[fileIndex],
		VS_ENTRY_TABLE[entryIndex],
		VS_PROFILE
	};

	// Get or compile shader
	return GetOrCompileShader(SHADER_STAGE::STAGE_VS, desc, stageDefines, commonDefines);
}

// Get pixel shader
Microsoft::WRL::ComPtr<ID3DBlob> ShaderLibrary::GetPS(PS_FILE_ID fieldID, PS_ENTRY_ID entryID, uint64_t stageDefines, uint64_t commonDefines)
{
	// Validate psId
	const size_t fileIndex = static_cast<size_t>(fieldID);
	if (fileIndex >= sizeof(PS_FILE_TABLE) / sizeof(PS_FILE_TABLE[0]))
	{
		OutputDebugStringA("[ShaderLibrary] Invalid PS_FILE_ID\n");
		return {};
	}
	const size_t entryIndex = static_cast<size_t>(entryID);
	if (entryIndex >= sizeof(PS_ENTRY_TABLE) / sizeof(PS_ENTRY_TABLE[0]))
	{
		OutputDebugStringA("[ShaderLibrary] Invalid PS_ID\n");
		return {};
	}

	ShaderDesc desc = {
		PS_FILE_TABLE[fileIndex],
		PS_ENTRY_TABLE[entryIndex],
		PS_PROFILE
	};

	// Get or compile shader
	return GetOrCompileShader(SHADER_STAGE::STAGE_PS, desc, stageDefines, commonDefines);
}

// Get or compile shader
Microsoft::WRL::ComPtr<ID3DBlob> ShaderLibrary::GetOrCompileShader(SHADER_STAGE stage, const ShaderDesc& desc, uint64_t stageDefines, uint64_t commonDefines)
{
	// Create shader key
	ShaderKey key = std::make_tuple(
		desc.filePath,
		desc.entryPoint,
		desc.profile,
		stageDefines,
		commonDefines,
		GetCompileFlags()
	);

	// Check if shader is already cached
	auto it = m_shaderCache.find(key);
	if (it != m_shaderCache.end())
	{// Return cached shader
		return it->second;
	}

	// Compile shader
	Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	// Build macros for compilation
	auto macros = BuildMacros(stage, stageDefines, commonDefines);

	HRESULT hr = D3DCompileFromFile(
		desc.filePath.c_str(),
		macros.data(),
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		desc.entryPoint.c_str(),
		desc.profile.c_str(),
		GetCompileFlags(),
		0,
		shaderBlob.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if(FAILED(hr) || !shaderBlob)
	{
		OutputCompileError(errorBlob.Get(), desc.filePath.c_str());
		return {};
	}

	// Cache the compiled shader
	m_shaderCache.emplace(key, shaderBlob);
	return shaderBlob;
}

std::vector<D3D_SHADER_MACRO> ShaderLibrary::BuildMacros(SHADER_STAGE stage, uint64_t stageDefines, uint64_t commonDefines)
{
	std::vector<D3D_SHADER_MACRO> macros;

	// Append common macros
	AppendMacros(macros, commonDefines, COMMON_SHADER_MACROS, COMMON_SHADER_MACROS_COUNT);

	// Append stage-specific macros
	switch (stage)
	{
	case SHADER_STAGE::STAGE_VS:
		AppendMacros(macros, stageDefines, VS_SHADER_MACROS, VS_SHADER_MACROS_COUNT);
		break;
	case SHADER_STAGE::STAGE_PS:
		AppendMacros(macros, stageDefines, PS_SHADER_MACROS, PS_SHADER_MACROS_COUNT);
		break;
	default:
		break;
	}

	macros.push_back({ nullptr, nullptr });
	return macros;
}

// Append macros based on defines and macro definition tables
void ShaderLibrary::AppendMacros(std::vector<D3D_SHADER_MACRO>& out, uint64_t defines, const MacroDefinition* table, size_t count)
{
	for (size_t i = 0; i < count; ++i)
	{
		if ((defines & table[i].bit) != 0)
		{
			out.push_back({ table[i].name, table[i].value == nullptr ? "1" : table[i].value });
		}
	}
}
