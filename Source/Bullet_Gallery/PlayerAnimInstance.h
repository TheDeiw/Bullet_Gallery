#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PlayerAnimInstance.generated.h"

/**
 * Parent class of the player's Animation Blueprint ABP_Player (Lab 6, a copy of Epic's ABP_Pistol).
 * Mauser C96 reload: the rounds go in from the top, from a stripper clip standing in the clip guide of the open action.
 * NativeUpdateAnimation reads the reload montage and drives two nodes on the left hand: Two Bone IK moves the wrist
 * to the clip and Transform (Modify) Bone turns the hand into the holding pose. The hand comes to the clip at the end
 * of ReloadStart, follows the top round down while the thumb pushes the rounds into the magazine (ReloadLoop)
 * and returns to the montage pose in ReloadEnd.
 */
UCLASS()
class BULLET_GALLERY_API UPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Weight of the left-hand IK, 0..1 */
	float GetLeftHandIKAlpha() const { return LeftHandIKAlpha; }

	/** The clip is in the guide (otherwise the left hand holds it) */
	bool IsClipInGuide() const { return ClipToGuide >= 1.f; }

	/** Clip pose relative to hand_l while the hand holds it: the hand pose on the clip turned inside out, so with the hand in place the clip is exactly in the guide */
	FTransform GetClipInHand(float StackDrop) const { return HandOnClip(StackDrop).Inverse(); }

protected:
	/** Weight of the left-hand IK: 0 = the montage pose, 1 = the hand at the pocket or on the clip */
	UPROPERTY(BlueprintReadOnly, Category = "Reload IK")
	float LeftHandIKAlpha = 0.f;

	/** Weight of the holding rotation of the left hand */
	UPROPERTY(BlueprintReadOnly, Category = "Reload IK")
	float LeftHandGuideAlpha = 0.f;

	/** Way of the hand between the pocket (0) and the clip guide (1) */
	float ClipToGuide = 1.f;

	/** Where the left wrist goes, in the bone space of hand_r (the pistol hand), so it stays on the pistol without lag */
	UPROPERTY(BlueprintReadOnly, Category = "Reload IK")
	FVector LeftHandIKTarget = FVector::ZeroVector;

	/** Rotation of hand_l in component space for the holding pose */
	UPROPERTY(BlueprintReadOnly, Category = "Reload IK")
	FRotator LeftHandIKRotation = FRotator::ZeroRotator;

	/** Where the pistol hand goes while loading, in its own bone space (= PistolOffset turned into hand_r axes) */
	UPROPERTY(BlueprintReadOnly, Category = "Reload IK")
	FVector RightHandIKTarget = FVector::ZeroVector;

	/** The pistol is lowered by this much while loading, so the clip and the left hand stay in the first-person view, cm (mesh space: Y forward, Z up) */
	UPROPERTY(EditAnywhere, Category = "Reload IK")
	FVector PistolOffset = FVector(0.f, 5.f, -25.f);

	/** Socket on the weapon mesh at the top of the action, where the clip stands */
	UPROPERTY(EditDefaultsOnly, Category = "Reload IK")
	FName ClipGuideSocketName = TEXT("ClipGuide");

	/** Wrist position on a full clip, relative to the clip guide socket, cm (X = left of the pistol, Y = along the barrel, Z = up) */
	UPROPERTY(EditAnywhere, Category = "Reload IK")
	FVector HandOffset = FVector(2.f, -5.f, 10.1f);

	/** Hand rotation relative to the clip guide socket (holding pose: palm on the top round, fingers along the barrel) */
	UPROPERTY(EditAnywhere, Category = "Reload IK")
	FRotator HandRotation = FRotator(0.f, 90.f, -90.f);

	/** Left front trouser pocket where the hand takes the clip (wrist position), cm (mesh space: X left, Y forward, Z up from the feet) */
	UPROPERTY(EditAnywhere, Category = "Reload IK")
	FVector PocketLocation = FVector(16.f, 10.f, 92.f);

	/** Montage time span at the start while the hand reaches the empty clip in the guide, seconds */
	UPROPERTY(EditAnywhere, Category = "Reload IK")
	FVector2f ReachTime = FVector2f(0.f, 0.2f);

	/** Montage time span while the hand pulls the empty clip out and takes it down to the pocket, seconds */
	UPROPERTY(EditAnywhere, Category = "Reload IK")
	FVector2f PullOutTime = FVector2f(0.2f, 0.45f);

	/** On the way up from the pocket the hand swings this far forward, so it comes up from below in front of the body, cm */
	UPROPERTY(EditAnywhere, Category = "Reload IK")
	float PocketArc = 10.f;

	/** Montage time span in ReloadStart while the hand brings the clip from the pocket to the clip guide, seconds */
	UPROPERTY(EditAnywhere, Category = "Reload IK")
	FVector2f BlendInTime = FVector2f(0.5f, 0.8f);

	/** Montage time span in ReloadEnd while the hand goes back to the montage pose, seconds */
	UPROPERTY(EditAnywhere, Category = "Reload IK")
	FVector2f BlendOutTime = FVector2f(1.35f, 1.7f);

private:
	/** Hand pose relative to the clip guide; StackDrop = how far the top round is below the top of a full clip, cm */
	FTransform HandOnClip(float StackDrop) const { return FTransform(HandRotation, HandOffset - FVector(0.f, 0.f, StackDrop)); }
};
