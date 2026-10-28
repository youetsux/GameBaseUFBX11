# テストの再実行

Visual Studio の C++ デスクトップ開発と Windows SDK が必要です。
`build-and-run.cmd` を実行すると、Debug x64 をビルドし、コンソールテストを実行します。
テストは Direct3D 11 WARP（CPUレンダラー）で描画し、デバッグ層が利用できればエラーを検査します。
成功時は `RESULT: PASS (0 failures)`、終了コード0です。画面操作やネット接続は不要です。

確認内容：9モデルの読み込み、0/15/30フレームの評価、評価済み形状とのレイ距離照合、
ボーン位置、共有ハンドルごとの姿勢切替、描画画素、サブフォルダの拡散テクスチャ、
WIC画像アップロード、存在しないファイル、共有モデルの解放・再利用。
形状の基準値は同梱の ufbx を直接評価して計算しています。Autodesk SDKとの画像比較ではありません。
WARPでの描画検証であり、ゲーム本体の操作・実GPU・全FBXデータの互換性を保証するものではありません。

Fixtures のFBXは https://github.com/ufbx/ufbx/tree/5955c5c0b042ac2dc6f32955ab206aaec0620cfc/data から取得。
ufbxリポジトリのライセンスを同梱しています。
`textures/checkerboard_diffuse.png` は元エンジンの `Assets/char.png` のコピーです。
元のcheckerboard画像は使用せず、相対パスの解決とSRVのバインドを検証します。
BoxCollider/SphereColliderは元エンジンのAssetsを使用します。

Results に、この納品時のビルド・テスト結果を収録しています。


## 左右反転の回帰検査

`orientation_rh.fbx`／`orientation.bmp`は今回作成した検査専用素材です。
右手系のZ=+2・法線+Zの板を、左手系のZ=-2・法線-Zへ変換すること、
U値を保持すること、左上赤／右上緑／左下青／右下黄で描画されることを確認します。
旧ローダーでは64件失敗し、修正後は0件です。

## v3追加テスト

Lambertの鏡面反射フラグをGPUから読み戻し、無効（0）であることを確認します。
Model::SetTransformでZを0→+1へ移動し、LHカメラで描画幅が154→124になることも確認します。
テストはゲームと同じアルファブレンド状態を使います。
環境変数UFBX_TEST_HARDWARE=1を設定してbuild-and-run.cmdを実行すると、WARPではなく実GPUを使います。
実GPUが利用できない環境ではこのモードは失敗し、WARPへの自動切替は行いません。
今回、実GPUで9つの同梱モデルとローカルのBall／Odenを検証しました。
Ball／Odenはユーザーの配置先だけに存在するため、ZIPの自動テスト対象には含めません。
