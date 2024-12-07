// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/BlasterHUD.h"
#include "GameFramework/PlayerController.h"
#include "HUD/CharacterOverlay.h"

void ABlasterHUD::BeginPlay()
{
	Super::BeginPlay();

	AddCharacterOverlay();
}

void ABlasterHUD::AddCharacterOverlay()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (IsValid(PlayerController) && CharacterOverlayClass)
	{
		CharacterOverlay = CreateWidget<UCharacterOverlay>(PlayerController, CharacterOverlayClass);
		if (IsValid(CharacterOverlay))
		{
			CharacterOverlay->AddToViewport();
		}
	}
}

void ABlasterHUD::DrawHUD()
{
	Super::DrawHUD();
	//CrossHair 에 관한 정보는 무기에 있어야 한다(무기마다 다라질 수 있기 때문에)
	FVector2D ViewportSize;
	if (GEngine)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		const FVector2D ViewportCenter(ViewportSize.X * 0.5f, ViewportSize.Y * 0.5f);

		float SpreadScaled = CrosshairSpreadMax * HUDPackage.CrosshairSpread;

		if (IsValid(HUDPackage.CrossHairCenter))
		{
			FVector2D Spread(0.f, 0.f);
			DrawCrossHair(HUDPackage.CrossHairCenter, ViewportCenter, Spread, HUDPackage.CrosshairColor);
		}
		if (IsValid(HUDPackage.CrossHairLeft))
		{
			FVector2D Spread(-SpreadScaled, 0.f);
			DrawCrossHair(HUDPackage.CrossHairLeft, ViewportCenter, Spread, HUDPackage.CrosshairColor);
		}
		if (IsValid(HUDPackage.CrossHairRight))
		{
			FVector2D Spread(SpreadScaled, 0.f);
			DrawCrossHair(HUDPackage.CrossHairRight, ViewportCenter, Spread, HUDPackage.CrosshairColor);
		}
		if (IsValid(HUDPackage.CrossHairTop))
		{
			FVector2D Spread(0.f, -SpreadScaled);
			DrawCrossHair(HUDPackage.CrossHairTop, ViewportCenter, Spread, HUDPackage.CrosshairColor);
		}
		if (IsValid(HUDPackage.CrossHairBottom))
		{
			FVector2D Spread(0.f, SpreadScaled);
			DrawCrossHair(HUDPackage.CrossHairBottom, ViewportCenter, Spread, HUDPackage.CrosshairColor);
		}
	}
}

void ABlasterHUD::DrawCrossHair(UTexture2D* Texture, FVector2D ViewportCenter, FVector2D Spread, FLinearColor CrosshairColor)
{
	const float TextureWidth = Texture->GetSizeX();	
	const float TextureHeight = Texture->GetSizeY();
	const FVector2D TextureDrawPoint(
		ViewportCenter.X - (TextureWidth * 0.5f) + Spread.X,
		ViewportCenter.Y - (TextureHeight * 0.5f) + Spread.Y
	);
	DrawTexture(
		Texture,
		TextureDrawPoint.X,
		TextureDrawPoint.Y,
		TextureWidth,
		TextureHeight,
		0.f, 
		0.f, 
		1.f, 
		1.f, 
		CrosshairColor
	);
}
