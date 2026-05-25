# 101Engine



\## 別環境でのセットアップ手順



\### 必要な環境

\- Visual Studio 2022

\- Git



\### 手順

1\. リポジトリをclone

git clone --recursive https://github.com/JuicyKebabs/101Engine.git



2\. Developer Command Prompt for VS 2022を開く



3\. ビルドシステムを生成

cd 101Engine

mkdir build

cd build

cmake .. -G "Visual Studio 17 2022" -A x64



4\. `build/101Engine.sln`をVSで開いてビルド



\### 注意事項

\- `build/`フォルダはGitで管理していないため毎回生成が必要

\- `project.101`がプロジェクトルートに必要

