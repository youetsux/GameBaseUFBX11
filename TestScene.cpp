#include "TestScene.h"
#include "Engine\Model.h"

namespace
{
	int hmodel = -1;
}

//コンストラクタ
TestScene::TestScene(GameObject * parent)
	: GameObject(parent, "TestScene")
{
}

//初期化
void TestScene::Initialize()
{
	hmodel = Model::Load("skg.fbx");
	//Model::SetAnimation(hmodel, "mixamo.com");
	Model::SetAnimFrame(hmodel, 0, 152, 1.0f);
}

//更新
void TestScene::Update()
{
}

//描画
void TestScene::Draw()
{
	Transform trs;
	//trs.scale_ = { 0.01f,0.01f,0.01f };
	//Model::SetTransform(hmodel, trs);
	Model::Draw(hmodel);
}

//開放
void TestScene::Release()
{

}
