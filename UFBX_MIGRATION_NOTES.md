# ufbx移行・精査結果

検証日：2026年10月28日（日本時間）

## 対象と結論

元の `w-endo-gamebasedx11-53933e577ee7 (3).zip`、前の会話の添付 `ufbx.c` / `ufbx.h`、
`GameBaseDx11_ufbx_full.zip`、ユーザー配置先のプロジェクトを比較して修正しました。
配置先のエンジンソースは前のZIPと一致し、ツールセットはv145へ変更済みでした。
`Model.h` の公開APIは元のZIPとバイト単位で同一です。元のソース・Assetsに欠落はありません。

## 修正した問題

| 箇所 | 確認した問題と修正 |
|---|---|
| Texture.h | `LibXml2-MT.lib` と `zlib-MT.lib` のリンク指定が残存。実際にLNK1104が発生したため除去 |
| FbxParts | ノード／親階層／ジオメトリ変換が描画へ反映されない問題を修正。ローカルの評価済み頂点には `geometry_to_world` を一度適用し、シーン空間のスキニング済み頂点には再適用しない |
| 法線 | ローカル法線を `ufbx_matrix_for_normals()` で変換して正規化。Modelの変換には親階層も含む逆転置を適用 |
| ボーン | 初期シーンから取得していた位置を、評価済みシーンから取得。描画前の問い合わせにも対応 |
| Model共有 | キャッシュ再利用時のfileName未設定を修正。元のハンドルを解放した後も共有を維持。ハンドル別の現在フレームでボーン／レイを評価 |
| レイ判定 | 描画と同じ変形後の形状を使用。最近傍ヒットを全パーツで維持し、面法線を設定。Modelへの問い合わせで入力start/dirを書き換えない |
| 読み込み失敗 | パーツ初期化の失敗を上位へ返し、途中までロードした資源を解放。空のメッシュはスキップ。モデル全体に三角形がなければ失敗 |
| 評価と解放 | 同一フレームの評価結果を再利用。再ロード／Releaseの繰り返しで二重解放しない |
| テクスチャ探索 | FBXの相対サブフォルダ、ufbxが解決したファイルパス、モデルフォルダ内のbasenameを順に探索。読み込み失敗時はマテリアル色へフォールバック |
| Texture.cpp | WIC／GPU／COM資源と失敗経路を整理。画像をCPUへデコードし、正しい行ピッチでGPUへアップロード。以前のMap先へ幅×4で直接コピーする処理を置換 |
| プロジェクト | ufbx.cをCとして明示。全構成にシェーダーのVSエントリとVertex/5.0を設定。定数バッファ288バイトを静的検査 |

## ビルド検証

Visual Studio 2026 Community 18.5.3、MSVC v145（14.50.35717）、Windows SDK 10.0.26100.0を使用。

| ソリューション構成 | プロジェクト構成 | 結果 |
|---|---|---|
| Debug x64 | Debug x64 | 成功 |
| Release x64 | Release x64 | 成功 |
| Debug x86 | Debug Win32 | 成功 |
| Release x86 | Release Win32 | 成功 |

ソリューション側は `x86`、vcxproj側は `Win32` という既存の対応です。
SDK／libxml2／zlibの追加インストールや外部FBX DLLの配置は不要です。
ここでの「SDK不要」はAutodesk FBX SDKについてであり、ビルドにはWindows SDKが必要です。
既存部分のsigned/unsigned比較やHLSLの暗黙的切り詰め等の警告は残っています。
ログは `Tests/Results` に収録しています。

## 実行検証

`Tests/build-and-run.cmd` により、実際のFbx・FbxParts・Model・Texture・Direct3Dの実装をリンクして検証しました。
Direct3D 11 WARPを使用し、デバッグ層を有効化。専用のレンダーターゲットへ描画して画素を読み戻しています。

- 同梱のBoxCollider／SphereCollider、およびufbx公式テストの6モデル：計8モデルに加え、左右上下を検査する自作の非対称パネル：計9モデル。
- 0、15、30フレーム：27回すべてで描画画素を確認。
- レイ522本：評価済みジオメトリから計算した最近傍距離と一致。面法線も検査。
- ボーン位置24件：評価済みノードの位置と一致。Maxのスキンモデルでは4ボーンの位置変化を確認。
- 異なるフレームの共有Modelハンドルを交互に問い合わせ、姿勢を取り違えないことを確認。
- 相対サブフォルダの拡散テクスチャがGPUへバインドされること、WICロード、欠損ファイル、解放・キャッシュ再利用を確認。
- 最終結果：`RESULT: PASS (0 failures)`。Direct3Dデバッグ層のError／Corruptionは0件。

形状の基準値にも同梱のufbxを使っています。Autodesk SDKの出力との同一性検証ではありません。
ゲーム本体はVisual StudioのF5でDebug x64を起動し、背景の描画と約59 FPSの更新を確認。閉じるボタンで通常終了し、デバッガー出力で終了コード0を確認しました。
元のTestSceneは空で、ゲームプレイ用キャラクターやシーンは含まれていません。
デバッガーにはWindows/Intelドライバー等のfirst-chance例外通知や内部設定ファイルの未検出ログもありますが、処理は継続して終了コード0でした。これらの通知の原因調査は行っていません。

## 互換性と制限

- 公開Model APIと、フレーム番号を渡す使い方を維持。最初のAnimStackを `frame / scene FPS` 秒で評価します。
- フレーム番号はFBXタイムラインの絶対フレームです。AnimStackの開始時刻を自動加算しません。
- DirectXのLHビュー／射影に合わせ、ufbxのtarget_axesを左手系Y-upへ正規化します。右手系からの変換軸はZを明示し、ufbxに頂点・法線・ノード・ボーン・アニメーション・三角形の表裏をまとめて変換させます。単位の変更は行いません。UVのU値は変更しません。
- 単純な拡散・環境・鏡面色、shininess、外部の拡散テクスチャを既存のSimple3Dシェーダーへ渡します。
- 埋め込み画像、レイヤーテクスチャの合成、全PBRシェーダー、アニメーションする材質、外部頂点キャッシュは未対応です。
- テクスチャパスは従来のWindowsマルチバイト文字列に合わせています。全Unicodeパスは未検証です。
- CPU側でシーンを評価し頂点を更新します。大規模モデルや多数インスタンスでの性能測定は未実施です。
- レイのdistは `start + dir * dist` の係数です。dirが単位ベクトルならワールド距離になります。
- WARPでモデル描画を検証し、実GPUで空のゲーム本体の起動・終了を確認しました。ユーザーの全モデルやゲームプレイは未検証です。

## 同梱ufbxの確認

ufbx 0.23.1。ユーザー添付とバイト単位で一致し、本体の改変はありません。

| ファイル | SHA-256 |
|---|---|
| Engine/ufbx.c | 4A956C26A708E40D82ECB0AEAEE54DA709D5BA7C144C9642174970059BDA7E1D |
| Engine/ufbx.h | F787BE529AF577EFC04F3E3E735844A08556718DBB613D1211C2B313D72895DC |

公式ライセンスを `UFBX_LICENSE.txt` に同梱。
参照： https://github.com/ufbx/ufbx/tree/v0.23.1 、同梱ufbx.hのAPIコメント。



## 左右反転の追加修正（同日・v2）

ユーザーの左右反転の指摘を受け、読み込み時の座標系変換の欠落を修正しました。
旧版は右手系のFBXをLHのビュー・射影へそのまま渡していました。
`Fbx.cpp` で以下を設定し、単純なUV反転や描画だけのZ反転は行いません。

```
opts.target_axes = ufbx_axes_left_handed_y_up;
opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
opts.space_conversion = UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS;
```

ufbxの標準のwinding変換を利用し、エンジンの時計回り表面／裏面カリングの設定を維持します。
参考：https://ufbx.github.io/elements/nodes/ 、同梱ufbx.h／ufbx.c。

専用の `orientation_rh.fbx` は右手系Y-up、Z=+2、法線+Zの板です。
テクスチャは左上赤、右上緑、左下青、右下黄の4色。
GPU頂点を読み戻してZ=-2・法線-Z・U値不変を検査し、LHカメラからの描画像の四隅の色とカリングを照合しました。
レイの距離と面法線も、ufbxの計算結果を基準にせず既知の値（距離4、法線-Z）で確認しています。
同じテストを修正前Fbx.cppで実行すると64件失敗し、修正後は9モデル全体で0件でした。
以前の「画素が出る」検査では左右の向きを検出できなかったため、この検査を追加しました。

配置先にユーザーが追加したBall.fbx／Oden.fbxも各3フレームで描画・形状を確認し、147本のレイが一致しました。
Ballを使うユーザーのTestSceneをVisual Studioから起動し、約59 FPSでの描画を確認して通常終了しました。
配置先のユーザー追加Assets／TestSceneは保持しています。
v2 ZIPは元の空のTestSceneと元のAssetsを維持し、修正したエンジン・テスト・ログを収録します。
ユーザー追加のBall／Oden等は再配布用ZIPには含めていません。
同梱Release x64実行ファイルはZIP内のソースから別途ビルドしています。

## v3：実GPUでの球体の描画欠損を修正

「陰影だけで、形状は欠けていない」という前の説明は不適切でした。
WARPの別プログラムでは実GPUと同じ欠損を再現できず、その結果を本体へ適用してしまいました。
今回は配置先のTestScene（Ball、position=(0,0,1)）をVisual Studioから実行し、欠損を再現して修正しました。

### 原因と変更

移植版は全マテリアルのspecularをToColor()で設定し、鏡面反射のないLambertにも
specular.a=1を渡していました。既存シェーダーはこのaを「鏡面反射あり」のフラグとして使います。
Ballではspecular.rgb=0、shininess=0なので、画素によってpow(0,0)へ入る状態でした。
MicrosoftのHLSL仕様ではpow(0,0)はGPUによって0／1／NaNになり得ます。
https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-pow

Lambertとマテリアルなしの既定値をspecular=(0,0,0,0)に修正。
有効な鏡面色と正の指数を持つFBX Phongにだけ鏡面反射を有効化します。
元のAutodesk版もLambertのspecular.aは0で、この条件を移植時に失っていました。
材質判定だけを変更して実GPUで再実行したところ、同じZ=+1で球全体が表示されました。
NaNそのものの浮動小数点バッファ読出しは行っていませんが、実行時パラメーター、GPU依存のpow仕様、
材質フラグだけを変更した比較結果が、この欠損の発生機序と一致しています。

エンジンの左手系を維持しています。この修正でCamera.cpp、Transform.cpp、Direct3D.cpp、
射影行列、クリッピング平面、ユーザーのTestScene／シェーダー／Assetsは変更していません。
記録用に一時挿入したコードは除去済みです。

### 本体と回帰テストでの確認

- 配置先のx64 Debug本体でZ=+1のBallを表示し、球全体が描画されることを確認。
- 実行時のworld／view／projection／シェーダー種別／材質を記録。
  修正前後で行列は同じで、Ballのspecular.aが1から0へ変わっています。
- 実行時ビューから求まる球の中心の奥行きは、Z=0で10.4403、Z=+1で11.3981。
  +Zは奥向きであり、この欠損を右手系化や近クリップ面の移動で補正していません。
- 公開Model APIを通したGPU描画検査を追加。既知の板をZ=0から+1へ動かすと、
  視点からの奥行き4→5、描画幅154→124ピクセル。WARP／実GPUの両方で一致。
- ゲームと同じアルファブレンド状態で実GPUテストを追加。
- Lambertの定数バッファをGPUから読み戻し、鏡面反射フラグ0を検査。
  v2の材質処理では3件失敗し、v3では成功。
- WARPで同梱9モデル、実GPUで同梱9モデル＋ユーザー配置のBall／Odenの計11モデルが成功。
  Direct3Dのデバッグ層を有効化し、いずれもRESULT: PASS (0 failures)。
- Debug/Release × x64/x86の4構成を再ビルドして成功。

ログはTests/Resultsのv3-*およびgame-runtime-trace-before/after.logです。
ZIP内は元の空のTestSceneとAssetsを維持し、Release x64実行ファイルもそのソースから再ビルドしています。
配置先のユーザー追加シーンやモデルは保持し、ZIPには追加していません。
