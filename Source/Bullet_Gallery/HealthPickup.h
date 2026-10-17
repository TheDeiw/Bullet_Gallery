#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "HealthPickup.generated.h"

/** Medkit: restores part of the player's health. Ignored when the health is full */
UCLASS()
class BULLET_GALLERY_API AHealthPickup : public APickupBase
{
	GENERATED_BODY()

public:
	AHealthPickup();

protected:
	virtual bool ApplyPickup_Implementation(APlayerCharacter* Player) override;
	virtual FText GetPickupMessage_Implementation() const override;

	/** Health points restored by one medkit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = 1))
	float HealAmount = 50.f;
};
