# アニメーション再生修正版（2026-10-28）

## 原因
Assets/Run.fbx は60 FPSで、Take 001 (0～0.5秒) と mixamo.com (0～0.633333秒) の2スタックを持ちます。旧Fbx::Evaluateは常に先頭を選んでいました。先頭テイクで時刻を変更してもスキン頂点は変化せず、走行アニメーションは2番目にあります。Walking.fbxは1スタックで、旧評価でも変形します。今回のRun.fbxの停止原因はテイク選択です。

## 修正と使い方
既存APIを維持し、Model::SetAnimation(int handle, const std::string& name) を追加しました。

```cpp
int h = Model::Load("Run.fbx");
if (h >= 0) {
    bool selected = Model::SetAnimation(h, "mixamo.com");
    Model::SetAnimFrame(h, 0, 38, 1.0f);
}
// 毎描画時:
Model::Draw(h);
```

名前は大文字小文字を含め完全一致です。成功時true、無効ハンドル・存在しない名前はfalseで前の選択を保ちます。空文字は自動選択へ戻します。フレーム・速度は変更しません。切り替え後にSetAnimFrameを呼べば先頭へ戻せます。選択は各Modelハンドルに保持し、共有Fbxの描画・ボーン照会・レイ判定の前に適用します。Fbx自体のSetAnimationも使用できます。同名のスタックが複数あれば最初を選びます。

未指定時は、時間変化するカーブを持つ最初のスタックを選びます（値の差および3次補間の接線を検査）。すべて静止なら先頭、スタックがなければscene->animへフォールバックします。これは静止テイクを避ける初期値であり、複数の動くクリップのうち希望するものは名前で指定してください。

## コード追跡
SetAnimFrameはハンドルの現在フレーム・範囲・速度を設定します。Model::Drawがフレームを進め、選択を適用してFbx::Drawへ渡します。フレームは元API同様に整数化され、FBXの絶対タイムライン上のframe / FPS秒として評価します。開始時刻の加算は行いません。
Fbx::Evaluateは選択アニメーションとevaluate_skinning=trueでufbx_evaluate_sceneを呼び、ufbxが親子階層とスキン変形を評価します。テイク変更時はフレームキャッシュを無効にし、同一フレームでも再評価します。
FbxPartsはtyped_idで評価済みノードを対応させ、三角形化時のコーナー番号からskinned_position / skinned_normalを取得します。skinned_is_localの場合だけgeometry_to_worldと法線行列を適用します。ufbxのワールド座標はエンジンのモデル座標として保持し、DrawでユーザーのTransformを一度適用します。左手系Y-up・Z反転はufbxのロード設定で形状と階層をまとめて変換しています。
更新した頂点をD3D11_MAP_WRITE_DISCARDで頂点バッファへ転送し、DrawIndexedで描画します。既存の座標系修正は維持しました。

## 検証
WindowsのMSVC v145 / Windows SDKでDebug・Release × x64・Win32をビルド。ufbx.cはCとしてコンパイルし、Autodesk SDKへの依存はありません。
Tests/build-and-run.cmdでWARPおよび実GPUのD3D11テストを実行しました。詳細はTests/Results/animation-*.logとanimation-build-all.logに収録。
従来9モデルの検査に加え、実際のRun.fbxとWalking.fbxをModel::Drawで1～16フレーム進め、描画にバインドされたGPU頂点バッファを読み戻しました。最大座標変化はRun=133.03、Walking=0.957862で、有限値を確認しました。
同一フレームでのテイク切替、共有ハンドルの異なるテイク、無効名・無効ハンドル、自動選択への復帰、フレーム維持も検査しています。

## 未検証・制限
ゲーム本体のウィンドウを目視して走行の見た目・カメラ・接地・ループの継ぎ目を確認する検証は行っていません。実GPUテストはオフスクリーン描画・GPUバッファ読み戻しです。全FBX形式、全機種、VS2022/v143の互換性、Autodesk SDKとの一致は未検証です。複数クリップのブレンド、クロスフェード、サブフレーム再生は追加していません。

## 納品内容
指定されたデスクトップのv3ソースへ直接反映しました。変更ファイルはEngine/Fbx.cpp・Fbx.h・Model.cpp・Model.h、TestScene.cpp、Tests/Smoke.cppです。変更前のファイルは作業用フォルダーに退避しています。4構成のビルドと実GPU検査は同一内容の作業コピーで実施し、反映先ではDebug x64ビルドとWARP検査を再実行しました。過去のREADME/移行メモ/Resultsは履歴資料で、今回の結果はこの文書とanimation-*ログを参照してください。

