# C++ Build Tools Upgrade - Execution Plan

## 概要
ビルドツールのアップグレード後に発生した32個の警告をすべて修正します。エラーは0個で、ビルドは成功していますが、警告をゼロにしてコード品質を向上させます。

## 修正戦略

修正は以下の優先順で実行します：
1. **制御フロー問題（C4715）** - バグの可能性があり最優先
2. **型変換問題（C4244/C4838）** - データ型の一貫性
3. **Signed/Unsigned比較（C4018）** - 型の統一
4. **エンコーディング（C4819）** - ファイル形式の修正
5. **リンカー警告（LNK4099）** - プロジェクト設定で抑制

---

## タスク詳細

### Task 1: 制御フロー問題の修正（C4715）

**ファイル**: `C:\Users\kamka\Desktop\101Engine\101Engine\src\Engine\Physics\CollisionData.cpp`

**問題**: `MakeLayerMask()` 関数が `COLLISION_LAYER::MAX_LAYER` ケースで `break` を実行した後、関数の最後に到達し値を返さないパスが存在します。

**修正内容**:
```cpp
case COLLISION_LAYER::MAX_LAYER:
    return 0;
    break;  // ← このbreakは不要（returnの後なので）
```

既に `return 0;` が存在するため、警告は誤検出の可能性がありますが、コンパイラが全ケースをカバーしていないと判断しています。`default` ケースが存在するため、実際には問題ありませんが、明示的に確認します。

**検証**: ビルド後、C4715警告が消えることを確認

---

### Task 2: 型変換問題の修正（C4244/C4838）

#### 2.1 GpuTexture.h - width/heightの型修正

**ファイル**: `C:\Users\kamka\Desktop\101Engine\101Engine\src\Engine\Resource\GpuTexture.h`

**問題**: 
- `UINT` 型のメンバー変数を `float` リテラル（0.0f）で初期化
- `UINT` 型を `float` 型で返すゲッター関数

**修正内容**:

**1. ParamDesc構造体（行115-116）**:
```cpp
// 修正前:
UINT width = 0.0f;   // ← floatリテラルでUINTを初期化（警告）
UINT height = 0.0f;

// 修正後:
UINT width = 0;      // ← 整数リテラルを使用
UINT height = 0;
```

**2. メンバー変数宣言（行160-161）**:
```cpp
// 修正前:
UINT m_width = 0.0f;   // ← floatリテラルでUINTを初期化（警告）
UINT m_height = 0.0f;

// 修正後:
UINT m_width = 0;      // ← 整数リテラルを使用
UINT m_height = 0;
```

**3. ゲッター関数（行142-143）**:

このゲッター関数は `float` を返していますが、内部では `UINT` を保持しています。設計意図を確認する必要があります。

**オプションA（推奨）**: ゲッター関数を `UINT` 型で返す
```cpp
// 修正前:
float GetWidth() const { return m_width; }   // UINT → floatの暗黙変換
float GetHeight() const { return m_height; }

// 修正後:
UINT GetWidth() const { return m_width; }    // 型を統一
UINT GetHeight() const { return m_height; }
```

**オプションB**: メンバー変数を `float` 型に変更（より大きな変更）
```cpp
float m_width = 0.0f;
float m_height = 0.0f;
```

**推奨**: オプションAを選択します。テクスチャのサイズは常に整数ピクセルであり、`UINT` が適切です。

#### 2.2 ShaderLibrary.h - ハッシュ関数の修正

**ファイル**: `C:\Users\kamka\Desktop\101Engine\101Engine\src\Engine\Graphics\ShaderLibrary.h`

**問題**: `std::get<4>(k)` は `uint64_t` 型を返しますが、`std::hash<uint32_t>` に渡されています。

**修正内容**:
```cpp
// 修正前（行33）:
size_t h5 = std::hash<uint32_t>{}(std::get<4>(k));  // uint64_t → uint32_tへの暗黙変換

// 修正後:
size_t h5 = std::hash<uint64_t>{}(std::get<4>(k));  // 型を統一
```

コメントには "common defines" とありますが、実際の型は `uint64_t` です。`std::hash` のテンプレート引数を合わせます。

**検証**: ビルド後、C4244/C4838警告が消えることを確認

---

### Task 3: Signed/Unsigned比較の修正（C4018）

#### 3.1 Engine.cpp

**ファイル**: `C:\Users\kamka\Desktop\101Engine\101Engine\src\Engine\Engine.cpp`

**問題**: `int idx` と `swcDesc.BufferCount`（`UINT` 型）の比較

**修正内容**:
```cpp
// 修正前（行426）:
for (int idx = 0; idx < swcDesc.BufferCount; idx++)

// 修正後:
for (UINT idx = 0; idx < swcDesc.BufferCount; idx++)
```

#### 3.2 RenderData.cpp

**ファイル**: `C:\Users\kamka\Desktop\101Engine\101Engine\src\Engine\Graphics\RenderData.cpp`

複数箇所で `uint32_t` のループ変数と `int` 型の変数を比較しています。

**問題箇所と修正**:

**1. 行371, 373** - MakeCapsuleModel関数:
```cpp
// 修正前:
for (uint32_t stack = 0; stack < cylinderStacks; stack++)  // cylinderStacksはint型
    for (uint32_t slice = 0; slice < slices; slice++)      // slicesはint型

// 修正後: ループ変数をintに変更
for (int stack = 0; stack < cylinderStacks; stack++)
    for (int slice = 0; slice < slices; slice++)
```

**2. 行391, 393** - halfSphereTopのインデックス生成:
```cpp
// 修正前:
for (uint32_t stack = 0; stack < ringCountTop - 1; stack++)  // ringCountTopはint型
    for (uint32_t slice = 0; slice < slices; slice++)        // slicesはint型

// 修正後:
for (int stack = 0; stack < ringCountTop - 1; stack++)
    for (int slice = 0; slice < slices; slice++)
```

**3. 行410, 412** - halfSphereBottomのインデックス生成:
```cpp
// 修正前:
for (uint32_t stack = 0; stack < ringCountBottom - 1; stack++)  // ringCountBottomはint型
    for (uint32_t slice = 0; slice < slices; slice++)           // slicesはint型

// 修正後:
for (int stack = 0; stack < ringCountBottom - 1; stack++)
    for (int slice = 0; slice < slices; slice++)
```

**4. 行496, 498** - MakeCylinderModel関数:
```cpp
// 修正前:
for (uint32_t stack = 0; stack < cylinderStacks; stack++)  // cylinderStacksはint型
    for (uint32_t slice = 0; slice < slices; slice++)      // slicesはint型

// 修正後:
for (int stack = 0; stack < cylinderStacks; stack++)
    for (int slice = 0; slice < slices; slice++)
```

**注意**: インデックス計算で `uint32_t` にキャストしている箇所は、ループ変数を `int` に変更しても安全です（値は常に正）。

**検証**: ビルド後、C4018警告が消えることを確認

---

### Task 4: エンコーディング問題の修正（C4819）

**ファイル**: `C:\Users\kamka\Desktop\101Engine\101Engine\src\Engine\Physics\CollisionManager.cpp`

**問題**: ファイルに日本語コメント（例: `//分離軸あり`）が含まれていますが、現在のコードページ（932 = Shift-JIS）で保存されているため、警告が出ています。

**修正内容**:
- ファイルを **UTF-8 with BOM** 形式で保存し直す
- Visual Studioでファイルを開き、「ファイル」→「保存オプションの詳細設定」→「Unicode (UTF-8 シグネチャ付き) - コードページ 65001」を選択

**代替方法**: PowerShellスクリプトで自動変換
```powershell
$filePath = "C:\Users\kamka\Desktop\101Engine\101Engine\src\Engine\Physics\CollisionManager.cpp"
$content = Get-Content $filePath -Raw -Encoding Default
[System.IO.File]::WriteAllText($filePath, $content, [System.Text.UTF8Encoding]::new($true))
```

**検証**: ビルド後、C4819警告が消えることを確認

---

### Task 5: リンカー警告の抑制（LNK4099）

**問題**: 外部ライブラリ `DirectXTex_Debug.lib` のPDBファイルが見つからない警告（10個）

**修正内容**:
プロジェクト設定でこの特定の警告を抑制します。これは外部ライブラリの問題であり、修正できません。

**手順**:
1. プロジェクトをアンロード
2. `101Engine.vcxproj` を編集
3. `<Link>` セクションに警告抑制フラグを追加:
```xml
<Link>
  ...
  <AdditionalOptions>/ignore:4099 %(AdditionalOptions)</AdditionalOptions>
  ...
</Link>
```

または、`<PropertyGroup>` に追加:
```xml
<PropertyGroup>
  <LinkErrorReporting>NoErrorReport</LinkErrorReporting>
  <LinkWarningLevel>Level1</LinkWarningLevel>
</PropertyGroup>
```

**最も簡単な方法**: `DisableSpecificWarnings` プロパティを使用
```xml
<Link>
  <AdditionalOptions>/ignore:4099 %(AdditionalOptions)</AdditionalOptions>
</Link>
```

**検証**: リビルド後、LNK4099警告が消えることを確認

---

## 実行順序

1. ✅ **Task 1**: CollisionData.cpp - 制御フロー問題（最優先）
2. ✅ **Task 2**: 型変換問題
   - 2.1: GpuTexture.h - width/height
   - 2.2: ShaderLibrary.h - ハッシュ関数
3. ✅ **Task 3**: Signed/Unsigned比較
   - 3.1: Engine.cpp
   - 3.2: RenderData.cpp
4. ✅ **Task 4**: CollisionManager.cpp - エンコーディング
5. ✅ **Task 5**: プロジェクト設定 - リンカー警告抑制

## 検証計画

各タスク後にインクリメンタルビルドを実行し、警告が減少することを確認します。

**最終検証**:
- 完全リビルドを実行
- すべての警告（32個）が解消されていることを確認
- エラーが0個のまま維持されていることを確認
- 新しい警告やエラーが導入されていないことを確認

## 成功基準

- ✅ ビルドエラー: 0個（現状維持）
- ✅ ビルド警告: 0個（32個 → 0個）
- ✅ ビルド成功
- ✅ 既存の機能が正常に動作

---

## リスク評価

| リスク | 影響度 | 対策 |
|--------|--------|------|
| GpuTexture.hのAPI変更 | 中 | 呼び出し側で `float` を期待している場合、コンパイルエラーが発生。その場合は型キャストを追加 |
| RenderData.cppの型変更 | 低 | インデックス計算は問題なし（値は常に正） |
| エンコーディング変更 | 低 | UTF-8は後方互換性あり |
| リンカー警告抑制 | 極低 | 外部ライブラリのみに影響、実行時には無関係 |

---

## 推定時間

- Task 1: 5分
- Task 2: 10分
- Task 3: 15分
- Task 4: 5分
- Task 5: 10分
- 検証: 10分

**合計**: 約55分
