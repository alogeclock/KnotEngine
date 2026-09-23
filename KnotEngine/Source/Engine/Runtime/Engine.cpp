#include "Engine.h"
#include "World/World.h"
#include "Object/ReferenceCollector.h"

UEngine* GEngine = nullptr;

UEngine::UEngine()
{
}

UEngine::~UEngine()
{
	while (!WorldContexts.empty())
	{
		DestroyWorldContext(WorldContexts.back().ContextId);
	}
	AssetManager.Release();
}

void UEngine::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	for (const auto& Context : WorldContexts)
	{
		Collector.AddReferencedObject(Context.World);
	}
	AssetManager.AddReferencedObjects(Collector); // FAssetManager는 UObject가 아니라 일반 C++ 객체이므로, 자동으로 수집되지 않는다.
}

// 실제 Engine 인스턴스의 Asset 서비스를 시작하며 CDO 생성자에서는 런타임 작업을 수행하지 않는다.
void UEngine::Startup(FWindowsApplication&)
{
	check(!IsTemplate());
	AssetManager.Create();
}

void UEngine::Shutdown()
{
	AssetManager.Release();
}

uint64 UEngine::CreateWorldContext(EWorldType WorldType)
{
	UWorld* World = NewObject<UWorld>(this);
	const uint64 Id = NextContextId++;
	WorldContexts.push_back({ Id, WorldType, World });
	return Id;
}

UWorld* UEngine::FindWorld(uint64 ContextId) const
{
	for (const auto& Context : WorldContexts)
	{
		if (Context.ContextId == ContextId)
		{
			return Context.World.Get();
		}
	}
	return nullptr;
}

void UEngine::DestroyWorldContext(uint64 ContextId)
{
	for (SIZE_T Index = 0; Index < WorldContexts.size(); ++Index)
	{
		if (WorldContexts[Index].ContextId == ContextId)
		{
			UWorld* World = WorldContexts[Index].World.Get();
			WorldContexts.erase(WorldContexts.begin() + Index);
			GUObjectManager.Destroy(World);
			return;
		}
	}
}

UWorld* UEngine::GetWorld() const
{
	return WorldContexts.empty() ? nullptr : WorldContexts.front().World.Get();
}
