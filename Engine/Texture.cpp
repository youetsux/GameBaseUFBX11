#include "Texture.h"
#include "Direct3D.h"
#include <wrl/client.h>
#include <vector>
#include <climits>
using Microsoft::WRL::ComPtr;

Texture::Texture() : pSampleLinear_(nullptr), pTextureSRV_(nullptr), size_(0, 0, 0)
{
}

Texture::~Texture() = default;

// 画像をWICでRGBAへ展開し、GPU用テクスチャとサンプラーを作成する。
HRESULT Texture::Load(const std::filesystem::path &filePath)
{
    if (!Direct3D::pDevice_)
        return E_FAIL;

    // この関数でCOMを初期化した場合だけ、終了時に初期化を解除する。
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE)
        return com;

    struct ComScope
    {
        bool owned;

        ~ComScope()
        {
            if (owned)
                CoUninitialize();
        }
    } scope{SUCCEEDED(com)};


    // WICで画像ファイルを開き、先頭フレームを読み込む。
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr))
        return hr;
    hr = factory->CreateDecoderFromFilename(filePath.c_str(), nullptr, GENERIC_READ,
                                            WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr))
        return hr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
        return hr;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr))
        return hr;

    // 元の画像形式にかかわらず、GPUへ渡す色をRGBAの4バイトに揃える。
    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                               nullptr, 0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
        return hr;

    // 画像サイズから必要なバイト数を求める。乗算があふれないかも確認する。
    UINT width = 0, height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr))
        return hr;
    if (!width || !height || width > UINT_MAX / 4 || height > UINT_MAX / (width * 4))
        return E_FAIL;
    std::vector<BYTE> pixels((size_t)width * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, (UINT)pixels.size(), pixels.data());
    if (FAILED(hr))
        return hr;

    // CPU側の画素を初期データとして、描画専用のGPUテクスチャを作成する。
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = pixels.data();
    data.SysMemPitch = width * 4;
    ComPtr<ID3D11Texture2D> texture;
    hr = Direct3D::pDevice_->CreateTexture2D(&desc, &data, &texture);
    if (FAILED(hr))
        return hr;
    ComPtr<ID3D11ShaderResourceView> srv;
    hr = Direct3D::pDevice_->CreateShaderResourceView(texture.Get(), nullptr, &srv);
    if (FAILED(hr))
        return hr;

    // 拡大縮小時の補間方法と、画像の範囲外を参照したときの扱いを設定する。
    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    ComPtr<ID3D11SamplerState> sampler;
    hr = Direct3D::pDevice_->CreateSamplerState(&samplerDesc, &sampler);
    if (FAILED(hr))
        return hr;

    // 作成に成功してからメンバーへ所有権を移し、読み込み結果を確定する。
    pTextureSRV_ = std::move(srv);
    pSampleLinear_ = std::move(sampler);
    size_ = XMFLOAT3((float)width, (float)height, 0);
    return S_OK;
}
