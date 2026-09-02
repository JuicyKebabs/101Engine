# Engineering Task Example

この文書は、Engineering Task用GitHub Issue Templateの記入例です。
実際のIssueでは、各タスクの状況と決定済みの設計に合わせて内容を調整してください。

---

# [Editor] Add WASD movement to SceneView camera

## Context

現在のSceneView Cameraでは視点回転は可能だが、カメラ位置を自由に移動する機能が存在しない。

そのため、広い3D空間でActorを配置、確認する際に、対象地点まで視点を移動する操作が困難になっている。

SceneViewを3Dレベル編集に使用するため、基本的なフライカメラ操作としてWASD移動を追加する。

## Goal

SceneView上で、右クリックによるカメラ操作中にWASDキーを使用してカメラを前後左右へ移動できるようにする。

## Design Constraints

- 入力デバイスの状態取得と操作の解釈は `EditorCameraController` の外側が担当する
- `EditorCameraController` は入力システムを直接参照せず、カメラ操作の計算のみを担当する
- `EditorViewCamera` はEditorの3D Viewが所有するCamera、pivot、navigation stateをまとめる
- カメラ移動速度は `EditorCameraNavigationState` から取得する
- 移動方向は `EditorViewCamera` が所有するCameraの現在の向きを基準とする
- Runtime用Cameraの実装には依存させない

## Implementation Scope

- `EditorCameraController`
- `EditorViewCamera`
- `EditorCameraNavigationState`
- SceneViewにおけるEditor View Cameraの入力および更新処理
- 上記に関連する最小限のコード

## Out of Scope

- マウスドラッグによるカメラ回転
- Shiftキーなどによる高速移動
- カメラ移動速度を変更するEditor UI
- Config値のファイル保存
- Runtime Cameraの変更
- Cameraシステム全体のリファクタリング

## Acceptance Criteria

- [ ] SceneViewでカメラ操作中にWキーを押すと前方へ移動する
- [ ] Sキーを押すと後方へ移動する
- [ ] Aキーを押すと左方向へ移動する
- [ ] Dキーを押すと右方向へ移動する
- [ ] 移動方向がWorld座標固定ではなく、現在のCamera方向を基準に計算される
- [ ] 移動速度として `EditorCameraNavigationState` の設定値が使用される
- [ ] Editor View Camera以外のCameraの挙動に影響しない
- [ ] 既存のSceneView Camera操作が壊れていない
- [ ] プロジェクトが正常にビルドできる
