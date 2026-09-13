# 101Engine コード記述スタイル調査

## 目的

この文書は、101Engine の将来の実装・レビューで既存コードに調和する判断を行うための、コード記述スタイルと実装慣習の参照資料である。単なる見た目の整形規則だけでなく、所有権、ライフタイム、失敗の表現、テスト構成、ビルド境界まで含めて記録する。

これは新しい規約を制定する文書ではない。現行リポジトリから安定して観測できるパターン、局所的な揺れ、今後の実装で模倣すべきでない偶発的不整合を分離した調査結果である。

## 結論

101Engine の中心的な C++ スタイルは、次の組み合わせである。

- C++20、Windows、DirectX 12、CMake を前提とする。
- 型、関数、メソッドは PascalCase、ローカル変数と引数は lowerCamelCase、非 static データメンバーは `m_` 接頭辞を使う。
- インデントはタブ、波括弧は Allman 形式を基本とする。
- ヘッダーはほぼ常に `#pragma once` を使い、クラス宣言と実装を `.h` / `.cpp` に分離する。
- 所有権は値型、`std::unique_ptr`、COM 用 `ComPtr` で表し、raw pointer は非所有参照として使う。
- 回復可能な失敗は `bool`、`nullptr`、0 などのセンチネルで返し、呼び出し側が即時に検査する。診断は `DBG`、内部不変条件は `assert` を使う。
- 深いネストより、入力検証と早期 return を優先する。
- テストは小さな独立実行ファイルで、匿名 namespace、`Check`、`g_failures`、`main` を使う軽量な構成が多数派である。
- ただし自動フォーマッタは設定されておらず、タブと 4 スペース、include 順、1 行 `if`、ポインター名の `p` 接頭辞などには揺れがある。変更対象の近傍に合わせ、無関係な再整形を行わない。

この結論の確度は「高い」が、全ファイルへ機械的に適用できる絶対規則ではない。実装時は、まず変更対象と同じサブシステムの未除外ファイルを局所的な正解として扱う。

## 証拠範囲

### 調査対象

主要な対象は、`FrameWork/src`、`Editor/src`、`Game/src`、`Game/GameCode`、`Tests`、ルートおよび各ターゲットの `CMakeLists.txt`、`.gitattributes`、Git 履歴である。`third_party`、生成物、バイナリ資産、`build` はスタイル学習対象外とした。

添付の AI 生成除外リストを適用した後の C++ コーパスは 214 ファイル、約 31,802 行である。ファイル単位の優勢インデントはタブ 182、4 スペース 22、同数または実質無内容 10 だった。独立行の開始波括弧は 2,521 件、関数宣言等と同一行の開始波括弧は 65 件だった。対象ヘッダー 107 件のうち 105 件が先頭で `#pragma once` を使っている。

この数値は「何を書くべきか」を単独で決めるものではないが、タブ + Allman がコードベース全体の強い多数派であることを裏付ける。

### AI 由来コードの扱い

添付資料の分類は、スタイル学習にのみ適用する。除外されたコードも、現行の仕様、依存関係、API、挙動を理解するためには必要に応じて参照してよい。除外はアーキテクチャ上の妥当性や採用可否を判定するものではない。[^1]

次のファイルは AI 新規作成として、記述スタイルの学習元から完全に除外する。

```text
FrameWork/src/Engine/Core/Reflection/PropertyMetadata.h
FrameWork/src/Engine/Core/Reflection/PropertyMetadata.cpp
FrameWork/src/Engine/Core/Reflection/PropertyPath.h
FrameWork/src/Engine/Core/Reflection/PropertyPath.cpp
FrameWork/src/Engine/Core/Reflection/ReflectionSerialization.h
FrameWork/src/Engine/Core/Reflection/ReflectionSerialization.cpp
FrameWork/src/Engine/Core/Reflection/ActorReferenceCodec.h
FrameWork/src/Engine/Core/Reflection/ActorReferenceCodec.cpp
FrameWork/src/Engine/Scene/SceneActorReferenceContext.h
FrameWork/src/Engine/Scene/SceneActorReferenceContext.cpp
FrameWork/src/Engine/Resource/AssetType.h
FrameWork/src/Engine/Resource/AssetReference.h
FrameWork/src/Engine/Core/Reflection/AssetReferenceCodec.h
FrameWork/src/Engine/Core/Reflection/AssetReferenceCodec.cpp
FrameWork/src/Engine/Resource/AssetManagerAssetReferenceContext.h
FrameWork/src/Engine/Resource/AssetManagerAssetReferenceContext.cpp
FrameWork/src/Engine/Component/ComponentReflection.h
FrameWork/src/Engine/Component/ComponentReflection.cpp
FrameWork/src/Engine/Component/PersistentComponentMetadata.h
FrameWork/src/Engine/Component/PersistentComponentMetadata.cpp
FrameWork/src/Engine/Component/PersistentCameraMetadata.cpp
FrameWork/src/Engine/Component/PersistentColliderMetadata.cpp
FrameWork/src/Engine/Component/PersistentTransformMetadata.cpp
Editor/src/UI/Inspector/ReflectionInspector.h
Editor/src/UI/Inspector/ReflectionInspector.cpp
Editor/src/Command/ComponentPropertyEditCommand.h
Editor/src/Command/ComponentPropertyEditCommand.cpp
Tests/PropertyMetadataTests.cpp
Tests/ReflectionSerializationTests.cpp
Tests/ActorReferenceCodecTests.cpp
Tests/AssetReferenceCodecTests.cpp
Tests/ReflectionInspectorTests.cpp
Tests/ComponentPropertyEditCommandTests.cpp
```

次のファイルは既存コードに AI 変更が混在するため、一次的なスタイル見本には使わない。

```text
Editor/CMakeLists.txt
Editor/src/Core/EditorApp.cpp
Editor/src/Core/EditorApp.h
Editor/src/UI/Inspector/InspectorContext.h
Editor/src/UI/Inspector/InspectorPanel.h
Editor/src/UI/Inspector/InspectorPanel.cpp
FrameWork/src/Engine/Component/Behavior.h
FrameWork/src/Engine/Component/Camera.cpp
FrameWork/src/Engine/Component/Camera.h
FrameWork/src/Engine/Component/Collider.cpp
FrameWork/src/Engine/Component/Collider.h
FrameWork/src/Engine/Component/MeshRenderer.cpp
FrameWork/src/Engine/Component/MeshRenderer.h
FrameWork/src/Engine/Component/RectTransform.cpp
FrameWork/src/Engine/Component/RectTransform.h
FrameWork/src/Engine/Component/RendererComponent.h
FrameWork/src/Engine/Component/SpriteRenderer.cpp
FrameWork/src/Engine/Component/SpriteRenderer.h
FrameWork/src/Engine/Component/Transform.cpp
FrameWork/src/Engine/Component/Transform.h
FrameWork/src/Engine/EngineComponentRegistration.cpp
FrameWork/src/Engine/Resource/AssetManager.h
FrameWork/src/Engine/Scene/ComponentRegistry.h
FrameWork/src/Engine/Scene/ComponentSerializer.cpp
FrameWork/src/Engine/Scene/ComponentDeserializer.cpp
FrameWork/src/Engine/UI/Canvas.cpp
FrameWork/src/Engine/UI/Canvas.h
FrameWork/src/Engine/UI/UIImage.cpp
FrameWork/src/Engine/UI/UIimage.h
FrameWork/src/Engine/UI/UIRenderer.cpp
FrameWork/src/Engine/UI/UIRenderer.h
Tests/CMakeLists.txt
Tests/ComponentCommandTests.cpp
Tests/ComponentSerializationTests.cpp
Tests/ComponentSnapshotTests.cpp
```

次のファイルは設計レビューの変更記録から AI 変更が推定されるため、同様に保守的に除外する。

```text
FrameWork/src/Engine/Component/Component.h
FrameWork/src/Engine/Component/Component.cpp
Editor/src/Command/EditorCommandHistory.h
Editor/src/Command/EditorCommandHistory.cpp
Tests/EditorCommandHistoryTests.cpp
```

`Game/GameCode/TestBehavior.cpp`、`Game/GameCode/TestBehavior.h`、`asset/scenes/test.scene` は元資料で明示的に分類対象外であり、人間由来とも AI 由来とも確定できない。そのためスタイルの一次資料にはせず、現在のサンプル挙動としてのみ扱う。[^1]

## プロジェクト構造と依存境界

ルートの CMake は C++20 を要求し、`101Framework`、`GameCode`、`101Editor`、`101Game`、テスト群を構成する。`101Framework` と `GameCode` は DLL、Editor と Game は Win32 実行ファイルである。[^2]

概念上の依存方向は次のように読める。

```text
101Editor ─┐
           ├──> 101Framework ───> Win32 / DirectX 12 / Assimp / DirectXTex
101Game ───┤
           └──> GameCode ───────> 101Framework
```

実装では、この既存境界を崩す include やライブラリ依存を安易に追加しない。外部ライブラリ型をエンジン公開 API に露出させる必要が生じた場合は、単なる記述スタイルではなく API 境界の設計判断として扱う。

実ディレクトリ名は `FrameWork` だが CMake の一部は `Framework` と表記している。Windows の大文字小文字非区別に依存して成立しているため、既存表記の一括修正はスタイル整理として行わない。新しいソースパスは Git 上の実表記 `FrameWork` に合わせる。

## C++ 記述スタイル

### インデントと波括弧

基本形はタブ 1 個を論理インデント 1 段として使う Allman 形式である。

```cpp
bool CommandQueue::Flush()
{
	const uint64_t fenceValue = Signal();

	if (fenceValue == 0)
	{
		return false;
	}

	return WaitForFence(fenceValue);
}
```

継続行は、呼び出しや条件式の構造が読み取れる位置まで追加インデントする。既存コードには閉じ括弧の位置や継続行の字下げに複数の流儀があるため、編集対象ファイルの近傍を優先する。

4 スペースが優勢なまとまりも存在する。特に Editor camera、template generator、StringEncoding、古い Math / keyboard の一部が該当する。これらを触る場合は、そのファイル内で一貫させる。Ticket に整形が含まれない限り、タブへ一括変換しない。

### 空白と改行

- `if`、`for`、`while` と `(` の間には空白を置く。
- 関数呼び出し名と `(` の間には空白を置かない。
- 二項演算子の前後に空白を置く。
- ポインターと参照の `*` / `&` は型側に付ける。例: `Actor* actor`、`const Guid& guid`。
- 意味のまとまりごとに空行を入れ、検証、処理、結果返却を分離する。
- 短い getter、単純な guard、空のライフサイクルメソッドは 1 行の場合がある。ただし複雑な条件や副作用を 1 行へ圧縮しない。
- 列揃え用タブと末尾コメントが古いコードに多いが、新規コードで手作業の列揃えを増やす必要はない。差分耐性と可読性を優先する。

### 命名

| 対象 | 観測された形式 | 例 |
|---|---|---|
| class / struct / enum class | PascalCase | `ActorPool`, `FrameSlot`, `Window::Mode` |
| public / private method | PascalCase | `Initialize`, `CollectGarbage`, `WaitForFence` |
| local variable / parameter | lowerCamelCase | `frameIndex`, `fenceValue`, `actorGuid` |
| non-static member | `m_` + lowerCamelCase | `m_slots`, `m_nextFenceValue`, `m_hasResizeRequest` |
| static member | 多くは `s_`、古い singleton では `m_instance` も存在 | `s_projectRoot`, `m_instance` |
| file-scope test state | `g_` + lowerCamelCase | `g_failures`, `g_fileIndex` |
| enum enumerator | PascalCase が新しい型で優勢 | `Windowed`, `BorderlessFullscreen` |
| legacy constant / Win32 寄り定数 | UPPER_SNAKE_CASE | `WINDOW_WIDTH`, `MAX_RTV_DESCRIPTORS` |
| bool | `is` / `has` / `can` / `should` 等を含む | `m_isUsable`, `m_hasSnapshot` |
| output parameter | `out` 接頭辞 | `outGuid`, `outSize`, `outStyle` |

ポインターの `p` 接頭辞は一貫していない。Graphics や古い App では `pDevice`、`m_pFence`、`m_pEngine` が多い一方、Window や新しい Editor command では `device`、`m_scene`、`m_hwnd` が使われる。全体規則として `p` を強制せず、同一クラス／サブシステムの既存命名へ合わせる。`m_` は強い共通規則として維持する。

### ヘッダーと include

- 通常の新規ヘッダーは先頭に `#pragma once` を置く。
- `.cpp` は対応する自分のヘッダーを先頭に置く傾向が強い。
- プロジェクト内 include は `"Engine/..."`、同一ディレクトリの近接ヘッダーは `"CommandQueue.h"` のような相対名も使われる。
- STL / Windows / third-party とプロジェクト include の順序は統一されていない。既存ファイル内のグループを維持し、新規ファイルでは「対応ヘッダー、プロジェクトヘッダー、標準／プラットフォームヘッダー」のように空行で意味的に分離するのが安全である。ただしこれは現状の絶対規則ではない。
- ヘッダーでは、値型として完全型が必要な場合を除き forward declaration を活用する例が多い。
- include の大規模な並べ替えは、Ticket の必要性がない限り行わない。

### クラス宣言の配置

クラス内は `public`、`protected`、`private` の責務単位でまとめる。既存コードには同じアクセス指定子を複数回置き、公開 API、状態、内部 helper を視覚的に分ける例がある。`Window` と `ActorPool` が明瞭な見本である。[^3][^4]

小さな getter はヘッダー内 inline、状態変更や失敗し得る処理は `.cpp` 実装が基本である。コピーやムーブが危険なリソース所有型では、copy constructor / assignment を `= delete` する。OS ハンドルや COM オブジェクトを保持する型では、destructor / `Terminate` の責務を明示する。[^3][^5]

### コンストラクターと初期化

- メンバーは可能な限り宣言時初期化する。例: `= nullptr`、`= false`、`{}`。
- constructor initializer list を使う場合は、1 メンバー 1 行に分ける形式と同一行形式の両方がある。新規コードは複数メンバーなら 1 行ずつを優先する。
- `InitDesc` のような構造体で初期化パラメーターをまとめるパターンが複数サブシステムにある。
- リソース型は二段階初期化 (`Initialize` / `Terminate`) が多い。既存 API がこの形なら RAII への局所的な作り替えを混ぜず、その lifecycle contract を守る。

## 所有権とライフタイム

### 値、unique ownership、observer の区別

所有権を持つ単一オブジェクトには `std::unique_ptr` が使われる。`ActorPool::Slot` は `std::unique_ptr<Actor>` を保持し、`CollectGarbage` が `OnDestroy` 後に解放する。Scene は `ActorPool` を値として所有し、Actor の外部参照には世代付き `ActorHandle` と一時的な raw pointer を併用する。[^4][^6]

COM オブジェクトにはエンジンの `ComPtr` wrapper を使う。Win32 の `HANDLE` のように wrapper がないものは destructor で明示的に解放し、解放後に null へ戻す。`CommandQueue` が代表例である。[^5]

raw pointer は主に非所有 observer、サービス参照、解決済みオブジェクトへの短命な参照として使われる。新規 raw pointer メンバーを追加する場合は、所有者、無効化条件、参照可能期間が既存設計から明白でなければコメントまたは API で明示する。

### deferred destruction

Actor / Component 周辺では、破棄要求と実解放を分離する。`Destroy` または mark-for-destruction 後もフレーム末の収集までは解決可能な設計がある。新規コードは、`IsDestroyed`、owner scene、handle generation を検査し、raw pointer の存在だけで有効性を判断しない。[^4][^6]

この部分は単なる記述スタイルではなく、101Engine の重要なライフタイム規則である。即時 delete、所有権の複製、収集前の handle 再利用は行わない。

## 制御フロー、失敗、診断

### 早期 return

公開処理は、null、未初期化、範囲外、既に適用済み等を冒頭で検査する。回復可能な失敗は `false` / `nullptr` / 0 を返すパターンが中心である。Editor command は特に guard clause を多用し、処理が成功した場合だけ内部状態を更新する。[^7]

複数の状態を変更する処理では、途中失敗時に既存状態を破壊しない順序を選ぶ。Window 初期化失敗時の `Terminate`、Actor 復元の transaction、Editor command の snapshot などに、この意図が見える。

### `DBG` と `assert`

- 入力不備、初期化失敗、Win32 / DirectX API 失敗など、実行時に診断すべき事象には `DBG` を使う。
- メッセージは `Class::Method: reason` 形式が比較的新しいコードで安定している。
- OS エラーを得られる場合は `GetLastError` や HRESULT の文脈を残す。
- 呼び出し側が扱える失敗は return 値でも伝え、ログだけで成功扱いにしない。
- 到達してはいけない状態やプログラマーエラーには `assert` が使われる。ユーザー入力や通常のリソース欠落を assert だけで処理しない。

例外 (`throw` / `catch`) は一次コーパスで一般的な制御手段ではない。新規 API へ例外ベースの失敗モデルを導入するには、DLL 境界、既存呼び出し規約、cleanup 方針を含む明示的な設計判断が必要である。

### 状態フラグ

`m_isInitialized`、`m_isUsable`、`m_hasSnapshot`、`m_isApplied` のような bool フラグで lifecycle を表す例が多い。追加時は「どの操作で true/false になるか」「失敗後に再試行できるか」を明確にし、成功前にフラグを更新しない。

## コメントとドキュメント

コメントは英語が中心で、次の用途に使われる。

- class の責務を説明する罫線付きヘッダーコメント。
- public method の lifecycle、所有権、遅延処理、利用制約の説明。
- GPU/CPU 同期、undo/redo、transaction のような非自明な「なぜ」の説明。
- メンバー末尾の短い役割説明。

新規コメントはコードを英訳するだけでなく、責務、前提、順序制約、失敗時の意味を説明する。綴り間違い、古い説明、実装と重複する末尾コメントも存在するため、既存コメント量を機械的に模倣しない。

古い HLSL や input コードには文字化けして表示されるコメントがある。エンコーディングを確認せずに保存すると大量差分や情報消失を起こし得る。該当ファイルを変更する Ticket では、変更前の byte encoding と Git diff を確認する。

## テストスタイル

Tests の多くは外部テストフレームワークではなく、1 テスト対象につき 1 executable の構成である。代表的な構造は次のとおり。[^8]

```cpp
namespace
{
	int g_failures = 0;

	void Check(bool condition, const std::string& name)
	{
		// PASS / FAIL を表示し、失敗数を更新する
	}

	void TestSpecificBehavior()
	{
		// arrange, act, Check(...)
	}
}

int main()
{
	TestSpecificBehavior();
	return g_failures == 0 ? 0 : 1;
}
```

テスト名は `Test` + 振る舞い、assertion message は期待する observable behavior を英語で表す。内部実装ではなく、handle の無効化、deferred destruction、redo 後の identity 保持、失敗時の非変更などの契約を検証する。

一時ファイルを使うテストでは、小さな RAII helper を匿名 namespace に置き、destructor で `std::error_code` 付き cleanup を行う例がある。テスト追加時は対象に最も近い既存テストの harness を再利用し、新しいテストフレームワークを単独導入しない。

## CMake と HLSL

CMake は 4 スペースインデントが基本で、target 単位に source、include、link、post-build を記述する。既存の `file(GLOB_RECURSE ...)` 方針に従うため、新規 `.cpp` は通常自動的に対象へ入る。ただし新規 target、test executable、生成手順は明示的な CMake 変更が必要になる。[^2]

HLSL は 4 スペース、Allman 形式が基本である。定数バッファの field は lowerCamelCase、global shader resource は `g` 接頭辞 (`gTexture`、`gSampler`)、helper は PascalCase、entry point は `main` を使う。CPU 側 layout と HLSL cbuffer の順序・アラインメントは ABI なので、見た目の整理として変更しない。[^9]

## 一次的なスタイル見本

今後の実装では、変更対象に近い次の未除外ファイルを優先参照する。

| 領域 | 主な見本 | 学べる点 |
|---|---|---|
| Win32 lifecycle | `FrameWork/src/Engine/Window/Window.h/.cpp` | 初期化、cleanup、状態検証、copy/move 禁止、診断 |
| D3D12 command / sync | `FrameWork/src/Engine/Graphics/CommandQueue.h/.cpp` | ComPtr、HANDLE、HRESULT、fence、失敗伝播 |
| frame resource state | `FrameWork/src/Engine/Graphics/FrameCommandManager.h/.cpp` | 状態フラグ、frame slot、GPU 待機の責務 |
| Actor ownership | `FrameWork/src/Engine/Actor/ActorPool.h/.cpp` | unique ownership、handle generation、deferred destruction |
| GUID value type | `FrameWork/src/Engine/Core/GUID/Guid.h/.cpp` | 小さな値型、変換、parse API、hash specialization |
| Editor command | `Editor/src/Command/AddComponentCommand.h/.cpp` | Execute/Undo、guard clause、snapshot、状態遷移 |
| Editor command | `Editor/src/Command/DeleteActorCommand.h/.cpp` | identity を Guid で保持し dangling pointer を避ける設計 |
| テスト | `Tests/ActorPoolTests.cpp` | lightweight harness、ライフタイム契約のテスト |
| scene I/O テスト | `Tests/SceneLoaderTests.cpp` | temporary resource RAII、失敗時の非変更検証 |
| Build | ルート、`FrameWork`、`Game`、`Game/GameCode` の CMake | target 境界、C++20、DLL 構成 |

除外対象ファイルを変更する Ticket では、そのファイル自身の style を新しいコードの根拠にせず、同じ責務を持つ上表の未除外ファイルと変更箇所周辺の human-authored 部分を照合する。

## 模倣しない偶発的不整合

次は「既に存在する」という理由だけで新規コードへ広げない。

- タブとスペースの混在、同一ブロック内の不均一な字下げ。
- `if(condition)` のような keyword 後の空白欠落。
- `} else {` と Allman の混在。
- 1 行に複数の副作用を詰めた inline method。
- `size_t` と `std::size_t`、`NULL` と `nullptr`、C-style cast と `static_cast` の無目的な混在。
- include 順の無差別な並べ替え。
- コメントの綴り間違い、実装と食い違うコメント、単純な代入の逐語説明。
- `m_pX` と `m_x` の全体一括 rename。
- ファイル名・include の大小文字不一致を Ticket 外で修正すること。
- 文字コードを確認せず、文字化けしたファイル全体を保存し直すこと。

新規コードでは `nullptr`、`static_cast`、宣言時初期化、限定された `const`、明示的 ownership を優先する。ただし、これを口実に周辺の legacy code を同時改修しない。

## 実装時の判断順序

1. Active Task Ticket の Goal、Design Constraints、Scope、Out of Scope、Acceptance Criteria を確認する。
2. 変更対象が AI 除外リストに含まれるか確認する。
3. 同じサブシステムの未除外 `.h/.cpp` を 2 組以上確認する。
4. ownership、lifetime、thread / GPU synchronization、serialization identity を先に特定する。
5. 変更対象ファイルの局所スタイルへ合わせ、無関係な整形を避ける。
6. 回復可能な失敗の返し方と `DBG` の診断文脈を既存 API に合わせる。
7. observable contract を検証する最小のテストを、既存 harness へ追加する。
8. `git diff --check`、関連 build、関連 CTest、変更ファイルの diff を確認する。
9. 結果を「観測」「判断」「制約」「未検証」に分けてユーザーへ説明する。

## 実装前チェックリスト

- [ ] Ticket に必要な 6 セクションがあり、Active Task Ticket が確立している。
- [ ] 変更対象と同じ責務の未除外コードを読んだ。
- [ ] 所有者と observer が区別されている。
- [ ] destruction / cleanup の時点が既存 lifecycle と一致する。
- [ ] public API の失敗表現が呼び出し側と一致する。
- [ ] bool 状態の遷移と失敗後の状態が説明できる。
- [ ] raw pointer の有効期間と null 条件が説明できる。
- [ ] DirectX / Win32 resource の release と同期点が説明できる。
- [ ] serialization / editor command では identity と undo/redo の契約を確認した。
- [ ] タブ / スペースはファイル内で一貫し、無関係な再整形がない。
- [ ] コメントは「なぜ」と制約を説明し、コードの逐語訳になっていない。
- [ ] 関連テストは observable behavior と失敗時の非変更を検証する。
- [ ] encoding、line ending、大文字小文字だけの不要差分がない。

## 制約と未確定事項

プロジェクトには formatter / linter の正式設定がないため、この文書だけで機械的に書式を確定することはできない。特に include 順、継続行、pointer の `p` 接頭辞、短い guard の 1 行表記は局所判断が必要である。

また、添付資料は Reflection 関連作業の changelist を根拠にした分類であり、それ以外の全履歴について AI 関与がないことを証明するものではない。したがって「未除外 = 必ず人間のみが記述」とは断定せず、複数の未除外ファイルと Git 履歴で共通するパターンだけを強い慣習とした。

この不確実性があるため、将来の Ticket でスタイル上の選択肢が競合した場合は、全体多数派よりも変更対象サブシステムの一貫性と最小差分を優先する。

## Sources

[^1]: `1-Exclusion-Contents.txt`, “AI-Generated Style Exclusion List,” user-provided local attachment, accessed 2026-09-12. Reflection work の AI 新規、AI 混在、保守的除外、分類対象外ファイルの判定に使用。
[^2]: `CMakeLists.txt`, lines 1–24; `FrameWork/CMakeLists.txt`, lines 1–18; `Game/GameCode/CMakeLists.txt`, lines 1–29; `Game/CMakeLists.txt`, lines 1–36. C++ version、target、DLL / executable、依存境界に使用。
[^3]: `FrameWork/src/Engine/Window/Window.h`, lines 13–117; `FrameWork/src/Engine/Window/Window.cpp`, lines 5–123. class layout、resource lifecycle、guard、診断、copy/move 制御に使用。
[^4]: `FrameWork/src/Engine/Actor/ActorPool.h`, lines 11–61; `FrameWork/src/Engine/Actor/ActorPool.cpp`, lines 4–117. Actor ownership、generation handle、deferred destruction に使用。
[^5]: `FrameWork/src/Engine/Graphics/CommandQueue.h`, lines 6–40; `FrameWork/src/Engine/Graphics/CommandQueue.cpp`, lines 5–194. COM ownership、Win32 HANDLE、HRESULT、GPU fence、cleanup に使用。
[^6]: `FrameWork/src/Engine/Scene/SceneBase.h`, lines 28–252; `FrameWork/src/Engine/Scene/SceneBase.cpp`, lines 160–341. Scene / ActorPool ownership、owner 検証、遅延破棄、immediate editor operation に使用。
[^7]: `Editor/src/Command/AddComponentCommand.h`, lines 19–48; `Editor/src/Command/AddComponentCommand.cpp`, lines 7–116; `Editor/src/Command/DeleteActorCommand.h`, lines 13–38; `Editor/src/Command/DeleteActorCommand.cpp`, lines 6–56. Editor command の状態遷移、guard、identity、snapshot に使用。
[^8]: `Tests/ActorPoolTests.cpp`, lines 1–136; `Tests/SceneLoaderTests.cpp`, lines 1–774. lightweight test harness、RAII temporary file、契約テストに使用。
[^9]: `shader/PixelShader/MeshPS.hlsl`, lines 1–67; `shader/Constants/FrameConstants.hlsli`, lines 1–7. HLSL 命名、書式、resource binding、constant buffer に使用。
[^10]: `.gitattributes`, line 4. Git の text normalization に使用。

