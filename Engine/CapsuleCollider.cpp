#include "CapsuleCollider.h"
#include "BoxCollider.h"
#include "SphereCollider.h"
#include "Model.h"
#include <cassert>

CapsuleCollider::CapsuleCollider(XMFLOAT3 center, float radius, float height)
{
    assert(radius > 0.0f && height >= radius * 2.0f);
    center_ = center;
    radius_ = radius;
    halfSegment_ = (height - radius * 2.0f) * 0.5f;
    type_ = COLLIDER_CAPSULE;
#ifdef _DEBUG
    topModel_ = Model::Load("DebugCollision/CapsuleTop.fbx");
    middleModel_ = Model::Load("DebugCollision/CapsuleMiddle.fbx");
    bottomModel_ = Model::Load("DebugCollision/CapsuleBottom.fbx");
#endif
}

CapsuleCollider::~CapsuleCollider()
{
    Model::Release(topModel_);
    Model::Release(middleModel_);
    Model::Release(bottomModel_);
}

bool CapsuleCollider::IsHit(Collider *target)
{
    if (target == nullptr)
    {
        return false;
    }
    if (target->type_ == COLLIDER_BOX)
    {
        return IsHitBoxVsCapsule(static_cast<BoxCollider *>(target), this);
    }
    if (target->type_ == COLLIDER_CIRCLE)
    {
        return IsHitSphereVsCapsule(static_cast<SphereCollider *>(target), this);
    }
    if (target->type_ == COLLIDER_CAPSULE)
    {
        return IsHitCapsuleVsCapsule(this, static_cast<CapsuleCollider *>(target));
    }
    return false;
}

void CapsuleCollider::Draw(XMFLOAT3 position)
{
#ifdef _DEBUG
    const XMFLOAT3 center(position.x + center_.x, position.y + center_.y, position.z + center_.z);
    Transform transform;
    transform.scale_ = XMFLOAT3(radius_, radius_, radius_);
    transform.position_ = XMFLOAT3(center.x, center.y + halfSegment_, center.z);
    Model::SetTransform(topModel_, transform);
    Model::Draw(topModel_);
    transform.position_.y = center.y - halfSegment_;
    Model::SetTransform(bottomModel_, transform);
    Model::Draw(bottomModel_);
    // 円柱部分だけを伸ばすので、上下の半球の半径は変わらない。
    if (halfSegment_ > 0.0f)
    {
        transform.position_ = center;
        transform.scale_ = XMFLOAT3(radius_, halfSegment_ * 2.0f, radius_);
        Model::SetTransform(middleModel_, transform);
        Model::Draw(middleModel_);
    }
#endif
}
