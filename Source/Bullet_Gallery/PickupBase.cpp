#include "PickupBase.h"

#include "PlayerCharacter.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

APickupBase::APickupBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// The sphere only overlaps pawns: bullets and physics props fly through it
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(60.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Collision);
	Glow->SetIntensity(800.f);
	Glow->SetAttenuationRadius(180.f);
	Glow->SetCastShadows(false);
}

void APickupBase::BeginPlay()
{
	Super::BeginPlay();

	MeshBaseLocation = Mesh->GetRelativeLocation();
	// Items placed next to each other should not move in sync
	RunningTime = FMath::FRandRange(0.f, 10.f);

	Collision->OnComponentBeginOverlap.AddDynamic(this, &APickupBase::OnSphereBeginOverlap);
}

void APickupBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RunningTime += DeltaTime;

	// Spin around the vertical axis and float up and down
	Mesh->AddRelativeRotation(FRotator(0.f, RotationSpeed * DeltaTime, 0.f));
	const float Bob = BobAmplitude * FMath::Sin(2.f * PI * BobFrequency * RunningTime);
	Mesh->SetRelativeLocation(MeshBaseLocation + FVector(0.f, 0.f, Bob));
}

void APickupBase::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player || Player->IsDead()) { return; }

	// The player does not need the item right now (full health, full ammo pouch)
	if (!ApplyPickup(Player)) { return; }

	Player->ShowHUDMessage(GetPickupMessage());
	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}
	OnPickedUp(Player);

	if (RespawnTime > 0.f)
	{
		SetPickupHidden(true);
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &APickupBase::Respawn, RespawnTime, false);
	}
	else
	{
		Destroy();
	}
}

void APickupBase::SetPickupHidden(bool bHide)
{
	SetActorHiddenInGame(bHide);
	SetActorTickEnabled(!bHide);
	Collision->SetCollisionEnabled(bHide ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}

void APickupBase::Respawn()
{
	SetPickupHidden(false);
}

bool APickupBase::ApplyPickup_Implementation(APlayerCharacter* Player)
{
	return false;
}

FText APickupBase::GetPickupMessage_Implementation() const
{
	return FText::GetEmpty();
}
