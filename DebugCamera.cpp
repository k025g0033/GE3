#include "DebugCamera.h"
#include "Input.h"
#include "MathFunctions.h"

void DebugCamera::Initialize() {
    matRot_ = MakeIdentity4x4();
    viewMatrix_ = MakeIdentity4x4();

    projectionMatrix_ = MakePerspectiveFovMatrix(
        0.45f,
        1280.0f / 720.0f,
        0.1f,
        100.0f);
}

void DebugCamera::Update(const Input& input) {
    float rotateX = 0.0f;
    float rotateY = 0.0f;
    const float rotateSpeed = 0.02f;

    if (input.IsKeyPressed(DIK_UP)) {
        rotateX += rotateSpeed;
    }
    if (input.IsKeyPressed(DIK_DOWN)) {
        rotateX -= rotateSpeed;
    }
    if (input.IsKeyPressed(DIK_LEFT)) {
        rotateY -= rotateSpeed;
    }
    if (input.IsKeyPressed(DIK_RIGHT)) {
        rotateY += rotateSpeed;
    }

    const DIMOUSESTATE2& mouse = input.GetMouseState();
    if (mouse.rgbButtons[1] & 0x80) {
        const float mouseRotateSpeed = 0.005f;
        rotateX += mouse.lY * mouseRotateSpeed;
        rotateY += mouse.lX * mouseRotateSpeed;
    }

    const float zoomSpeed = 0.01f;
    translation_.z += mouse.lZ * zoomSpeed;

    // 今回フレーム分の回転行列
    Matrix4x4 matRotDelta = MakeIdentity4x4();
    matRotDelta = Multiply(
        matRotDelta,
        MakeRotateXMatrix(rotateX));
    matRotDelta = Multiply(
        matRotDelta,
        MakeRotateYMatrix(rotateY));

    // これまでの回転へ今回の回転を追加
    matRot_ = Multiply(matRotDelta, matRot_);

    Matrix4x4 translationMatrix =
        MakeTranslateMatrix(translation_);

    Matrix4x4 cameraMatrix =
        Multiply(translationMatrix, matRot_);

    viewMatrix_ = Inverse(cameraMatrix);
}
