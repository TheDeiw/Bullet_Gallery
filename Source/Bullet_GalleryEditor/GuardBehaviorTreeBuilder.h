#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GuardBehaviorTreeBuilder.generated.h"

/**
 * Builds the guard's AI assets in /Game/_MuseumProject/AI: the Blackboard BB_MuseumGuard and the
 * Behavior Tree BT_MuseumGuard with its editor graph, exactly as they would be made by hand in the
 * Behavior Tree editor (so the tree opens, shows and can be edited there as usual).
 *
 * The build is requested by setting bBuildRequested on the class default object
 * (/Script/Bullet_GalleryEditor.Default__GuardBehaviorTreeBuilder); the editor module checks the flag
 * twice a second, runs the build and writes the outcome to LastResult.
 */
UCLASS()
class UGuardBehaviorTreeBuilder : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Build")
	bool bBuildRequested = false;

	UPROPERTY(VisibleAnywhere, Category = "Build")
	FString LastResult;

	/** Creates or rebuilds the Blackboard and the Behavior Tree; returns a short report */
	static FString BuildGuardAssets();
};
