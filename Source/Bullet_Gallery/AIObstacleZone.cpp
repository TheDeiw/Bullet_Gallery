#include "AIObstacleZone.h"

#include "Bullet_Gallery.h"
#include "GuardNPC.h"
#include "PlayerCharacter.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "NavAreas/NavArea_Default.h"
#include "NavModifierComponent.h"
#include "UObject/ConstructorHelpers.h"

UNavArea_Poison::UNavArea_Poison()
{
	// Same cost as a normal floor: the guard does not avoid the poison zone, it walks straight through it
	// and is slowed down there (see AAIObstacleZone::OnZoneBeginOverlap)
	DefaultCost = 1.f;
	DrawColor = FColor(90, 200, 40);
}

AAIObstacleZone::AAIObstacleZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	RootComponent = Trigger;
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	// Bullets and the guards' shots fly through the zone
	Trigger->SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->SetCanEverAffectNavigation(false);

	// Unit cube (100 cm) scaled to the zone size in OnConstruction
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Trigger);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCanEverAffectNavigation(false);
	Visual->CastShadow = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Visual->SetStaticMesh(CubeMesh.Object);
	}

	NavModifier = CreateDefaultSubobject<UNavModifierComponent>(TEXT("NavModifier"));
	NavModifier->SetAreaClass(UNavArea_Poison::StaticClass());

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AAIObstacleZone::OnZoneBeginOverlap);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &AAIObstacleZone::OnZoneEndOverlap);
}

void AAIObstacleZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Trigger->SetBoxExtent(ZoneExtent);
	Visual->SetRelativeScale3D(ZoneExtent / 50.f);

	const bool bPoison = ZoneType == EAIObstacleZoneType::PoisonSlow;
	if (UMaterialInterface* Material = bPoison ? PoisonMaterial : BlindMaterial)
	{
		Visual->SetMaterial(0, Material);
	}

	// The trigger itself is not part of the navigation, so the modifier uses its fail-safe box: the zone's size
	NavModifier->FailsafeExtent = ZoneExtent;

	// Only the poison is marked on the navigation mesh; a blind zone does not change the guard's paths
	NavModifier->SetAreaClass(bPoison ? UNavArea_Poison::StaticClass() : UNavArea_Default::StaticClass());
}

void AAIObstacleZone::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ZoneType == EAIObstacleZoneType::PoisonSlow)
	{
		if (AGuardNPC* Guard = Cast<AGuardNPC>(OtherActor))
		{
			Guard->SetSpeedMultiplier(SlowMultiplier);
		}
	}
	else if (APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor))
	{
		Player->SetInHidingZone(true);
		Player->ShowHUDMessage(NSLOCTEXT("Zone", "Hidden", "You are hidden in the dark: the guards cannot see you"));
	}
}

void AAIObstacleZone::OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ZoneType == EAIObstacleZoneType::PoisonSlow)
	{
		if (AGuardNPC* Guard = Cast<AGuardNPC>(OtherActor))
		{
			Guard->SetSpeedMultiplier(1.f);
		}
	}
	else if (APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor))
	{
		Player->SetInHidingZone(false);
	}
}
