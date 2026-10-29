
#include "Model.h"

// 3Dモデル（FBXファイル）を管理する
namespace Model
{
    // ロード済みのモデルデータ一覧
    std::vector<std::unique_ptr<ModelData>> _datas;

    // 初期化
    void Initialize()
    {
        AllRelease();
    }

    // モデルをロード
    int Load(std::string fileName)
    {
        std::unique_ptr<ModelData> data = std::make_unique<ModelData>();
        data->fileName = fileName;

        // 同じファイルを使うハンドル同士で、読み込み済みのデータを共有する。
        for (const std::unique_ptr<ModelData> &loaded : _datas)
        {
            if (loaded != nullptr && loaded->fileName == fileName)
            {
                data->pFbx = loaded->pFbx;
                break;
            }
        }
        if (data->pFbx == nullptr)
        {
            data->pFbx = std::make_shared<Fbx>();
            if (FAILED(data->pFbx->Load(fileName)))
            {
                return -1;
            }
        }

        int handle = -1;
        for (size_t index = 0; index < _datas.size(); ++index)
        {
            if (_datas[index] == nullptr)
            {
                handle = static_cast<int>(index);
                _datas[index] = std::move(data);
                break;
            }
        }
        if (handle < 0)
        {
            _datas.push_back(std::move(data));
            handle = static_cast<int>(_datas.size()) - 1;
        }
        return handle;
    }

    // ハンドルごとの再生位置を進め、共有FBXにその姿勢と配置を適用する。
    void Draw(int handle)
    {
        if (handle < 0 || handle >= _datas.size() || _datas[handle] == nullptr)
        {
            return;
        }

        // アニメーションを進める
        _datas[handle]->nowFrame += _datas[handle]->animSpeed;

        // 最後までアニメーションしたら戻す
        if (_datas[handle]->nowFrame > (float)_datas[handle]->endFrame)
            _datas[handle]->nowFrame = (float)_datas[handle]->startFrame;

        if (_datas[handle]->pFbx)
        {
            _datas[handle]->pFbx->SetAnimation(_datas[handle]->animationName);
            _datas[handle]->pFbx->Draw(_datas[handle]->transform, (int)_datas[handle]->nowFrame);
        }
    }

    // 任意のモデルを開放
    void Release(int handle)
    {
        if (handle < 0 || handle >= static_cast<int>(_datas.size()))
        {
            return;
        }
        // 最後の共有ハンドルが消えたとき、読み込み済みのデータも解放される。
        _datas[handle].reset();
    }

    void AllRelease()
    {
        _datas.clear();
    }

    // アニメーションのフレーム数をセット
    void SetAnimFrame(int handle, int startFrame, int endFrame, float animSpeed)
    {
        _datas[handle]->SetAnimFrame(startFrame, endFrame, animSpeed);
    }

    // Bake the selected animation once for all shared handles.
    bool BakeAnimation(int handle, int startFrame, int endFrame)
    {
        if (handle < 0 || handle >= static_cast<int>(_datas.size()) || !_datas[handle])
        {
            return false;
        }
        ModelData &model = *_datas[handle];
        if (!model.pFbx || !model.pFbx->SetAnimation(model.animationName))
        {
            return false;
        }
        return model.pFbx->BakeAnimation(startFrame, endFrame);
    }

    // Change the playback position without changing its loop range.
    bool SetCurrentFrame(int handle, int frame)
    {
        if (handle < 0 || handle >= static_cast<int>(_datas.size()) || !_datas[handle])
        {
            return false;
        }
        ModelData &model = *_datas[handle];
        if (frame < model.startFrame || frame > model.endFrame)
        {
            return false;
        }
        model.nowFrame = static_cast<float>(frame);
        return true;
    }

    // 現在のアニメーションのフレームを取得
    int GetAnimFrame(int handle)
    {
        return (int)_datas[handle]->nowFrame;
    }

    // 任意のボーンの位置を取得
    XMFLOAT3 GetBonePosition(int handle, std::string boneName)
    {
        _datas[handle]->pFbx->SetAnimation(_datas[handle]->animationName);
        _datas[handle]->pFbx->Evaluate((int)_datas[handle]->nowFrame);
        XMFLOAT3 pos = _datas[handle]->pFbx->GetBonePosition(boneName);
        XMVECTOR vec =
            XMVector3TransformCoord(XMLoadFloat3(&pos), _datas[handle]->transform.GetWorldMatrix());
        XMStoreFloat3(&pos, vec);
        return pos;
    }

    // ワールド行列を設定
    void SetTransform(int handle, Transform &transform)
    {
        if (handle < 0 || handle >= _datas.size())
        {
            return;
        }

        _datas[handle]->transform = transform;
    }

    // ワールド行列の取得
    XMMATRIX GetMatrix(int handle)
    {
        return _datas[handle]->transform.GetWorldMatrix();
    }

    // レイキャスト（レイを飛ばして当たり判定）
    void RayCast(int handle, RayCastData *data)
    {
        if (!data || handle < 0 || handle >= _datas.size() || !_datas[handle])
            return;
        const XMFLOAT3 originalStart = data->start, originalDir = data->dir;
        _datas[handle]->pFbx->SetAnimation(_datas[handle]->animationName);
        _datas[handle]->pFbx->Evaluate((int)_datas[handle]->nowFrame);
        XMFLOAT3 target = Transform::Float3Add(data->start, data->dir);
        XMMATRIX matInv = XMMatrixInverse(nullptr, _datas[handle]->transform.GetWorldMatrix());
        XMVECTOR vecStart = XMVector3TransformCoord(XMLoadFloat3(&data->start), matInv);
        XMVECTOR vecTarget = XMVector3TransformCoord(XMLoadFloat3(&target), matInv);
        XMVECTOR vecDir = vecTarget - vecStart;

        XMStoreFloat3(&data->start, vecStart);
        XMStoreFloat3(&data->dir, vecDir);

        _datas[handle]->pFbx->RayCast(data);
        data->start = originalStart;
        data->dir = originalDir;
        if (data->hit)
        {
            XMMATRIX normals = XMMatrixTranspose(matInv);
            XMStoreFloat3(&data->normal, XMVector3Normalize(XMVector3TransformNormal(
                                             XMLoadFloat3(&data->normal), normals)));
        }
    }
} // namespace Model

namespace Model
{
    bool SetAnimation(int handle, const std::string &name)
    {
        if (handle < 0 || handle >= (int)_datas.size() || !_datas[handle] || !_datas[handle]->pFbx)
            return false;
        if (!_datas[handle]->pFbx->SetAnimation(name))
            return false;
        _datas[handle]->animationName = name;
        return true;
    }
} // namespace Model