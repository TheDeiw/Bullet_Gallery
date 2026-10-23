#include "GuardAnimInstance.h"

#include "GuardNPC.h"

void UGuardAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const AGuardNPC* Guard = Cast<AGuardNPC>(TryGetPawnOwner());
	if (!Guard) { return; }

	const FVector Velocity = Guard->GetVelocity();
	Speed = Velocity.Size2D();
	Direction = Speed > 1.f ? FRotator::NormalizeAxis(Velocity.Rotation().Yaw - Guard->GetActorRotation().Yaw) : 0.f;

	WeaponRaiseAlpha = Guard->GetWeaponRaiseAlpha();
	bWeaponRaised = Guard->IsWeaponRaised();
	// The AI controller turns the control rotation towards its focus (the player), pitch included
	AimPitch = FMath::Clamp(FRotator::NormalizeAxis(Guard->GetBaseAimRotation().Pitch), -90.f, 90.f);
	bIsDead = Guard->IsDead();
}
