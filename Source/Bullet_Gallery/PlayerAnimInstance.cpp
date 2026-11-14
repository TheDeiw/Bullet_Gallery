#include "PlayerAnimInstance.h"

#include "PlayerCharacter.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

void UPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const APlayerCharacter* Player = Cast<APlayerCharacter>(TryGetPawnOwner());
	if (!Player || !Player->WeaponMesh || !Player->WeaponMesh->DoesSocketExist(ClipGuideSocketName))
	{
		LeftHandIKAlpha = 0.f;
		return;
	}

	// Both weights follow the montage time: the hand reaches the empty clip in the guide, pulls it out and takes it down
	// to the pocket, then brings a full clip up to the guide. Outside a reload the clip stays in the guide.
	float Alpha = 0.f;
	float ToGuide = 1.f;
	if (const UAnimMontage* Montage = GetCurrentActiveMontage())
	{
		const FName Section = Montage_GetCurrentSection(Montage);
		const float Time = Montage_GetPosition(Montage);
		if (Section == TEXT("ReloadLoop"))
		{
			Alpha = ToGuide = 1.f;
		}
		else if (Section == TEXT("ReloadStart"))
		{
			Alpha = FMath::SmoothStep(ReachTime.X, ReachTime.Y, Time);
			ToGuide = Time < BlendInTime.X ? 1.f - FMath::SmoothStep(PullOutTime.X, PullOutTime.Y, Time)
				: FMath::SmoothStep(BlendInTime.X, BlendInTime.Y, Time);
		}
		else if (Section == TEXT("ReloadEnd"))
		{
			// The hand lets go; the empty clip stays in the guide until the next reload
			Alpha = 1.f - FMath::SmoothStep(BlendOutTime.X, BlendOutTime.Y, Time);
		}
	}

	LeftHandIKAlpha = Alpha;
	ClipToGuide = ToGuide;
	LeftHandGuideAlpha = Alpha * ToGuide;
	if (Alpha > 0.f)
	{
		// The thumb rests on the top round, so the hand goes down as the rounds are pushed into the magazine
		const FTransform Hand = HandOnClip(Player->GetClipStackDrop()) * Player->WeaponMesh->GetSocketTransform(ClipGuideSocketName);
		const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
		const FTransform& ToWorld = Mesh->GetComponentTransform();
		// From the pocket the hand comes up from below, swinging forward so it does not go through the body
		const FVector Target = FMath::Lerp(ToWorld.TransformPosition(PocketLocation), Hand.GetLocation(), ToGuide)
			+ ToWorld.TransformVectorNoScale(FVector(0.f, PocketArc * FMath::Sin(PI * ToGuide), 0.f));
		// The pistol is fixed to hand_r, so the guide target in hand_r space does not depend on the frame the transforms come from
		LeftHandIKTarget = Mesh->GetSocketTransform(TEXT("hand_r")).InverseTransformPosition(Target);
		LeftHandIKRotation = ToWorld.InverseTransformRotation(Hand.GetRotation()).Rotator();
		// Two Bone IK keeps the rotation of hand_r, so its axes from the last frame are the montage ones
		RightHandIKTarget = Mesh->GetSocketTransform(TEXT("hand_r"), RTS_Component).InverseTransformVectorNoScale(PistolOffset);
	}
}
