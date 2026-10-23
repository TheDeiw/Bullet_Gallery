#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GuardAnimInstance.generated.h"

/**
 * Parent class of the guard's Animation Blueprint ABP_Guard (Lab 6).
 * NativeUpdateAnimation is the C++ form of "Event Blueprint Update Animation": every frame it reads the
 * owning AGuardNPC and fills the variables the AnimGraph uses (blend space inputs, weapon raise weight, aim pitch).
 */
UCLASS()
class BULLET_GALLERY_API UGuardAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	/** Ground speed, cm/s (input of the locomotion blend spaces) */
	UPROPERTY(BlueprintReadOnly, Category = "Guard")
	float Speed = 0.f;

	/** Movement direction relative to the facing, degrees -180..180 */
	UPROPERTY(BlueprintReadOnly, Category = "Guard")
	float Direction = 0.f;

	/** 0 = revolver lowered, 1 = aimed: weight of the aimed upper-body pose */
	UPROPERTY(BlueprintReadOnly, Category = "Guard")
	float WeaponRaiseAlpha = 0.f;

	/** The guard wants the weapon up (target of WeaponRaiseAlpha) */
	UPROPERTY(BlueprintReadOnly, Category = "Guard")
	bool bWeaponRaised = false;

	/** Vertical aim angle for the Aim Offset, degrees -90..90 */
	UPROPERTY(BlueprintReadOnly, Category = "Guard")
	float AimPitch = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Guard")
	bool bIsDead = false;
};
