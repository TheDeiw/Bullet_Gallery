#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeatherController.generated.h"

class ADirectionalLight;
class AExponentialHeightFog;
class ASkyLight;
class UMaterialParameterCollection;
class UNiagaraComponent;

/**
 * Lab 6, task 4: two natural phenomena at random intervals, driven by one actor placed in the level.
 * - Rain showers: dry pause -> fade in -> shower -> fade out -> dry ... The rain (Niagara, NS_Rain) follows the
 *   player; while it rains the sun and the sky light get darker and the fog thicker; with bThunderstorm the sky
 *   flashes with lightning now and then.
 * - Wind gusts: calm -> gust rises -> holds -> dies down -> calm ... The strength goes to MPC_Weather.WindStrength,
 *   which the park plant materials use for their World Position Offset sway.
 * All timings are editable on the placed actor (short values for screenshots, longer ones for the demo).
 */
UCLASS()
class BULLET_GALLERY_API AWeatherController : public AActor
{
	GENERATED_BODY()

public:
	AWeatherController();

	virtual void Tick(float DeltaTime) override;

	/** 0 = dry, 1 = full shower (fades in and out over RainFadeTime) */
	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetRainAmount() const { return RainAmount; }

	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetWindStrength() const { return WindStrength; }

protected:
	virtual void BeginPlay() override;

	void UpdateRain(float DeltaTime);
	void UpdateLightning(float DeltaTime);
	void UpdateWind(float DeltaTime);
	/** Writes the current rain, lightning and wind into the lights, the fog, the Niagara rain and MPC_Weather */
	void ApplyWeather();

	/** Rain particles; set the Niagara System asset (NS_Rain) on this component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rain")
	TObjectPtr<UNiagaraComponent> RainComponent;

	/** MPC_Weather: WindStrength, WindSpeed, RainAmount */
	UPROPERTY(EditAnywhere, Category = "Weather")
	TObjectPtr<UMaterialParameterCollection> WeatherParameters;

	UPROPERTY(EditAnywhere, Category = "Weather")
	TObjectPtr<ADirectionalLight> Sun;

	UPROPERTY(EditAnywhere, Category = "Weather")
	TObjectPtr<ASkyLight> SkyLight;

	UPROPERTY(EditAnywhere, Category = "Weather")
	TObjectPtr<AExponentialHeightFog> Fog;

	// ---------- Rain ----------

	/** Dry pause between showers, seconds (random in the range) */
	UPROPERTY(EditAnywhere, Category = "Rain", meta = (ClampMin = 0))
	FVector2D DryTime = FVector2D(30.f, 90.f);

	/** Length of a shower, seconds (random in the range) */
	UPROPERTY(EditAnywhere, Category = "Rain", meta = (ClampMin = 0))
	FVector2D ShowerTime = FVector2D(20.f, 60.f);

	/** Seconds for the rain to start or stop */
	UPROPERTY(EditAnywhere, Category = "Rain", meta = (ClampMin = 0.1))
	float RainFadeTime = 5.f;

	/** Raindrops per second in a full shower (Niagara user parameter SpawnRate) */
	UPROPERTY(EditAnywhere, Category = "Rain", meta = (ClampMin = 0))
	float RainSpawnRate = 3000.f;

	UPROPERTY(EditAnywhere, Category = "Rain")
	FName SpawnRateParameter = TEXT("SpawnRate");

	/** The rain emitter hangs this high above the player, cm */
	UPROPERTY(EditAnywhere, Category = "Rain")
	float RainHeight = 1500.f;

	/** Sun and sky light intensity during a full shower, as a part of the normal one */
	UPROPERTY(EditAnywhere, Category = "Rain", meta = (ClampMin = 0, ClampMax = 1))
	float RainLightFactor = 0.35f;

	/** Fog density during a full shower, as a multiple of the normal one */
	UPROPERTY(EditAnywhere, Category = "Rain", meta = (ClampMin = 1))
	float RainFogFactor = 2.f;

	// ---------- Thunderstorm ----------

	/** Lightning flashes during showers */
	UPROPERTY(EditAnywhere, Category = "Thunderstorm")
	bool bThunderstorm = true;

	/** Pause between lightning flashes, seconds (random in the range) */
	UPROPERTY(EditAnywhere, Category = "Thunderstorm", meta = (ClampMin = 0.5))
	FVector2D LightningInterval = FVector2D(6.f, 15.f);

	/** Light intensity multiplier during a flash */
	UPROPERTY(EditAnywhere, Category = "Thunderstorm", meta = (ClampMin = 1))
	float FlashMultiplier = 6.f;

	/** One flash of the double flicker, seconds */
	UPROPERTY(EditAnywhere, Category = "Thunderstorm", meta = (ClampMin = 0.01))
	float FlashTime = 0.08f;

	// ---------- Wind ----------

	/** Wind strength between gusts (random in the range, 0..1) */
	UPROPERTY(EditAnywhere, Category = "Wind", meta = (ClampMin = 0, ClampMax = 1))
	FVector2D CalmStrength = FVector2D(0.1f, 0.2f);

	/** The calm wind is at least this strong during a full shower */
	UPROPERTY(EditAnywhere, Category = "Wind", meta = (ClampMin = 0, ClampMax = 1))
	float RainCalmStrength = 0.35f;

	/** Calm time between gusts, seconds (random in the range) */
	UPROPERTY(EditAnywhere, Category = "Wind", meta = (ClampMin = 0))
	FVector2D CalmTime = FVector2D(8.f, 20.f);

	/** Peak strength of a gust (random in the range, 0..1) */
	UPROPERTY(EditAnywhere, Category = "Wind", meta = (ClampMin = 0, ClampMax = 1))
	FVector2D GustStrength = FVector2D(0.7f, 1.f);

	/** Seconds for a gust to reach its peak */
	UPROPERTY(EditAnywhere, Category = "Wind", meta = (ClampMin = 0.1))
	float GustRiseTime = 1.5f;

	/** How long a gust holds its peak, seconds (random in the range) */
	UPROPERTY(EditAnywhere, Category = "Wind", meta = (ClampMin = 0))
	FVector2D GustHoldTime = FVector2D(3.f, 6.f);

	/** Seconds for a gust to die down (random in the range) */
	UPROPERTY(EditAnywhere, Category = "Wind", meta = (ClampMin = 0.1))
	FVector2D GustDecayTime = FVector2D(2.f, 3.f);

	/** Sway speed written once to MPC_Weather.WindSpeed (changing it at runtime would make the sway jump) */
	UPROPERTY(EditAnywhere, Category = "Wind", meta = (ClampMin = 0))
	float WindSpeed = 1.f;

private:
	enum class EGustPhase : uint8 { Calm, Rising, Holding, Decaying };

	static float RandomIn(const FVector2D& Range) { return FMath::FRandRange(Range.X, Range.Y); }

	bool bRaining = false;
	float RainAmount = 0.f;
	float RainPhaseLeft = 0.f;

	float LightningLeft = 0.f;
	/** Time since the current flash started; negative = no flash */
	float FlashElapsed = -1.f;

	EGustPhase GustPhase = EGustPhase::Calm;
	float WindStrength = 0.f;
	float WindPhaseLeft = 0.f;
	float CalmLevel = 0.f;
	float GustPeak = 0.f;
	float GustDecaySpeed = 1.f;

	float BaseSunIntensity = 0.f;
	float BaseSkyIntensity = 0.f;
	float BaseFogDensity = 0.f;
};
