#include "BTTask_FindPatrolPoint.h"

#include "GuardAIController.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"

UBTTask_FindPatrolPoint::UBTTask_FindPatrolPoint()
{
	NodeName = TEXT("Find Patrol Point");
}

EBTNodeResult::Type UBTTask_FindPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AGuardAIController* Controller = Cast<AGuardAIController>(OwnerComp.GetAIOwner());
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetNavigationSystem(&OwnerComp);
	if (!Controller || !NavSystem) { return EBTNodeResult::Failed; }

	const float PatrolRadius = Controller->GetPatrolRadius();
	if (PatrolRadius <= 0.f) { return EBTNodeResult::Failed; }

	const FVector Origin = Controller->GetNavAgentLocation();
	FNavLocation ResultLocation;
	bool bFound = false;

	// A few tries for a point that is not right next to the guard; the last found one is used anyway
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		FNavLocation Candidate;
		if (NavSystem->GetRandomReachablePointInRadius(Origin, PatrolRadius, Candidate))
		{
			ResultLocation = Candidate;
			bFound = true;
			if (FVector::Dist2D(Origin, Candidate.Location) >= MinDistance) { break; }
		}
	}
	if (!bFound) { return EBTNodeResult::Failed; }

	Controller->GetBlackboardComponent()->SetValueAsVector(Controller->GetPatrolTargetKey(), ResultLocation.Location);
	return EBTNodeResult::Succeeded;
}

FString UBTTask_FindPatrolPoint::GetStaticDescription() const
{
	return FString::Printf(TEXT("Random reachable point, at least %.0f cm away"), MinDistance);
}
