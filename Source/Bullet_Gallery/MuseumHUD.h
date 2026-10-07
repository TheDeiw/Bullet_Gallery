#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MuseumHUD.generated.h"

class UTexture2D;

/** Draws the crosshair in the centre of the screen: the point the bullet flies to */
UCLASS()
class BULLET_GALLERY_API AMuseumHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void DrawHUD() override;

	/** Optional crosshair image; when empty, a simple cross is drawn with lines */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	TObjectPtr<UTexture2D> CrosshairTex;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor CrosshairColor = FLinearColor::White;

	/** Length of each crosshair line, pixels */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float LineLength = 10.f;

	/** Empty space between the centre and each line, pixels */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float Gap = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float Thickness = 2.f;
};
