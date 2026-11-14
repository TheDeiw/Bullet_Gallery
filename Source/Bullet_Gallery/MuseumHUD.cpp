#include "MuseumHUD.h"

#include "CanvasItem.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MuseumDirector.h"
#include "MuseumUserSettings.h"
#include "PlayerCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "TextureResource.h"

void AMuseumHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas) { return; }

	// No crosshair, hit markers or numbers over the menus
	const UMuseumDirector* Director = UMuseumDirector::Get(this);
	if (Director && !Director->IsGameplayActive()) { return; }

	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	// The crosshair opens a little while the player moves; the weapon is less accurate than it looks when running
	const APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwningPawn());
	const float TargetBonus = Player ? FMath::Clamp(Player->GetVelocity().Size2D() / 400.f, 0.f, 1.f) * 6.f : 0.f;
	GapBonus = FMath::FInterpTo(GapBonus, TargetBonus, GetWorld()->GetDeltaSeconds(), 10.f);

	if (Player && !Player->IsDead())
	{
		DrawDamageIndicators(Center);
		DrawDamageNumbers();
	}

	if (CrosshairTex)
	{
		// Shift by half the texture size so its middle sits exactly on the screen centre
		const FVector2D DrawPosition(Center.X - CrosshairTex->GetSurfaceWidth() * 0.5f,
			Center.Y - CrosshairTex->GetSurfaceHeight() * 0.5f);

		FCanvasTileItem TileItem(DrawPosition, CrosshairTex->GetResource(), CrosshairColor);
		TileItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(TileItem);
		if (Player) { DrawHitMarker(Center); }
		return;
	}

	// Four lines around the centre and a dot in the middle
	const float G = Gap + GapBonus;
	const FLinearColor Shadow(0.f, 0.f, 0.f, 0.55f);
	auto Cross = [&](const FVector2D& Offset, const FLinearColor& Color, float Width)
	{
		DrawLine(Center.X - G - LineLength + Offset.X, Center.Y + Offset.Y, Center.X - G + Offset.X, Center.Y + Offset.Y, Color, Width);
		DrawLine(Center.X + G + Offset.X, Center.Y + Offset.Y, Center.X + G + LineLength + Offset.X, Center.Y + Offset.Y, Color, Width);
		DrawLine(Center.X + Offset.X, Center.Y - G - LineLength + Offset.Y, Center.X + Offset.X, Center.Y - G + Offset.Y, Color, Width);
		DrawLine(Center.X + Offset.X, Center.Y + G + Offset.Y, Center.X + Offset.X, Center.Y + G + LineLength + Offset.Y, Color, Width);
	};
	Cross(FVector2D(1.f, 1.f), Shadow, Thickness);
	Cross(FVector2D::ZeroVector, CrosshairColor, Thickness);
	DrawRect(Shadow, Center.X - 0.f, Center.Y - 0.f, 2.f, 2.f);
	DrawRect(CrosshairColor, Center.X - 1.f, Center.Y - 1.f, 2.f, 2.f);

	if (Player)
	{
		DrawHitMarker(Center);
	}
}

void AMuseumHUD::DrawHitMarker(const FVector2D& Center)
{
	const APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwningPawn());
	if (!Player) { return; }

	const float Age = Player->GetHitMarkerAge();
	const float Life = Player->IsHitMarkerKill() ? 0.35f : 0.2f;
	if (Age >= Life) { return; }

	const float Alpha = 1.f - Age / Life;
	FLinearColor Color = Player->IsHitMarkerKill() ? FLinearColor(1.f, 0.25f, 0.2f) : (Player->IsHitMarkerHeadshot() ? FLinearColor(1.f, 0.82f, 0.3f) : FLinearColor::White);
	Color.A = Alpha;
	const float Scale = Canvas->ClipY / 1080.f;
	const float Inner = (10.f + 6.f * (1.f - Alpha)) * Scale;
	const float Outer = Inner + (Player->IsHitMarkerKill() ? 16.f : 10.f) * Scale;
	const float Width = (Player->IsHitMarkerKill() ? 3.f : 2.f) * Scale;
	for (int32 i = 0; i < 4; ++i)
	{
		const float Sx = (i & 1) ? 1.f : -1.f;
		const float Sy = (i & 2) ? 1.f : -1.f;
		DrawLine(Center.X + Sx * Inner, Center.Y + Sy * Inner, Center.X + Sx * Outer, Center.Y + Sy * Outer, Color, Width);
	}
}

void AMuseumHUD::DrawDamageNumbers()
{
	const APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwningPawn());
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Player || !Font) { return; }

	const float Scale = Canvas->ClipY / 1080.f;
	for (const APlayerCharacter::FDamageNumber& Number : Player->GetDamageNumbers())
	{
		FVector2D Position;
		const FVector World = Number.Location + FVector(0.f, 0.f, 20.f + Number.Age * 70.f);
		if (!UGameplayStatics::ProjectWorldToScreen(GetOwningPlayerController(), World, Position)) { continue; }

		const float Alpha = 1.f - FMath::SmoothStep(0.45f, 0.9f, Number.Age);
		FLinearColor Color = Number.bKill ? FLinearColor(1.f, 0.35f, 0.25f) : (Number.bHeadshot ? FLinearColor(1.f, 0.85f, 0.35f) : FLinearColor::White);
		Color.A = Alpha;
		// A quick pop at the start
		const float Pop = 1.f + 0.5f * FMath::Square(FMath::Clamp(1.f - Number.Age / 0.15f, 0.f, 1.f));
		const float Size = (Number.bHeadshot || Number.bKill ? 1.5f : 1.05f) * Scale * Pop;

		FCanvasTextItem Text(Position, FText::AsNumber(FMath::RoundToInt(Number.Amount)), Font, Color);
		Text.Scale = FVector2D(Size, Size);
		Text.bCentreX = true;
		Text.bCentreY = true;
		Text.EnableShadow(FLinearColor(0.f, 0.f, 0.f, Alpha * 0.8f), FVector2D(1.5f, 1.5f));
		Canvas->DrawItem(Text);
	}
}

void AMuseumHUD::DrawDamageIndicators(const FVector2D& Center)
{
	const APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwningPawn());
	const APlayerController* PC = GetOwningPlayerController();
	if (!Player || !PC) { return; }

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const float Scale = Canvas->ClipY / 1080.f;
	const float Radius = 190.f * Scale;
	for (const APlayerCharacter::FDamageIndicator& Indicator : Player->GetDamageIndicators())
	{
		const FVector ToSource = Indicator.Source - Player->GetActorLocation();
		const float Angle = FRotator::NormalizeAxis(ToSource.Rotation().Yaw - ViewRotation.Yaw);
		const float Alpha = 1.f - FMath::SmoothStep(0.5f, 1.2f, Indicator.Age);
		const FLinearColor Color(0.9f, 0.1f, 0.08f, Alpha);

		// A short arc, 0 degrees = straight ahead (up on the screen)
		FVector2D Previous = FVector2D::ZeroVector;
		for (int32 Step = -4; Step <= 4; ++Step)
		{
			const float A = FMath::DegreesToRadians(Angle + Step * 3.5f);
			const FVector2D Point(Center.X + FMath::Sin(A) * Radius, Center.Y - FMath::Cos(A) * Radius);
			if (Step > -4) { DrawLine(Previous.X, Previous.Y, Point.X, Point.Y, Color, 5.f * Scale); }
			Previous = Point;
		}
	}
}
