# 自作オブジェクトのスマートポインター化

指定先へ直接反映しました。ラムダは追加していません。

## 所有関係
- ModelData / ImageData: 各ハンドルをunique_ptrで所有。
- Fbx / Sprite: 同じファイルを使うハンドル間でshared_ptrにより共有。
- FbxParts: Fbxがunique_ptrで所有。
- GameObjectの子、Collider: 親GameObjectがunique_ptrで所有。
- RootObject: Mainのunique_ptrが所有。
- VFXのEmitter / Particle: 管理リストがunique_ptrで所有。
- BillBoard: Emitterがunique_ptrで所有。
- Sprite / BillBoard内のTexture: 各オブジェクトがunique_ptrで所有。
- FBX材質のTexture: 既存のshared_ptrによる共有を維持。
- CSV一時配列: unique_ptr配列。Audioの波形バイト列・ボイス参照の配列はvector。

DirectX・Windowsインターフェース、HANDLE、ufbx_sceneは既存の専用解放方式を維持。
親GameObject、Particleの発生元Emitter、ufbxノードなど、参照だけのポインターは生ポインターのままです。

## APIと注意点
Model / ImageのハンドルAPIは維持しました。
GetChildList()はconst std::list<std::unique_ptr<GameObject>>*を返す参照専用APIへ変更しました。要素の生ポインターが必要なら.get()を使います。外部からのeraseやdeleteは行わず、KillMe / KillAllChildren / ReleaseSubを使ってください。
PushBackChild / PushFrontChild / AddColliderにはunique_ptrを受け取る版を追加。生ポインター版も互換用に残しましたが、渡した時点で所有権を移します。既に所有されているオブジェクトやスタック上のオブジェクトは渡せません。
Instantiate(parent)の戻り値は参照用で、親が子を所有します。親のない旧Instantiate(nullptr)のみ互換性のため従来どおり呼び出し側へ所有権を返します。親なしオブジェクトはMainの例のようにmake_uniqueで作成してください。
VFX::CreateParticleの内部用イテレーター引数もunique_ptrのリストに合わせて変更しています。

shared_ptrの最後の所有者が消えると資源も解放されます。unique_ptrをコンテナーに移す箇所はstd::moveで所有権を渡します。
Spriteの破棄では、従来漏れていたTextureと定数バッファも解放します。BillBoardはデストラクターから既存Releaseを呼びます。
ReleaseSubでは、子を解放した後に一覧を空にし、解放済みポインターが残らないようにしました。
画像ハンドルの空き番号を再利用するときも切り抜き矩形を初期化します。

## 検証
Tests/build-and-run.cmdへ以下を追加しました:
- モデルの共有・共有ハンドルの解放（既存検査）
- 画像の共有、元のハンドル解放後の存続、最終ハンドルでの解放、失敗時の後片付け、空き番号の再利用
- 子と孫のKillMe / KillAllChildren / ReleaseSubによる解放回数・デストラクター回数
- Colliderの仮想デストラクターによる解放
- Particle寿命とEmitterの解放順序、VFX::Releaseの繰り返し
WARPおよび実GPUで既存描画・アニメーション検査を含めPASS。
ゲーム画面の目視、実際の音声再生、CSV入力の全形式、外部ユーザーコードとの互換性は未検証です。
GetChildListと内部用VFX::CreateParticleを直接使う外部コードには上記の型変更が必要です。
Tests/Results/smart-* にログを保存しています。

Debug/Release × x64/x86の全4構成でビルド成功。skg.fbxの読み込み直後122.67MiB、600回評価後123.28MiB（単独の評価・頂点転送計測）。

テクスチャ保持方式を更新: TextureManagerもshared_ptrで保持し、TextureManager::Clear()でキャッシュを明示的に解放します。シーン切替・終了時の呼び出しを追加済みです。
