#include "RenderData.h"
#include <cmath>
#include "Engine/Resource/AssimpLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Actor/Actor.h"

using namespace DirectX;

Model MakeQuadModel()
{
	Mesh mesh;		//メッシュデータ構造体
	Model model;	//モデルデータ構造体

	//頂点データの設定
	mesh.vertices.assign(
		std::begin(QuadVertices),	//頂点データ配列の先頭アドレス
		std::end(QuadVertices)		//頂点データ配列の終端アドレス
	);

	//インデックスデータの設定
	mesh.indices.assign(
		std::begin(QuadIndices),	//インデックスデータ配列の先頭アドレス
		std::end(QuadIndices)		//インデックスデータ配列の終端アドレス
	);

	//頂点数の設定
	mesh.vertexCount = mesh.vertices.size();

	//インデックス数の設定
	mesh.indexCount = mesh.indices.size();

	//メッシュデータをモデルデータに追加
	model.push_back(mesh);

	return model;	//メッシュデータ構造体を返す
}

//立方体のメッシュデータ作成関数
Model MakeCubeModel()
{
	{
		Mesh mesh;		//メッシュデータ構造体
		Model model;	//モデルデータ構造体

		//頂点データの設定
		mesh.vertices.assign(
			std::begin(CubeVertices),	//頂点データ配列の先頭アドレス
			std::end(CubeVertices)		//頂点データ配列の終端アドレス
		);

		//インデックスデータの設定
		mesh.indices.assign(
			std::begin(CubeIndices),	//インデックスデータ配列の先頭アドレス
			std::end(CubeIndices)		//インデックスデータ配列の終端アドレス
		);

		//頂点数の設定
		mesh.vertexCount = mesh.vertices.size();

		//インデックス数の設定
		mesh.indexCount = mesh.indices.size();

		//メッシュデータをモデルデータに追加
		model.push_back(mesh);

		return model;	//メッシュデータ構造体を返す
	}

}

//円形平面のメッシュデータ作成関数
Model MakeCircleModel(int slice)
{
	Model model;
	Mesh  mesh;

	// 分割数が少なすぎたら何もしない
	if (slice < 3)
	{
		return model;
	}

	using namespace DirectX;

	const float radius = 0.5f;          // Quad と同じ ±0.5 のスケール
	const float twoPi = XM_2PI;

	// 頂点 / インデックスのメモリを事前確保
	mesh.vertices.reserve(static_cast<size_t>(slice) + 1);
	mesh.indices.reserve(static_cast<size_t>(slice) * 3);

	// ① 中心頂点
	{
		Vertex v{};
		v.position = Vector3(0.0f, 0.0f, 0.0f);       // 原点
		v.normal = Vector3(0.0f, 0.0f, 1.0f);       // +Z 向き
		v.uv = Vector2(0.5f, 0.5f);             // テクスチャ中央
		v.tangent = Vector3(1.0f, 0.0f, 0.0f);       // +X 方向
		v.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // 白

		mesh.vertices.push_back(v);
	}

	// ② 外周頂点
	for (int i = 0; i < slice; ++i)
	{
		float t = static_cast<float>(i) / static_cast<float>(slice);
		float angle = twoPi * t;

		float x = std::cos(angle) * radius;
		float y = std::sin(angle) * radius;

		Vertex v{};
		v.position = Vector3(x, y, 0.0f);             // XY 平面上
		v.normal = Vector3(0.0f, 0.0f, 1.0f);       // +Z 向き

		// UV：中心(0.5,0.5) から半径 0.5 の円にマッピング
		float u = 0.5f + 0.5f * std::cos(angle);
		float vTex = 0.5f - 0.5f * std::sin(angle);    // テクスチャ座標は上が 0 なので -sin

		v.uv = Vector2(u, vTex);
		v.tangent = Vector3(1.0f, 0.0f, 0.0f);        // 一律 +X
		v.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);  // 白

		mesh.vertices.push_back(v);
	}

	// ③ インデックス（三角形ファン）
	// 中心を 0、外周を 1..slice として
	// Quad の面向きに合わせて時計回りになるよう center, i1, i0 の順で追加
	for (int i = 0; i < slice; ++i)
	{
		uint32_t center = 0;
		uint32_t i0 = 1 + static_cast<uint32_t>(i);
		uint32_t i1 = 1 + static_cast<uint32_t>((i + 1) % slice); // 最後は 1 にループ

		mesh.indices.push_back(center);
		mesh.indices.push_back(i1);
		mesh.indices.push_back(i0);
	}

	// ④ カウントを設定
	mesh.vertexCount = mesh.vertices.size();
	mesh.indexCount = mesh.indices.size();

	// ⑤ モデルに追加して返す
	model.push_back(std::move(mesh));
	return model;
}

//球体のメッシュデータ作成関数
Model MakeSphereModel(int slices, int stacks)
{
	Mesh mesh;		//メッシュデータ構造体
	Model model;	//モデルデータ構造体

	//頂点データの作成
	for (int stack = 0; stack <= stacks; stack++)
	{//スタックループ
		float v = static_cast<float>(stack) / static_cast<float>(stacks);	//V座標
		float phi = (v - 0.5f) * XM_PI;							//緯度角

		float y = sin(phi);	 //Y座標
		float r = cos(phi);	 //半径

		for (int slice = 0; slice <= slices; slice++)
		{//スライスループ
			float u = static_cast<float>(slice) / static_cast<float>(slices);	//U座標
			float theta = u * XM_2PI;									//経度角

			float x = r * cos(theta);	//X座標
			float z = r * sin(theta);	//Z座標

			//頂点データの作成
			Vertex vertex{};
			vertex.position = Vector3(x * 0.5f, y * 0.5f, z * 0.5f);	//位置
			vertex.normal = Vector3(x, y, z);							//法線
			vertex.uv = Vector2(u, 1.0f - v);							//UV座標
			vertex.tangent = Vector3(-sin(theta), 0.0f, cos(theta));	//接空間
			vertex.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);			//頂点色

			mesh.vertices.push_back(vertex);	//頂点データを配列に追加
		}
	}

	//インデックスデータの作成
	int ringStride = slices + 1; //リングあたりの頂点数
	for (int stack = 0; stack < stacks; stack++)
	{//スタックループ
		for (int slice = 0; slice < slices; slice++)
		{//スライスループ
			//インデックスの計算
			uint32_t index0 = stack * ringStride + slice;
			uint32_t index1 = (stack + 1) * ringStride + slice;
			uint32_t index2 = (stack + 1) * ringStride + (slice + 1);
			uint32_t index3 = stack * ringStride + (slice + 1);

			//三角形の追加
			mesh.indices.push_back(index0);
			mesh.indices.push_back(index1);
			mesh.indices.push_back(index2);
			mesh.indices.push_back(index0);
			mesh.indices.push_back(index2);
			mesh.indices.push_back(index3);
		}
	}

	mesh.vertexCount = mesh.vertices.size();	//頂点数の設定
	mesh.indexCount = mesh.indices.size();		//インデックス数の設定

	model.push_back(mesh);	//メッシュデータをモデルデータに追加

	return model;	//メッシュデータ構造体を返す
}

//カプセルのメッシュデータ作成関数
Model MakeCapsuleModel(int slices, int stacks)
{
	//引数の補正
	if (slices < 3) slices = 3;	//スライスは最低3
	if (stacks < 4) stacks = 4;	//スタックは最低4

	//必要な構造体の生成
	Mesh cylinder;				//シリンダー部分のメッシュデータ構造体
	Mesh halfSphereTop;			//上半球の部分のメッシュデータ構造体
	Mesh halfSphereBottom;		//下半球部分のメッシュデータ構造体
	Model model;	//モデルデータ構造体

	//単位カプセルのパラメータ
	const float radius = 0.5f;		//半径
	const float halfHeight = 0.5f;	//半分の高さ

	//スタック数の内訳計算
	const int hemiStacks = stacks / 4;				//半球のスタック数
	int cylinderStacks = stacks - hemiStacks * 2;	//円柱のスタック数
	if (cylinderStacks < 1) cylinderStacks = 1;		//円柱のスタック数は最低1にする

	//頂点・インデックスデータ配列の予約
	const int ringStride = slices + 1;					//リングあたりの頂点数	
	const int cylinderRingCount = cylinderStacks + 1;	//円柱部分のリング数
	const int topHemiRingCount = hemiStacks + 1;		//上半球部分のリング数
	const int bottomHemiRingCount = hemiStacks + 1;		//下半球部分のリング数

	int estimatedVertexCount =							//頂点数の見積もり
		(cylinderRingCount + topHemiRingCount + bottomHemiRingCount) * ringStride;

	cylinder.vertices.reserve(estimatedVertexCount);			//頂点データ配列の予約
	cylinder.indices.reserve(estimatedVertexCount * 6);			//インデックスデータ配列の予約
	halfSphereTop.vertices.reserve(estimatedVertexCount);		//頂点データ配列の予約
	halfSphereTop.indices.reserve(estimatedVertexCount * 6);	//インデックスデータ配列の予約
	halfSphereBottom.vertices.reserve(estimatedVertexCount);	//頂点データ配列の予約
	halfSphereBottom.indices.reserve(estimatedVertexCount * 6);//インデックスデータ配列の予約

	//頂点データの作成
	//シリンダ部分の頂点データ作成
	for (int stack = 0; stack <= cylinderStacks; stack++)
	{
		float t = static_cast<float>(stack) / static_cast<float>(cylinderStacks);	//スタック比率
		float y = -halfHeight + t * (halfHeight * 2.0f);							//Y座標

		for (int slice = 0; slice <= slices; slice++)
		{
			float u = static_cast<float>(slice) / static_cast<float>(slices);	//U座標
			float theta = u * XM_2PI;											//経度角

			float x = radius * cos(theta);	//X座標
			float z = radius * sin(theta);	//Z座標

			Vertex vertex{};
			vertex.position = Vector3(x, y, z);						//位置
			vertex.normal = Vector3(x / radius, 0.0f, z / radius);		//法線
			vertex.uv = Vector2(u, t);									//UV座標

			float tx = -radius * sin(theta);	//接空間X
			float tz = radius * cos(theta);		//接空間Z
			vertex.tangent = Vector3(tx, 0.0f, tz);					//接空間

			vertex.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);			//頂点色

			cylinder.vertices.push_back(vertex);	//頂点データを配列に追加
		}
	}

	//上半球部分の頂点データ作成
	for (int stack = 0; stack <= hemiStacks; stack++)
	{
		float v = static_cast<float>(stack) / static_cast<float>(hemiStacks);	//V座標
		float phi = v * XM_PIDIV2;												//緯度角

		float sinPhi = sin(phi);	//sinφ
		float cosPhi = cos(phi);	//cosφ

		float y = radius * sinPhi;	//Y座標

		float ringR = cosPhi * radius;	//リングの半径

		for (int slice = 0; slice <= slices; slice++)
		{
			float u = static_cast<float>(slice) / static_cast<float>(slices);	//U座標
			float theta = u * XM_2PI;											//経度角

			float x = ringR * cos(theta);	//X座標
			float z = ringR * sin(theta);	//Z座標

			float nx = cosPhi * cos(theta);	//法線X
			float ny = sinPhi;				//法線Y
			float nz = cosPhi * sin(theta);	//法線Z

			Vertex vertex{};
			vertex.position = Vector3(x, y, z);				//位置
			vertex.normal = Vector3(nx, ny, nz);				//法線
			vertex.uv = Vector2(u, 1.0f - (v * 0.5f + 0.5f));	//UV座標

			float tx = -ringR * sin(theta);		//接空間X
			float tz = ringR * cos(theta);		//接空間Z
			vertex.tangent = Vector3(tx, 0.0f, tz);			//接空間

			vertex.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);	//頂点色

			halfSphereTop.vertices.push_back(vertex);	//頂点データを配列に追加
		}
	}

	//下半球部分の頂点データ作成
	for (int stack = 0; stack <= hemiStacks; stack++)
	{
		float v = static_cast<float>(stack) / static_cast<float>(hemiStacks);	//V座標
		float phi = v * XM_PIDIV2;												//緯度角

		float sinPhi = sin(phi);	//sinφ
		float cosPhi = cos(phi);	//cosφ

		float y = -sinPhi * radius;	//Y座標

		float ringR = cosPhi * radius;	//リングの半径

		for (int slice = 0; slice <= slices; slice++)
		{
			float u = static_cast<float>(slice) / static_cast<float>(slices);	//U座標
			float theta = u * XM_2PI;											//経度角

			float x = ringR * cos(theta);	//X座標
			float z = ringR * sin(theta);	//Z座標

			float nx = cosPhi * cos(theta);	//法線X
			float ny = -sinPhi;				//法線Y
			float nz = cosPhi * sin(theta);	//法線Z

			Vertex vertex{};
			vertex.position = Vector3(x, y, z);				//位置
			vertex.normal = Vector3(nx, ny, nz);				//法線
			vertex.uv = Vector2(u, (v * 0.5f));				//UV座標

			float tx = -ringR * sin(theta);		//接空間X
			float tz = ringR * cos(theta);		//接空間Z
			vertex.tangent = Vector3(tx, 0.0f, tz);			//接空間

			vertex.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);	//頂点色

			halfSphereBottom.vertices.push_back(vertex);	//頂点データを配列に追加
		}
	}

	//インデックスデータの作成
	//シリンダ部分のインデックスデータ作成
	for (int stack = 0; stack < cylinderStacks; stack++)
	{
		for (int slice = 0; slice < slices; slice++)
		{
			uint32_t i0 = stack * ringStride + slice;
			uint32_t i1 = i0 + 1;
			uint32_t i2 = i0 + ringStride;
			uint32_t i3 = i2 + 1;

			cylinder.indices.push_back(i0);
			cylinder.indices.push_back(i1);
			cylinder.indices.push_back(i2);
			cylinder.indices.push_back(i1);
			cylinder.indices.push_back(i3);
			cylinder.indices.push_back(i2);
		}
	}

	//上半球部分のインデックスデータ作成
	const int ringCountTop = hemiStacks + 1;
	for (int stack = 0; stack < ringCountTop - 1; stack++)
	{
		for (int slice = 0; slice < slices; slice++)
		{
			uint32_t i0 = stack * ringStride + slice;
			uint32_t i1 = i0 + 1;
			uint32_t i2 = i0 + ringStride;
			uint32_t i3 = i2 + 1;
			halfSphereTop.indices.push_back(i0);
			halfSphereTop.indices.push_back(i1);
			halfSphereTop.indices.push_back(i2);
			halfSphereTop.indices.push_back(i1);
			halfSphereTop.indices.push_back(i3);
			halfSphereTop.indices.push_back(i2);
		}
	}

	//下半球部分のインデックスデータ作成
	const int ringCountBottom = hemiStacks + 1;
	for (int stack = 0; stack < ringCountBottom - 1; stack++)
	{
		for (int slice = 0; slice < slices; slice++)
		{
			uint32_t i0 = stack * ringStride + slice;
			uint32_t i1 = i0 + 1;
			uint32_t i2 = i0 + ringStride;
			uint32_t i3 = i2 + 1;
			halfSphereBottom.indices.push_back(i0);
			halfSphereBottom.indices.push_back(i1);
			halfSphereBottom.indices.push_back(i2);
			halfSphereBottom.indices.push_back(i1);
			halfSphereBottom.indices.push_back(i3);
			halfSphereBottom.indices.push_back(i2);
		}
	}

	//頂点数とインデックス数の設定
	cylinder.vertexCount = cylinder.vertices.size();					//頂点数の設定
	cylinder.indexCount = cylinder.indices.size();						//インデックス数の設定
	halfSphereTop.vertexCount = halfSphereTop.vertices.size();			//頂点数の設定
	halfSphereTop.indexCount = halfSphereTop.indices.size();			//インデックス数の設定
	halfSphereBottom.vertexCount = halfSphereBottom.vertices.size();	//頂点数の設定
	halfSphereBottom.indexCount = halfSphereBottom.indices.size();		//インデックス数の設定

	//モデルデータにメッシュデータを追加
	model.push_back(cylinder);			//円柱部分のメッシュデータをモデルデータに追加
	model.push_back(halfSphereTop);		//上半球部分のメッシュデータをモデルデータに追加
	model.push_back(halfSphereBottom);	//下半球部分のメッシュデータをモデルデータに追加

	return model;	//メッシュデータ構造体を返す
}

//円柱のメッシュデータ作成関数
Model MakeCylinderModel(int slices, int stacks)
{
	Mesh mesh;	//シリンダー部分のメッシュデータ構造体
	Model model;	//モデルデータ構造体

	const float radius = 0.5f;		//半径
	const float halfHeight = 0.5f;	//半分の高さ

	const int hemiStacks = stacks / 4;				//半球のスタック数
	int cylinderStacks = stacks - hemiStacks * 2;	//円柱のスタック数
	if (cylinderStacks < 1) cylinderStacks = 1;		//円柱のスタック数は最低1にする

	const int ringStride = slices + 1; //リングあたりの頂点数	

	const int cylinderRingCount = cylinderStacks + 1;	//円柱部分のリング数

	int estimatedVertexCount = cylinderRingCount * ringStride;	//頂点数の見積もり

	mesh.vertices.reserve(estimatedVertexCount);		//頂点データ配列の予約
	mesh.indices.reserve(estimatedVertexCount * 6);	//インデックスデータ配列の予約

	//頂点データの作成
	//シリンダ部分の頂点データ作成
	for (int stack = 0; stack <= cylinderStacks; stack++)
	{
		float t = static_cast<float>(stack) / static_cast<float>(cylinderStacks);	//スタック比率
		float y = -halfHeight + t * (halfHeight * 2.0f);							//Y座標

		for (int slice = 0; slice <= slices; slice++)
		{
			float u = static_cast<float>(slice) / static_cast<float>(slices);	//U座標
			float theta = u * XM_2PI;									//経度角

			float x = radius * cos(theta);	//X座標
			float z = radius * sin(theta);	//Z座標

			Vertex vertex{};
			vertex.position = Vector3(x, y, z);						//位置
			vertex.normal = Vector3(x / radius, 0.0f, z / radius);		//法線
			vertex.uv = Vector2(u, t);									//UV座標

			float tx = -radius * sin(theta);	//接空間X
			float tz = radius * cos(theta);		//接空間Z
			vertex.tangent = Vector3(tx, 0.0f, tz);					//接空間

			vertex.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);			//頂点色

			mesh.vertices.push_back(vertex);	//頂点データを配列に追加
		}
	}

	//インデックスデータの作成
	for (int stack = 0; stack < cylinderStacks; stack++)
	{
		for (int slice = 0; slice < slices; slice++)
		{
			uint32_t i0 = stack * ringStride + slice;
			uint32_t i1 = i0 + 1;
			uint32_t i2 = i0 + ringStride;
			uint32_t i3 = i2 + 1;

			mesh.indices.push_back(i0);
			mesh.indices.push_back(i1);
			mesh.indices.push_back(i2);
			mesh.indices.push_back(i1);
			mesh.indices.push_back(i3);
			mesh.indices.push_back(i2);
		}
	}

	//頂点数とインデックス数の設定
	mesh.vertexCount = mesh.vertices.size();					//頂点数の設定
	mesh.indexCount = mesh.indices.size();						//インデックス数の設定

	model.push_back(mesh);			//円柱部分のメッシュデータをモデルデータに追加

	return model;	//メッシュデータ構造体を返す
}