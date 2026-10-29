#include "BoxCollider.h"
#include "SphereCollider.h"
#include "GameObject.h"
#include "Model.h"
#include "Transform.h"

// コンストラクタ
Collider::Collider() : pGameObject_(nullptr)
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
    const float distanceSquared = distanceX * distanceX + distanceY * distanceY +
                                  distanceZ * distanceZ;
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
