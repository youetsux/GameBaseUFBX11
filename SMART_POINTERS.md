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

## DirectXのCOM資源をComPtrへ移行（2026-10-28）

Direct3Dのデバイス・コンテキスト・スワップチェーン・各ビュー・シェーダー・描画ステート、FbxParts / Sprite / BillBoardのバッファ、TextureのSRVとサンプラー、DirectInput、XAudio2本体をMicrosoft::WRL::ComPtrで保持します。
Windows SDKのwrl/client.hを使用し、WILなどの依存は追加していません。

作成・再作成の出力引数にはReleaseAndGetAddressOf()、APIへ渡す借用ポインターにはGet()、バッファ等の設定APIに渡す配列にはGetAddressOf()を使います。設定APIでReleaseAndGetAddressOf()を使うと保持中の資源を解放するため、使い分けが必要です。
Texture::GetSRV / GetSamplerの戻り値と、TextureManagerのデバイス識別用ポインターは借用参照のままです。呼び出し側でReleaseしないでください。
Direct3D::pDevice_ / pContext_とSHADER_BUNDLEの各メンバーはComPtr型になりました。外部コードで生ポインターを必要とする場合は.Get()を使ってください。通常の -> によるメソッド呼び出しは従来どおりです。

Direct3D::Releaseはコンテキストの設定を解除してから描画資源、コンテキスト、デバイスの順にResetします。
Mainの終了時にInput::Releaseを呼びます。
XAudio2のボイスはCOMではないためDestroyVoiceを維持します。シーン切替時には音声エンジンを再作成せず再利用し、終了時はソースボイス、マスターボイス、XAudio2本体、所有するCOM初期化の順に後片付けします。
WindowsのHANDLE / HWND等の管理、モデル配置、アニメーションの進行方式は変更していません。

確認結果:
- Debug / Release × x64 / x86の4構成でビルド成功。
- 既存SmokeテストはWARP・実GPUともRESULT: PASS (0 failures)。Direct3Dデバッグ層のError / Corruptionは0件。
- Run / Walkingの頂点変形、1,000体（2,298,000三角形）の描画、共有・解放、ボーン・レイ判定を確認。
- SmokeテストにDirect3D::Releaseの繰り返しと保持資源のクリア確認を追加。
- 別の非表示ウィンドウの実行検査で、描画・DirectInput・XAudio2の初期化、Texture再読み込みと失敗後の保持、Sprite / BillBoard描画、解放の繰り返し、再初期化を2サイクル確認。
- ゲーム画面の目視と実際の音声再生は未確認。

Tests/Results/comptr-*.logに検証ログを保存しています。
