// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "LobbyGameMode.generated.h"

/**
* GameMode는 서버에만 존재한다.
 * 1. 현재 로비에 있는 유저의 수를 알고있어야한다.
 * 2. 인원수가 차면 현재 로비의 인원을 이동시켜야한다.
 */
UCLASS()
class BLASTER_API ALobbyGameMode : public AGameMode
{
	GENERATED_BODY()
public:
	virtual void PostLogin(APlayerController* NewPlayer) override;
};
