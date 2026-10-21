#include "BTTask_Dodge.h"

#include "GuardAIController.h"
#include "GuardNPC.h"

#include "BehaviorTree/BlackboardComponent.h"

UBTTask_Dodge::UBTTask_Dodge()
{
	NodeName = TEXT("Dodge");
	bNotifyTick = true;
}

uint16 UBTTask_Dodge::GetInstanceMemorySize() const
{
	return sizeof(FDodgeMemory);
}

float UBTTask_Dodge::GetDodgeChance(float Distance) const
{
	// Proportional to the distance of the shot, between MinChance and MaxChance
	return FMath::Lerp(MinChance, MaxChance, FMath::Clamp(Distance / FullChanceDistance, 0.f, 1.f));
}

EBTNodeResult::Type UBTTask_Dodge::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AGuardAIController* Controller = Cast<AGuardAIController>(OwnerComp.GetAIOwner());
	AGuardNPC* Guard = Controller ? Cast<AGuardNPC>(Controller->GetPawn()) : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Guard || !Blackboard) { return EBTNodeResult::Failed; }

	// The shot is handled now, whatever the result
	Blackboard->SetValueAsBool(Controller->GetIncomingShotKey(), false);
	if (Guard->IsDead() || Guard->IsDodging()) { return EBTNodeResult::Failed; }

	const FVector ShotOrigin = Blackboard->GetValueAsVector(Controller->GetShotOriginKey());
	const float Distance = FVector::Dist(Guard->GetActorLocation(), ShotOrigin);
	const float Chance = GetDodgeChance(Distance);
	const bool bLucky = FMath::FRand() < Chance;

	// Jump to a random side; if it is blocked, to the other one
	float Side = FMath::RandBool() ? 1.f : -1.f;
	if (!Guard->CanDodgeTo(Side))
	{
		Side = -Side;
	}
	const bool bSuccess = bLucky && Guard->CanDodgeTo(Side);

	UE_LOG(LogTemp, Log, TEXT("Guard dodge: distance %.0f cm, chance %.0f%%, %s"),
		Distance, Chance * 100.f, bSuccess ? TEXT("dodged") : TEXT("failed"));

	if (!bSuccess) { return EBTNodeResult::Failed; }

	Guard->Dodge(Side);
	CastInstanceNodeMemory<FDodgeMemory>(NodeMemory)->TimeLeft = Guard->GetDodgeDuration();
	return EBTNodeResult::InProgress;
}

void UBTTask_Dodge::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FDodgeMemory* Memory = CastInstanceNodeMemory<FDodgeMemory>(NodeMemory);
	Memory->TimeLeft -= DeltaSeconds;
	if (Memory->TimeLeft <= 0.f)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

FString UBTTask_Dodge::GetStaticDescription() const
{
	return FString::Printf(TEXT("Chance %.0f%% .. %.0f%% (full at %.0f cm)"), MinChance * 100.f, MaxChance * 100.f, FullChanceDistance);
}
