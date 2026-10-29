#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include "TextureManager.h"
#include "Transform.h"
#include "ufbx.h"
using namespace DirectX;
struct RayCastData;
class FbxParts {
	struct VERTEX {
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT3 uv;
	};
	struct CONSTANT_BUFFER {
		XMMATRIX worldVewProj;
		XMMATRIX normalTrans;
		XMMATRIX world;
		XMFLOAT4 lightDirection;
		XMFLOAT4 diffuse;
		XMFLOAT4 ambient;
		XMFLOAT4 speculer;
		XMFLOAT4 cameraPosition;
		FLOAT shininess;
		BOOL isTexture;
		FLOAT padding[2];
	};
	static_assert(sizeof(CONSTANT_BUFFER) == 288, "Simple3D shader constant buffer layout");
	struct MATERIAL {
		XMFLOAT4 diffuse{ 1,1,1,1 };
		XMFLOAT4 ambient{ 0,0,0,1 };
		XMFLOAT4 specular{ 0,0,0,0 };
		float shininess = 0;
		std::shared_ptr<Texture> texture;
		std::vector<DWORD> indices;
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer = nullptr;
	};
	const ufbx_node* node_ = nullptr;
	const ufbx_node* currentNode_ = nullptr;
	const std::vector<XMFLOAT3>* bakedNodePositions_ = nullptr;
	std::vector<VERTEX> vertices_;
	std::vector<uint32_t> cornerIndices_;
	std::vector<MATERIAL> materials_;
	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_ = nullptr;

	void LoadMaterials(const ufbx_node* node, const std::string& modelDirectory);
	void BuildVertices(const ufbx_mesh* mesh);
	HRESULT CreateBuffers();
	void UpdateVertices(const ufbx_node* animatedNode);
	void UploadVertices();
public:
    using VertexSnapshot = std::vector<VERTEX>;
    bool CaptureVertices(const ufbx_scene* evaluated, VertexSnapshot* snapshot);
    void ApplyVertices(const VertexSnapshot& snapshot, const std::vector<XMFLOAT3>& nodePositions);
	FbxParts() = default;
	~FbxParts();
	HRESULT Init(const ufbx_node* node, const std::string& directory);
	void Draw(Transform& transform);
	void Animate(Transform& transform, const ufbx_scene* evaluated);
	void Evaluate(const ufbx_scene* evaluated);
	bool GetBonePosition(const std::string& name, XMFLOAT3* position) const;
	void RayCast(RayCastData* data);
};
