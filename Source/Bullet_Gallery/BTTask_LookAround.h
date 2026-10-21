#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_LookAround.generated.h"

/**
 * The guard stops and looks around: turns to one side, to the other and back to where it faced.
 * This is the "stop and look around to choose the route" part of the search for the player:
 * while it turns, the sight cone sweeps the room, and the perception can notice the player.
 */
UCLASS()
class BULLET_GALLERY_API UBTTask_LookAround : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_LookAround();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual FString GetStaticDescription() const override;

protected:
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** How far the guard turns to each side, degrees */
	UPROPERTY(EditAnywhere, Category = "Look Around", meta = (ClampMin = 10, ClampMax = 180))
	float LookAngle = 75.f;

	/** Turning speed, degrees per second */
	UPROPERTY(EditAnywhere, Category = "Look Around", meta = (ClampMin = 10))
	float TurnSpeed = 110.f;

	/** Pause at each side, seconds */
	UPROPERTY(EditAnywhere, Category = "Look Around", meta = (ClampMin = 0))
	float HoldTime = 0.6f;

private:
	struct FLookAroundMemory
	{
		/** Yaw the guard faced when the task started */
		float StartYaw = 0.f;
		/** 0 = first side, 1 = other side, 2 = back to the start */
		int32 Step = 0;
		float HoldLeft = 0.f;
	};
};
