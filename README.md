# 101Engine



\## 別環境でのセットアップ手順



\### 必要な環境

\- Visual Studio 2026（C++ CMake tools for Windows を含む。CMake 4.2 以上が必要）

\- Git



\### 手順

1\. リポジトリをclone

git clone --recursive https://github.com/JuicyKebabs/101Engine.git



2\. Developer Command Prompt for VS 2026を開く



3\. ビルドシステムを生成

cd 101Engine

mkdir build

cd build

cmake .. -G "Visual Studio 18 2026" -A x64



4\. `build/101Engine.sln`をVSで開いてビルド



\### 注意事項

\- `build/`フォルダはGitで管理していないため毎回生成が必要

\- VS 2022 の既存構成から移行する場合は、プロジェクトルートで `reconfigure.bat --fresh` を一度実行する。通常の再構成は `reconfigure.bat` を実行する。

\- `project.101`がプロジェクトルートに必要

