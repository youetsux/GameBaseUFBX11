#include "gameObject.h"
#include <assert.h>
#include "global.h"

//コンストラクタ（親も名前もなし）
GameObject::GameObject(void) :
	GameObject(nullptr, "")
{

}

//コンストラクタ（名前なし）
GameObject::GameObject(GameObject * parent) :
	GameObject(parent, "")
{
}

//コンストラクタ（標準）
GameObject::GameObject(GameObject * parent, const std::string& name)
	: pParent_(parent),



	objectName_(name)
{
	childList_.clear();
	state_ = { 0, 1, 1, 0 };

	if(parent)
		transform_.pParent_ = &parent->transform_;

}

//デストラクタ
GameObject::~GameObject()
{
    colliderList_.clear();
}

// 削除するかどうか
bool GameObject::IsDead()
{
	return (state_.dead != 0);
}

// 自分を削除する
void GameObject::KillMe()
{
	state_.dead = 1;
}

// Updateを許可
void GameObject::Enter()
{
	state_.entered = 1;
}

// Updateを拒否
void GameObject::Leave()
{
	state_.entered = 0;
}

// Drawを許可
void GameObject::Visible()
{
	state_.visible = 1;
}

// Drawを拒否
void GameObject::Invisible()
{
	state_.visible = 0;
}

// 初期化済みかどうか
bool GameObject::IsInitialized()
{
	return (state_.initialized != 0);
}

// 初期化済みにする
void GameObject::SetInitialized()
{
	state_.initialized = 1;
}

// Update実行していいか
bool GameObject::IsEntered()
{
	return (state_.entered != 0);
}

// Draw実行していいか
bool GameObject::IsVisibled()
{
	return (state_.visible != 0);
}

//子オブジェクトリストを取得
const std::list<std::unique_ptr<GameObject>>* GameObject::GetChildList() const
{
	return &childList_;
}

//親オブジェクトを取得
GameObject * GameObject::GetParent(void)
{
	return pParent_;
}

//名前でオブジェクトを検索（対象は自分の子供以下）
GameObject * GameObject::FindChildObject(const std::string & name)
{
	//子供がいないなら終わり
	if (childList_.empty())
		return nullptr;

	//イテレータ
	auto it = childList_.begin();	//先頭
	auto end = childList_.end();	//末尾

	//子オブジェクトから探す
	while (it != end) {
		//同じ名前のオブジェクトを見つけたらそれを返す
		if ((*it)->GetObjectName() == name)
			return it->get();

		//その子供（孫）以降にいないか探す
		GameObject* obj = (*it)->FindChildObject(name);
		if (obj != nullptr)
		{
			return obj;
		}

		//次の子へ
		it++;
	}

	//見つからなかった
	return nullptr;
}

//オブジェクトの名前を取得
const std::string& GameObject::GetObjectName(void) const
{
	return objectName_;
}

//子オブジェクトを追加（リストの最後へ）
// 生ポインター版は既存コードとの互換用。渡した時点で親が所有する。
void GameObject::PushBackChild(GameObject* object)
{
    PushBackChild(std::unique_ptr<GameObject>(object));
}

void GameObject::PushBackChild(std::unique_ptr<GameObject> object)
{
    assert(object != nullptr);
    object->pParent_ = this;
    object->transform_.pParent_ = &transform_;
    childList_.push_back(std::move(object));
}

void GameObject::PushFrontChild(GameObject* object)
{
    PushFrontChild(std::unique_ptr<GameObject>(object));
}

void GameObject::PushFrontChild(std::unique_ptr<GameObject> object)
{
    assert(object != nullptr);
    object->pParent_ = this;
    object->transform_.pParent_ = &transform_;
    childList_.push_front(std::move(object));
}

void GameObject::KillAllChildren()
{
    for (const std::unique_ptr<GameObject>& child : childList_) {
        KillObjectSub(child.get());
    }
    childList_.clear();
}

void GameObject::KillObjectSub(GameObject* object)
{
    object->ReleaseSub();
}

void GameObject::AddCollider(Collider* collider)
{
    AddCollider(std::unique_ptr<Collider>(collider));
}

void GameObject::AddCollider(std::unique_ptr<Collider> collider)
{
    collider->SetGameObject(this);
    colliderList_.push_back(std::move(collider));
}

void GameObject::ClearCollider()
{
    colliderList_.clear();
}

void GameObject::Collision(GameObject * pTarget)
{
	//自分同士の当たり判定はしない
	if (pTarget == nullptr || this == pTarget)
	{
		return;
	}

	//自分とpTargetのコリジョン情報を使って当たり判定
	//1つのオブジェクトが複数のコリジョン情報を持ってる場合もあるので二重ループ
	for (auto i = this->colliderList_.begin(); i != this->colliderList_.end(); i++)
	{
		for (auto j = pTarget->colliderList_.begin(); j != pTarget->colliderList_.end(); j++)
		{
			if ((*i)->IsHit(j->get()))
			{
				//当たった
				this->OnCollision(pTarget);
			}
		}
	}

	//子供がいないなら終わり
	if (pTarget->childList_.empty())
		return;

	//子供も当たり判定
	for (auto i = pTarget->childList_.begin(); i != pTarget->childList_.end(); i++)
	{
		Collision(i->get());
	}
}


//テスト用の衝突判定枠を表示
void GameObject::CollisionDraw()
{
	Direct3D::SetShader(Direct3D::SHADER_UNLIT);

	for (auto i = this->colliderList_.begin(); i != this->colliderList_.end(); i++)
	{
		(*i)->Draw(GetWorldPosition());
	}

	Direct3D::SetShader(Direct3D::SHADER_3D);
}

//RootJobを取得
GameObject * GameObject::GetRootJob()
{
	if (GetParent() == nullptr)
	{
		return this;
	}
	else return GetParent()->GetRootJob();
}




void GameObject::UpdateSub()
{
	Update();
	Transform();

	for (auto it = childList_.begin(); it != childList_.end(); it++)
	{
		(*it)->UpdateSub();
	}

	for (auto it = childList_.begin(); it != childList_.end();)
	{
		if ((*it)->IsDead() == true)
		{
			(*it)->ReleaseSub();
			it = childList_.erase(it);
		}
		else
		{
			//当たり判定
			(*it)->Collision(GetParent());
			it++;
		}
	}
}

void GameObject::DrawSub()
{
	Draw();


	//リリース時は削除
#ifdef _DEBUG
		//コリジョンの描画
	if (Direct3D::isDrawCollision_)
	{
		CollisionDraw();
	}
#endif

	//その子オブジェクトの描画処理
	for (auto it = childList_.begin(); it != childList_.end(); it++)
	{
		(*it)->DrawSub();
	}
}

void GameObject::ReleaseSub()
{
    ClearCollider();
    for (const std::unique_ptr<GameObject>& child : childList_) {
        child->ReleaseSub();
    }
    childList_.clear();
    Release();
}




////ローカル行列の取得（このオブジェクトの行列）
//XMMATRIX GameObject::GetLocalMatrix(void)
//{
//	return transform_.GetWorldMatrix();
//}

//ワールド行列の取得（親の影響を受けた最終的な行列）
XMMATRIX GameObject::GetWorldMatrix(void)
{
	return transform_.GetWorldMatrix();
}


