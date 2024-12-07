// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BlasterCharacter.h"
#include "Camera/CameraComponent.h"

#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"

#include "BlasterComponents/CombatComponent.h"
#include "Character/BlasterAnimInstance.h"
#include "PlayerController/BlasterPlayerController.h"

#include "HUD/OverheadWidget.h"

#include "Kismet/KismetMathLibrary.h"

#include "Net/UnrealNetwork.h"

#include "Weapon/Weapon.h"

#include "Blaster/Blaster.h"
// Sets default values
ABlasterCharacter::ABlasterCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	//Capsule Component�� ����� ���� ��� CameraBoom�� ���� �����̱� ������ Mesh�� ����
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 600.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	//�ɸ��Ͱ� ��Ʈ�ѷ��� �Բ� ȸ������ �ʴ´�.
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	//OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	//OverheadWidget->SetupAttachment(RootComponent);

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionObjectType(ECC_SkletalMesh);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 850.f);

	eTurningInPlace = ETurningInPlace::ETIP_NotTurning;
	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f;
}

void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	/*
	* DOREPLIFETIME(ABlasterCharacter, OverlappedWeapon);
	* �̷������� �ܼ��ϰ� �����ϸ� �� ĳ���Ϳ��� ���� �� ���� ��� ĳ���Ϳ��� ����Ǽ� ������ �߻��� ���� �ִ�.
	* DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappedWeapon, COND_OwnerOnly);
	* �̷������� �ϸ� ���ο��Ը� ����ȴ�.(������ ����ȴ�.)
	*/
	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappedWeapon, COND_OwnerOnly);
	DOREPLIFETIME(ABlasterCharacter, Health);
}

void ABlasterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (IsValid(Combat))
	{
		Combat->Character = this;
	}
}

void ABlasterCharacter::PlayFireMonatge(bool bAiming)
{
	if (!IsValid(Combat) || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (IsValid(AnimInstance) && FireWeaponMontage)
	{
		AnimInstance->Montage_Play(FireWeaponMontage);
		FName SectionName;
		SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::PlayHitReactMontage()
{
	if (!IsValid(Combat) || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (IsValid(AnimInstance) && HitReactMontage)
	{
		AnimInstance->Montage_Play(HitReactMontage);
		FName SectionName("FromFront");
		//SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}
//�������� ȣ��Ǵ� �Լ�.
void ABlasterCharacter::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser)
{
	//�������� onhit�ÿ� multicast rpc�� ����������, ������ �ٸ� ����� ����� ����.
	Health = FMath::Clamp(Health - Damage, 0, MaxHealth);
	UpdateHUDHealth();
	PlayHitReactMontage();
}

void ABlasterCharacter::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();
	if (GetLocalRole() == ENetRole::ROLE_SimulatedProxy)
	{
		SimProxiesTurn();
	}
	TimeSinceLastMovementReplication = 0.f;
}

void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();

	UpdateHUDHealth();
	//{
	//	UOverheadWidget* OverheadWidgetObeject = Cast<UOverheadWidget>(OverheadWidget->GetUserWidgetObject());
	//	OverheadWidgetObeject->ShowPlayerNetRole(this);
	//}
	if (HasAuthority())
	{
		OnTakeAnyDamage.AddDynamic(this, &ABlasterCharacter::ReceiveDamage);
	}
}

void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetLocalRole() > ENetRole::ROLE_SimulatedProxy && IsLocallyControlled())
	{
		AimOffset(DeltaTime);
	}
	else
	{
		TimeSinceLastMovementReplication += DeltaTime;
		if (TimeSinceLastMovementReplication > 0.25f)
		{
			OnRep_ReplicatedMovement();
		}
		CalculateAO_Pitch();
	}
	HideCameraIfCharacterClose();
}

void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ThisClass::Jump);
	PlayerInputComponent->BindAction("Equip", IE_Pressed, this, &ThisClass::EquipButtonPresssed);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ThisClass::CrouchButtonPressed);
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &ThisClass::AimButtonPressed);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &ThisClass::AimButtonReleased);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ThisClass::FireButtonPressed);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &ThisClass::FireButtonReleased);

	PlayerInputComponent->BindAxis("MoveForward", this, &ThisClass::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ThisClass::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &ThisClass::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &ThisClass::LookUp);
}

void ABlasterCharacter::MoveForward(float Value)
{
	if (IsValid(Controller) && Value != 0.f)
	{
		//��Ʈ�ѷ��� �ٶ󺸴� ������ �������� ������ ����
		//FrotationMaxtrix�� ���� YawRotation�� �������� ȸ�� ����� ���� ��(�𸮾� ���� xy���, DirectX ���� �޼� ��ǥ���� ��쿡�� xz����� �ȴ�.)2���� ������ x���� �ٶ󺸴� ������ �˾Ƴ� �� �ִ�.
		//Pitch�� �������� ��´ٸ� (�𸮾� ���� xz���)2���� ������ z�� �ٶ󺸴� ����(���Ʒ�)�� �˾Ƴ� �� �ִ�.
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X));
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::MoveRight(float Value)
{
	if (IsValid(Controller) && Value != 0.f)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y));
		AddMovementInput(Direction, Value);
	}

}

void ABlasterCharacter::Turn(float Value)
{
	AddControllerYawInput(Value);
}

void ABlasterCharacter::LookUp(float Value)
{
	AddControllerPitchInput(Value * -0.6f);
}

//���� ������ �������� �ؾ��Ѵ�. 
//Ŭ���̾�Ʈ���� �ϸ� �ȵȴ�.
void ABlasterCharacter::EquipButtonPresssed()
{
	//���������� �۵�
	if (IsValid(Combat))
	{
		if (HasAuthority())
		{
			Combat->EquipWeapon(OverlappedWeapon);
		}
		else
		{
			//Ŭ���̾�Ʈ�� �̰� ���� ������ ����� ������.
			ServerEquipButtonPressed();
		}
	}
}

void ABlasterCharacter::CrouchButtonPressed()
{
	//WantsToCrouch �� ���� (character�� ����) �̹� replicated �Ǿ��־ OnRep_IsCrouched()�Լ��� ȣ���Ѵ�.
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void ABlasterCharacter::AimButtonPressed()
{
	if (IsValid(Combat))
	{
		Combat->SetAiming(true);
	}
}

void ABlasterCharacter::AimButtonReleased()
{
	if (IsValid(Combat))
	{
		Combat->SetAiming(false);
	}
}

float ABlasterCharacter::CalculateSpeed()
{
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	return Velocity.Size();
}

void ABlasterCharacter::AimOffset(float DeltaTime)
{
	if (!IsValid(Combat) && !IsValid(Combat->EquippedWeapon)) return;

	float Speed = CalculateSpeed();
	bool bIsInAir = GetCharacterMovement()->IsFalling();

	//������ ���ְ� �������� ���� ��
	if (Speed == 0.f && !bIsInAir)
	{
		bRotateRootBone = true;
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		if (eTurningInPlace == ETurningInPlace::ETIP_NotTurning)
		{
			InterpAO_Yaw = AO_Yaw;
		}
		TurningInPlace(DeltaTime);
		if (!IsWeaponEquipped())
		{
			bUseControllerRotationYaw = false;
		}
		else
		{
			bUseControllerRotationYaw = true;
		}
	}
	//�ٰų� �������� ��
	if (Speed > 0.f || bIsInAir)
	{
		bRotateRootBone = false;
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		bUseControllerRotationYaw = true;
		eTurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}

	//�� ���� �߸� ���� �� �ִ�. (������ ����� �Ǵ� ����)
	//�𸮾� �������� Rotation�� ��Ʈ��ũ�� ���� ������ �߻��ϴ� �����̴�
	//�ֳĸ� ��Ʈ��ŷ�� �� �� ����ȭ(����)�� �ϱ� ������ �߻��Ҽ��ִ� ������.
	//ChracterMovementComponent�� GetPacketAngel�� ���� �����ö�, ���� �����ϱ� ������ (unsinged��) ������ �߻��Ѵ� (-1���� 359�� �� ��)
	//	return FMath::RoundToInt(Angle * (T)65536.f / (T)360.f) & 0xFFFF; 65536.f�� short �� �ִ밪. (360���� ���� 0 ~ 65536�� ������ ġȯ�Ѵ�.)

	CalculateAO_Pitch();
}

void ABlasterCharacter::CalculateAO_Pitch()
{
	AO_Pitch = GetBaseAimRotation().Pitch;
	if (AO_Pitch > 90.f && !IsLocallyControlled())
	{
		//Pitch�� (270, 360)������ ������ (-90, 0)������ �����Ѵ�
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);

		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}
}

void ABlasterCharacter::SimProxiesTurn()
{
	if (!IsValid(Combat) || !IsValid(Combat->EquippedWeapon)) return;

	float Speed = CalculateSpeed();
	if (Speed > 0.f)
	{
		eTurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}

	bRotateRootBone = false;
	CalculateAO_Pitch();

	ProxyRotationLastFrame = ProxyRotation;	
	ProxyRotation = GetActorRotation();
	ProxyYaw = UKismetMathLibrary::NormalizedDeltaRotator(ProxyRotation, ProxyRotationLastFrame).Yaw;

	if (FMath::Abs(ProxyYaw) > TurnThreshold)
	{
		if (ProxyYaw > TurnThreshold)
		{
			eTurningInPlace = ETurningInPlace::ETIP_Right;
		}
		else if (ProxyYaw < -TurnThreshold)
		{
			eTurningInPlace = ETurningInPlace::ETIP_Left;
		}
		else 
		{
			eTurningInPlace = ETurningInPlace::ETIP_NotTurning;
		}
		return;
	}

	eTurningInPlace = ETurningInPlace::ETIP_NotTurning;
}

void ABlasterCharacter::Jump()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Super::Jump();
	}
}

void ABlasterCharacter::FireButtonPressed()
{
	if (IsValid(Combat))
	{
		Combat->FireButtonPressed(true);
	}
}

void ABlasterCharacter::FireButtonReleased()
{
	if (IsValid(Combat))
	{
		Combat->FireButtonPressed(false);
	}
}

void ABlasterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon)
{
	if (IsValid(OverlappedWeapon))
	{
		OverlappedWeapon->ShowPickupWidget(true);
	}
	if (LastWeapon)
	{
		LastWeapon->ShowPickupWidget(false);
	}
}

//Ŭ���̾�Ʈ���� ȣ��Ǿ �������� ����ȴ�
//�� �������� �����ϴ� ����
//�� Implementation�� ���� �� �Լ��� �������� ȣ��ǰ� �̰��� ���� ���� �Լ��� Ŭ���̾�Ʈ���� ����ȴ�.
void ABlasterCharacter::ServerEquipButtonPressed_Implementation()
{
	//�����ϴ� ������ ����ȴ�.
	if (IsValid(Combat))
	{
		//�׷��� ������ ���⼭ ������ ����� ������ Ŭ���̾�Ʈ������ �۵����� �ʱ� ������ (���������� ������) Ŭ���̾�Ʈ������ ������ ���δ�.(���ٸ� ó���� ���� ������)
		Combat->EquipWeapon(OverlappedWeapon);
	}
}

void ABlasterCharacter::TurningInPlace(float DeltaTime)
{
	if (AO_Yaw > 90.f)
	{
		eTurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -90.f)
	{
		eTurningInPlace = ETurningInPlace::ETIP_Left;
	}
	if (eTurningInPlace != ETurningInPlace::ETIP_NotTurning)
	{
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 4.f);
		AO_Yaw = InterpAO_Yaw;
		if (FMath::Abs(AO_Yaw) < 15.f)
		{
			eTurningInPlace = ETurningInPlace::ETIP_NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		}
	}
}

//void ABlasterCharacter::MulticastHit_Implementation()
//{
//	PlayHitReactMontage();
//}

void ABlasterCharacter::HideCameraIfCharacterClose()
{
	if (!IsLocallyControlled()) return;
	if (IsValid(FollowCamera) && (FollowCamera->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold)
	{
		GetMesh()->SetVisibility(false);
		if (IsValid(Combat) && IsValid(Combat->EquippedWeapon) && IsValid(Combat->EquippedWeapon->GetWeaponMesh()))
		{
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
	}
	else
	{
		GetMesh()->SetVisibility(true);
		if (IsValid(Combat) && IsValid(Combat->EquippedWeapon) && IsValid(Combat->EquippedWeapon->GetWeaponMesh()))
		{
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
	}
}

void ABlasterCharacter::OnRep_Health()
{
	//ü���� ����� �� hitreact�� ����
	//��, ���� ��Ȳ�� ���� �� ������ ����ؾ��Ѵ�.
	//�̰� Ŭ���̾�Ʈ���� ����ȴ�
	UpdateHUDHealth();
	PlayHitReactMontage();
}

void ABlasterCharacter::UpdateHUDHealth()
{
	BlasterPlayerController = !IsValid(BlasterPlayerController) ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (IsValid(BlasterPlayerController))
	{
		BlasterPlayerController->SetHUDHealth(Health, MaxHealth);
	}
}
//���������� ȣ���� �ȴ�.
void ABlasterCharacter::SetOverlappingWeapon(AWeapon* Weapon)
{
	if (IsValid(OverlappedWeapon))
	{
		OverlappedWeapon->ShowPickupWidget(false);
	}
	OverlappedWeapon = Weapon;
	//�������� ������ ȣ���� �ϴ� �÷��̾��� ���
	//�������� ��Ʈ�� �ϴ� ĳ���͸� ������ �� �ִ�.
	//COND_OwnerOnly �̰����� �����߱� ������ �̷������� �ؾ��Ѵ�.
	if (IsLocallyControlled())
	{
		if (IsValid(OverlappedWeapon))
		{
			OverlappedWeapon->ShowPickupWidget(true);
		}
	}
}

bool ABlasterCharacter::IsWeaponEquipped()
{
	//EquippedWeapon �� ���� ���� ���������� ������ �Ǿ��ִ�.
	//���������� �����Ѱ����� ���̰�, �װ��� �پ��ٴ°� Ŭ���̾�Ʈ�� �������⸸ �ϴ� ����(���� ����)
	return ( IsValid(Combat) && IsValid(Combat->EquippedWeapon));
}

bool ABlasterCharacter::IsAiming()
{
	return (IsValid(Combat) && Combat->IsAiming());
}

AWeapon* ABlasterCharacter::GetEquippedWeapon()
{
	if (!IsValid(Combat)) return nullptr;
	return Combat->EquippedWeapon;
}

FVector ABlasterCharacter::GetHitTarget() const
{
	if (!IsValid(Combat)) return FVector();
	return Combat->HitTarget;
}



