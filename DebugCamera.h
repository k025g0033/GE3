#pragma once
#include "MathTypes.h"
#include "Input.h"

class DebugCamera{
public:

	void Initialize();

	void Update(const Input& input);

	const Matrix4x4& GetViewMatrix() const {
		return viewMatrix_;
	}

	const Matrix4x4& GetProjectionMatrix() const {
		return projectionMatrix_;
	}

private:
	
	// X,Y,Z軸回りのローカル回転角
	Matrix4x4 matRot_ = {};
	// ローカル座標
	Vector3 translation_ = { 0,0,-5 };
	// ビュー行列
	Matrix4x4 viewMatrix_ = {};
	// 射影行列
	Matrix4x4 projectionMatrix_ = {};
};

