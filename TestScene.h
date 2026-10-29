#pragma once
#include "Engine/GameObject.h"
#include <vector>

class TestScene : public GameObject
{
  public:
    TestScene(GameObject *parent);
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Release() override;

  private:
    std::vector<int> modelHandles_;
    XMFLOAT3 cameraStartPosition_;
    XMFLOAT3 cameraTarget_;
    ULONGLONG cameraStartTime_ = 0;
};
