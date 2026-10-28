#define NOMINMAX
#include "Fbx.h"
#include "Model.h"
#include "Direct3D.h"
#include "Camera.h"
#include "Texture.h"
#include <d3d11sdklayers.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <cfloat>
using Microsoft::WRL::ComPtr;
namespace Model { extern std::vector<ModelData*> _datas; }
namespace Direct3D { extern ID3D11BlendState* pBlendState[BLEND_MAX]; }
struct Triangle { XMFLOAT3 p[3]; };
static int failures=0;
static void check(bool ok,const char* text) { if(!ok){printf("FAIL: %s\n",text);++failures;} }
static XMFLOAT3 cv(ufbx_vec3 p) {return XMFLOAT3((float)p.x,(float)p.y,(float)p.z);}
static std::vector<Triangle> triangles(ufbx_scene* scene) {
 std::vector<Triangle> result;
 for(size_t n=0;n<scene->nodes.count;++n) {
  auto* node=scene->nodes.data[n]; auto* mesh=node->mesh; if(!mesh) continue;
  std::vector<uint32_t> indices(std::max<size_t>(3,mesh->max_face_triangles*3));
  for(size_t f=0;f<mesh->faces.count;++f) {
   uint32_t count=ufbx_triangulate_face(indices.data(),indices.size(),mesh,mesh->faces.data[f]);
   for(uint32_t t=0;t<count;++t) {
    Triangle tri{};
    for(int k=0;k<3;++k) {
     auto v=ufbx_get_vertex_vec3(&mesh->skinned_position,indices[t*3+k]);
     if(mesh->skinned_is_local) v=ufbx_transform_position(&node->geometry_to_world,v);
     tri.p[k]=cv(v);
    }
    result.push_back(tri);
   }
  }
 }
 return result;
}
int main(int argc,char** argv) {
 if(argc<2) return 2;
 const bool hardware = std::getenv("UFBX_TEST_HARDWARE") != nullptr;
 D3D_DRIVER_TYPE driver = hardware ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP;
 UINT flags=D3D11_CREATE_DEVICE_DEBUG;
 HRESULT hr=D3D11CreateDevice(nullptr,driver,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&Direct3D::pDevice_,nullptr,&Direct3D::pContext_);
 if(FAILED(hr)) { flags=0; hr=D3D11CreateDevice(nullptr,driver,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&Direct3D::pDevice_,nullptr,&Direct3D::pContext_); }
 check(SUCCEEDED(hr),"WARP device"); if(FAILED(hr)) return 1;
 printf("D3D11 %s debug layer: %s\n",hardware?"HARDWARE":"WARP",flags?"enabled":"unavailable");
 ComPtr<ID3D11InfoQueue> info; Direct3D::pDevice_->QueryInterface(IID_PPV_ARGS(&info));
 Direct3D::screenWidth_=Direct3D::screenHeight_=256;
 Direct3D::InitShaderBundle(); Direct3D::SetShader(Direct3D::SHADER_3D);
 D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=256;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_RENDER_TARGET;
 ComPtr<ID3D11Texture2D> target,staging;
 check(SUCCEEDED(Direct3D::pDevice_->CreateTexture2D(&td,nullptr,&target)),"render target");
 ComPtr<ID3D11RenderTargetView> rtv;Direct3D::pDevice_->CreateRenderTargetView(target.Get(),nullptr,&rtv);
 td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 Direct3D::pDevice_->CreateTexture2D(&td,nullptr,&staging);
 ID3D11RenderTargetView* view=rtv.Get(); Direct3D::pContext_->OMSetRenderTargets(1,&view,nullptr);
 D3D11_VIEWPORT vp{0,0,256,256,0,1};Direct3D::pContext_->RSSetViewports(1,&vp);
 Direct3D::pContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 Camera::Initialize();
 // Match the game's alpha blending so NaN/alpha-related holes are observable.
 D3D11_BLEND_DESC blendDesc{};
 blendDesc.RenderTarget[0].BlendEnable=TRUE;
 blendDesc.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;
 blendDesc.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
 blendDesc.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
 blendDesc.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
 blendDesc.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ZERO;
 blendDesc.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
 blendDesc.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
 ComPtr<ID3D11BlendState> blend;
 check(SUCCEEDED(Direct3D::pDevice_->CreateBlendState(&blendDesc,&blend)),"game alpha blend state");
 Direct3D::pBlendState[Direct3D::BLEND_DEFAULT]=blend.Get();
 {Texture texture;check(SUCCEEDED(texture.Load("char.png")),"WIC texture upload");check(texture.GetSRV()!=nullptr,"texture SRV");check(FAILED(texture.Load("__missing__.png")),"missing texture handled");}
 for(int file=1;file<argc;++file) {
  printf("MODEL %s\n",argv[file]);
  Fbx model;
  check(FAILED(model.Load("__missing__.fbx")),"missing FBX handled");
  check(SUCCEEDED(model.Load(argv[file])),"FBX load");
  ufbx_load_opts opts{};opts.evaluate_skinning=true;opts.generate_missing_normals=true;
  opts.target_axes=ufbx_axes_left_handed_y_up;
  opts.handedness_conversion_axis=UFBX_MIRROR_AXIS_Z;
  opts.space_conversion=UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS;
  ufbx_scene* source=ufbx_load_file(argv[file],&opts,nullptr); if(!source){check(false,"reference load");continue;}
  auto* anim=source->anim_stacks.count?source->anim_stacks.data[0]->anim:source->anim;
  const bool orientation = std::string(argv[file]).find("orientation_rh") != std::string::npos;
  size_t rayChecks=0,hitChecks=0,boneChecks=0,pixelFrames=0,movedBones=0;
  std::vector<XMFLOAT3> initialBones(source->nodes.count);
  std::string movingBoneName;
  for(int frame : {0,15,30}) {
   check(model.Evaluate(frame),"frame evaluation");
   ufbx_evaluate_opts eo{};eo.evaluate_skinning=true;
   double fps=source->settings.frames_per_second>0?source->settings.frames_per_second:30;
   auto* scene=ufbx_evaluate_scene(source,anim,frame/fps,&eo,nullptr);
   if(!scene){check(false,"reference evaluation");continue;}
   auto tris=triangles(scene);
   XMFLOAT3 lo(FLT_MAX,FLT_MAX,FLT_MAX),hi(-FLT_MAX,-FLT_MAX,-FLT_MAX);
   for(auto& tri:tris)for(auto& p:tri.p) {lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);}
   float radius=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z,1.0f});
   for(size_t t=0;t<tris.size();t+=std::max<size_t>(1,tris.size()/24)) {
    XMVECTOR a=XMLoadFloat3(&tris[t].p[0]),b=XMLoadFloat3(&tris[t].p[1]),c=XMLoadFloat3(&tris[t].p[2]);
    XMVECTOR cross=XMVector3Cross(b-a,c-a);
    if(XMVectorGetX(XMVector3LengthSq(cross))<1e-10f)continue;
    XMVECTOR normal=XMVector3Normalize(cross),center=(a+b+c)/3;
    RayCastData data; XMStoreFloat3(&data.start,center+normal*radius);XMStoreFloat3(&data.dir,-normal);
    data.dist=FLT_MAX; float expected=FLT_MAX;
    for(auto& triangle:tris) {float dist=0;if(Direct3D::Intersect(data.start,data.dir,triangle.p[0],triangle.p[1],triangle.p[2],&dist))expected=std::min(expected,dist);}
    model.RayCast(&data);++rayChecks;
    check((bool)data.hit==(expected<FLT_MAX),"ray hit matches evaluated geometry");
    if(data.hit) {++hitChecks;check(std::fabs(data.dist-expected)<std::max(0.002f,radius*0.0001f),"ray distance matches transformed/skinned mesh");float length=std::sqrt(data.normal.x*data.normal.x+data.normal.y*data.normal.y+data.normal.z*data.normal.z);check(std::fabs(length-1)<0.001f,"ray normal unit length");}
   }
   for(size_t n=0;n<scene->nodes.count;++n) {
    auto* node=scene->nodes.data[n];if(!node->bone)continue;
    auto got=model.GetBonePosition(std::string(node->name.data,node->name.length));
    check(std::fabs(got.x-node->node_to_world.m03)<0.002 && std::fabs(got.y-node->node_to_world.m13)<0.002 && std::fabs(got.z-node->node_to_world.m23)<0.002,"animated bone position");++boneChecks;
    if(frame==0)initialBones[n]=got;
    if(frame==30 && (std::fabs(initialBones[n].x-got.x)+std::fabs(initialBones[n].y-got.y)+std::fabs(initialBones[n].z-got.z))>0.01f) {++movedBones;movingBoneName=std::string(node->name.data,node->name.length);}
   }
   XMFLOAT3 center((lo.x+hi.x)/2,(lo.y+hi.y)/2,(lo.z+hi.z)/2);
   Camera::SetTarget(center);Camera::SetPosition(XMFLOAT3(center.x+radius,center.y+radius,center.z-radius*2));Camera::Update();
   if (orientation) { Camera::SetPosition(XMFLOAT3(0,0,-6)); Camera::SetTarget(XMFLOAT3(0,0,-2)); Camera::Update(); }
   float black[4]={0,0,0,1};Direct3D::pContext_->ClearRenderTargetView(rtv.Get(),black);
   Transform transform;model.Draw(transform,frame);
   if(std::string(argv[file]).find("maya_textured_cube")!=std::string::npos) {
    ComPtr<ID3D11ShaderResourceView> bound;Direct3D::pContext_->PSGetShaderResources(0,1,&bound);
    check(bound!=nullptr,"relative subdirectory diffuse texture bound");
   }
      if (orientation) {
        // Lambert must not enter the Phong pow() branch, especially with exponent 0.
    ComPtr<ID3D11Buffer> materialCB;Direct3D::pContext_->PSGetConstantBuffers(0,1,&materialCB);
    D3D11_BUFFER_DESC materialDesc{};materialCB->GetDesc(&materialDesc);
    materialDesc.Usage=D3D11_USAGE_STAGING;materialDesc.BindFlags=0;materialDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;materialDesc.MiscFlags=0;
    ComPtr<ID3D11Buffer> cbReadback;Direct3D::pDevice_->CreateBuffer(&materialDesc,nullptr,&cbReadback);
    Direct3D::pContext_->CopyResource(cbReadback.Get(),materialCB.Get());D3D11_MAPPED_SUBRESOURCE cbData{};
    if(SUCCEEDED(Direct3D::pContext_->Map(cbReadback.Get(),0,D3D11_MAP_READ,0,&cbData))) {
     const float* values=(const float*)cbData.pData;
     check(values[63]==0.0f,"Lambert specular enable flag is zero");
     Direct3D::pContext_->Unmap(cbReadback.Get(),0);
    } else check(false,"material constant buffer readback");
    // Read the actual GPU vertices: independently require z reflection, outward normals,
    // and the original U values. This catches an attempted 1-U workaround.
    ComPtr<ID3D11Buffer> vb;UINT stride=0,offset=0;
    Direct3D::pContext_->IAGetVertexBuffers(0,1,&vb,&stride,&offset);
    D3D11_BUFFER_DESC desc{};vb->GetDesc(&desc);
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    ComPtr<ID3D11Buffer> readback;Direct3D::pDevice_->CreateBuffer(&desc,nullptr,&readback);
    Direct3D::pContext_->CopyResource(readback.Get(),vb.Get());D3D11_MAPPED_SUBRESOURCE vertices{};
    if(SUCCEEDED(Direct3D::pContext_->Map(readback.Get(),0,D3D11_MAP_READ,0,&vertices))) {
     for(UINT v=0;v<desc.ByteWidth/stride;++v) {
      const float* data=(const float*)((const BYTE*)vertices.pData+v*stride);
      check(std::fabs(data[2]+2.0f)<0.001f,"RH +Z converted to LH -Z");
      check(std::fabs(data[5]+1.0f)<0.001f,"normal Z converted with geometry");
      check(std::fabs(data[6]-(data[0]+1.0f)/2)<0.001f,"U coordinates preserved");
     }
     Direct3D::pContext_->Unmap(readback.Get(),0);
    } else check(false,"vertex readback");
    RayCastData ray;ray.start=XMFLOAT3(0,0,-6);ray.dir=XMFLOAT3(0,0,1);model.RayCast(&ray);
    check(ray.hit && std::fabs(ray.dist-4)<0.001f,"converted panel ray distance");
    check(ray.normal.z < -0.999f,"converted panel outward winding");
   }
   Direct3D::pContext_->CopyResource(staging.Get(),target.Get());
   D3D11_MAPPED_SUBRESOURCE map{};
   if(SUCCEEDED(Direct3D::pContext_->Map(staging.Get(),0,D3D11_MAP_READ,0,&map))) {
    size_t pixels=0;for(UINT y=0;y<256;++y)for(UINT x=0;x<256;++x){auto* pixel=(BYTE*)map.pData+y*map.RowPitch+x*4;if(pixel[0]||pixel[1]||pixel[2])++pixels;}
        if(pixels)++pixelFrames;
    if (orientation) {
     const int locations[4][2]={{100,100},{156,100},{100,156},{156,156}};
     const int colors[4][3]={{255,0,0},{0,255,0},{0,0,255},{255,255,0}};
     for(int q=0;q<4;++q) {
      const BYTE* pixel=(const BYTE*)map.pData+locations[q][1]*map.RowPitch+locations[q][0]*4;
      for(int c=0;c<3;++c) check(colors[q][c]?pixel[c]>200:pixel[c]<20,"orientation quadrant colors and front-face culling");
     }
     if(frame==0) printf("  orientation quadrants: red/green above blue/yellow; Z=-2, normal=-Z, U unchanged\n");
    }
    Direct3D::pContext_->Unmap(staging.Get(),0);
   }
   ufbx_free_scene(scene);
  }
  if (orientation) {
   // Test the public Model API on the same left-handed camera: +Z must recede.
   int handle=Model::Load(argv[file]);check(handle>=0,"Model translation test load");
   int widths[2]={0,0};
   if(handle>=0) {
    Camera::SetPosition(XMFLOAT3(0,0,-6));Camera::SetTarget(XMFLOAT3(0,0,-2));Camera::Update();
    for(int z=0;z<2;++z) {
     Transform transform;transform.position_.z=(float)z;Model::SetTransform(handle,transform);
     float black[4]={0,0,0,1};Direct3D::pContext_->ClearRenderTargetView(rtv.Get(),black);Model::Draw(handle);
     Direct3D::pContext_->CopyResource(staging.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE pixels{};
     if(SUCCEEDED(Direct3D::pContext_->Map(staging.Get(),0,D3D11_MAP_READ,0,&pixels))) {
      int low=256,high=-1;
      for(int y=0;y<256;++y)for(int x=0;x<256;++x) {
       const BYTE* pixel=(const BYTE*)pixels.pData+y*pixels.RowPitch+x*4;
       if(pixel[0]||pixel[1]||pixel[2]){low=std::min(low,x);high=std::max(high,x);}
      }
      widths[z]=high-low+1;Direct3D::pContext_->Unmap(staging.Get(),0);
     }
    }
    check(widths[0]>0&&widths[1]>0&&widths[1]<widths[0],"Model +Z moves away in LH coordinates");
    check(std::fabs(widths[1]-widths[0]*0.8f)<3.0f,"projected size follows depth 4 to 5");
    printf("  Model Z=0 -> Z=+1 projected width: %d -> %d (depth 4 -> 5)\n",widths[0],widths[1]);
    Model::Release(handle);
   }
  }
  check(hitChecks>0,"geometry hit coverage");check(pixelFrames>0,"render produces visible pixels");
  printf("  rays=%zu hits=%zu bones=%zu moved_bones=%zu rendered_frames=%zu/3\n",rayChecks,hitChecks,boneChecks,movedBones,pixelFrames);
  if(boneChecks) {
   const ufbx_node* bone=nullptr;for(size_t n=0;n<source->nodes.count;++n)if(source->nodes.data[n]->bone){bone=source->nodes.data[n];break;}
   if(bone) {
    std::string name(bone->name.data,bone->name.length);
    if(!movingBoneName.empty())name=movingBoneName;
    int h0=Model::Load(argv[file]),h1=Model::Load(argv[file]);
    check(h0>=0&&h1>=0,"shared animated handles");
    if(h0>=0&&h1>=0) {
     Model::SetAnimFrame(h0,0,30,0);Model::SetAnimFrame(h1,30,30,0);
     model.Evaluate(0);auto e0=model.GetBonePosition(name);
     model.Evaluate(30);auto e1=model.GetBonePosition(name);
     auto b1=Model::GetBonePosition(h1,name);auto b0=Model::GetBonePosition(h0,name);auto again=Model::GetBonePosition(h1,name);
     check(std::fabs(e0.x-b0.x)+std::fabs(e0.y-b0.y)+std::fabs(e0.z-b0.z)<0.002f,"frame 0 handle pose");
     check(std::fabs(e1.x-b1.x)+std::fabs(e1.y-b1.y)+std::fabs(e1.z-b1.z)<0.002f,"frame 30 handle pose");
     check(std::fabs(again.x-b1.x)+std::fabs(again.y-b1.y)+std::fabs(again.z-b1.z)<0.002f,"shared pose switches back");
    }
    Model::AllRelease();
   }
  }
  ufbx_free_scene(source); model.Release();model.Release();
 }
 // Regression: Run.fbx has a static first take and a moving second take.
 // Exercise public playback and read back the buffer actually submitted to Draw.
 for (const char* path : {"Run.fbx", "Walking.fbx"}) {
  int h = Model::Load(path); check(h >= 0, "user animation load");
  if (h < 0) continue;
  Model::SetAnimFrame(h, 0, 38, 1.0f);
  std::vector<float> first;
  double movement = 0.0;
  for (int frame = 0; frame < 16; ++frame) {
   Model::Draw(h);
   check(Model::GetAnimFrame(h) == frame + 1, "public playback advances");
   ComPtr<ID3D11Buffer> vb; UINT stride=0,offset=0;
   Direct3D::pContext_->IAGetVertexBuffers(0,1,&vb,&stride,&offset);
   check(vb != nullptr, "animation draw binds vertex buffer");
   if (!vb) continue;
   D3D11_BUFFER_DESC d{}; vb->GetDesc(&d);
   d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;
   ComPtr<ID3D11Buffer> readback;
   check(SUCCEEDED(Direct3D::pDevice_->CreateBuffer(&d,nullptr,&readback)), "animation readback buffer");
   if (!readback) continue;
   Direct3D::pContext_->CopyResource(readback.Get(),vb.Get());
   D3D11_MAPPED_SUBRESOURCE m{};
   if (SUCCEEDED(Direct3D::pContext_->Map(readback.Get(),0,D3D11_MAP_READ,0,&m))) {
    for (UINT v=0;v<d.ByteWidth/stride;++v) {
     const float* pos=(const float*)((const BYTE*)m.pData+v*stride);
     for (int k=0;k<3;++k) {
      check(std::isfinite(pos[k]), "animated vertex is finite");
      if(frame==0) first.push_back(pos[k]);
      else movement=std::max(movement,(double)std::fabs(pos[k]-first[v*3+k]));
     }
    }
    Direct3D::pContext_->Unmap(readback.Get(),0);
   } else check(false,"animation GPU readback");
  }
  check(movement > 0.01, "public playback changes GPU skin vertices");
  printf("PLAYBACK %s frames=1..16 max_vertex_delta=%g\n",path,movement);
  Model::Release(h);
 }
 {
  auto snapshot = [] (int h) {
   Model::Draw(h);
   std::vector<BYTE> bytes;
   ComPtr<ID3D11Buffer> vb; UINT stride=0,offset=0;
   Direct3D::pContext_->IAGetVertexBuffers(0,1,&vb,&stride,&offset);
   if (!vb) { check(false,"take buffer binding"); return bytes; }
   D3D11_BUFFER_DESC d{};vb->GetDesc(&d);
   d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;
   ComPtr<ID3D11Buffer> copy;
   if(FAILED(Direct3D::pDevice_->CreateBuffer(&d,nullptr,&copy))) {check(false,"take readback allocation");return bytes;}
   Direct3D::pContext_->CopyResource(copy.Get(),vb.Get());D3D11_MAPPED_SUBRESOURCE m{};
   if(SUCCEEDED(Direct3D::pContext_->Map(copy.Get(),0,D3D11_MAP_READ,0,&m))) {
    bytes.assign((BYTE*)m.pData,(BYTE*)m.pData+d.ByteWidth);
    Direct3D::pContext_->Unmap(copy.Get(),0);
   } else check(false,"take readback map");
   return bytes;
  };
  int a=Model::Load("Run.fbx"), b=Model::Load("Run.fbx");
  check(a>=0&&b>=0,"take handles load");
  if(a>=0&&b>=0) {
   Model::SetAnimFrame(a,15,15,0);Model::SetAnimFrame(b,15,15,0);
   check(Model::SetAnimation(a,"Take 001"),"select static take");
   check(Model::SetAnimation(b,"mixamo.com"),"select moving take");
   auto still=snapshot(a), moving=snapshot(b), stillAgain=snapshot(a);
   check(!still.empty() && still!=moving,"same-frame take switch changes GPU vertices");
   check(still==stillAgain,"shared handles retain separate takes");
   check(!Model::SetAnimation(a,"__missing__"),"missing take rejected");
   check(snapshot(a)==still,"failed selection preserves previous take");
   check(Model::SetAnimation(a,""),"automatic take restored");
   check(snapshot(a)==moving,"automatic selection skips static take");
   check(Model::GetAnimFrame(a)==15,"selection preserves frame");
  }
  check(!Model::SetAnimation(-1,"mixamo.com"),"invalid animation handle rejected");
  Model::AllRelease();
 }
 // Shared cache survives releasing the initial handle; pose queries use each handle's frame.
 int a=Model::Load(argv[1]),b=Model::Load(argv[1]),c=Model::Load(argv[1]);
 check(a>=0&&b>=0&&c>=0,"three shared Model handles");
 if(a>=0&&b>=0&&c>=0) {check(Model::_datas[a]->pFbx==Model::_datas[b]->pFbx&&Model::_datas[b]->pFbx==Model::_datas[c]->pFbx,"shared FBX cache");Model::Release(a);int d=Model::Load(argv[1]);check(Model::_datas[d]->pFbx==Model::_datas[b]->pFbx,"cache after original released");
  RayCastData ray;ray.start=XMFLOAT3(0,0,-100);ray.dir=XMFLOAT3(0,0,1);Model::RayCast(b,&ray);check(ray.start.z==-100 && ray.dir.z==1,"Model ray input preserved");}
 Model::AllRelease();
 if(info)for(UINT64 i=0;i<info->GetNumStoredMessagesAllowedByRetrievalFilter();++i){SIZE_T bytes=0;info->GetMessage(i,nullptr,&bytes);std::vector<BYTE> buffer(bytes);auto* msg=(D3D11_MESSAGE*)buffer.data();info->GetMessage(i,msg,&bytes);if(msg->Severity<=D3D11_MESSAGE_SEVERITY_ERROR){printf("D3D ERROR: %s\n",msg->pDescription);++failures;}}
 Direct3D::pContext_->ClearState();
 printf("RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);return failures?1:0;
}






