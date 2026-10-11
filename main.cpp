#include <Windows.h>
#include <cassert>

#include "Game.h"

int WINAPI WinMain(
	_In_ HINSTANCE,
	_In_opt_ HINSTANCE,
	_In_ LPSTR,
	_In_ int) {
	assert(false && "assertのテストだよ");
	Game game;
	game.Run();
	return 0;
}