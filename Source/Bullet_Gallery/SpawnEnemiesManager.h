#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpawnEnemiesManager.generated.h"

class AEnemy;

/**
 * Spawns an enemy at every SpawnEnemyTargetPoint of the level when the game starts
 * and gives it the route stored in that point.
 */
UCLASS()
class BULLET_GALLERY_API ASpawnEnemiesManager : public AActor
{
	GENERATED_BODY()

public:
	ASpawnEnemiesManager();

	/** Enemy class spawned at the points (BP_Enemy) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TSubclassOf<AEnemy> BaseEnemyBP;

protected:
	virtual void BeginPlay() override;
};
