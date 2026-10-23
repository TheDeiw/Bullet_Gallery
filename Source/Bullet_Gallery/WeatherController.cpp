#include "WeatherController.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "NiagaraComponent.h"

AWeatherController::AWeatherController()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	RainComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Rain"));
	RainComponent->SetupAttachment(RootComponent);
	RainComponent->SetAutoActivate(false);
	// Moved to the player every frame in world space
	RainComponent->SetUsingAbsoluteLocation(true);
}

void AWeatherController::BeginPlay()
{
	Super::BeginPlay();

	if (Sun) { BaseSunIntensity = Sun->GetLightComponent()->Intensity; }
	if (SkyLight) { BaseSkyIntensity = SkyLight->GetLightComponent()->Intensity; }
	if (Fog) { BaseFogDensity = Fog->GetComponent()->FogDensity; }

	if (WeatherParameters)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, WeatherParameters, TEXT("WindSpeed"), WindSpeed);
	}

	// Start dry and calm
	RainPhaseLeft = RandomIn(DryTime);
	CalmLevel = RandomIn(CalmStrength);
	WindStrength = CalmLevel;
	WindPhaseLeft = RandomIn(CalmTime);
}

void AWeatherController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateRain(DeltaTime);
	UpdateLightning(DeltaTime);
	UpdateWind(DeltaTime);
	ApplyWeather();
}

void AWeatherController::UpdateRain(float DeltaTime)
{
	// Dry and Raining phases last a random time; FadeIn / FadeOut are the RainAmount ramps between them
	if ((RainPhaseLeft -= DeltaTime) <= 0.f)
	{
		bRaining = !bRaining;
		RainPhaseLeft = RainFadeTime + RandomIn(bRaining ? ShowerTime : DryTime);
		LightningLeft = RandomIn(LightningInterval);
		if (bRaining)
		{
			RainComponent->Activate(true);
		}
	}

	RainAmount = FMath::FInterpConstantTo(RainAmount, bRaining ? 1.f : 0.f, DeltaTime, 1.f / RainFadeTime);
	if (!bRaining && RainAmount <= 0.f && RainComponent->IsActive())
	{
		RainComponent->Deactivate();
	}
}

void AWeatherController::UpdateLightning(float DeltaTime)
{
	if (FlashElapsed >= 0.f)
	{
		// Double flicker: on, off, on - then the flash is over
		FlashElapsed += DeltaTime;
		if (FlashElapsed >= FlashTime * 3.f)
		{
			FlashElapsed = -1.f;
		}
		return;
	}

	if (bThunderstorm && bRaining && RainAmount > 0.5f && (LightningLeft -= DeltaTime) <= 0.f)
	{
		FlashElapsed = 0.f;
		LightningLeft = RandomIn(LightningInterval);
	}
}

void AWeatherController::UpdateWind(float DeltaTime)
{
	// The calm wind is stronger in the rain
	const float Calm = FMath::Lerp(CalmLevel, FMath::Max(CalmLevel, RainCalmStrength), RainAmount);
	WindPhaseLeft -= DeltaTime;

	switch (GustPhase)
	{
	case EGustPhase::Calm:
		WindStrength = FMath::FInterpConstantTo(WindStrength, Calm, DeltaTime, 0.2f);
		if (WindPhaseLeft <= 0.f)
		{
			GustPhase = EGustPhase::Rising;
			GustPeak = FMath::Max(RandomIn(GustStrength), Calm);
		}
		break;

	case EGustPhase::Rising:
		WindStrength = FMath::FInterpConstantTo(WindStrength, GustPeak, DeltaTime, (GustPeak - Calm) / GustRiseTime);
		if (WindStrength >= GustPeak - KINDA_SMALL_NUMBER)
		{
			GustPhase = EGustPhase::Holding;
			WindPhaseLeft = RandomIn(GustHoldTime);
		}
		break;

	case EGustPhase::Holding:
		if (WindPhaseLeft <= 0.f)
		{
			GustPhase = EGustPhase::Decaying;
			GustDecaySpeed = (WindStrength - Calm) / RandomIn(GustDecayTime);
		}
		break;

	case EGustPhase::Decaying:
		WindStrength = FMath::FInterpConstantTo(WindStrength, Calm, DeltaTime, FMath::Max(GustDecaySpeed, 0.01f));
		if (WindStrength <= Calm + KINDA_SMALL_NUMBER)
		{
			GustPhase = EGustPhase::Calm;
			CalmLevel = RandomIn(CalmStrength);
			WindPhaseLeft = RandomIn(CalmTime);
		}
		break;
	}
}

void AWeatherController::ApplyWeather()
{
	// Lightning: two short flashes separated by a dark gap of the same length
	const bool bFlashOn = FlashElapsed >= 0.f && (FlashElapsed < FlashTime || FlashElapsed >= FlashTime * 2.f);
	const float LightScale = FMath::Lerp(1.f, RainLightFactor, RainAmount) * (bFlashOn ? FlashMultiplier : 1.f);

	if (Sun) { Sun->GetLightComponent()->SetIntensity(BaseSunIntensity * LightScale); }
	if (SkyLight) { SkyLight->GetLightComponent()->SetIntensity(BaseSkyIntensity * LightScale); }
	if (Fog) { Fog->GetComponent()->SetFogDensity(BaseFogDensity * FMath::Lerp(1.f, RainFogFactor, RainAmount)); }

	if (RainComponent->IsActive())
	{
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			RainComponent->SetWorldLocation(Player->GetActorLocation() + FVector(0.f, 0.f, RainHeight));
		}
		RainComponent->SetVariableFloat(SpawnRateParameter, RainSpawnRate * RainAmount);
	}

	if (WeatherParameters)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, WeatherParameters, TEXT("WindStrength"), WindStrength);
		UKismetMaterialLibrary::SetScalarParameterValue(this, WeatherParameters, TEXT("RainAmount"), RainAmount);
	}
}
