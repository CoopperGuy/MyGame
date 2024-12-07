// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiplayerSessionSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Kismet/KismetSystemLibrary.h"

UMultiplayerSessionSubsystem::UMultiplayerSessionSubsystem()
	:CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete))
	, FindSessionCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionComplete))
	, JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete))
	, DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete))
	, StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete))

{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		SessionInterface = Subsystem->GetSessionInterface();
	}
}

void UMultiplayerSessionSubsystem::CreateSession(int32 NumPublicConnections, FString MatchType)
{
	if (!SessionInterface.IsValid())
	{
		return;
	}

	//�̰� ȣ�� �� CreateSession�� ȣ���ϱ� ������ ������ ����.
	//CreateSession�� ������ ���ɼ��� ����.
	//�׷��� ������ DestroySession�� delegate�� ���ε� �ߴ�. �̰� ó���ؾ���.
	auto ExistingSessiion = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSessiion != nullptr)
	{
		bCreateSessionOnDestory = true;
		LastNumPublicConnections = NumPublicConnections;
		LastMatchType = MatchType;

		DestroySession();
	}


	//Store the delegate in a FDeleagtehandle 
	//Can Remove it from delegate List;
	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);
	LastSessionSettings = MakeShared<FOnlineSessionSettings>();
	//Null���� �ƴϸ� �ٸ� �÷��������� ���� �ٸ� ���� �޴´�.
	LastSessionSettings->bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	LastSessionSettings->NumPublicConnections = NumPublicConnections;
	//�������� ���ǿ� ���� ����
	LastSessionSettings->bAllowJoinInProgress = true;
	//�ٸ� �Ҽ� ����� ���� ������ ���������� ���� ����
	LastSessionSettings->bAllowJoinViaPresence = true;
	//�ٸ� �÷��̾�� ������ ���̰�, ��õ�ϰ� �� �������� ���� ����
	LastSessionSettings->bShouldAdvertise = true;
	//�Ҽ� ��ɰ����� ���� (ģ����Ͽ� ���� ���� �����ֱ� ��)
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->bUseLobbiesIfAvailable = true;
	/*������ �� ������ ������Ǹ� ���� ������ ��ȿ���� ���� �� �ֽ��ϴ�. ���� �� ������ ���ӿ����� �� ID�� �����ؾ� �մϴ�.
	�̸� ���� Ŭ���̾�Ʈ�� ������ ���� �ٸ� ������ ������ ����ϰ� �ִ����� ������ �� �ֽ��ϴ�.
	���� ȣȯ�� �˻� � ���� �� �ֽ��ϴ�.*/
	//��, �� UniqueId�� ���尡 ������ �Ǹ� ����Ǿ���Ѵ�.
	LastSessionSettings->BuildUniqueId = 1;
	//MatchType : Key�� �̸� 
	//FreeForAll  : ��� �÷��̾ ����
	//� �¶��� ��������(steam or ps or xbox etc)�� ping ǥ���Ѵ�.
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	//World�� Player�� ��Ʈ�ѷ� ���� Player�� ��´�.
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	//��Ʈ��ũ�� ID�� ��ȯ�Ѵ�.
	//���۷����� �䱸������ �����ؼ� ����
	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		//���н� �۾�
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);

		//Boradcast custom delegate
		MultiplayerOnCreateSessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionSubsystem::FindSession(int32 MaxSearchResults)
{
	if (!SessionInterface.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Yellow,
				FString(TEXT("InValidate SessionInterface"))
			);
		}
		return;
	}

	//������ ã���� �� ȣ�� �� Delegate�� �Ѱ��� �Ŀ� �����Ѵ�.
	FindSessionCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionCompleteDelegate);
	LastSessionSearch = MakeShared<FOnlineSessionSearch>();
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	//���� �˻��� ������ ����������Ѵ�.
	//��Ī ������ ã�� ���� ����
	LastSessionSearch->QuerySettings.Set(FName(TEXT("PRESENCESEARCH")), true, EOnlineComparisonOp::Equals);

	//localplayer�� ��Ʈ��ũ ID �� �̿��� ã��.
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		//������ ���
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Yellow,
				FString(TEXT("Fail To Find Session! In SubSystem"))
			);
		}
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionCompleteDelegateHandle);
		MultiplayerOnFindSessionComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
}

void UMultiplayerSessionSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	//SessionInterFace�� ��ȿ���� �ʴٸ� Error�� Broadcast�Ѵ�.
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}
	//������ ���� ������ handle�� �����Ѵ�. (���Ŀ� �����ϱ� ����)
	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	//���нÿ��� �Ȱ��� �ʱ�ȭ �ؾ��Ѵ�.
	//�׷��� ���࿡ �Ѱ��� ���� ����� �ִٸ� �� �ڵ�� ������ �� �� �ֳ�? ã�ƺ�����.
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}
}

void UMultiplayerSessionSubsystem::DestroySession()
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);
	if (SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}
}

void UMultiplayerSessionSubsystem::StartSession()
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnStartSessionComplete.Broadcast(false);
		return;
	}

	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegate);
	if (!SessionInterface->StartSession(NAME_GameSession))
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		MultiplayerOnStartSessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionSubsystem::QuitGame()
{
	UWorld* World = GetWorld();
	if (!IsValid(World)) return;

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!IsValid(PlayerController)) return;

	UKismetSystemLibrary::QuitGame(World, PlayerController, EQuitPreference::Quit, false);
}

//���� ������ �����ϸ� �ش� �Լ��� ȣ��ȴ� (delegate��)
void UMultiplayerSessionSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	//�����ϸ� create
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}
	MultiplayerOnCreateSessionComplete.Broadcast(true);

}

void UMultiplayerSessionSubsystem::OnFindSessionComplete(bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionCompleteDelegateHandle);
	}
	if (LastSessionSearch->SearchResults.Num() <= 0)
	{
		//����� ã�� ������ ��
		MultiplayerOnFindSessionComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}
	//�����߱� ������ ����Ʈ�� �Ѱ��ش�.(Menu�� ����ȭ �ϱ� ����)
	MultiplayerOnFindSessionComplete.Broadcast(LastSessionSearch->SearchResults, true);
}

void UMultiplayerSessionSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}
	MultiplayerOnJoinSessionComplete.Broadcast(Result);
}

void UMultiplayerSessionSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}

	//������ ����� �ߴµ� �̹� �������� ���
	if (bWasSuccessful && bCreateSessionOnDestory)
	{
		//������ ���� �ʱ�ȭ �س���.
		bCreateSessionOnDestory = false;
		CreateSession(LastNumPublicConnections, LastMatchType);
	}
	//Menu���� ����ϱ� ���� ���� Delegate
	MultiplayerOnDestroySessionComplete.Broadcast(true);
}

void UMultiplayerSessionSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Yellow,
				FString(TEXT("Fail to Start Session"))
			);
		}
		return;
	}
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}

	MultiplayerOnStartSessionComplete.Broadcast(bWasSuccessful);
}
