#include "FbxParts.h"
#include "Fbx.h"
#include "Direct3D.h"
#include "Camera.h"
#include <algorithm>
#include <cstring>

#include <vector>
static XMFLOAT3 ToFloat3(ufbx_vec3 v) { return XMFLOAT3((float)v.x, (float)v.y, (float)v.z); }
static XMFLOAT4 ToColor(ufbx_vec3 v) { return XMFLOAT4((float)v.x, (float)v.y, (float)v.z, 1); }
FbxParts::~FbxParts() {
	for (auto& mat : materials_) {
		if (mat.buffer) mat.buffer->Release();
		delete mat.texture;
	}
	if (vertexBuffer_) vertexBuffer_->Release();
	if (constantBuffer_) constantBuffer_->Release();
}
HRESULT FbxParts::Init(const ufbx_node* node, const std::string& dir) {
	if (!node || !node->mesh || !Direct3D::pDevice_) return E_FAIL;
	node_ = currentNode_ = node;
	const ufbx_mesh* mesh = node->mesh;

	const size_t n = std::max<size_t>(1, node->materials.count);
	materials_.resize(n);
	for (size_t m = 0; m < node->materials.count; ++m) {
		const auto* mat = node->materials.data[m];
		MATERIAL& dst = materials_[m];
		dst.diffuse = ToColor(mat->fbx.diffuse_color.value_vec3);
		dst.ambient = ToColor(mat->fbx.ambient_color.value_vec3);
		dst.specular = XMFLOAT4(0, 0, 0, 0);
		if (mat->shader_type == UFBX_SHADER_FBX_PHONG && mat->fbx.specular_exponent.value_real > 0.0 &&
			(mat->fbx.specular_color.value_vec3.x > 0.0 || mat->fbx.specular_color.value_vec3.y > 0.0 || mat->fbx.specular_color.value_vec3.z > 0.0))
			dst.specular = ToColor(mat->fbx.specular_color.value_vec3);
		dst.shininess = (float)mat->fbx.specular_exponent.value_real;
		const ufbx_texture* tex = mat->fbx.diffuse_color.texture;
		if (!tex) tex = mat->pbr.base_color.texture;
		if (tex) {
			std::string filename(tex->filename.data, tex->filename.length);
			std::string relative(tex->relative_filename.data, tex->relative_filename.length);
			std::replace(filename.begin(), filename.end(), char(92), '/');
			std::replace(relative.begin(), relative.end(), char(92), '/');
			const size_t slash = filename.find_last_of("/\\");
			const std::string base = slash == std::string::npos ? filename : filename.substr(slash + 1);
			// Prefer the exported relative path, then ufbx's resolved path, then a relocated basename.
			for (const auto& path : { dir + relative, filename, dir + base }) {
				if (path.empty() || GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES ||
					(GetFileAttributesA(path.c_str()) & FILE_ATTRIBUTE_DIRECTORY)) continue;
				Texture* candidate = new Texture();
				if (SUCCEEDED(candidate->Load(path))) { dst.texture = candidate; break; }
				delete candidate;
			}
			if (!dst.texture) OutputDebugStringA("ufbx: diffuse texture unavailable; using material color.\n");
		}
	}
	std::vector<uint32_t> tri;
	tri.resize(std::max<size_t>(3, mesh->max_face_triangles * 3));
	for (size_t f = 0; f < mesh->faces.count; ++f) {
		const auto face = mesh->faces.data[f];
		if (face.num_indices < 3) continue;
		uint32_t material = mesh->face_material.count > f ? mesh->face_material.data[f] : 0;
		if (material >= materials_.size()) material = 0;
		uint32_t count = ufbx_triangulate_face(tri.data(), tri.size(), mesh, face);
		for (uint32_t k = 0; k < count * 3; ++k) {
			uint32_t ix = tri[k];
			ufbx_vec3 p = ufbx_get_vertex_vec3(&mesh->vertex_position, ix);
			ufbx_vec3 norm = mesh->vertex_normal.exists ? ufbx_get_vertex_vec3(&mesh->vertex_normal, ix) : ufbx_vec3{ 0,1,0 };
			ufbx_vec2 uv = mesh->vertex_uv.exists ? ufbx_get_vertex_vec2(&mesh->vertex_uv, ix) : ufbx_vec2{ 0,0 };
			VERTEX v{ ToFloat3(p), ToFloat3(norm), XMFLOAT3((float)uv.x, 1.0f - (float)uv.y, 0) };
			const uint32_t dstIndex = (uint32_t)vertices_.size();
			vertices_.push_back(v);
			cornerIndices_.push_back(ix);
			materials_[material].indices.push_back(dstIndex);
		}
	}
	if (vertices_.empty()) return E_FAIL;

	D3D11_BUFFER_DESC vb{};
	vb.ByteWidth = (UINT)(vertices_.size() * sizeof(VERTEX));
	vb.Usage = D3D11_USAGE_DYNAMIC;
	vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_SUBRESOURCE_DATA vd{}; vd.pSysMem = vertices_.data();
	HRESULT hr = Direct3D::pDevice_->CreateBuffer(&vb, &vd, &vertexBuffer_);
	if (FAILED(hr)) return hr;
	for (auto& mat : materials_) {
		if (mat.indices.empty()) continue;
		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = (UINT)(mat.indices.size() * sizeof(DWORD));
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		D3D11_SUBRESOURCE_DATA init{}; init.pSysMem = mat.indices.data();
		hr = Direct3D::pDevice_->CreateBuffer(&bd, &init, &mat.buffer);
		if (FAILED(hr)) return hr;
	}
	D3D11_BUFFER_DESC cb{};
	cb.ByteWidth = sizeof(CONSTANT_BUFFER);
	cb.Usage = D3D11_USAGE_DYNAMIC;
	cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	return Direct3D::pDevice_->CreateBuffer(&cb, nullptr, &constantBuffer_);
}
void FbxParts::UpdateMesh(const ufbx_node* animatedNode) {
	if (!animatedNode || !animatedNode->mesh || !vertexBuffer_) return;
	const ufbx_mesh* mesh = animatedNode->mesh;
	const ufbx_matrix normalMatrix = ufbx_matrix_for_normals(&animatedNode->geometry_to_world);
	const auto& positions = mesh->skinned_position.exists ? mesh->skinned_position : mesh->vertex_position;
	const auto& normals = mesh->skinned_normal.exists ? mesh->skinned_normal : mesh->vertex_normal;
	for (size_t i = 0; i < vertices_.size(); ++i) {
		const uint32_t ix = cornerIndices_[i];
		if (ix >= positions.indices.count) continue;
		ufbx_vec3 pos = ufbx_get_vertex_vec3(&positions, ix);
		ufbx_vec3 norm = normals.exists && ix < normals.indices.count ? ufbx_get_vertex_vec3(&normals, ix) : ufbx_vec3{ 0,1,0 };
		// Bake into FBX scene space (model-local to the engine).
		if (mesh->skinned_is_local) {
			// Rigid and local skinned vertices require the geometry transform once.

			pos = ufbx_transform_position(&animatedNode->geometry_to_world, pos);
			norm = ufbx_transform_direction(&normalMatrix, norm);
		}
		vertices_[i].position = ToFloat3(pos);
		vertices_[i].normal = ToFloat3(ufbx_vec3_normalize(norm));
	}
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (SUCCEEDED(Direct3D::pContext_->Map(vertexBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
		memcpy(mapped.pData, vertices_.data(), vertices_.size() * sizeof(VERTEX));
		Direct3D::pContext_->Unmap(vertexBuffer_, 0);
	}
}
void FbxParts::Evaluate(const ufbx_scene* evaluated) {
	if (evaluated && node_ && node_->typed_id < evaluated->nodes.count) {
		const ufbx_node* node = evaluated->nodes.data[node_->typed_id];
		if (node && node->mesh) { currentNode_ = node; UpdateMesh(node); }
	}
}
void FbxParts::Animate(Transform& transform, const ufbx_scene* evaluated) {
	Evaluate(evaluated);
	Draw(transform);
}
void FbxParts::Draw(Transform& transform) {
	if (!vertexBuffer_ || !constantBuffer_ || !Direct3D::pContext_) return;
	UINT stride = sizeof(VERTEX), offset = 0;
	Direct3D::pContext_->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);
	Direct3D::pContext_->VSSetConstantBuffers(0, 1, &constantBuffer_);
	Direct3D::pContext_->PSSetConstantBuffers(0, 1, &constantBuffer_);
	for (auto& m : materials_) {
		if (!m.buffer) continue;
		Direct3D::pContext_->IASetIndexBuffer(m.buffer, DXGI_FORMAT_R32_UINT, 0);
		CONSTANT_BUFFER cb{};
		cb.worldVewProj = XMMatrixTranspose(transform.GetWorldMatrix() * Camera::GetViewMatrix() * Camera::GetProjectionMatrix());
		cb.world = XMMatrixTranspose(transform.GetWorldMatrix());
		cb.normalTrans = XMMatrixInverse(nullptr, transform.GetWorldMatrix());
		cb.ambient = m.ambient; cb.diffuse = m.diffuse; cb.speculer = m.specular;
		cb.shininess = m.shininess;
		const XMFLOAT3 camera = Camera::GetPosition();
		cb.cameraPosition = XMFLOAT4(camera.x, camera.y, camera.z, 0);
		cb.lightDirection = XMFLOAT4(1, -1, 1, 0);
		cb.isTexture = m.texture && m.texture->GetSRV();
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (SUCCEEDED(Direct3D::pContext_->Map(constantBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
			memcpy(mapped.pData, &cb, sizeof(cb));
			Direct3D::pContext_->Unmap(constantBuffer_, 0);
		}
		ID3D11SamplerState* sampler = m.texture ? m.texture->GetSampler() : nullptr;
		ID3D11ShaderResourceView* srv = m.texture ? m.texture->GetSRV() : nullptr;
		Direct3D::pContext_->PSSetSamplers(0, 1, &sampler);
		Direct3D::pContext_->PSSetShaderResources(0, 1, &srv);
		Direct3D::pContext_->DrawIndexed((UINT)m.indices.size(), 0, 0);
	}
}
bool FbxParts::GetBonePosition(const std::string& name, XMFLOAT3* position) const {
	if (!currentNode_ || !currentNode_->mesh || !position) return false;
	for (size_t s = 0; s < currentNode_->mesh->skin_deformers.count; ++s) {
		const auto* skin = currentNode_->mesh->skin_deformers.data[s];
		for (size_t i = 0; i < skin->clusters.count; ++i) {
			const auto* bone = skin->clusters.data[i]->bone_node;
			if (bone && name == std::string(bone->name.data, bone->name.length)) {
				*position = ToFloat3(ufbx_vec3{ bone->node_to_world.m03,bone->node_to_world.m13,bone->node_to_world.m23 });
				return true;
			}
		}
	}
	return false;
}
void FbxParts::RayCast(RayCastData* data) {
	if (!data) return;
	for (const auto& m : materials_) for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
		float dist = 0;
		if (Direct3D::Intersect(data->start, data->dir, vertices_[m.indices[i]].position, vertices_[m.indices[i + 1]].position, vertices_[m.indices[i + 2]].position, &dist) && dist < data->dist) {
			data->hit = TRUE; data->dist = dist;
			const XMVECTOR a = XMLoadFloat3(&vertices_[m.indices[i]].position);
			const XMVECTOR b = XMLoadFloat3(&vertices_[m.indices[i + 1]].position);
			const XMVECTOR c = XMLoadFloat3(&vertices_[m.indices[i + 2]].position);
			XMStoreFloat3(&data->normal, XMVector3Normalize(XMVector3Cross(b - a, c - a)));
		}
	}
}





