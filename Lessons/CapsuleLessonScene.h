#pragma once
#include "../Engine/GameObject.h"

// SceneManagerからInstantiateして使う、カプセル教材用の確認シーン。
class CapsuleLessonScene : public GameObject
{
  public:
    explicit CapsuleLessonScene(GameObject *parent);
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Release() override;
};
