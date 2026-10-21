#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_ShootTarget.generated.h"

/**
 * The guard fires the revolver once at the actor stored in the Blackboard key (DetectedEnemy).
 * Fails if the target is farther than the weapon's effective range, so the tree first brings
 * the guard closer (BTT_MoveToEffectiveRange). A Wait node after it sets the pause between shots.
 */
UCLASS()
class BULLET_GALLERY_API UBTTask_ShootTarget : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_ShootTarget();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** The target may be a little farther than the effective range (it keeps moving) */
	UPROPERTY(EditAnywhere, Category = "Shoot", meta = (ClampMin = 1))
	float RangeTolerance = 1.25f;
};
