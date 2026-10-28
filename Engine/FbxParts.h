#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include "Texture.h"
#include "Transform.h"
#include "ufbx.h"
using namespace DirectX;
struct RayCastData;
class FbxParts {
	struct VERTEX { XMFLOAT3 position, normal, uv; };
	struct CONSTANT_BUFFER {
		XMMATRIX worldVewProj, normalTrans, world;
		XMFLOAT4 lightDirection, diffuse, ambient, speculer, cameraPosition;
		FLOAT shininess;
		BOOL isTexture; FLOAT padding[2];
	};
	static_assert(sizeof(CONSTANT_BUFFER) == 288, "Simple3D shader constant buffer layout");
	struct MATERIAL {
		XMFLOAT4 diffuse{ 1,1,1,1 }, ambient{ 0,0,0,1 }, specular{ 0,0,0,0 };
		float shininess = 0;
		Texture* texture = nullptr;
		std::vector<DWORD> indices;
		ID3D11Buffer* buffer = nullptr;
	};
	const ufbx_node* node_ = nullptr;
	const ufbx_node* currentNode_ = nullptr;
	std::vector<VERTEX> vertices_;
	std::vector<uint32_t> cornerIndices_;
	std::vector<MATERIAL> materials_;
	ID3D11Buffer* vertexBuffer_ = nullptr;
	ID3D11Buffer* constantBuffer_ = nullptr;

	void UpdateMesh(const ufbx_node* animatedNode);
public:
	FbxParts() = default;
	~FbxParts();
	HRESULT Init(const ufbx_node* node, const std::string& directory);
	void Draw(Transform& transform);
	void Animate(Transform& transform, const ufbx_scene* evaluated);
	void Evaluate(const ufbx_scene* evaluated);
	bool GetBonePosition(const std::string& name, XMFLOAT3* position) const;
	void RayCast(RayCastData* data);
};



