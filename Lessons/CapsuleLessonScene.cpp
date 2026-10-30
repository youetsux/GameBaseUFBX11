#include "CapsuleLessonScene.h"
#include "../Engine/BoxCollider.h"
#include "../Engine/Camera.h"
#include "../Engine/CapsuleCollider.h"
#include "../Engine/Image.h"
#include "../Engine/Input.h"
#include "../Engine/SphereCollider.h"

namespace
{
class LessonActor : public GameObject
{
  public:
    explicit LessonActor(GameObject *parent) : GameObject(parent)
    {
    }
    bool movable = false;
    bool IsHitting() const
    {
        return hit_;
    }
    void Initialize() override
    {
        lastTime_ = GetTickCount64();
    }
    void Update() override
    {
        hit_ = false;
        const ULONGLONG now = GetTickCount64();
        float seconds = static_cast<float>(now - lastTime_) / 1000.0f;
        lastTime_ = now;
        if (seconds > 0.1f)
        {
            seconds = 0.1f;
        }
        if (!movable)
        {
            return;
        }
        XMFLOAT3 position = GetPosition();
        const float distance = 3.0f * seconds;
        if (Input::IsKey(DIK_LEFT))
        {
            position.x -= distance;
        }
        if (Input::IsKey(DIK_RIGHT))
        {
            position.x += distance;
        }
        if (Input::IsKey(DIK_UP))
        {
            position.z += distance;
        }
        if (Input::IsKey(DIK_DOWN))
        {
            position.z -= distance;
        }
        if (Input::IsKey(DIK_Q))
        {
            position.y += distance;
        }
        if (Input::IsKey(DIK_E))
        {
            position.y -= distance;
        }
        SetPosition(position);
    }
    void OnCollision(GameObject *) override
    {
        hit_ = true;
    }
    void Draw() override
    {
        if (movable && hit_ != reportedHit_)
        {
            if (hit_)
            {
                OutputDebugStringA("Capsule lesson: ENTER collision\n");
            }
            else
            {
                OutputDebugStringA("Capsule lesson: EXIT collision\n");
            }
            reportedHit_ = hit_;
        }
    }
    void Release() override
    {
    }

  private:
    ULONGLONG lastTime_ = 0;
    bool hit_ = false;
    bool reportedHit_ = false;
};
// 子の最後に登録し、3Dの枠をすべて描いた後で画面表示を行う。
class HitDisplay : public GameObject
{
  public:
    explicit HitDisplay(GameObject *parent) : GameObject(parent)
    {
    }
    LessonActor *player = nullptr; // 参照のみ。プレイヤーは同じシーンが所有する。
    void Initialize() override
    {
        imageHandle_ = Image::Load("Hit.png");
        if (imageHandle_ < 0)
        {
            OutputDebugStringA("Capsule lesson: Hit.png could not be loaded.\n");
            return;
        }
        Transform transform;
        // Imageの位置は画面中央0、左右上下がおおむね-1～1。
        transform.position_ = XMFLOAT3(0.0f, 0.8f, 0.0f);
        Image::SetTransform(imageHandle_, transform);
    }
    void Update() override
    {
    }
    void Draw() override
    {
        if (player != nullptr && player->IsHitting() && imageHandle_ >= 0)
        {
            Image::Draw(imageHandle_);
        }
    }
    void Release() override
    {
        Image::Release(imageHandle_);
        imageHandle_ = -1;
    }

  private:
    int imageHandle_ = -1;
};
} // namespace

CapsuleLessonScene::CapsuleLessonScene(GameObject *parent) : GameObject(parent, "CapsuleLesson")
{
}
void CapsuleLessonScene::Initialize()
{
    LessonActor *player = Instantiate<LessonActor>(this);
    player->movable = true;
    player->SetPosition(-6, 2, 0);
    player->AddCollider(new CapsuleCollider(XMFLOAT3(0, 0, 0), 0.5f, 3.0f));

    LessonActor *box = Instantiate<LessonActor>(this);
    box->SetPosition(-2, 2, 0);
    box->AddCollider(new BoxCollider(XMFLOAT3(0, 0, 0), XMFLOAT3(2, 2, 2)));
    LessonActor *sphere = Instantiate<LessonActor>(this);
    sphere->SetPosition(2, 2, 0);
    sphere->AddCollider(new SphereCollider(XMFLOAT3(0, 0, 0), 1.0f));
    LessonActor *capsule = Instantiate<LessonActor>(this);
    capsule->SetPosition(6, 2, 0);
    capsule->AddCollider(new CapsuleCollider(XMFLOAT3(0, 0, 0), 0.75f, 4.0f));
    HitDisplay *hitDisplay = Instantiate<HitDisplay>(this);
    hitDisplay->player = player;

    Camera::SetPosition(XMFLOAT3(0, 8, -18));
    Camera::SetTarget(XMFLOAT3(0, 2, 0));
    Camera::Update();
}
void CapsuleLessonScene::Update()
{
}
void CapsuleLessonScene::Draw()
{
}
void CapsuleLessonScene::Release()
{
}
