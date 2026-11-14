#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DayNightCycle.generated.h"

class ADirectionalLight;

/**
 * Lab 6, task 4: day and night. One actor placed in the level turns the sun around the sky once per DayLengthSeconds.
 * The sky atmosphere, clouds and the real-time sky light follow the sun by themselves. At sunset (18 h) the actors with
 * the tag NightLightTag (museum and garden lamps) are switched on, at sunrise (6 h) off. The optional Moon is a dim
 * second directional light that travels opposite to the sun and is on only at night.
 * The light intensities are not touched here: AWeatherController scales the sun, the sky light and the fog in the rain.
 */
UCLASS()
class BULLET_GALLERY_API ADayNightCycle : public AActor
{
	GENERATED_BODY()

public:
	ADayNightCycle();

	virtual void Tick(float DeltaTime) override;

	/** Time of day, 0..24 */
	UFUNCTION(BlueprintPure, Category = "DayNight")
	float GetHour() const { return Hour; }

	/** The game director takes over the clock: the time only changes through SetHour */
	UFUNCTION(BlueprintCallable, Category = "DayNight")
	void SetExternalControl(bool bExternal) { bExternalClock = bExternal; }

	/** Sets the time of day (any number: 30 means 6 h of the next day) and moves the sun and the lights at once */
	UFUNCTION(BlueprintCallable, Category = "DayNight")
	void SetHour(float NewHour);

protected:
	virtual void BeginPlay() override;

	/** Switches the NightLight actors and the moon; lights are looked up each time, so streamed-in ones are found too */
	void SetNightLights(bool bOn);

	UPROPERTY(EditAnywhere, Category = "DayNight")
	TObjectPtr<ADirectionalLight> Sun;

	/** Optional: a dim bluish directional light (Atmosphere Sun Light Index 1); rotated opposite the sun, visible only at night */
	UPROPERTY(EditAnywhere, Category = "DayNight")
	TObjectPtr<ADirectionalLight> Moon;

	/** Real seconds for 24 game hours */
	UPROPERTY(EditAnywhere, Category = "DayNight", meta = (ClampMin = 1))
	float DayLengthSeconds = 240.f;

	/** Time of day at BeginPlay, 0..24 */
	UPROPERTY(EditAnywhere, Category = "DayNight", meta = (ClampMin = 0, ClampMax = 24))
	float StartHour = 16.f;

	/** Actors with this tag have their light on at night only */
	UPROPERTY(EditAnywhere, Category = "DayNight")
	FName NightLightTag = TEXT("NightLight");

	/** Current time of day (read-only, for the Details panel) */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "DayNight")
	float Hour = 0.f;

private:
	/** Turns the sun / moon for the current Hour and switches the night lights when it becomes night or day */
	void ApplyHour();

	bool bExternalClock = false;
	float SunYaw = 0.f;
	/** -1 = lights not set yet, 0 = day, 1 = night */
	int8 NightState = -1;
};
