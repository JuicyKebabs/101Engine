#include "ProjectBuilder.h"
#include "GameCodeHotReloadConfig.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Core/Debug/Debug.h"
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <vector>

// 生成スクリプトはbuild/の直下に置く。内部のプロジェクトパスはすべて
// プロジェクトのルートからの相対パスとし、呼び出し側の作業ディレクトリに依存させない。
static std::filesystem::path GetBatchPath(const char* name)
{
	const auto build = std::filesystem::path(PathManager::GetProjectRoot()) / "build";
	std::filesystem::create_directories(build);
	return build / name;
}

static void WriteBatchPrologue(std::ofstream& bat)
{
	bat << "@echo off\nsetlocal EnableExtensions DisableDelayedExpansion\n";
	bat << "pushd \"%~dp0..\" >nul 2>&1 || exit /b 1\n";
}

static int RunBatch(std::ofstream& bat, const std::filesystem::path& path)
{
	bat.close();
	if (!bat)
	{
		DBG("ProjectBuilder: Could not write build script.");
		return 1;
	}
	// CALLを使い、空白を含むバッチパスの引用符を維持する。ワイド文字APIを使い、
	// cmd.exeの起動に用いるパスを狭い文字列へ変換しない。
	return _wsystem((L"call \"" + path.wstring() + L"\"").c_str());
}

// vswhere.exeでVisual Studioのインストール先を取得する補助関数。
// cmake.exeとvcvars64.batのパス解決に使用する。
static std::string GetVSInstallPath()
{
	FILE* pipe = _popen(
		"\"\"C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe\""
		" -latest -version \"[18.0,19.0)\" -property installationPath 2>NUL\"",
		"r");

	if (!pipe)
	{
		return "";
	}

	char buf[512] = {};
	std::string path;

	if (fgets(buf, sizeof(buf), pipe))
	{
		path = buf;
		while (!path.empty() &&
			(path.back() == '\n' || path.back() == '\r' || path.back() == ' '))
			path.pop_back();
	}

	_pclose(pipe);

	return path;
}

// whereコマンドを使ってcmake.exeのパスを解決する補助関数。
static std::string ResolveCMakePath()
{
	std::string installPath = GetVSInstallPath();

	if (!installPath.empty())
	{
		std::string cmakePath = installPath +
			"\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe";
		DBG("ProjectBuilder: cmake found at '%s'", cmakePath.c_str());
		return cmakePath;
	}

	DBG("ProjectBuilder: vswhere.exe not found, falling back to 'cmake' in PATH");
	return "cmake";
}

// 静的初期化時にcmake.exeのパスを解決する。
static const std::string kCMakePath = ResolveCMakePath();

bool ProjectBuilder::Reconfigure()
{
	const auto batPath = GetBatchPath("reconfigure_editor.bat");
	std::ofstream bat(batPath);
	WriteBatchPrologue(bat);
	bat << "\"" << kCMakePath << "\" -S . -B build"
		<< " -G \"Visual Studio 18 2026\" -A x64 > cmake_reconfigure.log 2>&1\n";
	bat << "exit /b %errorlevel%\n";

	DBG("ProjectBuilder: Running reconfigure via batch file...");
	int result = RunBatch(bat, batPath);

	if (result != 0)
	{
		DBG("ProjectBuilder: Reconfigure failed (code %d)", result);
		return false;
	}

	DBG("ProjectBuilder: Reconfigure succeeded.");
	return true;
}

bool ProjectBuilder::Build(const std::string& target, const std::string& config, bool buildDependencies)
{
	// 適切な引用符と出力リダイレクトでcmake --buildを呼ぶバッチファイルを生成する。
	const auto batPath = GetBatchPath("build_project.bat");
	std::ofstream bat(batPath);
	WriteBatchPrologue(bat);
	bat << "\"" << kCMakePath << "\" --build build"
		<< " --target \"" << target << "\" --config \"" << config << "\"";

	// 要求された場合は、依存先のビルドを省く。
	// （例：静的シングルトンと対象の接続が切れることを避けるため）
	if (!buildDependencies)
	{
		bat << " -- /p:BuildProjectReferences=false";
	}

	bat << " > cmake_build.log 2>&1\n";
	bat << "exit /b %errorlevel%\n";

	// バッチファイルを実行し、結果を確認する。
	DBG(
		"ProjectBuilder: Building target '%s' (%s)%s via batch file...",
		target.c_str(),
		config.c_str(),
		buildDependencies ? "" : " (skip deps)");
	int result = RunBatch(bat, batPath);

	if (result != 0)
	{
		DBG("ProjectBuilder: Build failed (code %d)", result);
		return false;
	}

	DBG("ProjectBuilder: Build succeeded.");
	return true;
}

bool ProjectBuilder::ReconfigureAndBuild(const std::string& target, const std::string& config, bool buildDependencies)
{
	if (!Reconfigure())
	{
		return false;
	}

	return Build(target, config, buildDependencies);
}

bool ProjectBuilder::BuildGameCodeForHotReload(const std::string& config)
{
	namespace fs = std::filesystem;

	// プロジェクトルートとVisual Studioのインストール先のパスを構築する。
	std::string projectRoot = PathManager::GetProjectRoot();
	const auto hotReloadConfig = ResolveGameCodeHotReloadConfig(projectRoot, config);

	if (!hotReloadConfig)
	{
		DBG("ProjectBuilder: Unsupported hot reload configuration '%s'", config.c_str());
		return false;
	}

	std::string vsPath = GetVSInstallPath();

	if (vsPath.empty())
	{
		DBG("ProjectBuilder: VS installation not found via vswhere.exe");
		return false;
	}

	// vcvars64.batと、101Frameworkが公開するインクルードディレクトリのパスを構築する。
	// ホットリロードではコンパイラを直接呼び出すため、CMakeの
	// target_include_directories設定は自動継承されない。
	std::string vcvars = vsPath + "\\VC\\Auxiliary\\Build\\vcvars64.bat";
	const auto gameCodeRoot = fs::path(projectRoot) / "Game" / "GameCode";
	std::string gameCodeDir = "Game\\GameCode";
	std::string frameworkInc = "Framework\\src";
	std::string thirdPartyInc = "third_party";
	std::string directXInc = thirdPartyInc + "\\DirectX\\d3dx12";
	std::string directXTexInc = thirdPartyInc + "\\DirectX\\DirectXTex\\include";
	std::string assimpInc = thirdPartyInc + "\\Assimp\\include";
	std::string imguiInc = thirdPartyInc + "\\ImGui";
	std::string imguiBackendInc = imguiInc + "\\backends";

	// ビルド生成物の出力先のパスを構築する。
	const auto scriptConfig = ResolveGameCodeHotReloadConfig(".", config);
	const std::string& objDir = scriptConfig->objectDirectory;
	const std::string& frameworkLib = scriptConfig->frameworkLibrary;
	const std::string& outputDll = scriptConfig->stagedDll;
	const std::string& outputLib = scriptConfig->stagedLibrary;
	std::string logPath = "hotreload_compile.log";
	const auto batPath = GetBatchPath("build_hotreload.bat");

	// クリーンなビルドのため、一時GameCode.dllとGameCode.libがあれば削除する。
	std::error_code error;

	fs::remove(hotReloadConfig->stagedDll, error);

	if (error)
	{
		DBG("ProjectBuilder: Failed to remove %s (error %s)", outputDll.c_str(), error.message().c_str());
		return false;
	}

	error.clear();

	fs::remove(hotReloadConfig->stagedLibrary, error);

	if (error)
	{
		DBG("ProjectBuilder: Failed to remove %s (error %s)", outputLib.c_str(), error.message().c_str());
		return false;
	}

	// ホットリロード用の全ソースファイルを収集する。
	std::vector<std::string> sources;

	for (const auto& entry : fs::recursive_directory_iterator(gameCodeRoot))
	{
		if (entry.path().extension() == ".cpp")
		{
			sources.push_back(entry.path().lexically_relative(fs::path(projectRoot)).string());
		}
	}

	// ホットリロードが必要か確認する。
	if (sources.empty())
	{
		DBG("ProjectBuilder: No .cpp files found in Game/GameCode/");
		return false;
	}

	// .objファイルの保存先ディレクトリがなければ作成する。
	fs::create_directories(hotReloadConfig->objectDirectory);

	// ホットリロード用にGameCodeをコンパイル・リンクするバッチファイルを生成する。
	std::ofstream bat(batPath);
	{
		WriteBatchPrologue(bat);
		bat << "if not exist \"" << objDir << "\" mkdir \"" << objDir << "\"\n";
		bat << "if errorlevel 1 exit /b 1\n";

		// vcvars64.batを呼び、Visual Studioの環境を設定する。
		// バッチファイル内でcl.exe、link.exeなどの付属ツールを使えるようにする。
		bat << "call \"" << vcvars << "\" >nul 2>&1\n";
		bat << "if errorlevel 1 (\n";
		bat << "    echo [HotReload] vcvars64.bat failed\n";
		bat << "    exit /b 1\n";
		bat << ")\n\n";

		// すべての.cppファイルを.objファイルへコンパイルする。
		// 全.cppファイルをcl.exeへ渡し、objファイルを生成する。
		bat << "echo [HotReload] Compiling " << (int)sources.size() << " files...\n";

		bat << "cl.exe /nologo " << hotReloadConfig->compilerFlags
			<< " /utf-8 /Z7 /EHsc /std:c++latest /DNOMINMAX /c";

		bat << " /I\"" << gameCodeDir << "\"";
		bat << " /I\"" << frameworkInc << "\"";
		bat << " /I\"" << thirdPartyInc << "\"";
		bat << " /I\"" << directXInc << "\"";
		bat << " /I\"" << directXTexInc << "\"";
		bat << " /I\"" << assimpInc << "\"";
		bat << " /I\"" << imguiInc << "\"";
		bat << " /I\"" << imguiBackendInc << "\"";

		bat << " /Fo\"" << objDir << "\\\\\"";

		for (const auto& src : sources)
		{
			bat << " \"" << src << "\"";
		}

		bat << " > \"" << logPath << "\" 2>&1\n";
		bat << "if errorlevel 1 (\n";
		bat << "    echo [HotReload] Compile FAILED - see hotreload_compile.log\n";
		bat << "    exit /b 1\n";
		bat << ")\n\n";

		// 全.objファイルをGameCode.dllとGameCode.libへリンクする。
		bat << "echo [HotReload] Linking...\n";
		bat << "link.exe /nologo /DLL /OPT:NOREF /DEBUG:NONE";
		bat << " /OUT:\"" << outputDll << "\"";
		bat << " /IMPLIB:\"" << outputLib << "\"";
		bat << " \"" << frameworkLib << "\"";

		for (const auto& src : sources)
		{
			std::string stem = fs::path(src).stem().string();
			bat << " \"" << objDir << "\\" << stem << ".obj\"";
		}

		bat << " >> \"" << logPath << "\" 2>&1\n";
		bat << "if errorlevel 1 (\n";
		bat << "    echo [HotReload] Link FAILED - see hotreload_compile.log\n";
		bat << "    exit /b 1\n";
		bat << ")\n\n";

		bat << "echo [HotReload] Build succeeded.\n";
		bat << "exit /b 0\n";
	}

	// バッチファイルを実行し、ホットリロード用のGameCodeをコンパイル・リンクする。
	DBG(
		"ProjectBuilder: Hot reload build (%s, %d files) via cl.exe...",
		hotReloadConfig->configuration.c_str(),
		(int)sources.size());
	int result = RunBatch(bat, batPath);

	// ホットリロードが成功したか失敗したか確認する。
	if (result != 0)
	{
		DBG("ProjectBuilder: Hot reload build FAILED (code %d)", result);
		DBG("ProjectBuilder: See hotreload_compile.log for details");
		return false;
	}

	// 成功
	DBG("ProjectBuilder: Hot reload build succeeded.");
	return true;
}
