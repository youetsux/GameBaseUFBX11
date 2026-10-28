#include "Fbx.h"
#include "FbxParts.h"
#include "Direct3D.h"
#include <filesystem>
namespace {
    bool HasChangingKeys(const ufbx_anim_curve* curve) {
        if (curve == nullptr) {
            return false;
        }

        for (size_t keyIndex = 1; keyIndex < curve->keyframes.count; ++keyIndex) {
            const ufbx_keyframe& previousKey = curve->keyframes.data[keyIndex - 1];
            const ufbx_keyframe& nextKey = curve->keyframes.data[keyIndex];
            if (nextKey.time <= previousKey.time) {
                continue;
            }
            if (previousKey.value != nextKey.value) {
                return true;
            }

            // 値が同じキーでも、3次補間の接線によって途中で動く場合がある。
            if (previousKey.interpolation == UFBX_INTERPOLATION_CUBIC) {
                if (previousKey.right.dy != 0.0f || nextKey.left.dy != 0.0f) {
                    return true;
                }
            }
        }
        return false;
    }

    bool HasAnimation(const ufbx_anim_stack* stack) {
        for (const ufbx_anim_layer* layer : stack->layers) {
            for (const ufbx_anim_value* value : layer->anim_values) {
                for (const ufbx_anim_curve* curve : value->curves) {
                    if (HasChangingKeys(curve)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    const ufbx_anim* SelectAnimation(const ufbx_scene* scene) {
        // 空のテイクを先頭に出力するFBXもあるため、動きのあるものを探す。
        for (const ufbx_anim_stack* stack : scene->anim_stacks) {
            if (HasAnimation(stack)) {
                return stack->anim;
            }
        }
        if (scene->anim_stacks.count > 0) {
            return scene->anim_stacks.data[0]->anim;
        }
        return scene->anim;
    }
}
Fbx::Fbx() = default;

Fbx::~Fbx() {
    Release();
}
void Fbx::Release() {
    parts_.clear();
    currentBakedFrame_ = nullptr;
    bakedAnimations_.clear();
    if (evaluated_) {
        ufbx_free_scene(evaluated_);
        evaluated_ = nullptr;
    }
    if (scene_) {
        ufbx_free_scene(scene_);
        scene_ = nullptr;
    }
    hasEvaluatedFrame_ = false;
    selectedAnim_ = nullptr;
}
HRESULT Fbx::Load(std::string filename) {
    Release();
    ufbx_load_opts options{};
    // Match the engine's LookAtLH/PerspectiveFovLH coordinate system.
    // Let ufbx convert geometry, normals, bones, and animation together.
    options.target_axes = ufbx_axes_left_handed_y_up;
    options.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
    options.space_conversion = UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS;
    options.generate_missing_normals = true;
    options.evaluate_skinning = true;
    options.ignore_missing_external_files = true;
    ufbx_error error{};
    scene_ = ufbx_load_file(filename.c_str(), &options, &error);
    if (!scene_) {
        char message[2048];
        ufbx_format_error(message, sizeof(message), &error);
        OutputDebugStringA(message);
        return E_FAIL;
    }
    fps_ = scene_->settings.frames_per_second;
    if (fps_ <= 0.0) {
        fps_ = 30.0;
    }
    const std::string modelDirectory = std::filesystem::path(filename).parent_path().string();
    HRESULT result = CheckNode(scene_->root_node, modelDirectory);
    if (FAILED(result) || parts_.empty() || !Evaluate(0)) {
        Release();
        if (FAILED(result)) {
            return result;
        }
        return E_FAIL;
    }
    return S_OK;
}
HRESULT Fbx::CheckNode(ufbx_node* node, const std::string& modelDirectory) {
    if (!node) {
        return S_OK;
    }
    if (node->mesh && node->mesh->num_triangles) {
        std::unique_ptr<FbxParts> part = std::make_unique<FbxParts>();
        HRESULT result = part->Init(node, modelDirectory);
        if (FAILED(result)) {
            return result;
        }
        parts_.push_back(std::move(part));
    }
    for (ufbx_node* child : node->children) {
        HRESULT result = CheckNode(child, modelDirectory);
        if (FAILED(result)) {
            return result;
        }
    }
    return S_OK;
}
bool Fbx::Evaluate(int frame) {
    if (!scene_) {
        return false;
    }
    if (hasEvaluatedFrame_ && evaluatedFrame_ == frame) {
        return true;
    }
    for (const std::unique_ptr<BakedAnimation>& animation : bakedAnimations_) {
        if (animation->selection != selectedAnim_ ||
            frame < animation->startFrame || frame > animation->endFrame) {
            continue;
        }
        const size_t frameIndex = static_cast<size_t>(
            static_cast<int64_t>(frame) - animation->startFrame);
        currentBakedFrame_ = &animation->frames[frameIndex];
        for (size_t partIndex = 0; partIndex < parts_.size(); ++partIndex) {
            parts_[partIndex]->ApplyVertices(currentBakedFrame_->parts[partIndex],
                currentBakedFrame_->nodePositions);
        }
        evaluatedFrame_ = frame;
        hasEvaluatedFrame_ = true;
        return true;
    }
    // Frame numbers remain absolute FBX timeline frames, as in the original API.
    const ufbx_anim* anim = selectedAnim_;
    if (anim == nullptr) {
        anim = SelectAnimation(scene_);
    }
    ufbx_evaluate_opts options{};
    options.evaluate_skinning = true;
    const double timeInSeconds = static_cast<double>(frame) / fps_;
    ufbx_scene* next = ufbx_evaluate_scene(scene_, anim, timeInSeconds, &options, nullptr);
    if (!next) {
        return false;
    }
    if (evaluated_) {
        ufbx_free_scene(evaluated_);
    }
    evaluated_ = next;
    currentBakedFrame_ = nullptr;
    evaluatedFrame_ = frame;
    hasEvaluatedFrame_ = true;
    for (const std::unique_ptr<FbxParts>& part : parts_) {
        part->Evaluate(evaluated_);
    }
    return true;
}
bool Fbx::BakeAnimation(int startFrame, int endFrame) {
    if (!scene_ || endFrame < startFrame) {
        return false;
    }
    for (const std::unique_ptr<BakedAnimation>& cached : bakedAnimations_) {
        if (cached->selection == selectedAnim_ && cached->startFrame == startFrame &&
            cached->endFrame == endFrame) {
            return true;
        }
    }
    const int restoreFrame = evaluatedFrame_;
    const ufbx_anim* animation = selectedAnim_;
    if (!animation) {
        animation = SelectAnimation(scene_);
    }
    std::unique_ptr<BakedAnimation> baked = std::make_unique<BakedAnimation>();
    baked->selection = selectedAnim_;
    baked->startFrame = startFrame;
    baked->endFrame = endFrame;
    const size_t frameCount = static_cast<size_t>(
        static_cast<int64_t>(endFrame) - startFrame + 1);
    baked->frames.resize(frameCount);
    for (size_t frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        const int frame = static_cast<int>(static_cast<int64_t>(startFrame) + frameIndex);
        ufbx_evaluate_opts options{};
        options.evaluate_skinning = true;
        const double time = static_cast<double>(frame) / fps_;
        std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> evaluated(
            ufbx_evaluate_scene(scene_, animation, time, &options, nullptr), ufbx_free_scene);
        if (!evaluated) {
            hasEvaluatedFrame_ = false;
            Evaluate(restoreFrame);
            return false;
        }
        BakedFrame& snapshot = baked->frames[frameIndex];
        snapshot.parts.resize(parts_.size());
        for (size_t partIndex = 0; partIndex < parts_.size(); ++partIndex) {
            if (!parts_[partIndex]->CaptureVertices(evaluated.get(), &snapshot.parts[partIndex])) {
                hasEvaluatedFrame_ = false;
                Evaluate(restoreFrame);
                return false;
            }
        }
        snapshot.nodePositions.resize(scene_->nodes.count);
        for (const ufbx_node* node : evaluated->nodes) {
            snapshot.nodePositions[node->typed_id] = XMFLOAT3(
                static_cast<float>(node->node_to_world.m03),
                static_cast<float>(node->node_to_world.m13),
                static_cast<float>(node->node_to_world.m23));
        }
    }
    bakedAnimations_.push_back(std::move(baked));
    hasEvaluatedFrame_ = false;
    return Evaluate(restoreFrame);
}

void Fbx::Draw(Transform& transform, int frame) {
    if (!Evaluate(frame)) {
        return;
    }
    Direct3D::SetBlendMode(Direct3D::BLEND_DEFAULT);
    for (const std::unique_ptr<FbxParts>& part : parts_) {
        part->Draw(transform);
    }
}
XMFLOAT3 Fbx::GetBonePosition(std::string boneName) {
    if (currentBakedFrame_ && scene_) {
        for (const ufbx_node* node : scene_->nodes) {
            if (boneName == std::string(node->name.data, node->name.length)) {
                return currentBakedFrame_->nodePositions[node->typed_id];
            }
        }
        return XMFLOAT3(0, 0, 0);
    }
    const ufbx_scene* current = scene_;
    if (evaluated_ != nullptr) {
        current = evaluated_;
    }
    if (current == nullptr) {
        return XMFLOAT3(0, 0, 0);
    }

    // 従来のAPIと同じく、最初に見つかった同名ノードの位置を返す。
    for (const ufbx_node* node : current->nodes) {
        const std::string nodeName(node->name.data, node->name.length);
        if (boneName == nodeName) {
            return XMFLOAT3((float)node->node_to_world.m03,
            (float)node->node_to_world.m13,
            (float)node->node_to_world.m23);
        }
    }
    return XMFLOAT3(0, 0, 0);
}
void Fbx::RayCast(RayCastData* data) {
    if (!data) {
        return;
    }
    data->hit = FALSE;
    for (const std::unique_ptr<FbxParts>& part : parts_) {
        part->RayCast(data);
    }
}



bool Fbx::SetAnimation(const std::string& name) {
    if (scene_ == nullptr) {
        return false;
    }

    // 空文字は自動選択。存在しない名前なら現在の選択を変えない。
    const ufbx_anim* next = nullptr;
    if (!name.empty()) {
        for (const ufbx_anim_stack* stack : scene_->anim_stacks) {
            const std::string stackName(stack->name.data, stack->name.length);
            if (name == stackName) {
                next = stack->anim;
                break;
            }
        }
        if (next == nullptr) {
            return false;
        }
    }
    if (selectedAnim_ != next) {
        selectedAnim_ = next;
        hasEvaluatedFrame_ = false;
    }
    return true;
}


