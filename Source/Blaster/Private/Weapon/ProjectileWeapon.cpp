// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/ProjectileWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Weapon/Projectile.h"

void AProjectileWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);

	//���� ����ϴ� ���������� �߻�ȴ�.
	if (!HasAuthority()) return;

	APawn* ProjectileInstigator = Cast<APawn>(GetOwner());
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName(FName("MuzzleFlash"));
	if (MuzzleFlashSocket && MuzzleFlashSocket->IsValidLowLevel())
	{
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		//ũ�ν� �� ���� ���� �ѱ� ������ ����
		FVector ToTarget = HitTarget - SocketTransform.GetLocation();
		FRotator TargetRotation = ToTarget.Rotation();
		if (ProjectileClass && ProjectileInstigator)
		{
			UWorld* World = GetWorld();
			FActorSpawnParameters SpawnPrams;
			SpawnPrams.Owner = GetOwner();
			//APawn�� �����Ѵ�. �ν�Ƽ������ - > Ư�� �׼��̳� �̺�Ʈ�� ���������� �߻���Ų actor�� �ǹ�
			/*
			* Owner:
				��ü�� ������ ������
				�Ϲ������� �����ϰų� ���� �����ϴ� ����
				��κ� �θ�-�ڽ� ���質 ���� ���踦 ��Ÿ��

			Instigator:
				Ư�� �׼��� ������ �� ����
				�̺�Ʈ�� �������� �������� Ʈ����
				���� �������� "���� �� �ൿ�� �����ߴ���"�� ����
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
