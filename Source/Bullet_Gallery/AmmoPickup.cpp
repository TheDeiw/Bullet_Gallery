#include "AmmoPickup.h"

#include "PlayerCharacter.h"

#include "Components/PointLightComponent.h"

AAmmoPickup::AAmmoPickup()
{
	// Warm brass-coloured light
	Glow->SetLightColor(FLinearColor(1.f, 0.7f, 0.25f));
}

bool AAmmoPickup::ApplyPickup_Implementation(APlayerCharacter* Player)
{
	LastAdded = Player->AddAmmo(AmmoAmount);
	return LastAdded > 0;
}

FText AAmmoPickup::GetPickupMessage_Implementation() const
{
	return FText::Format(NSLOCTEXT("Pickup", "Ammo", "+{0} Bullets"), FText::AsNumber(LastAdded));
}
