// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"


#include "Menu.generated.h"

/**
 * 
 */
class UButton;
UCLASS()
class MULTIPLAYERSESSIONS_API UMenu : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	void MenuSetup(int32 NumberOfPublicConnections = 4, FString TypeOfMatch = FString(TEXT("FreeForAll")), FString LobbyPath = FString(TEXT("/Game/ThirdPerson/Maps/Lobby")));

protected:
	virtual bool Initialize() override;
	virtual void NativeDestruct() override;

	//
	//Callbacks for the custom delegates on the MultiplayerSessionSubsystem
	//Delegate에 바인딩하는 함수는 UFUNCTION을 붙여야한다.
	//
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful);
	//복잡한 형식의 Delegate호출 함수에 UFUNCTION()붙이지 말기, 블루프린트에서 사용할 것이 아님.
	void OnFindSession(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);
	UFUNCTION()
	void OnStartSession(bool bWasSuccessful);

private:
	UPROPERTY(meta = (BindWidget))
	UButton* HostButton;
	UPROPERTY(meta = (BindWidget))
	UButton* JoinButton;
	UPROPERTY(meta = (BindWidget))
	UButton* QuitButton;

	
	UFUNCTION()
	void HostButtonClicked();
	UFUNCTION()
	void JoinButtonClicked();
	UFUNCTION()
	void StartButtonClicked();
	UFUNCTION()
	void QuitButtonClicked();

	void MenuTearDown();

	// The Subsystem desinged to handle all online session functionality
	class UMultiplayerSessionSubsystem* MultiplayerSessionSubsystem;

	int32 NumPublicConnections{ 4 };
	FString MatchType{TEXT("FreeForAll")};
	FString PathToLobby{};
	FString PathToGameSession{ TEXT("/Game/ThirdPerson/Maps/ThirdPersonMap?Listen'") };
};
