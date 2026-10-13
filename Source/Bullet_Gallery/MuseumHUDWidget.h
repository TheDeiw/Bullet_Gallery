#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MuseumHUDWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;

/**
 * On-screen HUD of the player: health bar, ammo bar (one step per bullet), reload hint
 * and a red flash when the player is hit. It is drawn on the screen, so it stays readable
 * whatever the camera looks at and whatever the field of view is (aiming zooms the camera).
 * Widgets with the names below in the designer are bound automatically;
 * if the designer is empty, the same layout is built in code.
 */
UCLASS()
class BULLET_GALLERY_API UMuseumHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Health bar (the guide's "Set Health" method) */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetHealth(float Health, float MaxHealth);

	/** Ammo bar: fills in steps of 1 / MagazineSize */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetAmmo(int32 AmmoInMagazine, int32 MagazineSize, int32 ReserveAmmo);

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
	TObjectPtr<UProgressBar> AmmoProgressBar;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmmoText;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReloadText;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UImage> DamageOverlay;

	/** Row of bright bullets: the ammo bar shows as much of it as there are bullets left */
	UPROPERTY(EditAnywhere, Category = "Style")
	TObjectPtr<UTexture2D> AmmoFillTexture;

	/** The same row of bullets, dimmed: shows the empty places in the magazine */
	UPROPERTY(EditAnywhere, Category = "Style")
	TObjectPtr<UTexture2D> AmmoBackgroundTexture;

	/** Red frame over the whole screen with a transparent middle */
	UPROPERTY(EditAnywhere, Category = "Style")
	TObjectPtr<UTexture2D> DamageOverlayTexture;

	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor HealthColor = FLinearColor(0.85f, 0.08f, 0.06f);

	/** Size of each bar in the layout built in code, pixels */
	UPROPERTY(EditAnywhere, Category = "Style")
	FVector2D HealthBarSize = FVector2D(320.f, 26.f);

	UPROPERTY(EditAnywhere, Category = "Style")
	FVector2D AmmoBarSize = FVector2D(320.f, 52.f);

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
	FText HealthLabel = NSLOCTEXT("MuseumHUD", "Health", "HEALTH");

	UPROPERTY(EditAnywhere, Category = "Text")
	FText AmmoLabel = NSLOCTEXT("MuseumHUD", "Ammo", "AMMO");

	UPROPERTY(EditAnywhere, Category = "Text")
	FText ReloadLabel = NSLOCTEXT("MuseumHUD", "Reloading", "RELOADING...");

private:
	/** Creates the whole HUD when the Widget Blueprint designer has no widgets */
	void BuildDefaultLayout();

	UTextBlock* MakeText(FName Name, const FText& Text, int32 FontSize);

	/** Pushes the stored values into the widgets (values can arrive before the widgets exist) */
	void ApplyValues();

	float CurrentHealth = 100.f;
	float CurrentMaxHealth = 100.f;
	int32 CurrentAmmo = 0;
	int32 CurrentMagazineSize = 1;
	int32 CurrentReserve = 0;
	bool bIsReloading = false;
	float FlashOpacity = 0.f;
};
