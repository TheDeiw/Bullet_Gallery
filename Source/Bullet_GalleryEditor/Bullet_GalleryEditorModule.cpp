#include "GuardBehaviorTreeBuilder.h"
#include "Lab6AssetBuilder.h"
#include "Lab6FbxImporter.h"

#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"

/** Editor module of the project: watches the build requests of the guard's AI assets (Lab 5) and the Lab 6 assets */
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
			ULab6AssetBuilder* Lab6Builder = GetMutableDefault<ULab6AssetBuilder>();
			if (Lab6Builder->bBuildRequested)
			{
				Lab6Builder->bBuildRequested = false;
				Lab6Builder->LastResult = Lab6Builder->BuildAll();
				UE_LOG(LogTemp, Display, TEXT("Lab6Assets: %s"), *Lab6Builder->LastResult);
			}
			ULab6FbxImporter* Importer = GetMutableDefault<ULab6FbxImporter>();
			if (Importer->bImportRequested)
			{
				Importer->bImportRequested = false;
				Importer->LastResult = Importer->ImportAll();
				UE_LOG(LogTemp, Display, TEXT("Lab6Fbx: %s"), *Importer->LastResult);
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
