#pragma once

class ObjectStatics
{
public:
	static uint32 GenerateUUID() { return NextUUID++; }

private:
	static inline uint32 NextUUID = 1; // 0 for Invalid UUID 
};