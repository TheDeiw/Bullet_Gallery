#pragma once

#include "CoreMinimal.h"
#include "PickupBase.h"
#include "AmmoPickup.generated.h"

/** Ammo box: adds bullets to the reserve. Ignored when the reserve is full */
UCLASS()
class BULLET_GALLERY_API AAmmoPickup : public APickupBase
{
	GENERATED_BODY()

public:
	AAmmoPickup();

protected:
	virtual bool ApplyPickup_Implementation(APlayerCharacter* Player) override;
	virtual FText GetPickupMessage_Implementation() const override;

	/** Bullets added to the reserve (two 10-round stripper clips by default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = 1))
	int32 AmmoAmount = 20;

private:
	/** How many bullets were actually taken (the pouch may have room for fewer) */
	int32 LastAdded = 0;
};
