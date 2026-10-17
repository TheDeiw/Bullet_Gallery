#include "HealthPickup.h"

#include "PlayerCharacter.h"

#include "Components/PointLightComponent.h"

AHealthPickup::AHealthPickup()
{
	// Green light, like the cross on a first-aid kit
	Glow->SetLightColor(FLinearColor(0.2f, 1.f, 0.3f));
}

bool AHealthPickup::ApplyPickup_Implementation(APlayerCharacter* Player)
{
	return Player->AddHealth(HealAmount);
}

FText AHealthPickup::GetPickupMessage_Implementation() const
{
	return FText::Format(NSLOCTEXT("Pickup", "Health", "+{0} Health"), FText::AsNumber(FMath::RoundToInt(HealAmount)));
}
