# 101Engine

DirectX 12を使用したゲームエンジンです。シーン編集用のEditor、ゲーム実行環境、描画機能の検証用Sandboxを含みます。

## 必要な環境

- Windowsの64ビット環境、DirectX 12に対応したGPU
- Visual Studio 2026
  - 「C++によるデスクトップ開発」
  - Windows SDK（シェーダーコンパイラの`fxc.exe`を含む）
  - C++ CMake tools for Windows（CMake 4.2以降）

外部ライブラリのヘッダーと必要なライブラリファイルは、`third_party`に同梱しています。

## ビルド・実行手順

1. プロジェクトのルートにある`reconfigure.bat`を実行します。
2. `[OK] Configuration complete.`と表示されたことを確認します。`build`フォルダにVisual Studio用のビルド設定が生成されます。
3. `build/101Engine.slnx`をVisual Studioで開きます。
4. 構成を`Debug`または`Release`、プラットフォームを`x64`に設定します。
5. 次のいずれかをスタートアッププロジェクトに設定し、ビルド・実行します。必要な依存プロジェクトもビルドされます。

| プロジェクト | 用途 |
|---|---|
| `101Editor` | シーン・Actor・Componentなどを編集するEditor |
| `101Game` | `project.101`で指定した開始シーンからゲームを実行 |
| `RendererSandbox` | Renderer単体の描画機能を検証するアプリケーション |

ソースファイルを追加・削除した場合は、`reconfigure.bat`を再実行してください。プロジェクトの配置場所を変更した場合は、移動先で`build`フォルダを削除してから、同バッチを実行してください。

## フォルダ構成

| フォルダ | 内容 |
|---|---|
| `asset` | ゲームで使用するシーン、ActorImprint、モデル、テクスチャ、音声などのアセット |
| `Config` | Windowsアプリケーションのマニフェストなど、実行環境に関する設定 |
| `Editor` | `101Editor`の実装。編集画面、編集コマンド、インスペクター、ビルド・ホットリロード機能など |
| `FrameWork` | `101Framework`の実装。Actor・Component・Scene、入力、音声、衝突判定、アセット管理など |
| `Game` | `101Game`の実装。`GameCode`配下には、ゲーム固有の振る舞いを実装した`GameCode.dll`のソースを格納 |
| `Renderer` | `101Renderer`の実装。DirectX 12のリソース管理、描画処理、レンダーグラフ、シェーダーなど |
| `RendererAssets` | `101RendererAssets`の実装。モデル・画像ファイルをCPU側で読み込み、Rendererに渡すデータへ変換 |
| `RendererSandbox` | Renderer単体の動作を確認するアプリケーションの実装 |
| `Saved` | Editorのウィンドウ配置などの保存済み設定 |
| `third_party` | Assimp、DirectXTex、ImGui、nlohmann/jsonなどの外部ライブラリ |

`build`フォルダは、`reconfigure.bat`の実行時に生成されます。ビルド設定・中間ファイル・ビルド結果の出力先です。

## ルートの主なファイル

- `CMakeLists.txt`：各プロジェクトのビルド構成を定義します。
- `reconfigure.bat`：CMakeを検出し、Visual Studio 2026・x64用のビルド設定を生成します。
- `project.101`：Editor・Gameの開始シーンやタグを保持するプロジェクト設定です。ルートに配置してください。
- `imgui.ini`：ImGuiのウィンドウ配置設定です。

アセットの`.meta`ファイルは参照先を識別するGUIDを保持するため、対応するアセットと一緒に維持してください。

## 外部ライブラリのライセンス

[licenses](licenses/)にライセンス原文を保存しています。
