# GameBaseDx11 — ufbx移行検証版

Autodesk FBX SDKをufbx 0.23.1へ置換し、Windows上で修正・ビルド・モデル処理を検証した版です。
既存のModel APIは維持しています。詳細と未検証項目は `UFBX_MIGRATION_NOTES.md` を参照してください。

## ビルドと実行

1. Visual Studio 2026で `GameBaseDx11.sln` を開きます。
2. 「C++によるデスクトップ開発」、MSVC v145、Windows SDKが必要です。
3. Debug/Release、x64/x86を選択してビルドできます。
4. Visual Studioから実行する際の作業ディレクトリはプロジェクトフォルダです。
   エンジンはそこから `Assets` へ移動してシェーダーや画像を読み込みます。

Visual Studio 2022を使う場合は、プロジェクトをv143へリターゲットしてください。この環境ではv143は未検証です。
Autodesk FBX SDK、libxml2、zlibは不要です。

`RunGame.cmd` は同梱のRelease x64実行ファイルをプロジェクトフォルダから起動します。
元のTestSceneは空です。モデルをゲーム画面へ出すには既存の `Model::Load` / `Draw` 等を利用してください。
Debug x64の本体はVisual Studioから起動し、約59 FPSでの更新と終了コード0を確認済みです。モデル描画は別のテストプログラムで検証しています。

## 検証

`Tests/build-and-run.cmd` でモデル・アニメーション・スキニング・テクスチャ・レイ判定のテストを再実行できます。
テストの説明、素材の出典、ログは `Tests` に同梱しています。
ZIPにはソース、元のAssets、テスト、Release x64実行ファイルを収録し、.vsやビルド中間ファイルは除いています。



左右反転の修正版（v2）です。FBXを左手系Y-upへ変換し、Z軸変換・法線・ボーン・アニメーションを整合させています。U値は反転していません。


最新版はv3です。Lambert材質を誤って鏡面反射ありとした不具合による、実GPUでの球体の欠損を修正しました。エンジンの左手系は維持しています。詳細は移行メモのv3節を参照してください。

