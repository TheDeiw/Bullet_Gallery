#include "BTTask_LookAround.h"

#include "AIController.h"
#include "GameFramework/Pawn.h"

UBTTask_LookAround::UBTTask_LookAround()
{
	NodeName = TEXT("Look Around");
	bNotifyTick = true;
}

uint16 UBTTask_LookAround::GetInstanceMemorySize() const
{
	return sizeof(FLookAroundMemory);
}

EBTNodeResult::Type UBTTask_LookAround::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn) { return EBTNodeResult::Failed; }

	FLookAroundMemory* Memory = CastInstanceNodeMemory<FLookAroundMemory>(NodeMemory);
	Memory->StartYaw = Pawn->GetActorRotation().Yaw;
	Memory->Step = 0;
	Memory->HoldLeft = 0.f;

	// Runs in TickTask until the guard has looked both ways
	return EBTNodeResult::InProgress;
}

void UBTTask_LookAround::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	FLookAroundMemory* Memory = CastInstanceNodeMemory<FLookAroundMemory>(NodeMemory);

	// Pause at the side, looking there
	if (Memory->HoldLeft > 0.f)
	{
		Memory->HoldLeft -= DeltaSeconds;
		return;
	}

	static const float Offsets[] = { 1.f, -1.f, 0.f };
	const float TargetYaw = Memory->StartYaw + Offsets[Memory->Step] * LookAngle;
	const float CurrentYaw = Pawn->GetActorRotation().Yaw;
	const float Delta = FRotator::NormalizeAxis(TargetYaw - CurrentYaw);
	const float MaxStep = TurnSpeed * DeltaSeconds;

	if (FMath::Abs(Delta) <= MaxStep)
	{
		Pawn->SetActorRotation(FRotator(0.f, TargetYaw, 0.f));
		if (++Memory->Step >= UE_ARRAY_COUNT(Offsets))
		{
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
			return;
		}
		Memory->HoldLeft = HoldTime;
	}
	else
	{
		Pawn->SetActorRotation(FRotator(0.f, CurrentYaw + FMath::Sign(Delta) * MaxStep, 0.f));
	}
}

FString UBTTask_LookAround::GetStaticDescription() const
{
	return FString::Printf(TEXT("Turn %.0f deg to each side, hold %.1f s"), LookAngle, HoldTime);
}
