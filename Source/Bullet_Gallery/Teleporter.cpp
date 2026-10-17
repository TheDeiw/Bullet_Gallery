#include "Teleporter.h"

#include "PlayerCharacter.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ATeleporter::ATeleporter()
{
	PrimaryActorTick.bCanEverTick = false;

	// The actor origin is on the floor in the middle of the pad
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(SceneRoot);
	Trigger->SetBoxExtent(FVector(70.f, 70.f, 100.f));
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	// Flat decoration only: the player walks over it without bumping into it
	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(SceneRoot);
	PadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(SceneRoot);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	Light->SetLightColor(FLinearColor(0.2f, 0.6f, 1.f));
	Light->SetIntensity(3000.f);
	Light->SetAttenuationRadius(350.f);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(SceneRoot);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 240.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(28.f);
	Label->SetTextRenderColor(FColor(120, 200, 255));

	// In front of the pad, outside the trigger, so the arrival does not start another teleport
	ExitPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ExitPoint"));
	ExitPoint->SetupAttachment(SceneRoot);
	ExitPoint->SetRelativeLocation(FVector(180.f, 0.f, 0.f));
	ExitPoint->ArrowColor = FColor::Cyan;
	ExitPoint->bIsScreenSizeScaled = true;
}

void ATeleporter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Label->SetText(LabelText);
}

void ATeleporter::BeginPlay()
{
	Super::BeginPlay();

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ATeleporter::OnTriggerBeginOverlap);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &ATeleporter::OnTriggerEndOverlap);
}

FTransform ATeleporter::GetExitTransform() const
{
	return FTransform(FRotator(0.f, ExitPoint->GetComponentRotation().Yaw, 0.f), ExitPoint->GetComponentLocation());
}

void ATeleporter::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player || Player->IsDead() || !Destination) { return; }

	// Just arrived here from the other pad: wait until the player steps off
	if (ArrivedActors.Contains(Player)) { return; }

	TeleportPlayer(Player);
}

void ATeleporter::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ArrivedActors.Remove(OtherActor);
}

void ATeleporter::TeleportPlayer(APlayerCharacter* Player)
{
	const FTransform Exit = Destination->GetExitTransform();

	// The actor location of a character is the capsule centre: lift it so the feet touch the floor
	const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector TargetLocation = Exit.GetLocation() + FVector(0.f, 0.f, HalfHeight + 2.f);
	const FRotator TargetRotation = Exit.Rotator();

	if (TeleportSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, TeleportSound, GetActorLocation());
	}

	Destination->ArrivedActors.Add(Player);
	Player->GetCharacterMovement()->StopMovementImmediately();
	Player->TeleportTo(TargetLocation, TargetRotation);

	// If the exit point is outside the destination trigger there will be no end-overlap event
	if (!Destination->Trigger->IsOverlappingActor(Player))
	{
		Destination->ArrivedActors.Remove(Player);
	}

	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		// The body follows the controller yaw, so the view has to be turned as well
		PC->SetControlRotation(TargetRotation);
		if (ArrivalFadeTime > 0.f && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, ArrivalFadeTime, FLinearColor::Black);
		}
	}

	if (TeleportSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, TeleportSound, TargetLocation);
	}
	// This pad's caption names the place the player has just arrived at
	Player->ShowHUDMessage(LabelText);
}
