#include "MuseumHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"

void AMuseumHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas) { return; }

	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	if (CrosshairTex)
	{
		// Shift by half the texture size so its middle sits exactly on the screen centre
		const FVector2D DrawPosition(Center.X - CrosshairTex->GetSurfaceWidth() * 0.5f,
			Center.Y - CrosshairTex->GetSurfaceHeight() * 0.5f);

		FCanvasTileItem TileItem(DrawPosition, CrosshairTex->GetResource(), CrosshairColor);
		TileItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(TileItem);
		return;
	}

	// Four lines around the centre and a dot in the middle
	DrawLine(Center.X - Gap - LineLength, Center.Y, Center.X - Gap, Center.Y, CrosshairColor, Thickness);
	DrawLine(Center.X + Gap, Center.Y, Center.X + Gap + LineLength, Center.Y, CrosshairColor, Thickness);
	DrawLine(Center.X, Center.Y - Gap - LineLength, Center.X, Center.Y - Gap, CrosshairColor, Thickness);
	DrawLine(Center.X, Center.Y + Gap, Center.X, Center.Y + Gap + LineLength, CrosshairColor, Thickness);
	DrawRect(CrosshairColor, Center.X - 1.f, Center.Y - 1.f, 2.f, 2.f);
}
