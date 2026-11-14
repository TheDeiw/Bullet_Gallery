#include "EnemyHealthBarComponent.h"

#include "EnemyHealthBarWidget.h"

#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

UEnemyHealthBarComponent::UEnemyHealthBarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// A flat panel in the world (not glued to the screen), so it belongs to its enemy
	SetWidgetSpace(EWidgetSpace::World);
	SetDrawSize(FVector2D(220.f, 34.f));
	SetPivot(FVector2D(0.5f, 0.5f));
	SetRelativeScale3D(FVector(0.36f));
	// Dimmed: the bar is unlit and glows with the bloom otherwise
	SetTintColorAndOpacity(FLinearColor(0.62f, 0.62f, 0.62f, 1.f));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CastShadow = false;

	// The Blueprint replaces this with its own Widget Blueprint
	WidgetClass = UEnemyHealthBarWidget::StaticClass();
}

void UEnemyHealthBarComponent::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	UpdateHealthValue();
}

void UEnemyHealthBarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FaceCamera();
}

void UEnemyHealthBarComponent::SetMaxHealth(float NewMax)
{
	MaxHealth = FMath::Max(NewMax, 1.f);
	Health = MaxHealth;
	UpdateHealthValue();
}

bool UEnemyHealthBarComponent::ApplyDamage(float Amount)
{
	Health = FMath::Clamp(Health - Amount, 0.f, MaxHealth);
	UpdateHealthValue();
	return Health <= 0.f;
}

void UEnemyHealthBarComponent::UpdateHealthValue()
{
	if (UEnemyHealthBarWidget* HealthWidget = Cast<UEnemyHealthBarWidget>(GetUserWidgetObject()))
	{
		HealthWidget->SetNewHealthValue(GetHealthPercent());
	}
}

void UEnemyHealthBarComponent::FaceCamera()
{
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		// The widget is drawn facing its +X axis, so point +X at the camera
		const FVector ToCamera = Camera->GetCameraLocation() - GetComponentLocation();
		if (!ToCamera.IsNearlyZero())
		{
			SetWorldRotation(ToCamera.Rotation());
		}
	}
}
