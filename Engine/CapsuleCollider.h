#pragma once
#include "Collider.h"

// Y軸固定のカプセル。回転・拡縮は判定へ適用しない。
class CapsuleCollider : public Collider
{
    friend class Collider;

  public:
    // heightは半球を含めた全高。height >= radius * 2 が必要。
    CapsuleCollider(XMFLOAT3 center, float radius, float height);
    ~CapsuleCollider() override;
    void Draw(XMFLOAT3 position) override;

  private:
    bool IsHit(Collider *target) override;
    float radius_;
    float halfSegment_;
    int topModel_ = -1;
    int middleModel_ = -1;
    int bottomModel_ = -1;
};
