#include "Texture.h"
#include "Direct3D.h"
#include <wrl/client.h>
#include <vector>
#include <climits>
using Microsoft::WRL::ComPtr;
Texture::Texture() : pSampleLinear_(nullptr), pTextureSRV_(nullptr), size_(0,0,0) {}
Texture::~Texture() {
 if (pSampleLinear_) pSampleLinear_->Release();
 if (pTextureSRV_) pTextureSRV_->Release();
}
HRESULT Texture::Load(std::string fileName) {
 if (!Direct3D::pDevice_) return E_FAIL;
 // Keep the engine's existing Windows multibyte path convention.
 int length = MultiByteToWideChar(CP_ACP, 0, fileName.c_str(), -1, nullptr, 0);
 if (!length) return E_INVALIDARG;
 std::vector<wchar_t> path(length);
 MultiByteToWideChar(CP_ACP, 0, fileName.c_str(), -1, path.data(), length);
 const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
 if (FAILED(com) && com != RPC_E_CHANGED_MODE) return com;
 struct ComScope { bool owned; ~ComScope() { if (owned) CoUninitialize(); } } scope{SUCCEEDED(com)};
 ComPtr<IWICImagingFactory> factory;
 ComPtr<IWICBitmapDecoder> decoder;
 ComPtr<IWICBitmapFrameDecode> frame;
 ComPtr<IWICFormatConverter> converter;
 HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf()));
 if (FAILED(hr)) return hr;
 hr = factory->CreateDecoderFromFilename(path.data(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
 if (FAILED(hr)) return hr;
 hr = decoder->GetFrame(0, &frame); if (FAILED(hr)) return hr;
 hr = factory->CreateFormatConverter(&converter); if (FAILED(hr)) return hr;
 hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom);
 if (FAILED(hr)) return hr;
 UINT width=0, height=0;
 hr = converter->GetSize(&width, &height); if (FAILED(hr)) return hr;
 if (!width || !height || width > UINT_MAX / 4 || height > UINT_MAX / (width * 4)) return E_FAIL;
 std::vector<BYTE> pixels((size_t)width * height * 4);
 hr = converter->CopyPixels(nullptr, width * 4, (UINT)pixels.size(), pixels.data());
 if (FAILED(hr)) return hr;
 D3D11_TEXTURE2D_DESC desc{};
 desc.Width=width; desc.Height=height; desc.MipLevels=1; desc.ArraySize=1;
 desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
 desc.Usage=D3D11_USAGE_IMMUTABLE; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 D3D11_SUBRESOURCE_DATA data{}; data.pSysMem=pixels.data(); data.SysMemPitch=width*4;
 ComPtr<ID3D11Texture2D> texture;
 hr=Direct3D::pDevice_->CreateTexture2D(&desc,&data,&texture); if (FAILED(hr)) return hr;
 ComPtr<ID3D11ShaderResourceView> srv;
 hr=Direct3D::pDevice_->CreateShaderResourceView(texture.Get(),nullptr,&srv); if (FAILED(hr)) return hr;
 D3D11_SAMPLER_DESC samplerDesc{};
 samplerDesc.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
 samplerDesc.AddressU=samplerDesc.AddressV=samplerDesc.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
 samplerDesc.MaxLOD=D3D11_FLOAT32_MAX; samplerDesc.ComparisonFunc=D3D11_COMPARISON_NEVER;
 ComPtr<ID3D11SamplerState> sampler;
 hr=Direct3D::pDevice_->CreateSamplerState(&samplerDesc,&sampler); if (FAILED(hr)) return hr;
 if (pTextureSRV_) pTextureSRV_->Release();
 if (pSampleLinear_) pSampleLinear_->Release();
 pTextureSRV_=srv.Detach(); pSampleLinear_=sampler.Detach();
 size_=XMFLOAT3((float)width,(float)height,0);
 return S_OK;
}
