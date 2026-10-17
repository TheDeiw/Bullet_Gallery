#include "PhysicsDoor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "UObject/ConstructorHelpers.h"

APhysicsDoor::APhysicsDoor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Engine cube: 100 cm, pivot in the centre; the Blueprint can pick another mesh and material
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Door = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Door"));
	Door->SetupAttachment(SceneRoot);
	Door->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Door->SetMobility(EComponentMobility::Movable);
	if (CubeMesh.Succeeded())
	{
		Door->SetStaticMesh(CubeMesh.Object);
	}

	Handle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Handle"));
	Handle->SetupAttachment(Door);
	Handle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// The leaf is scaled to the door size; the handle keeps its own size
	Handle->SetUsingAbsoluteScale(true);
	if (CubeMesh.Succeeded())
	{
		Handle->SetStaticMesh(CubeMesh.Object);
	}

	// The hinge sits on the root: the constraint Z axis (Swing1) is the vertical door axis
	Hinge = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(SceneRoot);
	Hinge->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
	Hinge->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
	Hinge->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
	Hinge->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.f);
	Hinge->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.f);
	Hinge->SetDisableCollision(true);
}

void APhysicsDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateDoorLayout();
}

void APhysicsDoor::UpdateDoorLayout()
{
	// The leaf starts at the hinge line and goes along +X
	const FVector LeafScale = DoorSize / 100.f;
	Door->SetRelativeLocation(FVector(DoorSize.X * 0.5f, 0.f, DoorSize.Z * 0.5f));
	Door->SetRelativeRotation(FRotator::ZeroRotator);
	Door->SetRelativeScale3D(LeafScale);

	// A bar through the leaf near the free edge at hand height; the location is in the leaf's scaled space
	const FVector HandleSize(4.f, DoorSize.Y + 10.f, 30.f);
	const FVector HandleOffset(DoorSize.X * 0.5f - 12.f, 0.f, 105.f - DoorSize.Z * 0.5f);
	Handle->SetRelativeLocation(HandleOffset / LeafScale);
	Handle->SetWorldScale3D(HandleSize / 100.f);
}

void APhysicsDoor::BeginPlay()
{
	Super::BeginPlay();

	Hinge->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, OpenAngle);

	// Weak spring towards the closed position
	Hinge->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
	Hinge->SetAngularOrientationTarget(FRotator::ZeroRotator);
	Hinge->SetOrientationDriveTwistAndSwing(false, CloseStrength > 0.f);
	Hinge->SetAngularDriveParams(CloseStrength, CloseDamping, 0.f);

	Door->SetMassOverrideInKg(NAME_None, DoorMass, true);
	Door->SetAngularDamping(1.f);
	Door->SetSimulatePhysics(true);

	// No first component = the leaf is attached to the world at the hinge position
	Hinge->SetConstrainedComponents(nullptr, NAME_None, Door, NAME_None);
}
