#pragma once
#include <wrl/client.h>
#include <memory>
#include "Direct3D.h"
#include "Texture.h"
#include <DirectXMath.h>
#include <string>
using namespace DirectX;

// コンスタントバッファー
struct CONSTANT_BUFFER
{
    XMMATRIX matWVP;
    XMFLOAT4 color;
};

// 頂点情報
struct VERTEX
{
    XMFLOAT3 position;
    XMFLOAT3 uv;
};

// 常にカメラの方を向く四角形ポリゴン
class BillBoard
{
    Microsoft::WRL::ComPtr<ID3D11Buffer> pVertexBuffer_ = nullptr;   // 頂点バッファ
    Microsoft::WRL::ComPtr<ID3D11Buffer> pIndexBuffer_ = nullptr;    // インデックスバッファ
    Microsoft::WRL::ComPtr<ID3D11Buffer> pConstantBuffer_ = nullptr; // コンスタントバッファ

    std::unique_ptr<Texture> pTexture_; // 画像

  public:
    BillBoard();
    ~BillBoard();

    // ロード
    // 引数：fileName	画像ファイル名
    // 戻値：成功/失敗
    HRESULT Load(std::string fileName);

    // 描画
    // 引数：matrix	変換行列（ワールド行列）
    // 引数：color	色
    void Draw(XMMATRIX matWorld, XMFLOAT4 color);

    // 解放
    void Release();
};