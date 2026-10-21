#include "BTTask_ShootTarget.h"

#include "GuardNPC.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"

UBTTask_ShootTarget::UBTTask_ShootTarget()
{
	NodeName = TEXT("Shoot Target");

	// Only actor keys can be picked
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_ShootTarget, BlackboardKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_ShootTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	AGuardNPC* Guard = Controller ? Cast<AGuardNPC>(Controller->GetPawn()) : nullptr;
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Guard || Guard->IsDead() || !Blackboard) { return EBTNodeResult::Failed; }

	AActor* Target = Cast<AActor>(Blackboard->GetValue<UBlackboardKeyType_Object>(BlackboardKey.GetSelectedKeyID()));
	if (!Target) { return EBTNodeResult::Failed; }

	// Too far for the revolver: let the tree move the guard closer first
	if (FVector::Dist(Guard->GetActorLocation(), Target->GetActorLocation()) > Guard->GetWeaponEffectiveRange() * RangeTolerance)
	{
		return EBTNodeResult::Failed;
	}

	Guard->FireAt(Target);
	return EBTNodeResult::Succeeded;
}

FString UBTTask_ShootTarget::GetStaticDescription() const
{
	return FString::Printf(TEXT("Fire at: %s"), *GetSelectedBlackboardKey().ToString());
}
