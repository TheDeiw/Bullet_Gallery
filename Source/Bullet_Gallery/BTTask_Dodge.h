#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_Dodge.generated.h"

/**
 * Dodge from a shot. The guard sets the IncomingShot key when the player fires at it;
 * this task decides whether the dodge succeeds. The chance grows with the distance to the shooter:
 * a shot from far away is easier to notice in time and to step out of its way.
 * On success the guard jumps to a free side and cannot be hit during the jump.
 */
UCLASS()
class BULLET_GALLERY_API UBTTask_Dodge : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_Dodge();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual FString GetStaticDescription() const override;

	/** Chance of a successful dodge for a shot from this distance (0..1) */
	float GetDodgeChance(float Distance) const;

protected:
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** Chance at point-blank range */
	UPROPERTY(EditAnywhere, Category = "Dodge", meta = (ClampMin = 0, ClampMax = 1))
	float MinChance = 0.05f;

	/** Chance at FullChanceDistance and farther */
	UPROPERTY(EditAnywhere, Category = "Dodge", meta = (ClampMin = 0, ClampMax = 1))
	float MaxChance = 0.9f;

	/** From this distance on the chance is MaxChance; closer it falls proportionally, cm */
	UPROPERTY(EditAnywhere, Category = "Dodge", meta = (ClampMin = 1))
	float FullChanceDistance = 2000.f;

private:
	struct FDodgeMemory
	{
		float TimeLeft = 0.f;
	};
};
