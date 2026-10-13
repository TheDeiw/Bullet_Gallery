#pragma once

#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include "SpawnEnemyTargetPoint.generated.h"

class AEnemy;

/**
 * A place on the level where an enemy appears.
 * It also holds the enemy's patrol route: the points are stored relative to this actor
 * and can be dragged in the viewport by their diamond handles.
 */
UCLASS()
class BULLET_GALLERY_API ASpawnEnemyTargetPoint : public ATargetPoint
{
	GENERATED_BODY()

public:
	/** Route in world space; it starts at the spawn point itself */
	TArray<FVector> GetWorldPatrolRoute() const;

	/** Enemy to spawn here; empty = the manager's BaseEnemyBP */
	UPROPERTY(EditAnywhere, Category = "Spawn")
	TSubclassOf<AEnemy> EnemyClassOverride;

	/** Route points relative to this actor */
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (MakeEditWidget))
	TArray<FVector> PatrolPoints;

	/** true: A-B-C-A-B-C..., false: A-B-C-B-A... */
	UPROPERTY(EditAnywhere, Category = "Spawn")
	bool bLoopRoute = true;
};
