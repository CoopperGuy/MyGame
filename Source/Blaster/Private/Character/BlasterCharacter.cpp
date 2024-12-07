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
	//Capsule Component의 사이즈가 변할 경우 CameraBoom도 같이 움직이기 때문에 Mesh에 붙임
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 600.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	//케릭터가 컨트롤러와 함께 회전하지 않는다.
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
	* 이런식으로 단순하게 설정하면 한 캐릭터에게 적용 된 값이 모든 캐릭터에게 적용되서 문제가 발생할 수도 있다.
	* DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappedWeapon, COND_OwnerOnly);
	* 이런식으로 하면 주인에게만 복사된다.(서버는 복사된다.)
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
//서버에서 호출되는 함수.
void ABlasterCharacter::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser)
{
	//기존에는 onhit시에 multicast rpc로 전파했지만, 지금은 다른 방법을 쓰기로 하자.
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
		//컨트롤러가 바라보는 방향을 기준으로 앞으로 전진
		//FrotationMaxtrix를 통해 YawRotation을 기준으로 회전 행렬을 만든 후(언리얼 기준 xy평면, DirectX 등의 왼손 좌표계의 경우에는 xz평면이 된다.)2차원 평면상의 x축을 바라보는 방향을 알아낼 수 있다.
		//Pitch를 기준으로 잡는다면 (언리얼 기준 xz평면)2차원 평면상의 z축 바라보는 방향(위아래)를 알아낼 수 있다.
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

//무기 장착은 서버에서 해야한다. 
//클라이언트에서 하면 안된다.
void ABlasterCharacter::EquipButtonPresssed()
{
	//서버에서만 작동
	if (IsValid(Combat))
	{
		if (HasAuthority())
		{
			Combat->EquipWeapon(OverlappedWeapon);
		}
		else
		{
			//클라이언트는 이걸 통해 서버로 통신을 보낸다.
			ServerEquipButtonPressed();
		}
	}
}

void ABlasterCharacter::CrouchButtonPressed()
{
	//WantsToCrouch 이 값은 (character에 존재) 이미 replicated 되어있어서 OnRep_IsCrouched()함수를 호출한다.
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

	//가만히 서있고 점프하지 않을 때
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
	//뛰거나 점프했을 때
	if (Speed > 0.f || bIsInAir)
	{
		bRotateRootBone = false;
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		bUseControllerRotationYaw = true;
		eTurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}

	//이 값이 잘못 들어올 수 있다. (음수가 양수가 되는 문제)
	//언리얼 언진에서 Rotation을 네트워크를 통해 보낼때 발생하는 문제이다
	//왜냐면 네트워킹을 할 떄 직렬화(압축)을 하기 때문에 발생할수있는 문제다.
	//ChracterMovementComponent의 GetPacketAngel을 통해 가져올때, 값을 압축하기 때문에 (unsinged로) 문제가 발생한다 (-1도가 359도 가 됨)
	//	return FMath::RoundToInt(Angle * (T)65536.f / (T)360.f) & 0xFFFF; 65536.f은 short 의 최대값. (360도의 값을 0 ~ 65536의 범위로 치환한다.)

	CalculateAO_Pitch();
}

void ABlasterCharacter::CalculateAO_Pitch()
{
	AO_Pitch = GetBaseAimRotation().Pitch;
	if (AO_Pitch > 90.f && !IsLocallyControlled())
	{
		//Pitch의 (270, 360)범위의 각도를 (-90, 0)각도로 매핑한다
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

//클라이언트에서 호출되어서 서버에서 실행된다
//즉 서버에서 실행하는 로직
//즉 Implementation이 붙은 이 함수가 서버에서 호출되고 이것이 붙지 않은 함수가 클라이언트에서 실행된다.
void ABlasterCharacter::ServerEquipButtonPressed_Implementation()
{
	//장착하는 로직이 실행된다.
	if (IsValid(Combat))
	{
		//그렇기 때문에 여기서 위젯을 지우는 로직이 클라이언트에서는 작동하지 않기 때문에 (서버에서만 지워짐) 클라이언트에서는 위젯이 보인다.(별다른 처리를 하지 않으면)
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
	//체력이 변경될 때 hitreact를 진행
	//단, 여러 상황이 있을 수 있으니 고려해야한다.
	//이건 클라이언트에서 실행된다
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
//서버에서만 호출이 된다.
void ABlasterCharacter::SetOverlappingWeapon(AWeapon* Weapon)
{
	if (IsValid(OverlappedWeapon))
	{
		OverlappedWeapon->ShowPickupWidget(false);
	}
	OverlappedWeapon = Weapon;
	//서버에서 게임을 호스팅 하는 플레이어의 경우
	//서버에서 컨트롤 하는 캐릭터를 가져올 수 있다.
	//COND_OwnerOnly 이것으로 설정했기 때문에 이런식으로 해야한다.
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
	//EquippedWeapon 이 값은 오직 서버에서만 설정이 되어있다.
	//서버에서만 장착한것으로 보이고, 그것이 붙었다는게 클라이언트에 보여지기만 하는 로직(현재 상태)
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



