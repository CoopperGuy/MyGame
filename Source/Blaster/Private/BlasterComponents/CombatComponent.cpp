// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterComponents/CombatComponent.h"
#include "Engine/SkeletalMeshSocket.h"

#include "GameFramework/CharacterMovementComponent.h"

#include "Components/SphereComponent.h"

#include "Camera/CameraComponent.h"

#include "Character/BlasterCharacter.h"
#include "Playercontroller/BlasterPlayerController.h"

#include "Net/UnrealNetwork.h"

#include "Weapon/Weapon.h"

#include "Kismet/GameplayStatics.h"

#include "TimerManager.h"

#include "DrawDebugHelpers.h"

// Sets default values for this component's properties
UCombatComponent::UCombatComponent()
	:BaseWalkSpeed(600.f), AimWalkSpeed(350.f)
{
	PrimaryComponentTick.bCanEverTick = true;

}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	DOREPLIFETIME(UCombatComponent, bAiming);
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(Character))
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;

		if (IsValid(Character->GetFollowCamera()))
		{
			DefaultFOV = Character->GetFollowCamera()->FieldOfView;
			CurrentFOV = DefaultFOV;
		}
	}
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsValid(Character) && Character->IsLocallyControlled())
	{
		FHitResult HitResult;
		TraceunderCrossHair(HitResult);
		HitTarget = HitResult.ImpactPoint;
		
		SetHUDCrossHairs(DeltaTime);
		InterpFOV(DeltaTime);
	}
}

void UCombatComponent::OnRep_EquippedWeapon()
{
	if (IsValid(EquippedWeapon) && IsValid(Character))
	{
		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		Character->bUseControllerRotationYaw = true;
	}
}

void UCombatComponent::SetAiming(bool _bIsAiming)
{
	bAiming = _bIsAiming;
	//서버에서 보유한 캐릭터가 이 함수를 호출하면 서버에서만 작동하고 클라이언트로 보내지않는다.
	//서버에서 보유하지 않은 캐릭터가 이 함수를 호출하면 서버로 보낸다.
	ServerSetAiming(_bIsAiming);
	//조준시에 max speed를 조절해보자. 
	//단순하게 maxwalkspeed만 클라이언트에서 조절하면 안된다. movement component에 존재하는 서버 동기화 코드가 강제적으로 위치를 동기화 시키기 때문이다.
	if (IsValid(Character))
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = bAiming ? AimWalkSpeed : BaseWalkSpeed;
	}
}

void UCombatComponent::InterpFOV(float DeltaTime)
{
	if (!IsValid(EquippedWeapon)) return;

	if (bAiming)
	{
		CurrentFOV = FMath::FInterpTo(CurrentFOV, EquippedWeapon->GetZoomedFOV(), DeltaTime, EquippedWeapon->GetZoomInterSpeed());
	}
	else
	{
		//어떤 무기던 간에 같은 속도로 돌아가게 하기 위해 combatcomponent의 zoomeiterpspeed를 사용한다.
		CurrentFOV = FMath::FInterpTo(CurrentFOV, DefaultFOV, DeltaTime, ZoomInterpSpeed);
	}
	if (IsValid(Character) && IsValid(Character->GetFollowCamera()))
	{
		Character->GetFollowCamera()->SetFieldOfView(CurrentFOV);
	}
}

void UCombatComponent::FireButtonPressed(bool bPressed)
{
	//이걸 각 클라이언트에 복사하도록 설정하면 쉽게 다른 클라이언트와 동기화 된다
	//하지만 총을 사격하는 것은 한번 꾹 누르면 지속해서 발사해야 하기 때문에 이런식으로만 하면 안된다.
	//multicast rpc를 이용하자.
	bFireButtonPressed = bPressed;

	if (bFireButtonPressed)
	{
		//FHitResult result;
		//TraceunderCrossHair(result);
		Fire();
	}
}

void UCombatComponent::Fire()
{
	if (bCanFire)
	{
		ServerFire(HitTarget);
		if (EquippedWeapon)
		{
			CrosshairShootingFactor = 0.75f;
		}
		StartFireTimer();
	}
}

void UCombatComponent::StartFireTimer()
{
	if (!IsValid(EquippedWeapon) || !IsValid(Character)) return;
	bCanFire = false;
	Character->GetWorldTimerManager().SetTimer(
		FireTimer,
		this,
		&UCombatComponent::FireTimerFinished,
		EquippedWeapon->FireDelay
	);
}

void UCombatComponent::FireTimerFinished()
{
	if (!IsValid(EquippedWeapon)) return;
	bCanFire = true;
	if (bFireButtonPressed && EquippedWeapon->bAutomatic)
	{
		//FHitResult result;
		//TraceunderCrossHair(result);
		Fire();
	}
}
//서버 RPC 함수
void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	//서버에서 이 함수를 호출하면 서버와 클라이언트 둘 다 에서 모두 실행된다.
	MulticastFire(TraceHitTarget);
}

void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	if (EquippedWeapon == nullptr) return;

	//baiming과 equippedweapon은  항상 동기화되는 변수이다.
	if (IsValid(Character))
	{
		Character->PlayFireMonatge(bAiming);
		EquippedWeapon->Fire(TraceHitTarget);
	}
}

void UCombatComponent::ServerSetAiming_Implementation(bool bIsAiming)
{
	bAiming = bIsAiming;
	if (IsValid(Character))
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = bAiming ? AimWalkSpeed : BaseWalkSpeed;
	}
}

void UCombatComponent::EquipWeapon(AWeapon* WeaponToEquip)
{
	if (Character == nullptr || WeaponToEquip == nullptr) return;

	EquippedWeapon = WeaponToEquip;
	//이 WeaponState를 Replicate 변수로 만들어서 클라이언트에서 관리하도록 하자
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (HandSocket)
	{
		HandSocket->AttachActor(EquippedWeapon, Character->GetMesh());
	}
	//장비의 Owner를 설정해줘야한다.
	//SetOwener을 하면서 설정하는 Owner은
	//UPROPERTY(ReplicatedUsing=OnRep_Owner)
	//TObjectPtr<AActor> Owner;
	//이와 같은 형태로 이미 Replicate되어 있기 때문에 클라이언트에서 따로 신경 쓸 필요가 없다.
	//OnRep_Owner()은 virtual 함수기 때문에 프로그래머가 덮어씌울 수 있음.
	EquippedWeapon->SetOwner(Character);
	Character->GetCharacterMovement()->bOrientRotationToMovement = false;
	Character->bUseControllerRotationYaw = true;
	//이 함수는 클라이언트에서 호출이 되지 않는다 (왜냐면 이 함수는 서버에서 호출되었기 때문에 -> 그래서 클라이언트에 전달이 되지 않는다.)
	//EquippedWeapon->ShowPickupWidget(false);
	//EquippedWeapon->GetAreaSphere()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UCombatComponent::TraceunderCrossHair(FHitResult& TraceHitResult)
{
	//Trace하기 위해서는 Viewport의 사이즈가 필요하다.
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	FVector2D CorssHairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	FVector CrosshairWorldPosition;
	FVector CrossHairWorldDriection;
	//projection - > 좌표를 얻을 수 있다.(크로스헤어의 월드 포지션)
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		UGameplayStatics::GetPlayerController(this, 0),
		CorssHairLocation,
		CrosshairWorldPosition,
		CrossHairWorldDriection
	);

	if (bScreenToWorld)
	{
		FVector Start = CrosshairWorldPosition;
		if (IsValid(Character))
		{
			float DistanceToCharacter = (Character->GetActorLocation() - Start).Size();
			Start += CrossHairWorldDriection * (DistanceToCharacter + 100.f);
		}
		FVector End = Start + CrossHairWorldDriection * TRACE_LENGTH;

		//라인 트레이스 (크로스헤어의 시작과 끝으로)
		GetWorld()->LineTraceSingleByChannel(
			TraceHitResult,
			Start,
			End,
			ECollisionChannel::ECC_Visibility
		);
		//if (!TraceHitResult.bBlockingHit)
		//{
		//	TraceHitResult.ImpactPoint = End;
		//	HitTarget = End;
		//}
		//피격 했다면 크로스 헤어를 빨간색으로 변경시키자.


		if (TraceHitResult.GetActor() && TraceHitResult.GetActor()->GetClass()->ImplementsInterface(UInteractWithCrosshairInterface::StaticClass()))
		{
			HUDPackage.CrosshairColor = FLinearColor::Red;
		}
		else
		{
			HUDPackage.CrosshairColor = FLinearColor::White;
		}
		//else
		//{
		//	HitTarget = TraceHitResult.ImpactPoint;
		//	//DrawDebugSphere(GetWorld(), TraceHitResult.ImpactPoint, 12.f, 12, FColor::Red);
		//}
	}
}

void UCombatComponent::SetHUDCrossHairs(float DeltaTime)
{
	//playercontroller는 hud에 접근할 수 있다.
	if (!IsValid(Character) || !IsValid(Character->GetController())) return;

	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->GetController()) : Controller;
	if (IsValid(Controller))
	{
		HUD = HUD == nullptr ? Cast<ABlasterHUD>(Controller->GetHUD()) : HUD;
		if (IsValid(HUD))
		{
			if (IsValid(EquippedWeapon))
			{
				HUDPackage.CrossHairCenter = EquippedWeapon->CrossHairsCenter;
				HUDPackage.CrossHairLeft = EquippedWeapon->CrossHairsLeft;
				HUDPackage.CrossHairRight = EquippedWeapon->CrossHairsRight;
				HUDPackage.CrossHairTop = EquippedWeapon->CrossHairsTop;
				HUDPackage.CrossHairBottom = EquippedWeapon->CrossHairsBottom;
			}
			else
			{
				HUDPackage.CrossHairCenter = nullptr;
				HUDPackage.CrossHairLeft = nullptr;
				HUDPackage.CrossHairRight = nullptr;
				HUDPackage.CrossHairTop = nullptr;
				HUDPackage.CrossHairBottom = nullptr;
			}
			//크로스 헤어 퍼짐 계산 
			//보통은 캐릭터의 움직임에 따라 변경되므로 그것을 반영하자.

			//[0, 600] -> [0, 1] (0~1)범위로 제한하고싶다.
			//clamp를 하자
			//FMath::GetMappedRangeValueClamped(0.f, Character->GetCharacterMovement()->MaxWalkSpeed);

			FVector2D WalkSpeedRange(0.f, Character->GetCharacterMovement()->MaxWalkSpeed);
			FVector2D VelocityMultiplierRange(0.f, 1.f);
			FVector Velocity = Character->GetVelocity();
			Velocity.Z = 0.f;

			CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpeedRange, VelocityMultiplierRange, Velocity.Size());
			//하늘에 있다면 좀 더 보강하자
			if (Character->GetCharacterMovement()->IsFalling())
			{
				CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f);
			}
			else
			{
				CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);
			}

			if (bAiming)
			{
				CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.58f, DeltaTime, 30.f);
			}
			else
			{
				CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.f, DeltaTime, 30.f);
			}

			CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime, 40.f);

			HUDPackage.CrosshairSpread = 0.5f + CrosshairVelocityFactor + CrosshairInAirFactor - CrosshairAimFactor + CrosshairShootingFactor;

			HUD->SetHUDPackage(HUDPackage);
		}
	}
}
