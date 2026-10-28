#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <memory>
#include "Transform.h"
#include "ufbx.h"
using namespace DirectX;
#include "FbxParts.h"
struct RayCastData {
 XMFLOAT3 start{}, dir{};
 float dist = 99999.0f;
 BOOL hit = FALSE;
 XMFLOAT3 normal{};
};
class Fbx {
 std::vector<std::unique_ptr<FbxParts>> parts_;
 ufbx_scene* scene_ = nullptr;
 ufbx_scene* evaluated_ = nullptr;
 int evaluatedFrame_ = 0;
 bool hasEvaluatedFrame_ = false;
 double fps_ = 30.0;
 const ufbx_anim* selectedAnim_ = nullptr;
 struct BakedFrame {
  std::vector<FbxParts::VertexSnapshot> parts;
  std::vector<XMFLOAT3> nodePositions;
 };
 struct BakedAnimation {
  const ufbx_anim* selection = nullptr;
  int startFrame = 0;
  int endFrame = 0;
  std::vector<BakedFrame> frames;
 };
 std::vector<std::unique_ptr<BakedAnimation>> bakedAnimations_;
 const BakedFrame* currentBakedFrame_ = nullptr;
 HRESULT CheckNode(ufbx_node* node, const std::string& directory);
public:
 Fbx();
 ~Fbx();
 virtual HRESULT Load(std::string fileName);
 void Draw(Transform& transform, int frame);
 bool Evaluate(int frame);
 bool BakeAnimation(int startFrame, int endFrame);
 bool SetAnimation(const std::string& name);
 void Release();
 XMFLOAT3 GetBonePosition(std::string boneName);
 void RayCast(RayCastData* data);
};


