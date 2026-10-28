#include "TestScene.h"
#include "Engine/Model.h"
#include "Engine/Camera.h"

namespace {
    const int ModelColumns = 50;
    const int ModelRows = 50;
    const float ModelSpacing = 1.0f;
    const float CameraMoveSeconds = 30.0f;
    const float CameraEndDistanceRatio = 0.15f;
    const char* ModelFilename = "skg.fbx";
}

TestScene::TestScene(GameObject* parent)
    : GameObject(parent, "TestScene")
{
}

void TestScene::Initialize()
{
    Release();

    const float halfWidth = (ModelColumns - 1) * ModelSpacing * 0.5f;
    const float halfDepth = (ModelRows - 1) * ModelSpacing * 0.5f;

    for (int row = 0; row < ModelRows; ++row) {
        for (int column = 0; column < ModelColumns; ++column) {
            // �n���h���Ɣz�u�͌ʁBFBX�Ɖ摜�͓ǂݍ��ݍς݃f�[�^����L����B
            const int handle = Model::Load(ModelFilename);
            if (handle < 0) {
                OutputDebugStringA("Crowd test: model load failed.\n");
                Release();
                return;
            }

            Transform transform;
            transform.position_.x = column * ModelSpacing - halfWidth;
            transform.position_.z = row * ModelSpacing - halfDepth;
            Model::SetTransform(handle, transform);

            // �S���𓯂������ōĐ����A���ʂ̎p����g���đS����`�悷��B
            Model::SetAnimFrame(handle, 0, 152, 1.0f);
            modelHandles_.push_back(handle);
        }
    }

    Camera::SetPosition(XMFLOAT3(0.0f, 10.0f, -30.0f));
    Camera::SetTarget(XMFLOAT3(0.0f, 1.0f, 0.0f));
    Camera::Update();
    cameraStartPosition_ = Camera::GetPosition();
    cameraTarget_ = Camera::GetTarget();
    cameraStartTime_ = GetTickCount64();
}

void TestScene::Update()
{
    if (modelHandles_.empty()) {
        return;
    }

    // �o�ߕb���Ői�߂�̂ŁA�`��FPS�ɂ���ăJ�����̑����͕ς��Ȃ��B
    const ULONGLONG elapsedMilliseconds = GetTickCount64() - cameraStartTime_;
    const float elapsedSeconds = static_cast<float>(elapsedMilliseconds) / 1000.0f;
    float progress = elapsedSeconds / CameraMoveSeconds;
    if (progress > 1.0f) {
        progress = 1.0f;
    }

    // �J�n�ʒu���璍���_�ւ̒������ړ����A��O�Ŏ~�߂�B
    const float distanceRatio = 1.0f - progress * (1.0f - CameraEndDistanceRatio);
    XMFLOAT3 position;
    position.x = cameraTarget_.x + (cameraStartPosition_.x - cameraTarget_.x) * distanceRatio;
    position.y = cameraTarget_.y + (cameraStartPosition_.y - cameraTarget_.y) * distanceRatio;
    position.z = cameraTarget_.z + (cameraStartPosition_.z - cameraTarget_.z) * distanceRatio;
    Camera::SetPosition(position);
}

void TestScene::Draw()
{
    for (int handle : modelHandles_) {
        Model::Draw(handle);
    }
}

void TestScene::Release()
{
    for (int handle : modelHandles_) {
        Model::Release(handle);
    }
    modelHandles_.clear();
}
