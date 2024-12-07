// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/ProjectileBullet.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"


void AProjectileBullet::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalInpulse, const FHitResult& Hit)
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (IsValid(OwnerCharacter))
	{
		AController* OwnerController = OwnerCharacter->Controller;
		if (IsValid(OwnerController))
		{
			UGameplayStatics::ApplyDamage(OtherActor, Damage, OwnerController, this, UDamageType::StaticClass());
		}
	}

	//���� �ڵ忡�� ����� Destory�� ����Ǳ� ������ �� �ļ����� �̷���Ѵ�.
	Super::OnHit(HitComp, OtherActor, OtherComp, NormalInpulse, Hit);
}
