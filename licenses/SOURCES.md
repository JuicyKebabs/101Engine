# ライセンス原文の取得元

確認日：2026年11月5日。著作権表示・許諾条件・免責事項の原文を保存しています。

取得コミットを固定したものでも、同梱コードとの一致が確認できた範囲はTHIRD_PARTY_NOTICES.mdの記載に従います。

| 保存ファイル | 取得元 | 確認・抽出方法 |
|---|---|---|
| [imgui-LICENSE.txt](imgui-LICENSE.txt) | [third_party/ImGui/LICENSE.txt](../third_party/ImGui/LICENSE.txt) | 同梱ファイルのバイト列をそのままコピー |
| [assimp-LICENSE.txt](assimp-LICENSE.txt) | [https://github.com/assimp/assimp/blob/v6.0.5/LICENSE](https://github.com/assimp/assimp/blob/v6.0.5/LICENSE) | 公式 v6.0.5（392a658）から VS 2026 x64 で Debug・Release をビルド。ヘッダーは同じビルドのインストール出力 |
| [nlohmann-json-LICENSE.txt](nlohmann-json-LICENSE.txt) | [https://github.com/nlohmann/json/blob/v3.12.0/LICENSE.MIT](https://github.com/nlohmann/json/blob/v3.12.0/LICENSE.MIT) | 同梱両ヘッダーがv3.12.0と改行・BOMを除き一致 |
| [directxtex-LICENSE.txt](directxtex-LICENSE.txt) | [https://github.com/microsoft/DirectXTex/blob/c0cb97764379ebd20dbd2016c58965db85f5b307/LICENSE](https://github.com/microsoft/DirectXTex/blob/c0cb97764379ebd20dbd2016c58965db85f5b307/LICENSE) | 同梱コードのMIT・Microsoft著作権表記と照合。ソースのリリース版の一致を示すものではない |
| [directxtk-LICENSE.txt](directxtk-LICENSE.txt) | [https://github.com/microsoft/DirectXTK/blob/861f3e7df437b50d1f28c964769329e9b23a50fb/LICENSE](https://github.com/microsoft/DirectXTK/blob/861f3e7df437b50d1f28c964769329e9b23a50fb/LICENSE) | 同梱keyboard.h/.cppのDirectXTK由来・MITという表記を公式原文と照合。元コミットと中間配布物の由来は未確定 |
| [d3dx12-LICENSE.txt](d3dx12-LICENSE.txt) | [https://github.com/microsoft/DirectX-Headers/blob/faf928dc6feb8b86e9e3d6a39fc5171de7d8ac36/LICENSE](https://github.com/microsoft/DirectX-Headers/blob/faf928dc6feb8b86e9e3d6a39fc5171de7d8ac36/LICENSE) | 同梱コードのMIT・Microsoft著作権表記と照合。ソースのリリース版の一致を示すものではない |
| [imgui-stb/imstb_rectpack.h.LICENSE.txt](imgui-stb/imstb_rectpack.h.LICENSE.txt) | [third_party/ImGui/imstb_rectpack.h](../third_party/ImGui/imstb_rectpack.h) | 同梱ヘッダーに含まれるライセンスコメントを原文で抽出 |
| [imgui-stb/imstb_textedit.h.LICENSE.txt](imgui-stb/imstb_textedit.h.LICENSE.txt) | [third_party/ImGui/imstb_textedit.h](../third_party/ImGui/imstb_textedit.h) | 同梱ヘッダーに含まれるライセンスコメントを原文で抽出 |
| [imgui-stb/imstb_truetype.h.LICENSE.txt](imgui-stb/imstb_truetype.h.LICENSE.txt) | [third_party/ImGui/imstb_truetype.h](../third_party/ImGui/imstb_truetype.h) | 同梱ヘッダーに含まれるライセンスコメントを原文で抽出 |
| [nlohmann-json-THIRD_PARTY.txt](nlohmann-json-THIRD_PARTY.txt) | [third_party/nlohmann/json.hpp](../third_party/nlohmann/json.hpp) / [https://github.com/nlohmann/json/blob/v3.12.0/LICENSE.MIT](https://github.com/nlohmann/json/blob/v3.12.0/LICENSE.MIT) | 同梱amalgamatedヘッダーの全SPDX著作権者を抽出し、対応するMIT原文とともに収録 |
| [assimp-headers-NOTICES.txt](assimp-headers-NOTICES.txt) | [third_party/Assimp/include/](../third_party/Assimp/include/) | 同梱ヘッダーの再配布条件を含むコメントを重複排除して原文収録 |
| [assimp-dependencies/clipper.txt](assimp-dependencies/clipper.txt) | [https://github.com/assimp/assimp/blob/v6.0.5/contrib/clipper/License.txt](https://github.com/assimp/assimp/blob/v6.0.5/contrib/clipper/License.txt) | 対応コミット内の依存ライセンス原文。DLLへの個別組み込みは未確定 |
| [assimp-dependencies/earcut.txt](assimp-dependencies/earcut.txt) | [https://github.com/assimp/assimp/blob/v6.0.5/contrib/earcut-hpp/LICENSE](https://github.com/assimp/assimp/blob/v6.0.5/contrib/earcut-hpp/LICENSE) | 対応コミット内の依存ライセンス原文。DLLへの個別組み込みは未確定 |
| [assimp-dependencies/openddlparser.txt](assimp-dependencies/openddlparser.txt) | [https://github.com/assimp/assimp/blob/v6.0.5/contrib/openddlparser/LICENSE](https://github.com/assimp/assimp/blob/v6.0.5/contrib/openddlparser/LICENSE) | 対応コミット内の依存ライセンス原文。DLLへの個別組み込みは未確定 |
| [assimp-dependencies/poly2tri.txt](assimp-dependencies/poly2tri.txt) | [https://github.com/assimp/assimp/blob/v6.0.5/contrib/poly2tri/LICENSE](https://github.com/assimp/assimp/blob/v6.0.5/contrib/poly2tri/LICENSE) | 対応コミット内の依存ライセンス原文。DLLへの個別組み込みは未確定 |
| [assimp-dependencies/pugixml.txt](assimp-dependencies/pugixml.txt) | [https://github.com/assimp/assimp/blob/v6.0.5/contrib/pugixml/LICENSE.md](https://github.com/assimp/assimp/blob/v6.0.5/contrib/pugixml/LICENSE.md) | 対応コミット内の依存ライセンス原文。DLLへの個別組み込みは未確定 |
| [assimp-dependencies/rapidjson.txt](assimp-dependencies/rapidjson.txt) | [https://github.com/assimp/assimp/blob/v6.0.5/contrib/rapidjson/license.txt](https://github.com/assimp/assimp/blob/v6.0.5/contrib/rapidjson/license.txt) | 対応コミット内の依存ライセンス原文。DLLへの個別組み込みは未確定 |
| [assimp-dependencies/utf8cpp.txt](assimp-dependencies/utf8cpp.txt) | [https://github.com/assimp/assimp/blob/v6.0.5/contrib/utf8cpp/doc/LICENSE](https://github.com/assimp/assimp/blob/v6.0.5/contrib/utf8cpp/doc/LICENSE) | 対応コミット内の依存ライセンス原文。DLLへの個別組み込みは未確定 |
| [assimp-dependencies/zlib.txt](assimp-dependencies/zlib.txt) | [https://github.com/assimp/assimp/blob/v6.0.5/contrib/zlib/LICENSE](https://github.com/assimp/assimp/blob/v6.0.5/contrib/zlib/LICENSE) | 対応コミット内の依存ライセンス原文。DLLへの個別組み込みは未確定 |
| [assimp-dependencies/zip.txt](assimp-dependencies/zip.txt) | [https://github.com/assimp/assimp/blob/v6.0.5/contrib/zip/UNLICENSE](https://github.com/assimp/assimp/blob/v6.0.5/contrib/zip/UNLICENSE) | 対応コミット内の依存ライセンス原文。DLLへの個別組み込みは未確定 |
| [assimp-dependencies/unzip.txt](assimp-dependencies/unzip.txt) | [https://github.com/assimp/assimp/blob/v6.0.5/contrib/unzip/unzip.h](https://github.com/assimp/assimp/blob/v6.0.5/contrib/unzip/unzip.h) | 対応コミットのソース内ライセンスコメントを原文抽出。DLLへの個別組み込みは未確定 |
| [assimp-dependencies/stb-image.txt](assimp-dependencies/stb-image.txt) | [https://github.com/assimp/assimp/blob/v6.0.5/contrib/stb/stb_image.h](https://github.com/assimp/assimp/blob/v6.0.5/contrib/stb/stb_image.h) | 対応コミットのソース内ライセンスコメントを原文抽出。DLLへの個別組み込みは未確定 |
| [assimp-dependencies/open3dgc.txt](assimp-dependencies/open3dgc.txt) | [https://github.com/assimp/assimp/blob/v6.0.5/contrib/Open3DGC/o3dgcCommon.h](https://github.com/assimp/assimp/blob/v6.0.5/contrib/Open3DGC/o3dgcCommon.h) | 対応コミットのソース内ライセンスコメントを原文抽出。DLLへの個別組み込みは未確定 |
| [assimp-dependencies/miniz.txt](assimp-dependencies/miniz.txt) | [https://github.com/assimp/assimp/blob/v6.0.5/contrib/zip/src/miniz.h](https://github.com/assimp/assimp/blob/v6.0.5/contrib/zip/src/miniz.h) | 対応コミットのソース内ライセンスコメントを原文抽出。DLLへの個別組み込みは未確定 |
| [imgui-fonts/proggyclean-LICENSE.txt](imgui-fonts/proggyclean-LICENSE.txt) | [https://github.com/bluescan/proggyfonts/blob/139ec08a38096161291792313ef5803fc4f0e37b/LICENSE](https://github.com/bluescan/proggyfonts/blob/139ec08a38096161291792313ef5803fc4f0e37b/LICENSE) | 同梱imgui_draw.cppのMIT・著作権者表記と公式原文を照合。フォントの元コミットの一致を示すものではない |
| [imgui-fonts/proggyforever-LICENSE.txt](imgui-fonts/proggyforever-LICENSE.txt) | [https://github.com/ocornut/proggyforever/blob/f8868ba43bb42c4d88454d03ad81cbd139789f72/LICENSE.txt](https://github.com/ocornut/proggyforever/blob/f8868ba43bb42c4d88454d03ad81cbd139789f72/LICENSE.txt) | 同梱imgui_draw.cppのMIT・著作権者表記と公式原文を照合。フォントの元コミットの一致を示すものではない |

## ハッシュ一覧

COMPONENTS.sha256は、同梱ライブラリと外部由来のキーボード処理のファイルを識別するための記録です。パスはプロジェクトのルート基準です。ライセンス条件の完全性を証明するものではありません。
