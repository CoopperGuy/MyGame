// Fill out your copyright notice in the Description page of Project Settings.


#include "Menu.h"
#include "Components/Button.h"
#include "MultiplayerSessionSubsystem.h"
#include "OnlineSubsystem.h"
void UMenu::MenuSetup(int32 NumberOfPublicConnections, FString TypeOfMatch, FString LobbyPath)
{
	PathToLobby = FString::Printf(TEXT("%s?Listen"), *LobbyPath);
	NumPublicConnections = NumberOfPublicConnections;
	MatchType = TypeOfMatch;
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	bIsFocusable = true;

	UWorld* World = GetWorld();
	if (World)
	{
		//���⼭ �÷��̾��� inputmode�� �����߱� ������ ���Ŀ� �������� ������ ����ؼ� �Է��� �����ִ�.
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	//Gameinstance subsystem�̱� ������ Gameinstance���� ������ �����ϴ�.
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		MultiplayerSessionSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionSubsystem>();
	}

	if (MultiplayerSessionSubsystem)
	{
		//Dynamic Delegate�� AddDynamic�� ���� �߰�.
		//Deleagte�� AddUObject�� ���� �߰�.
		MultiplayerSessionSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &ThisClass::OnCreateSession);
		MultiplayerSessionSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &ThisClass::OnDestroySession);
		MultiplayerSessionSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &ThisClass::OnStartSession);
		MultiplayerSessionSubsystem->MultiplayerOnFindSessionComplete.AddUObject(this, &ThisClass::OnFindSession);
		MultiplayerSessionSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSession);
	}
}

bool UMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (IsValid(HostButton))
	{
		HostButton->OnClicked.AddDynamic(this, &ThisClass::HostButtonClicked);
	}
	if (IsValid(JoinButton))
	{
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::JoinButtonClicked);
	}
	if (IsValid(QuitButton))
	{
		QuitButton->OnClicked.AddDynamic(this, &ThisClass::QuitButtonClicked);
	}

	return true;
}

void UMenu::NativeDestruct()
{
	MenuTearDown();

	Super::NativeDestruct();
}

void UMenu::OnCreateSession(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		//Listen������ �κ� �����Ѵ�.
		UWorld* World = GetWorld();
		if (World)
		{
			//���� GameMode�� ��� ������ �̵���Ų��.
			World->ServerTravel(PathToLobby);
		}
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Red,
				FString(TEXT("Fail to Create Session!"))
			);
		}
		HostButton->SetIsEnabled(true);
	}
}

//MultiplayerSessionSubsystem���� Boardcast�� Delegate�� �޾Ƽ� ȣ��ȴ�.
void UMenu::OnFindSession(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	if (!IsValid(MultiplayerSessionSubsystem))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Yellow,
				FString(TEXT("Fail To Find Session!"))
			);
		}
		JoinButton->SetIsEnabled(true);
		return;
	}
	for (auto& Result : SessionResults)
	{
		//���� Type�� �ִٸ� �ٷ� �����Ѵ�.
		//FString SettingsValue;
		//Result.Session.SessionSettings.Get(FName("MatchType"), SettingsValue);
		//if (SettingsValue == MatchType)
		//{
		//	MultiplayerSessionSubsystem->JoinSession(Result);
		//}

		FString Id = Result.GetSessionIdStr();
		FString User = Result.Session.OwningUserName;
		//ã�� ������ MatchType(Key)�� ��ġ�ϴ��� Ȯ���Ѵ�.
		FString SettingsValue;
		Result.Session.SessionSettings.Get(FName("MatchType"), SettingsValue);
		if (MatchType == SettingsValue)
		{
			MultiplayerSessionSubsystem->JoinSession(Result);
		}
	}

	if (!bWasSuccessful || SessionResults.Num() == 0)
	{
		JoinButton->SetIsEnabled(true);
	}
}

void UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	//��Ȯ�� IP�ּҸ� ã�Ƽ� �����ؾ��Ѵ�.
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Yellow,
				FString(TEXT("Fail To Join Session!"))
			);
		}
		JoinButton->SetIsEnabled(true);
		return;
	}
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			FString Address;
			if (SessionInterface->GetResolvedConnectString(NAME_GameSession, Address))
			{
				//PlayerController�� �����Ѵ�.
				//GameInstance�� ���� ���� �� �ִ�.
				//Player�� �������� ��쿡�� ��� �ؾ��ұ�?
				APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
				if (PlayerController)
				{
					PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
				}
			}
		}
	}
}

void UMenu::OnDestroySession(bool bWasSuccessful)
{
}

void UMenu::OnStartSession(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			//���� GameMode�� ��� ������ �̵���Ų��.
			World->ServerTravel(PathToGameSession);
		}
	}
}

void UMenu::HostButtonClicked()
{
	HostButton->SetIsEnabled(false);
	//��ư�� Ŭ���ϸ� ������ �����
	if (IsValid(MultiplayerSessionSubsystem))
	{
		MultiplayerSessionSubsystem->CreateSession(NumPublicConnections, MatchType);
	}
}

void UMenu::JoinButtonClicked()
{
	JoinButton->SetIsEnabled(false);
	if (IsValid(MultiplayerSessionSubsystem))
	{
		MultiplayerSessionSubsystem->FindSession(100000);
	}
}

void UMenu::StartButtonClicked()
{
	if (IsValid(MultiplayerSessionSubsystem))
	{
		MultiplayerSessionSubsystem->StartSession();
	}
}

void UMenu::QuitButtonClicked()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (IsValid(GameInstance))
	{
		MultiplayerSessionSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionSubsystem>();
	}
	if (IsValid(MultiplayerSessionSubsystem))
	{
		MultiplayerSessionSubsystem->QuitGame();
	}
}

void UMenu::MenuTearDown()
{
	RemoveFromParent();
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
}
