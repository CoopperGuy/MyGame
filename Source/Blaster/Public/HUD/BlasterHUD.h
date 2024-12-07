// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BlasterHUD.generated.h"


USTRUCT(BlueprintType)
struct FHUDPackage
{
	GENERATED_BODY()
public:
	UTexture2D* CrossHairCenter;
	UTexture2D* CrossHairLeft;
	UTexture2D* CrossHairRight;
	UTexture2D* CrossHairTop;
	UTexture2D* CrossHairBottom;
	float CrosshairSpread;
	FLinearColor CrosshairColor;
};

class UCharacterOverlay;
class UUserWidget;
class UTexture2D;

/**
 * 
 */
UCLASS()
class BLASTER_API ABlasterHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;

	UPROPERTY(EditAnywhere, Category="Player Stats")
	TSubclassOf<UUserWidget> CharacterOverlayClass;

	UCharacterOverlay* CharacterOverlay;
protected:
	virtual void BeginPlay() override;
	void AddCharacterOverlay();
private:
	FHUDPackage HUDPackage;

	void DrawCrossHair(UTexture2D* Texture, FVector2D ViewportCenter, FVector2D Spread, FLinearColor CrosshairColor);

	UPROPERTY(EditAnywhere)
	float CrosshairSpreadMax = 16.f;
public:
	//�̷��� �����ϸ� ����ü�� ���� �״�� ����(�ּҰ�)�Ǳ� ������ ���� ������ �ȴ� (���� �߻����ɼ��� ����.)
	FORCEINLINE void SetHUDPackage(const FHUDPackage& Package) { HUDPackage = Package; }
};
