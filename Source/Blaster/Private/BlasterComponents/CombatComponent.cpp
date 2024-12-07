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
	//�������� ������ ĳ���Ͱ� �� �Լ��� ȣ���ϸ� ���������� �۵��ϰ� Ŭ���̾�Ʈ�� �������ʴ´�.
	//�������� �������� ���� ĳ���Ͱ� �� �Լ��� ȣ���ϸ� ������ ������.
	ServerSetAiming(_bIsAiming);
	//���ؽÿ� max speed�� �����غ���. 
	//�ܼ��ϰ� maxwalkspeed�� Ŭ���̾�Ʈ���� �����ϸ� �ȵȴ�. movement component�� �����ϴ� ���� ����ȭ �ڵ尡 ���������� ��ġ�� ����ȭ ��Ű�� �����̴�.
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
		//� ����� ���� ���� �ӵ��� ���ư��� �ϱ� ���� combatcomponent�� zoomeiterpspeed�� ����Ѵ�.
		CurrentFOV = FMath::FInterpTo(CurrentFOV, DefaultFOV, DeltaTime, ZoomInterpSpeed);
	}
	if (IsValid(Character) && IsValid(Character->GetFollowCamera()))
	{
		Character->GetFollowCamera()->SetFieldOfView(CurrentFOV);
	}
}

void UCombatComponent::FireButtonPressed(bool bPressed)
{
	//�̰� �� Ŭ���̾�Ʈ�� �����ϵ��� �����ϸ� ���� �ٸ� Ŭ���̾�Ʈ�� ����ȭ �ȴ�
	//������ ���� ����ϴ� ���� �ѹ� �� ������ �����ؼ� �߻��ؾ� �ϱ� ������ �̷������θ� �ϸ� �ȵȴ�.
	//multicast rpc�� �̿�����.
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
//���� RPC �Լ�
void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	//�������� �� �Լ��� ȣ���ϸ� ������ Ŭ���̾�Ʈ �� �� ���� ��� ����ȴ�.
	MulticastFire(TraceHitTarget);
}

void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	if (EquippedWeapon == nullptr) return;

	//baiming�� equippedweapon��  �׻� ����ȭ�Ǵ� �����̴�.
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
	//�� WeaponState�� Replicate ������ ���� Ŭ���̾�Ʈ���� �����ϵ��� ����
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (HandSocket)
	{
		HandSocket->AttachActor(EquippedWeapon, Character->GetMesh());
	}
	//����� Owner�� ����������Ѵ�.
	//SetOwener�� �ϸ鼭 �����ϴ� Owner��
	//UPROPERTY(ReplicatedUsing=OnRep_Owner)
	//TObjectPtr<AActor> Owner;
	//�̿� ���� ���·� �̹� Replicate�Ǿ� �ֱ� ������ Ŭ���̾�Ʈ���� ���� �Ű� �� �ʿ䰡 ����.
	//OnRep_Owner()�� virtual �Լ��� ������ ���α׷��Ӱ� ����� �� ����.
	EquippedWeapon->SetOwner(Character);
	Character->GetCharacterMovement()->bOrientRotationToMovement = false;
	Character->bUseControllerRotationYaw = true;
	//�� �Լ��� Ŭ���̾�Ʈ���� ȣ���� ���� �ʴ´� (�ֳĸ� �� �Լ��� �������� ȣ��Ǿ��� ������ -> �׷��� Ŭ���̾�Ʈ�� ������ ���� �ʴ´�.)
	//EquippedWeapon->ShowPickupWidget(false);
	//EquippedWeapon->GetAreaSphere()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UCombatComponent::TraceunderCrossHair(FHitResult& TraceHitResult)
{
	//Trace�ϱ� ���ؼ��� Viewport�� ����� �ʿ��ϴ�.
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	FVector2D CorssHairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	FVector CrosshairWorldPosition;
	FVector CrossHairWorldDriection;
	//projection - > ��ǥ�� ���� �� �ִ�.(ũ�ν������ ���� ������)
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

		//���� Ʈ���̽� (ũ�ν������ ���۰� ������)
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
		//�ǰ� �ߴٸ� ũ�ν� �� ���������� �����Ű��.


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
	//playercontroller�� hud�� ������ �� �ִ�.
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
			//ũ�ν� ��� ���� ��� 
			//������ ĳ������ �����ӿ� ���� ����ǹǷ� �װ��� �ݿ�����.

			//[0, 600] -> [0, 1] (0~1)������ �����ϰ�ʹ�.
			//clamp�� ����
			//FMath::GetMappedRangeValueClamped(0.f, Character->GetCharacterMovement()->MaxWalkSpeed);

			FVector2D WalkSpeedRange(0.f, Character->GetCharacterMovement()->MaxWalkSpeed);
			FVector2D VelocityMultiplierRange(0.f, 1.f);
			FVector Velocity = Character->GetVelocity();
			Velocity.Z = 0.f;

			CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpeedRange, VelocityMultiplierRange, Velocity.Size());
			//�ϴÿ� �ִٸ� �� �� ��������
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
