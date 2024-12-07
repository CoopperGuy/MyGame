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

	//상위 코드에서 종료시 Destory가 실행되기 때문에 최 후순위로 미뤄야한다.
	Super::OnHit(HitComp, OtherActor, OtherComp, NormalInpulse, Hit);
}
