#include "Lab6FbxImporter.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"

FString ULab6FbxImporter::ImportAll() const
{
	TArray<UAssetImportTask*> Tasks;
	for (const FString& File : SourceFiles)
	{
		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = File;
		Task->DestinationPath = DestinationPath;
		Task->bAutomated = true;
		Task->bReplaceExisting = true;
		Task->bSave = true;
		Tasks.Add(Task);
	}

	FAssetToolsModule::GetModule().Get().ImportAssetTasks(Tasks);

	FString Report;
	for (const UAssetImportTask* Task : Tasks)
	{
		Report += FString::Printf(TEXT("%s -> %d assets: "), *FPaths::GetCleanFilename(Task->Filename), Task->GetObjects().Num());
		for (UObject* Object : Task->GetObjects())
		{
			Report += Object->GetPathName() + TEXT("; ");
		}
	}
	return Report;
}
