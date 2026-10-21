#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GuardAIController.generated.h"

/**
 * Controller of the museum guard (the "ANPCAIController" of the lab guide).
 * The Blueprint child (BP_GuardAIController) starts the behaviour tree in Event On Possess.
 * The controller keeps the names of the Blackboard keys, so the C++ code (the guard and the
 * behaviour tree tasks) and the Blackboard asset agree on them in one place.
 */
UCLASS()
class BULLET_GALLERY_API AGuardAIController : public AAIController
{
	GENERATED_BODY()

public:
	AGuardAIController();

	float GetPatrolRadius() const { return PatrolRadius; }
	const FName& GetPatrolTargetKey() const { return PatrolTargetKey; }
	const FName& GetHasLineOfSightKey() const { return HasLineOfSightKey; }
	const FName& GetDetectedEnemyKey() const { return DetectedEnemyKey; }
	const FName& GetLastKnownLocationKey() const { return LastKnownLocationKey; }
	const FName& GetIncomingShotKey() const { return IncomingShotKey; }
	const FName& GetShotOriginKey() const { return ShotOriginKey; }

protected:
	/** A random patrol point is picked inside this radius around the guard, cm */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AI")
	float PatrolRadius = 1500.f;

	// ---------- Blackboard key names (must match the keys of BB_MuseumGuard) ----------

	/** Vector: the next patrol point */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AI|Blackboard")
	FName PatrolTargetKey = TEXT("PatrolTarget");

	/** Bool: the guard sees the player right now */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AI|Blackboard")
	FName HasLineOfSightKey = TEXT("HasLineOfSight");

	/** Object (Actor): the player the guard saw recently; cleared when the guard forgets them */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AI|Blackboard")
	FName DetectedEnemyKey = TEXT("DetectedEnemy");

	/** Vector: where the player was seen last */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AI|Blackboard")
	FName LastKnownLocationKey = TEXT("LastKnownLocation");

	/** Bool: the player has just fired at the guard (starts the dodge branch) */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AI|Blackboard")
	FName IncomingShotKey = TEXT("IncomingShot");

	/** Vector: where that shot was fired from */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AI|Blackboard")
	FName ShotOriginKey = TEXT("ShotOrigin");
};
