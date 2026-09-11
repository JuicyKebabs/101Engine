# Reflection Compression & Simplification — 実装レビュー

実装は完了候補です。採用・Issue の完了判断はユーザーが行います。

## 構造

`PropertyValue` の有限な 13 種類の値表現を維持しました。`PropertyMetadata` はパス、型から導出したアクセス処理、任意の Serialization / Inspector facet を持ち、`TypeMetadata` はプロパティ集合と型全体の検証を持ちます。

通常の登録は `Property(name, member)` または `Property(name, getter, setter)` です。C++ の型から値変換、分類、AssetType、enum 宣言を導出します。両 facet は既定で有効です。例外的な設定だけを `.Optional()`、`.Validate(...)`、`.Inspector(...)`、`.SerializedAs(...)` などで指定します。

失敗を返す読み取りには `Accessor<Value>(name, read, write)` を使用します。これは同じ PropertyMetadata を構築する低水準の入口で、別の型・policy 表現を保持しません。現在の組み込みメタデータでは、Canvas に Actor が存在しない場合を検出する `canvasActorId` の読み取りが該当します。

Camera の公開 aggregate は `Property(name, getter, setter, member)` で一つのフィールドを選択できます。コピーした aggregate を変更して Component setter に渡します。任意の struct を再帰的に反射する機能ではありません。

状態変更は Component の authoring API が担当します。Reflection は dirty flag、pending ID、キャッシュ、世代番号を操作しません。Inspector と Undo/Redo の既存の編集経路・安定した識別子は維持しています。

## 定量比較

基準 commit: `58515a528971c67034c10ddf1df5ff501f6768ad`。開始時の作業ツリーは clean でした。
行数はコメント・空行を含む物理行数です。追加ヘッダーも集計し、Component 側への移動による見かけの削減を別途確認しました。

| 指標 | Before | After |
|---|---:|---:|
| Reflection / Metadata / 連携コードの対象ファイル数 | 19 | 22 |
| 上記の production LOC | 4,229 | 3,415 |
| Core/Reflection 配下 | 2,447 | 2,262 |
| 組み込み Metadata（enum 宣言を含む） | 1,015 | 348 |
| ComponentReflection + ReflectionInspector | 767 | 768 |
| 新規の共通 Metadata / 数学検証ヘルパー | 0 | 37 |
| Metadata 専用 friend | 10 | 0 |
| 共通処理の重複定義（余分な定義数） | 11 | 0 |
| 組み込み metadata 宣言数 | 9 | 9 |
| factory / metadata の登録式 | 18（独立した 2 リスト） | 9（1 リスト） |
| 通常の getter/setter 登録 | 1 式・5 要素 | 1 式・3 要素 |
| 新しい既存対応型プロパティの変更ファイル数 | 2–3 | 2–3 |
| Inspector 表示方式の分類・接続箇所 | 5 | 2 |
| PropertyValue の alternative 数 | 13 | 13 |

対象 LOC は 814 行、約 19.2% 減少しました。組み込み Metadata は共通 Metadata ヘルパー 17 行を加えても 365 行で、従来の 1,015 行から減少しています。

変更した production ファイル全体（Component API、registry、GameCode を含む）では **646 行追加・1,344 行削除、正味 698 行削減**です。テストとこのレポートはこの集計から除外しています。登録処理を単に別ファイルへ移しただけではありません。

集計範囲:
- `FrameWork/src/Engine/Core/Reflection/*`
- `FrameWork/src/Engine/Component/Persistent*Metadata.*`
- `ComponentReflection.h/.cpp`、`ReflectionInspector.h/.cpp`
- 新規 `PersistentMetadataHelpers.h`、`Core/Math/ValueValidation.h`

旧共通処理は name 登録 4、Finish 3、Vector2 finite 2、Vector3 finite 3、回転検証 3、正規化座標検証 2 の計 17 定義でした。6 処理に対する余分な 11 定義を解消しています。

「登録要素」は、従来の名前・logical type・policy・read callback・write callbackに対し、現在は名前・getter・setterです。direct member なら名前と member の 2 要素です。ファイル数は元々 core 変更を必要としなかったため減っていません。既存 getter/setter を新たに公開登録するだけなら、Before/After とも metadata の 1 ファイルだけです。

Inspector の比較は、widget 実装と property 宣言を除いた分類・接続箇所です。従来の Color は logical enum、互換型判定、保存分岐、復元分岐、Inspector の分類に結合していました。現在、新しい表示方式は presentation enum と Inspector の選択処理に限定できます。既にある Color を適用するだけなら metadata の 1 行だけで、core の変更は不要です。

## 登録例

従来の position 登録:

```cpp
builder.AddAccessorProperty<Vector3>(
    "position", PropertyLogicalType::Vector3, kPolicy,
    [](const Transform& c, Vector3& value) {
        value = c.GetLocalPosition(); return true;
    },
    [](Transform& c, const Vector3& value) {
        if (!IsFinite(value)) return false;
        c.SetLocalPosition(value); return true;
    });
```

現在:

```cpp
builder.Property("position", &Transform::GetLocalPosition, &Transform::SetLocalPosition)
    .Validate(ValueValidation::Finite3);
builder.Property("rotation", &Transform::GetLocalRotationQuat, &Transform::SetAuthoredLocalRotation);
```

特殊設定の例:

```cpp
builder.Property("color", &T::GetColor, &T::SetColor)
    .Inspector(InspectorMetadata{.presentation = InspectorPresentation::Color});
builder.Property("scaleMode", &T::GetScaleMode, &T::SetScaleMode)
    .SerializedAs(EnumSerializationFormat::Integer).Optional();
```

Transform pilot の登録本体は 34 行から 8 行へ短縮し、ビルド・serialization・Inspector・Undo/Redo テストを通してから他の Component を移行しました。

## Component 所有の API

| Component | 追加・公開した API | 所有する処理 |
|---|---|---|
| Transform | SetAuthoredLocalRotation | 回転の検証・正規化と既存 setter による dirty 更新 |
| Camera | Get/SetTargetActorReference、Get/SetFollowActorReference | 参照更新、dirty 更新、追従世代のリセット |
| Camera | SetAuthoredRigRotation、SetAuthoredPoseRotation | 回転の検証・正規化、dirty 更新 |
| Collider | GetLocalCenter、GetLocalRotation、GetLocalScale | 公開 authoring 状態の読み取り |
| Collider | SetAuthoredLocalRotation、SetAuthoredType | 正規化、衝突情報・検出・削除・active・世代・dirty 状態の更新 |
| MeshRenderer | Get/SetMeshAssetReference | template 破棄、pending 参照と dirty 更新 |
| SpriteRenderer | Get/SetTextureAssetReference | template 再初期化、billboard 保持、pending 参照と dirty 更新 |
| UIImage | Get/SetTextureAssetReference | template 破棄、pending 参照と dirty 更新 |
| UIRenderer | Get/SetCanvasActorReference | Canvas 接続と pending Actor ID の更新、読み取り失敗の通知 |
| Canvas | 既存 SetAuthoredRenderMode、SetScaleMode、SetReferenceSize、SetMatchWidthOrHeight を公開 | 既存の renderer proxy 無効化を伴う authoring 操作 |

既存の runtime quaternion setter や Collider::SetType の意味は変えず、authoring 操作を分けました。Canvas の継承モード適用・復元は private のままです。SceneBase が階層解決を担当する境界も維持しています。

Metadata 専用 friend の残存例外はありません。EnumMetadata / TypeMetadata の builder 用 friend や SceneBase の権限は、Component metadata の private アクセスとは別の責務です。

## 削除と抽象化の対応

| 追加・整理したもの | 置き換えたコード |
|---|---|
| Property / PropertyConfiguration | 全組み込みの通常 read/write lambda、type/policy 指定、旧 AddMember / AddEnumMember / AddAccessorProperty API |
| 公開 aggregate の単一フィールド指定 | Camera の rig / pose / lens に反復していた読み取り・書き戻し処理 |
| EnumReflection | 9 enum の property 側での entries 構築と受け渡し |
| PersistentMetadataHelpers / ValueValidation | 重複した name、Finish、finite、座標・回転検証 |
| Serialization / Inspector facet | PropertyPolicy、重複 requirement / enum-format フィールド、policy 変換処理 |
| InspectorPresentation::Color | Color logical type、互換型判定、Color の保存・復元分岐 |
| RegisterReflected と組み込み登録式 | metadata 後付け API と独立した登録リスト、手書きの型名重複 |

`PropertyLogicalType` と `PropertyValue` は廃止していません。分類タグと有限 variant は異なる役割があり、C++ 型から導出して接続します。C++ value type の type_index も enum 宣言との一致確認に保持しています。表現を「完全に一つにした」という変更ではありません。

## 拡張コスト

A. 既存 Component に通常の float を追加する場合、Component の宣言・必要な実装と、その Component の metadata 宣言だけを変更します。例えば Transform なら `Transform.h`、必要に応じて `Transform.cpp`、`PersistentTransformMetadata.cpp` です。新しい Reflection core 分岐は不要です。

B. 新しい Vector3 も同じです。Component に意味のある getter/setter を用意し、metadata に `Property("newProperty", &T::GetNewProperty, &T::SetNewProperty)` を 1 件追加します。finite 検証が必要なら既存の `ValueValidation::Finite3` を指定します。core / serializer / Inspector dispatcher は変更しません。

C. 既存 Vector4 を Color 表示にする場合、metadata に `.Inspector(InspectorMetadata{.presentation = InspectorPresentation::Color})` を加えるだけです。PropertyValue、型変換、JSON の実装は変更しません。

D. 既存対応型だけを使う新しい組み込み Component には、Component ファイル、metadata 関数と `PersistentComponentMetadata.h` の宣言、必要な include、`EngineComponentRegistration.cpp` の `REGISTER_BUILTIN_COMPONENT(T)` 1 件を追加します。既存の集約 include は `EngineComponentrRegistration.h` です。新しい .cpp を作った場合は CMake の再構成が必要ですが、Reflection core の変更は不要です。

## 互換性と意図した例外

JSON の property 名、入れ子、enum の整数表現、Optional 条件、ActorReference / AssetReference の意味を維持しました。UI の legacy `sortOrderInCanvas` は Scene 互換性のため保持し、従来どおり UI の `order` を優先して二重適用しません。これは保存データの互換性処理で、旧 builder/policy API の互換層ではありません。

Ticket 指定の例外として、RectTransform の reflected position はゼロではなく入力値を保持します。既存の「ゼロにする」テストを変更し、直接の Reflection write と JSON 復元に回帰テストを追加しました。RectTransform の表示レイアウトは引き続き anchor / anchoredPosition / pivot から計算され、保存された XYZ を表示位置として使用する変更はしていません。

## 検証

2026-09-12、既存の Windows CMake build tree、Debug 構成で実行しました。

```text
cmake --build build --config Debug --parallel 4
ctest --test-dir build -C Debug --verbose
```

実際に使用した CMake はインストール済み Visual Studio の `CommonExtensions/Microsoft/CMake/CMake/bin` 配下です。初回の SDK 読み取りは sandbox で拒否され、承認された通常権限のビルドで検証しました。

| 段階 | 結果 |
|---|---|
| 改修前 full build / CTest | 成功 / 31 of 31 |
| Transform pilot full build / CTest | 成功 / 31 of 31 |
| RectTransform・参照移行 | 成功 / 31 of 31 |
| 全 Component・登録統合 | 成功 / 31 of 31 |
| policy 削除・Color 分離 | 成功 / 31 of 31 |
| 最終 full build / verbose CTest | 成功 / 31 of 31、1.70 秒 |

Framework、Editor、Game、GameCode、全 test targets をビルドしています。Reflection、Component serialization、Inspector、Component commands / Undo、ActorReference、AssetReference、Scene loader / serialization 関連の既存テストを含みます。

追加確認:
- 推論した direct member / getter-setter、bool setter の失敗伝播、property validator、null accessor の拒否。
- Transform の実際の Inspector preview/cancel と安定した identity を使う execute/undo/redo。
- RectTransform の supplied position 保持。
- Color / Vector4 の同一 variant・JSON と異なる Inspector 表示、編集取消。
- factory 公開前の metadata 名不一致拒否。
- GameCode registry の metadata 登録・解除・再登録を 2 サイクル確認。

GameCode.dll 自体のビルドと registry のライフサイクルを確認しました。Editor を操作して DLL を実際に再ロードする UI 試験は行っていません。登録・解除経路は維持しています。

手動コードレビュー対象は Transform、RectTransform、Camera、Collider、MeshRenderer、SpriteRenderer と UI 系です。metadata 内の private フィールド操作および metadata 用 friend がないこと、各 Component 操作が従来の side effect を所有することを確認しました。`git diff --check` は通過しています。

ログは `build/reflection-final-build.log`、`build/reflection-final-tests.log` に保存しています。既存の MSB8064（GameCode directory dependency）、C4819（Renderer.cpp）、LNK4099（DirectXTex PDB）の警告は別件として残しています。

## 保留事項

デシリアライズ transaction は変更しませんでした。`DeserializeReflectedComponent` は新しい candidate に読み込み、`CopySerializableState` で全 serializable 値を destination にコピーします。`ReflectionDeserializer` は欠落した Optional field を書き込み対象から除外します。そのため candidate を単純に除去すると、欠落 field が「Component 既定値になる」動作から「destination の既存値を保持する」動作へ変わります。

既存テストは schema エラー時の無書き込み、callback 失敗と rollback、Optional、Camera の型不変条件を検証しますが、二段階を削除する設計には default 値・失敗処理の追加検証が必要です。小さな修正で同じ意味を保証できるとは判断せず、Ticket が許可する保留を選択しました。Editor の transaction 移動も、既存の property editing を維持し、新しい一般 transaction framework は作っていません。

## 最終レビュー質問への回答

1. **最小の中核概念は何か。** 有限な property value、identity とアクセス処理を持つ property metadata、consumer 別の二つの facet、property 集合と型検証を持つ type metadata です。enum は有限値集合の宣言を持ちます。

2. **通常の登録で何を渡すか。** 名前と member、または名前と getter/setter です。member 型または getter の戻り値から C++ 値型を導出でき、setter の呼び出し結果から bool/void を処理できます。特殊な domain 検証・表示・Optional だけを追加します。

3. **新しい Vector3 では何が変わるか。** Component の状態・semantic API と対応する metadata の 1 宣言です。具体例とファイルは「拡張コスト B」のとおりです。

4. **Component の変更動作はどこにあるか。** Component の public authoring API です。dirty、cache、pending、generation の更新はそこにあります。

5. **metadata が private な実装状態を操作する箇所はあるか。** ありません。legacy UI order の互換処理は公開 setter を使う適用条件であり、内部状態遷移の再実装ではありません。

6. **値型の独立表現はいくつか。** 実装上は分類タグ、variant 値、C++ 型 ID の三つの情報があります。分類・変換・型 ID は C++ 型から導出し、通常の登録で独立に手動指定しません。variant は 13 alternative のまま、Color 専用分類と互換性フラグは削除しました。

7. **各抽象化が何を削除したか。** 「削除と抽象化の対応」の表に示しています。新しい helper を含めても production は正味 698 行減っています。

8. **意図的に解かなかった複雑さは何か。** 二段階の deserialization、既存 Editor transaction lifecycle、Scene の legacy order 表現、ビルド警告です。意味の変更や Ticket 外の作業を避けるためです。

9. **metadata 宣言から reflected interface を読めるか。** 名前、getter/setter、validator、enum format、Optional、Inspector 設定が登録箇所に直接現れます。例外は小さな共通 renderer 部分と fallible Canvas reference で、いずれも宣言から追えます。

10. **人間のレビュー対象は減ったか、移っただけか。** 減りました。metadata と core の重複コードを削除し、Component 側に加えた API・共通 helper も含めて正味 698 行削減しています。通常 property ごとの手書き accessor が不要になり、position を検証したのにゼロを書き込むような転記ミスの余地を減らしています。
