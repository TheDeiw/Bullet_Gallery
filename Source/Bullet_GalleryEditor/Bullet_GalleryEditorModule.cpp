#include "GuardBehaviorTreeBuilder.h"

#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"

/** Editor module of the project: watches the build request of the guard's AI assets */
class FBullet_GalleryEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FModuleManager::Get().LoadModuleChecked(TEXT("BehaviorTreeEditor"));

		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			UGuardBehaviorTreeBuilder* Builder = GetMutableDefault<UGuardBehaviorTreeBuilder>();
			if (Builder->bBuildRequested)
			{
				Builder->bBuildRequested = false;
				Builder->LastResult = UGuardBehaviorTreeBuilder::BuildGuardAssets();
				UE_LOG(LogTemp, Display, TEXT("GuardBT: %s"), *Builder->LastResult);
			}
			return true;
		}), 0.5f);
	}

	virtual void ShutdownModule() override
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}

private:
	FTSTicker::FDelegateHandle TickerHandle;
};

IMPLEMENT_MODULE(FBullet_GalleryEditorModule, Bullet_GalleryEditor)
