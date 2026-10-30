#include "BoxCollider.h"
#include "CapsuleCollider.h"
#include "SphereCollider.h"
#include "GameObject.h"
#include "Model.h"
#include "Transform.h"

// コンストラクタ
Collider::Collider() : pGameObject_(nullptr), hDebugModel_(-1)
{
}

// デストラクタ
Collider::~Collider()
{
}

// 箱型同士の衝突判定
// 引数：boxA	１つ目の箱型判定
// 引数：boxB	２つ目の箱型判定
// 戻値：接触していればtrue
bool Collider::IsHitBoxVsBox(BoxCollider *boxA, BoxCollider *boxB)
{

    XMFLOAT3 boxPosA = Transform::Float3Add(boxA->pGameObject_->GetWorldPosition(), boxA->center_);
    XMFLOAT3 boxPosB = Transform::Float3Add(boxB->pGameObject_->GetWorldPosition(), boxB->center_);

    if ((boxPosA.x + boxA->size_.x / 2) > (boxPosB.x - boxB->size_.x / 2) &&
        (boxPosA.x - boxA->size_.x / 2) < (boxPosB.x + boxB->size_.x / 2) &&
        (boxPosA.y + boxA->size_.y / 2) > (boxPosB.y - boxB->size_.y / 2) &&
        (boxPosA.y - boxA->size_.y / 2) < (boxPosB.y + boxB->size_.y / 2) &&
        (boxPosA.z + boxA->size_.z / 2) > (boxPosB.z - boxB->size_.z / 2) &&
        (boxPosA.z - boxA->size_.z / 2) < (boxPosB.z + boxB->size_.z / 2))
    {
        return true;
    }
    return false;
}

// 箱型と球体の衝突判定
// 引数：box	箱型判定
// 引数：sphere	球体判定
// 戻値：接触していればtrue
bool Collider::IsHitBoxVsCircle(BoxCollider *box, SphereCollider *sphere)
{
    const XMFLOAT3 spherePosition =
        Transform::Float3Add(sphere->pGameObject_->GetWorldPosition(), sphere->center_);
    const XMFLOAT3 boxPosition =
        Transform::Float3Add(box->pGameObject_->GetWorldPosition(), box->center_);

    // size_は箱の全幅・全高・全奥行きなので、中心からの距離はその半分。
    const XMFLOAT3 halfSize(box->size_.x * 0.5f, box->size_.y * 0.5f, box->size_.z * 0.5f);
    const XMFLOAT3 minimum(boxPosition.x - halfSize.x, boxPosition.y - halfSize.y,
                           boxPosition.z - halfSize.z);
    const XMFLOAT3 maximum(boxPosition.x + halfSize.x, boxPosition.y + halfSize.y,
                           boxPosition.z + halfSize.z);

    // 球の中心を箱の範囲へ収めると、箱の内側または表面にある最近点になる。
    XMFLOAT3 closestPoint = spherePosition;
    if (closestPoint.x < minimum.x)
    {
        closestPoint.x = minimum.x;
    }
    else if (closestPoint.x > maximum.x)
    {
        closestPoint.x = maximum.x;
    }
    if (closestPoint.y < minimum.y)
    {
        closestPoint.y = minimum.y;
    }
    else if (closestPoint.y > maximum.y)
    {
        closestPoint.y = maximum.y;
    }
    if (closestPoint.z < minimum.z)
    {
        closestPoint.z = minimum.z;
    }
    else if (closestPoint.z > maximum.z)
    {
        closestPoint.z = maximum.z;
    }

    // 平方根は不要。中心が箱の内側なら距離は0になり、接触も衝突として扱う。
    const float distanceX = spherePosition.x - closestPoint.x;
    const float distanceY = spherePosition.y - closestPoint.y;
    const float distanceZ = spherePosition.z - closestPoint.z;
    const float distanceSquared =
        distanceX * distanceX + distanceY * distanceY + distanceZ * distanceZ;
    const float radius = sphere->size_.x;
    return distanceSquared <= radius * radius;
}

// 球体同士の衝突判定
// 引数：circleA	１つ目の球体判定
// 引数：circleB	２つ目の球体判定
// 戻値：接触していればtrue
bool Collider::IsHitCircleVsCircle(SphereCollider *circleA, SphereCollider *circleB)
{
    XMFLOAT3 centerA = circleA->center_;
    XMFLOAT3 positionA = circleA->pGameObject_->GetWorldPosition();
    XMFLOAT3 centerB = circleB->center_;
    XMFLOAT3 positionB = circleB->pGameObject_->GetWorldPosition();

    XMVECTOR v = (XMLoadFloat3(&centerA) + XMLoadFloat3(&positionA)) -
                 (XMLoadFloat3(&centerB) + XMLoadFloat3(&positionB));

    if (XMVector3Length(v).m128_f32[0] <= circleA->size_.x + circleB->size_.x)
    {
        return true;
    }

    return false;
}

// テスト表示用の枠を描画
// 引数：position	オブジェクトの位置
void Collider::Draw(XMFLOAT3 position)
{
    Transform transform;
    transform.position_ =
        XMFLOAT3(position.x + center_.x, position.y + center_.y, position.z + center_.z);
    transform.scale_ = size_;
    transform.Calclation();
    Model::SetTransform(hDebugModel_, transform);
    Model::Draw(hDebugModel_);
}

namespace
{
// 数値を区間内へ収める。球の中心に最も近い線分上の位置を求める。
float ClampToInterval(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

// 重なる区間の距離は0。離れている場合は、近い端点同士の距離になる。
float IntervalGap(float firstMin, float firstMax, float secondMin, float secondMax)
{
    if (firstMax < secondMin)
    {
        return secondMin - firstMax;
    }
    if (secondMax < firstMin)
    {
        return firstMin - secondMax;
    }
    return 0.0f;
}
} // namespace

bool Collider::IsHitSphereVsCapsule(SphereCollider *sphere, CapsuleCollider *capsule)
{
    const XMFLOAT3 sphereCenter =
        Transform::Float3Add(sphere->pGameObject_->GetWorldPosition(), sphere->center_);
    const XMFLOAT3 center =
        Transform::Float3Add(capsule->pGameObject_->GetWorldPosition(), capsule->center_);
    const float nearestY = ClampToInterval(sphereCenter.y, center.y - capsule->halfSegment_,
                                           center.y + capsule->halfSegment_);
    const float dx = sphereCenter.x - center.x;
    const float dy = sphereCenter.y - nearestY;
    const float dz = sphereCenter.z - center.z;
    const float radiusSum = sphere->size_.x + capsule->radius_;
    return dx * dx + dy * dy + dz * dz <= radiusSum * radiusSum;
}

bool Collider::IsHitBoxVsCapsule(BoxCollider *box, CapsuleCollider *capsule)
{
    const XMFLOAT3 boxCenter =
        Transform::Float3Add(box->pGameObject_->GetWorldPosition(), box->center_);
    const XMFLOAT3 center =
        Transform::Float3Add(capsule->pGameObject_->GetWorldPosition(), capsule->center_);
    const float nearestX = ClampToInterval(center.x, boxCenter.x - box->size_.x * 0.5f,
                                           boxCenter.x + box->size_.x * 0.5f);
    const float nearestZ = ClampToInterval(center.z, boxCenter.z - box->size_.z * 0.5f,
                                           boxCenter.z + box->size_.z * 0.5f);
    const float dx = center.x - nearestX;
    const float dz = center.z - nearestZ;
    const float dy =
        IntervalGap(center.y - capsule->halfSegment_, center.y + capsule->halfSegment_,
                    boxCenter.y - box->size_.y * 0.5f, boxCenter.y + box->size_.y * 0.5f);
    return dx * dx + dy * dy + dz * dz <= capsule->radius_ * capsule->radius_;
}

bool Collider::IsHitCapsuleVsCapsule(CapsuleCollider *first, CapsuleCollider *second)
{
    const XMFLOAT3 firstCenter =
        Transform::Float3Add(first->pGameObject_->GetWorldPosition(), first->center_);
    const XMFLOAT3 secondCenter =
        Transform::Float3Add(second->pGameObject_->GetWorldPosition(), second->center_);
    const float dx = firstCenter.x - secondCenter.x;
    const float dz = firstCenter.z - secondCenter.z;
    const float dy =
        IntervalGap(firstCenter.y - first->halfSegment_, firstCenter.y + first->halfSegment_,
                    secondCenter.y - second->halfSegment_, secondCenter.y + second->halfSegment_);
    const float radiusSum = first->radius_ + second->radius_;
    return dx * dx + dy * dy + dz * dz <= radiusSum * radiusSum;
}
