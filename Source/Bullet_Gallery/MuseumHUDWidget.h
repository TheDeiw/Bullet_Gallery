#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MuseumHUDWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;

/**
 * On-screen HUD of the player, kept minimal in the top-left corner:
 * "Health" with a red bar, and "Bullets" with the numbers "in magazine / in reserve".
 * A red frame flashes over the whole screen when the player is hit.
 * Widgets with the names below in the designer are bound automatically;
 * if the designer is empty, the same layout is built in code.
 */
UCLASS()
class BULLET_GALLERY_API UMuseumHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetHealth(float Health, float MaxHealth);

	/** Shows the bullets left in the magazine and in the reserve */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetAmmo(int32 AmmoInMagazine, int32 MagazineSize, int32 ReserveAmmo);

	/** Shows or hides the "Reloading..." title */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetReloading(bool bReloading);

	/** Red flash over the screen that fades out */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void PlayDamageFlash();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmmoText;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReloadText;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UImage> DamageOverlay;

	/** Red frame over the whole screen with a transparent middle */
	UPROPERTY(EditAnywhere, Category = "Style")
	TObjectPtr<UTexture2D> DamageOverlayTexture;

	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor HealthColor = FLinearColor(0.85f, 0.05f, 0.05f);

	/** Size of the health bar in the layout built in code, pixels */
	UPROPERTY(EditAnywhere, Category = "Style")
	FVector2D HealthBarSize = FVector2D(220.f, 10.f);

	/** Overlay opacity right after a hit */
	UPROPERTY(EditAnywhere, Category = "Damage Flash")
	float MaxFlashOpacity = 0.7f;

	/** How fast the flash fades, opacity per second */
	UPROPERTY(EditAnywhere, Category = "Damage Flash")
	float FlashFadeSpeed = 1.4f;

	/** Below this health fraction the red frame stays on slightly as a warning */
	UPROPERTY(EditAnywhere, Category = "Damage Flash")
	float LowHealthFraction = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Damage Flash")
	float LowHealthOpacity = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Text")
	FText HealthLabel = NSLOCTEXT("MuseumHUD", "Health", "Health");

	UPROPERTY(EditAnywhere, Category = "Text")
	FText AmmoLabel = NSLOCTEXT("MuseumHUD", "Ammo", "Bullets");

	UPROPERTY(EditAnywhere, Category = "Text")
	FText ReloadLabel = NSLOCTEXT("MuseumHUD", "Reloading", "Reloading...");

private:
	/** Creates the whole HUD when the Widget Blueprint designer has no widgets */
	void BuildDefaultLayout();

	UTextBlock* MakeText(FName Name, const FText& Text, int32 FontSize);

	/** Pushes the stored values into the widgets (values can arrive before the widgets exist) */
	void ApplyValues();

	float CurrentHealth = 100.f;
	float CurrentMaxHealth = 100.f;
	int32 CurrentAmmo = 0;
	int32 CurrentReserve = 0;
	bool bIsReloading = false;
	float FlashOpacity = 0.f;
};
