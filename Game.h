#pragma once

#include <memory>

// ゲーム全体の初期化、更新、描画、終了までの流れを管理するクラス。
class Game {
public:
	Game();
	~Game();

	void Run();

private:
	struct State;

	void Initialize();
	void Update();
	void Draw();
	void Finalize();

	std::unique_ptr<State> state_;
};
