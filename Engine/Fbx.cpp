#include "Fbx.h"
#include "FbxParts.h"
#include "Direct3D.h"
namespace {
// Some exporters prepend an empty/static take. Keep the first moving take,
// falling back to the original first take for static models.
const ufbx_anim* SelectAnimation(const ufbx_scene* scene) {
 for (const auto* stack : scene->anim_stacks) {
  for (const auto* layer : stack->layers) {
   for (const auto* value : layer->anim_values) {
    for (const auto* curve : value->curves) {
     if (!curve) continue;
     for (size_t k = 1; k < curve->keyframes.count; ++k) {
      const auto& a = curve->keyframes.data[k - 1];
      const auto& b = curve->keyframes.data[k];
      if (b.time <= a.time) continue;
      if (a.value != b.value || (a.interpolation == UFBX_INTERPOLATION_CUBIC &&
          (a.right.dy != 0.0f || b.left.dy != 0.0f))) return stack->anim;
     }
    }
   }
  }
 }
 return scene->anim_stacks.count ? scene->anim_stacks.data[0]->anim : scene->anim;
}
}
Fbx::~Fbx() { Release(); }
void Fbx::Release() {
 for (auto* part : parts_) delete part;
 parts_.clear();
 if (evaluated_) { ufbx_free_scene(evaluated_); evaluated_ = nullptr; }
 if (scene_) { ufbx_free_scene(scene_); scene_ = nullptr; }
 hasEvaluatedFrame_ = false;
 selectedAnim_ = nullptr;
}
HRESULT Fbx::Load(std::string filename) {
 Release();
 ufbx_load_opts opts{};
 // Match the engine's LookAtLH/PerspectiveFovLH coordinate system.
 // Let ufbx convert geometry, normals, bones, and animation together.
 opts.target_axes = ufbx_axes_left_handed_y_up;
 opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
 opts.space_conversion = UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS;
 opts.generate_missing_normals = true;
 opts.evaluate_skinning = true;
 opts.ignore_missing_external_files = true;
 ufbx_error error{};
 scene_ = ufbx_load_file(filename.c_str(), &opts, &error);
 if (!scene_) {
  char message[2048]; ufbx_format_error(message, sizeof(message), &error);
  OutputDebugStringA(message);
  return E_FAIL;
 }
 fps_ = scene_->settings.frames_per_second > 0.0 ? scene_->settings.frames_per_second : 30.0;
 const auto end = filename.find_last_of("/\\");
 const std::string dir = end == std::string::npos ? std::string() : filename.substr(0, end + 1);
 HRESULT hr = CheckNode(scene_->root_node, dir);
 if (FAILED(hr) || parts_.empty() || !Evaluate(0)) {
  Release();
  return FAILED(hr) ? hr : E_FAIL;
 }
 return S_OK;
}
HRESULT Fbx::CheckNode(ufbx_node* node, const std::string& dir) {
 if (!node) return S_OK;
 if (node->mesh && node->mesh->num_triangles) {
  auto* part = new FbxParts();
  HRESULT hr = part->Init(node, dir);
  if (FAILED(hr)) { delete part; return hr; }
  parts_.push_back(part);
 }
 for (size_t i = 0; i < node->children.count; ++i) {
  HRESULT hr = CheckNode(node->children.data[i], dir);
  if (FAILED(hr)) return hr;
 }
 return S_OK;
}
bool Fbx::Evaluate(int frame) {
 if (!scene_) return false;
 if (hasEvaluatedFrame_ && evaluatedFrame_ == frame) return true;
 // Frame numbers remain absolute FBX timeline frames, as in the original API.
 const ufbx_anim* anim = selectedAnim_ ? selectedAnim_ : SelectAnimation(scene_);
 ufbx_evaluate_opts opts{}; opts.evaluate_skinning = true;
 ufbx_scene* next = ufbx_evaluate_scene(scene_, anim, static_cast<double>(frame) / fps_, &opts, nullptr);
 if (!next) return false;
 if (evaluated_) ufbx_free_scene(evaluated_);
 evaluated_ = next;
 evaluatedFrame_ = frame;
 hasEvaluatedFrame_ = true;
 for (auto* part : parts_) part->Evaluate(evaluated_);
 return true;
}
void Fbx::Draw(Transform& transform, int frame) {
 if (!Evaluate(frame)) return;
 Direct3D::SetBlendMode(Direct3D::BLEND_DEFAULT);
 for (auto* part : parts_) part->Draw(transform);
}
XMFLOAT3 Fbx::GetBonePosition(std::string boneName) {
 const ufbx_scene* current = evaluated_ ? evaluated_ : scene_;
 if (current) for (size_t i = 0; i < current->nodes.count; ++i) {
  const auto* node = current->nodes.data[i];
  if (boneName == std::string(node->name.data, node->name.length))
   return XMFLOAT3((float)node->node_to_world.m03, (float)node->node_to_world.m13, (float)node->node_to_world.m23);
 }
 return XMFLOAT3(0, 0, 0);
}
void Fbx::RayCast(RayCastData* data) {
 if (!data) return;
 data->hit = FALSE;
 for (auto* part : parts_) part->RayCast(data);
}



bool Fbx::SetAnimation(const std::string& name) {
 if (!scene_) return false;
 const ufbx_anim* next = nullptr;
 if (!name.empty()) {
  for (const auto* stack : scene_->anim_stacks)
   if (name == std::string(stack->name.data, stack->name.length)) { next = stack->anim; break; }
  if (!next) return false;
 }
 if (selectedAnim_ != next) { selectedAnim_ = next; hasEvaluatedFrame_ = false; }
 return true;
}
