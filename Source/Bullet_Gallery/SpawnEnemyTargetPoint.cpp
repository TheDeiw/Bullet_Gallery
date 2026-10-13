#include "SpawnEnemyTargetPoint.h"

TArray<FVector> ASpawnEnemyTargetPoint::GetWorldPatrolRoute() const
{
	TArray<FVector> Route;
	Route.Reserve(PatrolPoints.Num() + 1);

	// The enemy comes back to where it appeared
	Route.Add(GetActorLocation());
	for (const FVector& LocalPoint : PatrolPoints)
	{
		Route.Add(GetActorTransform().TransformPosition(LocalPoint));
	}
	return Route;
}
