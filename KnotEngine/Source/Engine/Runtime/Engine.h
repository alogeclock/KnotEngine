#pragma once

enum class EEngineType
{
	Editor,
	Game,
};

struct FEngineInitParams
{
	HINSTANCE hInstance;
	int nShowCmd;
    EEngineType EngineType;
};

class UEngine
{
public:
    
};
