# FbxParts整理・テクスチャ共有化（2026-10-28）

指定フォルダーへ直接反映。TestScene.cppなど利用者側の設定は変更していません。

FbxParts::InitはLoadMaterials、BuildVertices、CreateBuffersを順に呼びます。
評価時はUpdateVerticesでCPU頂点を更新し、UploadVerticesでGPUへ転送します。
TexturePathsは相対パス・解決済みパス・同一フォルダーのファイル名の順で候補を作成し、LoadMaterialTextureが読み込みを試みます。従来のフォールバック順序と座標変換は維持しました。

TextureManager.h/.cppを追加しました。Load(path)はstd::shared_ptr<Texture>を返します。
絶対パス化・区切り文字と大文字小文字の正規化により、同じデバイス上の同じ画像を共有します。
材質が共有所有し、管理表はweak_ptrなので、最後の材質が解放されるとTextureも解放されます。
期限切れの管理項目は次回Load時に除去します。失敗した読み込みはキャッシュしません。
画像の変更検知・自動再読み込みや別スレッドからの同時利用には対応していません。
既存Texture::LoadやSprite等のAPIは維持し、今回はFBX内のテクスチャ読み込みを共有管理へ切り替えています。

VSプロジェクトとフィルター、Tests/build-and-run.cmdにも新ファイルを登録しました。
既存Model API、ユーザーのシーン設定・Assetsは維持しています。

検証結果:
- WARPおよび実GPU: 既存描画・アニメーション・座標系・レイ判定検査がPASS。
- 新規: 相対パス・大文字小文字違いの共有、最終所有者による解放、再読み込み、失敗パスを検査。
- skg.fbxの読み込み直後: プロセスPrivateUsage 1927.46MiB → 122.67MiB。
- 600回評価後: 1927.92MiB → 123.57MiB。単独計測プログラム・Debug x64・実GPUで計測。
- この計測はデバイス生成、モデル読み込み、アニメーション評価と頂点転送まで。ゲーム全体のメモリ使用量の保証ではありません。
- skg.fbxの実GPU追加描画では3/3フレームに画素、75/75レイ判定を確認。
  ただしボーン位置検査は51件失敗: 同名のメッシュとボーンがあり、既存GetBonePositionは最初の同名ノードを返すためです。
  Fbx.cppの照会処理は今回変更していません。この曖昧さは別途修正が必要です。
- ゲーム画面での目視確認は未実施です。

Tests/Results/texture-* に今回のログを収録しています。

Debug/Release × x64/x86 の全4構成でビルド成功（MSVC v145）。

## std::filesystemによるパス処理の整理
Fbx.cppのモデル親フォルダー取得をpath::parent_pathへ変更しました。
FbxParts.cppはfilesystem::pathで候補を作り、/で結合し、filename()で名前を取得します。
is_regular_file(path, error_code)で読み込み候補を判定します。
TextureManagerはabsolute(path, error_code).lexically_normal()を共有キーの元に使います。
Windowsの大文字小文字を同一視する処理は別途維持しました。
既存Texture::LoadのWindowsマルチバイト文字列APIは維持しています。
VSの全4構成とテストのコンパイル設定をC++17へ変更しました。
WARPおよび実GPUで既存回帰テストがPASSしています。

## 初心者向けの書き方への整理
対象はEngine/Fbx.cpp、FbxParts.cpp/.h、TextureManager.cppとテストのラムダ部分です。
ラムダは通常関数へ置き換えました。三項演算子、複数処理の1行記述、波括弧のないifを展開し、条件の意味と変数の用途が分かる名前にしました。
アニメーション選択はHasChangingKeys、HasAnimation、SelectAnimationへ分けています。
レイ判定は頂点取得、交差判定、距離比較、法線計算を順に記述しています。
テクスチャ管理は、名前付き構造体の一覧を通常のループで検索する形です。検索時間は登録数に比例します。共有所有・最後の利用者で解放・デバイス別管理という動作は維持しています。
日本語コメントを含むファイルはUTF-8 BOM付きで保存しています。
既存のModel API、ユーザーのTestScene.cpp、Assets、ufbx本体は変更していません。
従来のボーン名重複時の曖昧さは今回の対象外です。ゲーム画面の目視確認は未実施です。

整理後の検証: Debug/Release × x64/x86の全4構成ビルド成功。WARP・実GPUテストともPASS。skg.fbxは読み込み直後122.72MiB、600回評価後123.37MiB。
