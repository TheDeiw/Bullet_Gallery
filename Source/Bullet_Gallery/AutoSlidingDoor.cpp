#include "AutoSlidingDoor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Thickness of the glass and size of the metal profiles, cm
	constexpr float GlassThickness = 2.f;
	constexpr float ProfileWidth = 5.f;
	constexpr float ProfileDepth = 6.f;
	constexpr float RailHeight = 10.f;
	constexpr float HeaderHeight = 22.f;
	constexpr float HeaderDepth = 18.f;
}

AAutoSlidingDoor::AAutoSlidingDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	// Ticks only while the leaves are moving
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(SceneRoot);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	LeftLeaf = CreateDefaultSubobject<USceneComponent>(TEXT("LeftLeaf"));
	LeftLeaf->SetupAttachment(SceneRoot);
	RightLeaf = CreateDefaultSubobject<USceneComponent>(TEXT("RightLeaf"));
	RightLeaf->SetupAttachment(SceneRoot);

	LeftGlass = CreatePart(TEXT("LeftGlass"), LeftLeaf, true);
	RightGlass = CreatePart(TEXT("RightGlass"), RightLeaf, true);
	LeftEdge = CreatePart(TEXT("LeftEdge"), LeftLeaf, false);
	RightEdge = CreatePart(TEXT("RightEdge"), RightLeaf, false);
	LeftRail = CreatePart(TEXT("LeftRail"), LeftLeaf, false);
	RightRail = CreatePart(TEXT("RightRail"), RightLeaf, false);
	Header = CreatePart(TEXT("Header"), SceneRoot, true);
	Transom = CreatePart(TEXT("Transom"), SceneRoot, true);

	// The parts are 100 cm engine cubes scaled to size; the Blueprint picks the materials
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		for (UStaticMeshComponent* Part : { LeftGlass, RightGlass, LeftEdge, RightEdge, LeftRail, RightRail, Header, Transom })
		{
			Part->SetStaticMesh(CubeMesh.Object);
		}
	}
}

UStaticMeshComponent* AAutoSlidingDoor::CreatePart(FName Name, USceneComponent* Parent, bool bBlocks)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(Parent);
	Part->SetMobility(EComponentMobility::Movable);
	if (bBlocks)
	{
		Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		// The glass next to it already blocks
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	return Part;
}

void AAutoSlidingDoor::PlaceBox(UStaticMeshComponent* Part, const FVector& Center, const FVector& Size)
{
	Part->SetRelativeLocation(Center);
	Part->SetRelativeScale3D(Size / 100.f);
}

void AAutoSlidingDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateLayout();
}

void AAutoSlidingDoor::UpdateLayout()
{
	const float LeafWidth = DoorwayWidth * 0.5f;

	// Each leaf is laid out around its own origin (the middle of the leaf on the floor)
	PlaceBox(LeftGlass, FVector(0.f, 0.f, DoorHeight * 0.5f), FVector(GlassThickness, LeafWidth, DoorHeight));
	PlaceBox(RightGlass, FVector(0.f, 0.f, DoorHeight * 0.5f), FVector(GlassThickness, LeafWidth, DoorHeight));

	// Vertical bars on the edges where the leaves meet
	PlaceBox(LeftEdge, FVector(0.f, LeafWidth * 0.5f - ProfileWidth * 0.5f, DoorHeight * 0.5f), FVector(ProfileDepth, ProfileWidth, DoorHeight));
	PlaceBox(RightEdge, FVector(0.f, -LeafWidth * 0.5f + ProfileWidth * 0.5f, DoorHeight * 0.5f), FVector(ProfileDepth, ProfileWidth, DoorHeight));

	PlaceBox(LeftRail, FVector(0.f, 0.f, RailHeight * 0.5f), FVector(ProfileDepth, LeafWidth, RailHeight));
	PlaceBox(RightRail, FVector(0.f, 0.f, RailHeight * 0.5f), FVector(ProfileDepth, LeafWidth, RailHeight));

	PlaceBox(Header, FVector(0.f, 0.f, DoorHeight + HeaderHeight * 0.5f), FVector(HeaderDepth, DoorwayWidth, HeaderHeight));

	// Fixed glass up to the ceiling, if the doorway is taller than the door
	const float TransomBottom = DoorHeight + HeaderHeight;
	const bool bHasTransom = TransomTop > TransomBottom + 1.f;
	Transom->SetVisibility(bHasTransom);
	Transom->SetCollisionEnabled(bHasTransom ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bHasTransom)
	{
		PlaceBox(Transom, FVector(0.f, 0.f, (TransomBottom + TransomTop) * 0.5f),
			FVector(GlassThickness, DoorwayWidth, TransomTop - TransomBottom));
	}

	Trigger->SetBoxExtent(FVector(TriggerDepth, DoorwayWidth * 0.5f, DoorHeight * 0.5f));
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, DoorHeight * 0.5f));

	for (UStaticMeshComponent* Part : { LeftGlass, RightGlass, Transom })
	{
		if (GlassMaterial) { Part->SetMaterial(0, GlassMaterial); }
	}
	for (UStaticMeshComponent* Part : { LeftEdge, RightEdge, LeftRail, RightRail, Header })
	{
		if (FrameMaterial) { Part->SetMaterial(0, FrameMaterial); }
	}

	ApplyOpenAmount(OpenAmount);
}

void AAutoSlidingDoor::ApplyOpenAmount(float Amount)
{
	const float LeafWidth = DoorwayWidth * 0.5f;
	// Smooth start and stop of the motion
	const float Eased = FMath::SmoothStep(0.f, 1.f, Amount);
	const float Slide = Eased * LeafWidth * OpenFraction;

	LeftLeaf->SetRelativeLocation(FVector(0.f, -LeafWidth * 0.5f - Slide, 0.f));
	RightLeaf->SetRelativeLocation(FVector(0.f, LeafWidth * 0.5f + Slide, 0.f));
}

void AAutoSlidingDoor::BeginPlay()
{
	Super::BeginPlay();

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AAutoSlidingDoor::OnTriggerBeginOverlap);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &AAutoSlidingDoor::OnTriggerEndOverlap);

	// Someone may already stand in the doorway when the level starts
	UpdateTargetState();
}

void AAutoSlidingDoor::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	UpdateTargetState();
}

void AAutoSlidingDoor::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	UpdateTargetState();
}

void AAutoSlidingDoor::UpdateTargetState()
{
	TArray<AActor*> Pawns;
	Trigger->GetOverlappingActors(Pawns, APawn::StaticClass());
	const bool bOpen = Pawns.Num() > 0;
	if (bOpen == bWantsOpen) { return; }

	bWantsOpen = bOpen;
	if (USoundBase* Sound = bWantsOpen ? OpenSound : CloseSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation() + FVector(0.f, 0.f, DoorHeight));
	}
	SetActorTickEnabled(true);
}

void AAutoSlidingDoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float Target = bWantsOpen ? 1.f : 0.f;
	OpenAmount = FMath::FInterpConstantTo(OpenAmount, Target, DeltaTime, OpenSpeed);
	ApplyOpenAmount(OpenAmount);

	if (FMath::IsNearlyEqual(OpenAmount, Target))
	{
		SetActorTickEnabled(false);
	}
}
