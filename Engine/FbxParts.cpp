#include "FbxParts.h"
#include "Fbx.h"
#include "Direct3D.h"
#include "Camera.h"
#include <algorithm>
#include <filesystem>
#include <cstring>

#include <vector>
static XMFLOAT3 ToFloat3(ufbx_vec3 value) {
    return XMFLOAT3((float)value.x, (float)value.y, (float)value.z);
}
static XMFLOAT4 ToColor(ufbx_vec3 value) {
    return XMFLOAT4((float)value.x, (float)value.y, (float)value.z, 1);
}

namespace {
    namespace fs = std::filesystem;

    // Relative export path, resolved path, then a relocated file beside the model.
    std::vector<fs::path> TexturePaths(const ufbx_texture* texture,
    const fs::path& modelDirectory) {
        const fs::path filename(std::string(texture->filename.data, texture->filename.length));
        const fs::path relative(std::string(texture->relative_filename.data, texture->relative_filename.length));
        std::vector<fs::path> paths;
        paths.push_back(modelDirectory / relative);
        paths.push_back(filename);
        paths.push_back(modelDirectory / filename.filename());
        return paths;
    }

    std::shared_ptr<Texture> LoadMaterialTexture(const ufbx_texture* texture,
    const fs::path& modelDirectory) {
        for (const auto& path : TexturePaths(texture, modelDirectory)) {
            std::error_code error;
            if (!fs::is_regular_file(path, error)) {
                continue;
            }
            std::shared_ptr<Texture> loaded = TextureManager::Load(path);
            if (loaded) {
                return loaded;
            }
        }
        return nullptr;
    }
}

FbxParts::~FbxParts() = default;
HRESULT FbxParts::Init(const ufbx_node* node, const std::string& modelDirectory) {
    if (!node || !node->mesh || !Direct3D::pDevice_) {
        return E_FAIL;
    }
    node_ = node;
    currentNode_ = node;
    LoadMaterials(node, modelDirectory);
    BuildVertices(node->mesh);
    if (vertices_.empty()) {
        return E_FAIL;
    }
    return CreateBuffers();
}

void FbxParts::LoadMaterials(const ufbx_node* node, const std::string& modelDirectory) {
    size_t materialCount = node->materials.count;
    if (materialCount == 0) {
        materialCount = 1;
    }
    materials_.resize(materialCount);
    for (size_t materialIndex = 0; materialIndex < node->materials.count; ++materialIndex) {
        const ufbx_material* sourceMaterial = node->materials.data[materialIndex];
        MATERIAL& material = materials_[materialIndex];
        material.diffuse = ToColor(sourceMaterial->fbx.diffuse_color.value_vec3);
        material.ambient = ToColor(sourceMaterial->fbx.ambient_color.value_vec3);
        material.specular = XMFLOAT4(0, 0, 0, 0);
        const ufbx_vec3 specularColor = sourceMaterial->fbx.specular_color.value_vec3;
        const bool isPhong = sourceMaterial->shader_type == UFBX_SHADER_FBX_PHONG;
        const bool hasShininess = sourceMaterial->fbx.specular_exponent.value_real > 0.0;
        const bool hasSpecularColor = specularColor.x > 0.0 ||
        specularColor.y > 0.0 ||
        specularColor.z > 0.0;
        if (isPhong && hasShininess && hasSpecularColor) {
            material.specular = ToColor(specularColor);
        }
        material.shininess = (float)sourceMaterial->fbx.specular_exponent.value_real;
        const ufbx_texture* texture = sourceMaterial->fbx.diffuse_color.texture;
        if (!texture) {
            texture = sourceMaterial->pbr.base_color.texture;
        }
        if (texture) {
            material.texture = LoadMaterialTexture(texture, modelDirectory);
            if (!material.texture) {
                OutputDebugStringA("ufbx: diffuse texture unavailable; using material color.\n");
            }
        }
    }

}

void FbxParts::BuildVertices(const ufbx_mesh* mesh) {
    std::vector<uint32_t> triangleCorners;
    size_t cornerCapacity = mesh->max_face_triangles * 3;
    if (cornerCapacity < 3) {
        cornerCapacity = 3;
    }
    triangleCorners.resize(cornerCapacity);
    for (size_t faceIndex = 0; faceIndex < mesh->faces.count; ++faceIndex) {
        const ufbx_face face = mesh->faces.data[faceIndex];
        if (face.num_indices < 3) {
            continue;
        }
        uint32_t materialIndex = 0;
        if (faceIndex < mesh->face_material.count) {
            materialIndex = mesh->face_material.data[faceIndex];
        }
        if (materialIndex >= materials_.size()) {
            materialIndex = 0;
        }
        uint32_t triangleCount = ufbx_triangulate_face(triangleCorners.data(), triangleCorners.size(), mesh, face);
        for (uint32_t triangleCornerIndex = 0; triangleCornerIndex < triangleCount * 3; ++triangleCornerIndex) {
            uint32_t cornerIndex = triangleCorners[triangleCornerIndex];
            ufbx_vec3 position = ufbx_get_vertex_vec3(&mesh->vertex_position, cornerIndex);
            ufbx_vec3 normal = { 0, 1, 0 };
            if (mesh->vertex_normal.exists) {
                normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, cornerIndex);
            }
            ufbx_vec2 uv = { 0, 0 };
            if (mesh->vertex_uv.exists) {
                uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, cornerIndex);
            }
            VERTEX vertex{ ToFloat3(position), ToFloat3(normal), XMFLOAT3((float)uv.x, 1.0f - (float)uv.y, 0) };
            const uint32_t vertexIndex = (uint32_t)vertices_.size();
            vertices_.push_back(vertex);
            cornerIndices_.push_back(cornerIndex);
            materials_[materialIndex].indices.push_back(vertexIndex);
        }
    }

}

HRESULT FbxParts::CreateBuffers() {
    D3D11_BUFFER_DESC vertexBufferDesc{};
    vertexBufferDesc.ByteWidth = (UINT)(vertices_.size() * sizeof(VERTEX));
    vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11_SUBRESOURCE_DATA vertexData{};
    vertexData.pSysMem = vertices_.data();
    HRESULT result = Direct3D::pDevice_->CreateBuffer(&vertexBufferDesc, &vertexData, vertexBuffer_.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        return result;
    }
    for (MATERIAL& material : materials_) {
        if (material.indices.empty()) {
            continue;
        }
        D3D11_BUFFER_DESC indexBufferDesc{};
        indexBufferDesc.ByteWidth = (UINT)(material.indices.size() * sizeof(DWORD));
        indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA indexData{};
        indexData.pSysMem = material.indices.data();
        result = Direct3D::pDevice_->CreateBuffer(&indexBufferDesc, &indexData, material.buffer.ReleaseAndGetAddressOf());
        if (FAILED(result)) {
            return result;
        }
    }
    D3D11_BUFFER_DESC constantBufferDesc{};
    constantBufferDesc.ByteWidth = sizeof(CONSTANT_BUFFER);
    constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return Direct3D::pDevice_->CreateBuffer(&constantBufferDesc, nullptr, constantBuffer_.ReleaseAndGetAddressOf());
}
void FbxParts::UpdateVertices(const ufbx_node* animatedNode) {
    if (!animatedNode || !animatedNode->mesh || !vertexBuffer_) {
        return;
    }
    const ufbx_mesh* mesh = animatedNode->mesh;
    const ufbx_matrix normalMatrix = ufbx_matrix_for_normals(&animatedNode->geometry_to_world);
    const ufbx_vertex_vec3* positions = &mesh->vertex_position;
    if (mesh->skinned_position.exists) {
        positions = &mesh->skinned_position;
    }
    const ufbx_vertex_vec3* normals = &mesh->vertex_normal;
    if (mesh->skinned_normal.exists) {
        normals = &mesh->skinned_normal;
    }
    for (size_t vertexIndex = 0; vertexIndex < vertices_.size(); ++vertexIndex) {
        const uint32_t cornerIndex = cornerIndices_[vertexIndex];
        if (cornerIndex >= positions->indices.count) {
            continue;
        }
        ufbx_vec3 position = ufbx_get_vertex_vec3(positions, cornerIndex);
        ufbx_vec3 normal = { 0, 1, 0 };
        if (normals->exists && cornerIndex < normals->indices.count) {
            normal = ufbx_get_vertex_vec3(normals, cornerIndex);
        }
        // ローカル座標の頂点だけ、FBXシーンのワールド座標へ変換する。
        if (mesh->skinned_is_local) {
            // ゲーム内のモデル配置はDrawで適用するため、ここでは含めない。

            position = ufbx_transform_position(&animatedNode->geometry_to_world, position);
            normal = ufbx_transform_direction(&normalMatrix, normal);
        }
        vertices_[vertexIndex].position = ToFloat3(position);
        vertices_[vertexIndex].normal = ToFloat3(ufbx_vec3_normalize(normal));
    }
}

void FbxParts::UploadVertices() {
    if (!vertexBuffer_ || !Direct3D::pContext_) {
        return;
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(Direct3D::pContext_->Map(vertexBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, vertices_.data(), vertices_.size() * sizeof(VERTEX));
        Direct3D::pContext_->Unmap(vertexBuffer_.Get(), 0);
    }
}
void FbxParts::Evaluate(const ufbx_scene* evaluated) {
    if (evaluated && node_ && node_->typed_id < evaluated->nodes.count) {
        const ufbx_node* node = evaluated->nodes.data[node_->typed_id];
        if (node && node->mesh) {
            currentNode_ = node;
            bakedNodePositions_ = nullptr;
            UpdateVertices(node);
            UploadVertices();
        }
    }
}
// Capture CPU vertices only: baking does not create or upload GPU buffers.
bool FbxParts::CaptureVertices(const ufbx_scene* evaluated, VertexSnapshot* snapshot) {
    if (!evaluated || !node_ || !snapshot || node_->typed_id >= evaluated->nodes.count) {
        return false;
    }
    const ufbx_node* node = evaluated->nodes.data[node_->typed_id];
    if (!node || !node->mesh) {
        return false;
    }
    UpdateVertices(node);
    *snapshot = vertices_;
    return true;
}

void FbxParts::ApplyVertices(const VertexSnapshot& snapshot,
    const std::vector<XMFLOAT3>& nodePositions) {
    vertices_ = snapshot;
    currentNode_ = node_;
    bakedNodePositions_ = &nodePositions;
    UploadVertices();
}

void FbxParts::Animate(Transform& transform, const ufbx_scene* evaluated) {
    Evaluate(evaluated);
    Draw(transform);
}
void FbxParts::Draw(Transform& transform) {
    if (!vertexBuffer_ || !constantBuffer_ || !Direct3D::pContext_) {
        return;
    }
    UINT stride = sizeof(VERTEX);
    UINT offset = 0;
    Direct3D::pContext_->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
    Direct3D::pContext_->VSSetConstantBuffers(0, 1, constantBuffer_.GetAddressOf());
    Direct3D::pContext_->PSSetConstantBuffers(0, 1, constantBuffer_.GetAddressOf());
    const XMMATRIX worldMatrix = transform.GetWorldMatrix();
    const XMMATRIX viewMatrix = Camera::GetViewMatrix();
    const XMMATRIX projectionMatrix = Camera::GetProjectionMatrix();
    const XMMATRIX worldViewProjection = worldMatrix * viewMatrix * projectionMatrix;

    for (MATERIAL& material : materials_) {
        if (!material.buffer) {
            continue;
        }
        Direct3D::pContext_->IASetIndexBuffer(material.buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        CONSTANT_BUFFER constants{};
        constants.worldVewProj = XMMatrixTranspose(worldViewProjection);
        constants.world = XMMatrixTranspose(worldMatrix);
        constants.normalTrans = XMMatrixInverse(nullptr, worldMatrix);
        constants.ambient = material.ambient;
        constants.diffuse = material.diffuse;
        constants.speculer = material.specular;
        constants.shininess = material.shininess;
        const XMFLOAT3 camera = Camera::GetPosition();
        constants.cameraPosition = XMFLOAT4(camera.x, camera.y, camera.z, 0);
        constants.lightDirection = XMFLOAT4(1, -1, 1, 0);
        constants.isTexture = material.texture && material.texture->GetSRV();
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(Direct3D::pContext_->Map(constantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, &constants, sizeof(constants));
            Direct3D::pContext_->Unmap(constantBuffer_.Get(), 0);
        }
        ID3D11SamplerState* sampler = nullptr;
        ID3D11ShaderResourceView* textureView = nullptr;
        if (material.texture) {
            sampler = material.texture->GetSampler();
            textureView = material.texture->GetSRV();
        }
        Direct3D::pContext_->PSSetSamplers(0, 1, &sampler);
        Direct3D::pContext_->PSSetShaderResources(0, 1, &textureView);
        Direct3D::pContext_->DrawIndexed((UINT)material.indices.size(), 0, 0);
    }
}
bool FbxParts::GetBonePosition(const std::string& name, XMFLOAT3* position) const {
    if (!currentNode_ || !currentNode_->mesh || !position) {
        return false;
    }
    for (size_t skinIndex = 0; skinIndex < currentNode_->mesh->skin_deformers.count; ++skinIndex) {
        const ufbx_skin_deformer* skin = currentNode_->mesh->skin_deformers.data[skinIndex];
        for (size_t clusterIndex = 0; clusterIndex < skin->clusters.count; ++clusterIndex) {
            const ufbx_node* bone = skin->clusters.data[clusterIndex]->bone_node;
            if (bone && name == std::string(bone->name.data, bone->name.length)) {
                if (bakedNodePositions_ && bone->typed_id < bakedNodePositions_->size()) {
                    *position = (*bakedNodePositions_)[bone->typed_id];
                    return true;
                }
                *position = ToFloat3(ufbx_vec3{ bone->node_to_world.m03,bone->node_to_world.m13,bone->node_to_world.m23 });
                return true;
            }
        }
    }
    return false;
}
void FbxParts::RayCast(RayCastData* data) {
    if (data == nullptr) {
        return;
    }
    for (const MATERIAL& material : materials_) {
        // インデックス3個で1枚の三角形になる。
        for (size_t index = 0; index + 2 < material.indices.size(); index += 3) {
            XMFLOAT3& positionA = vertices_[material.indices[index]].position;
            XMFLOAT3& positionB = vertices_[material.indices[index + 1]].position;
            XMFLOAT3& positionC = vertices_[material.indices[index + 2]].position;

            float distance = 0;
            const bool intersects = Direct3D::Intersect(data->start, data->dir,
            positionA, positionB, positionC, &distance);
            if (!intersects) {
                continue;
            }
            if (!(distance < data->dist)) {
                continue;
            }

            data->hit = TRUE;
            data->dist = distance;
            const XMVECTOR pointA = XMLoadFloat3(&positionA);
            const XMVECTOR pointB = XMLoadFloat3(&positionB);
            const XMVECTOR pointC = XMLoadFloat3(&positionC);
            const XMVECTOR edgeAB = pointB - pointA;
            const XMVECTOR edgeAC = pointC - pointA;
            const XMVECTOR normal = XMVector3Normalize(XMVector3Cross(edgeAB, edgeAC));
            XMStoreFloat3(&data->normal, normal);
        }
    }
}

