#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <filesystem>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
// ufbx replaces Autodesk FBX SDK

#pragma comment(lib, "WindowsCodecs.lib")

using namespace DirectX;

//-----------------------------------------------------------
// テクスチャ関連を扱うクラス
//-----------------------------------------------------------
class Texture
{
    Microsoft::WRL::ComPtr<ID3D11SamplerState>
        pSampleLinear_; // テクスチャサンプラー（テクスチャの貼り方）
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
        pTextureSRV_; // シェーダーリソースビュー（テクスチャをシェーダーに送るためのもの）
    XMFLOAT3 size_;   // 画像ファイルのサイズ

  public:
    Texture();
    ~Texture();

    // ロード
    // 引数：filePath	画像ファイル名
    // 戻値：成功/失敗
    HRESULT Load(const std::filesystem::path &filePath);

    // 各アクセス関数
    ID3D11SamplerState *GetSampler()
    {
        return pSampleLinear_.Get();
    } // サンプラーの取得

    ID3D11ShaderResourceView *GetSRV()
    {
        return pTextureSRV_.Get();
    } // シェーダーリソースビューの取得

    XMFLOAT3 GetSize()
    {
        return size_;
    } // 画像サイズの取得
};
