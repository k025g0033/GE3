#pragma once
// 生成順の逆に破棄されるため、描画リソースより先に生成する。
class D3DResourceLeakChecker {
public:
    ~D3DResourceLeakChecker();
};
