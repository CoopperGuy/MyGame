// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/ProjectileWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Weapon/Projectile.h"

void AProjectileWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);

	//누가 사용하던 서버에서만 발사된다.
	if (!HasAuthority()) return;

	APawn* ProjectileInstigator = Cast<APawn>(GetOwner());
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName(FName("MuzzleFlash"));
	if (MuzzleFlashSocket && MuzzleFlashSocket->IsValidLowLevel())
	{
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		//크로스 헤어가 닿은 곳과 총구 사이의 벡터
		FVector ToTarget = HitTarget - SocketTransform.GetLocation();
		FRotator TargetRotation = ToTarget.Rotation();
		if (ProjectileClass && ProjectileInstigator)
		{
			UWorld* World = GetWorld();
			FActorSpawnParameters SpawnPrams;
			SpawnPrams.Owner = GetOwner();
			//APawn에 존재한다. 인스티게이터 - > 특정 액션이나 이벤트를 직접적으로 발생시킨 actor를 의미
			/*
			* Owner:
				객체의 물리적 소유권
				일반적으로 생성하거나 직접 관리하는 액터
				대부분 부모-자식 관계나 생성 관계를 나타냄

			Instigator:
				특정 액션의 원인이 된 액터
				이벤트나 데미지의 직접적인 트리거
				게임 로직에서 "누가 이 행동을 시작했는지"를 추적
			*/
			SpawnPrams.Instigator = ProjectileInstigator;

			if (World)
			{
				World->SpawnActor<AProjectile>(
					ProjectileClass,
					SocketTransform.GetLocation(),
					TargetRotation,
					SpawnPrams);
			}
		}
	}
}
