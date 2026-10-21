#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindPatrolPoint.generated.h"

/**
 * Patrol task (the "BTTaskPatrol" of the lab guide): finds a random point of the navigation mesh
 * that the guard can reach inside the controller's patrol radius and writes it to the PatrolTarget key.
 * The next nodes of the sequence (Move To, look around, Wait) take the guard there.
 */
UCLASS()
class BULLET_GALLERY_API UBTTask_FindPatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindPatrolPoint();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** A new point is at least this far from the guard, so the patrol does not stand on one spot, cm */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	float MinDistance = 400.f;

	/** Attempts to find a point farther than MinDistance */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = 1))
	int32 MaxAttempts = 8;
};
