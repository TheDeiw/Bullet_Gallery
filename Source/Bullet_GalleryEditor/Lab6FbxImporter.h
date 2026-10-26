#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Lab6FbxImporter.generated.h"

/**
 * Imports the Lab 6 exhibit FBX files (Lab6/Blender/*.fbx) into DestinationPath without a dialog.
 * Same trigger as ULab6AssetBuilder: set bImportRequested on the class default object
 * (/Script/Bullet_GalleryEditor.Default__Lab6FbxImporter); the editor module runs the import and writes LastResult.
 */
UCLASS()
class ULab6FbxImporter : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Import")
	bool bImportRequested = false;

	UPROPERTY(EditAnywhere, Category = "Import")
	TArray<FString> SourceFiles;

	UPROPERTY(EditAnywhere, Category = "Import")
	FString DestinationPath = TEXT("/Game/_MuseumProject/Lab6/Exhibits");

	UPROPERTY(VisibleAnywhere, Category = "Import")
	FString LastResult;

	FString ImportAll() const;
};
