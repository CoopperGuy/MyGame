// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "LobbyGameMode.generated.h"

/**
* GameMode�� �������� �����Ѵ�.
 * 1. ���� �κ� �ִ� ������ ���� �˰��־���Ѵ�.
 * 2. �ο����� ���� ���� �κ��� �ο��� �̵����Ѿ��Ѵ�.
 */
UCLASS()
class BLASTER_API ALobbyGameMode : public AGameMode
{
	GENERATED_BODY()
public:
	virtual void PostLogin(APlayerController* NewPlayer) override;
};
