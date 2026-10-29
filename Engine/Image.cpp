
#include "Image.h"

// 3D画像を管理する
namespace Image
{
    // ロード済みの画像データ一覧
    std::vector<std::unique_ptr<ImageData>> _datas;

    // 初期化
    void Initialize()
    {
        AllRelease();
    }

    // 画像をロード
    int Load(std::string fileName)
    {
        std::unique_ptr<ImageData> data = std::make_unique<ImageData>();
        data->fileName = fileName;

        // 同じファイルを使うハンドル同士で、読み込み済みのデータを共有する。
        for (const std::unique_ptr<ImageData> &loaded : _datas)
        {
            if (loaded != nullptr && loaded->fileName == fileName)
            {
                data->pSprite = loaded->pSprite;
                break;
            }
        }
        if (data->pSprite == nullptr)
        {
            data->pSprite = std::make_shared<Sprite>();
            if (FAILED(data->pSprite->Load(fileName)))
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
        ResetRect(handle);
        return handle;
    }

    void Draw(int handle)
    {
        if (handle < 0 || handle >= _datas.size() || _datas[handle] == nullptr)
        {
            return;
        }
        _datas[handle]->transform.Calclation();
        _datas[handle]->pSprite->Draw(_datas[handle]->transform, _datas[handle]->rect,
                                      _datas[handle]->alpha);
    }

    // 任意の画像を開放
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

    // 切り抜き範囲の設定
    void SetRect(int handle, int x, int y, int width, int height)
    {
        if (handle < 0 || handle >= _datas.size())
        {
            return;
        }

        _datas[handle]->rect.left = x;
        _datas[handle]->rect.top = y;
        _datas[handle]->rect.right = width;
        _datas[handle]->rect.bottom = height;
    }

    // 切り抜き範囲をリセット（画像全体を表示する）
    void ResetRect(int handle)
    {
        if (handle < 0 || handle >= _datas.size())
        {
            return;
        }

        XMFLOAT3 size = _datas[handle]->pSprite->GetTextureSize();

        _datas[handle]->rect.left = 0;
        _datas[handle]->rect.top = 0;
        _datas[handle]->rect.right = (long)size.x;
        _datas[handle]->rect.bottom = (long)size.y;
    }

    // アルファ値設定
    void SetAlpha(int handle, int alpha)
    {
        if (handle < 0 || handle >= _datas.size())
        {
            return;
        }
        _datas[handle]->alpha = (float)alpha / 255.0f;
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
        if (handle < 0 || handle >= _datas.size())
        {
            return XMMatrixIdentity();
        }
        return _datas[handle]->transform.GetWorldMatrix();
    }
} // namespace Image
