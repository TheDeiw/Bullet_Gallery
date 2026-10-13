#include "SpawnEnemiesManager.h"

#include "Enemy.h"
#include "SpawnEnemyTargetPoint.h"

#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"

ASpawnEnemiesManager::ASpawnEnemiesManager()
{
	// Spawning happens once in BeginPlay, so Tick is not needed
	PrimaryActorTick.bCanEverTick = false;
}

void ASpawnEnemiesManager::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World) { return; }

	// All spawn points of the level. A slow call, so it is made only once
	TArray<AActor*> SpawnPoints;
	UGameplayStatics::GetAllActorsOfClass(World, ASpawnEnemyTargetPoint::StaticClass(), SpawnPoints);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (AActor* Actor : SpawnPoints)
	{
		const ASpawnEnemyTargetPoint* Point = Cast<ASpawnEnemyTargetPoint>(Actor);
		const TSubclassOf<AEnemy> EnemyClass = Point->EnemyClassOverride ? Point->EnemyClassOverride : BaseEnemyBP;
		if (!EnemyClass) { continue; }

		// The point stands on the floor, while a character's origin is in the middle of its capsule
		const float HalfHeight = EnemyClass->GetDefaultObject<AEnemy>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Location = Point->GetActorLocation() + FVector(0.f, 0.f, HalfHeight);
		const FRotator Rotation(0.f, Point->GetActorRotation().Yaw, 0.f);

		if (AEnemy* Enemy = World->SpawnActor<AEnemy>(EnemyClass, Location, Rotation, SpawnParams))
		{
			Enemy->SetPatrolRoute(Point->GetWorldPatrolRoute(), Point->bLoopRoute);
		}
	}
}
