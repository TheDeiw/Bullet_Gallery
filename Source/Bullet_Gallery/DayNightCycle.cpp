#include "DayNightCycle.h"

#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Kismet/GameplayStatics.h"

ADayNightCycle::ADayNightCycle()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADayNightCycle::BeginPlay()
{
	Super::BeginPlay();

	Hour = StartHour;
	if (Sun) { SunYaw = Sun->GetActorRotation().Yaw; }
}

void ADayNightCycle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bExternalClock)
	{
		Hour = FMath::Fmod(Hour + DeltaTime * 24.f / DayLengthSeconds, 24.f);
		ApplyHour();
	}
}

void ADayNightCycle::SetHour(float NewHour)
{
	Hour = FMath::Fmod(NewHour, 24.f);
	ApplyHour();
}

void ADayNightCycle::ApplyHour()
{
	if (Sun)
	{
		// 6 h = horizon, 12 h = overhead, 18 h = opposite horizon, then the light points up (night)
		const float Pitch = -(Hour - 6.f) / 12.f * 180.f;
		Sun->SetActorRotation(FRotator(Pitch, SunYaw, 0.f));

		if (Moon)
		{
			Moon->SetActorRotation((-Sun->GetActorForwardVector()).Rotation());
		}
	}

	const int8 Night = (Hour < 6.f || Hour >= 18.f) ? 1 : 0;
	if (Night != NightState)
	{
		NightState = Night;
		SetNightLights(Night == 1);
	}
}

void ADayNightCycle::SetNightLights(bool bOn)
{
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsWithTag(this, NightLightTag, Actors);
	for (AActor* Actor : Actors)
	{
		ULightComponent* Light = Actor->FindComponentByClass<ULightComponent>();
		// Static lights are baked and cannot be switched at runtime
		if (Light && Light->Mobility != EComponentMobility::Static)
		{
			Light->SetVisibility(bOn);
		}
	}

	if (Moon) { Moon->GetLightComponent()->SetVisibility(bOn); }
}
