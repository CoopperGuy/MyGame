// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/OverheadWidget.h"
#include "Components/TextBlock.h"

void UOverheadWidget::SetDisplayText(FString TextToDisplay)
{
	if (DisplayText)
	{
		DisplayText->SetText(FText::FromString(TextToDisplay));
	}
}

void UOverheadWidget::ShowPlayerNetRole(APawn* InPawn)
{
	//Local (현재 컴퓨터에서 보여지는 Role)
	//ENetRole LocalRole = InPawn->GetLocalRole();
	//FString Role;
	//switch (LocalRole)
	//{
	//case ROLE_None:
	//	Role = FString("None");
	//	break;
	//case ROLE_SimulatedProxy:
	//	Role = FString("Simulated Proxy");
	//	break;
	//case ROLE_AutonomousProxy:
	//	Role = FString("Authority Proxy");
	//	break;
	//case ROLE_Authority:
	//	Role = FString("Authority");
	//	break;
	//case ROLE_MAX:
	//	break;
	//}
	//FString LocalRoleString = FString::Printf(TEXT("Local Role : %s"), *Role);
	//SetDisplayText(LocalRoleString);

	//이 경우에서는 Authority Proxy의 메인과 나머지 Simulated Proxy 이렇게 있음.
	ENetRole RemoteRol = InPawn->GetRemoteRole();
	FString Role;
	switch (RemoteRol)
	{
	case ROLE_None:
		Role = FString("None");
		break;
	case ROLE_SimulatedProxy:
		Role = FString("Simulated Proxy");
		break;
	case ROLE_AutonomousProxy:
		Role = FString("Authority Proxy");
		break;
	case ROLE_Authority:
		Role = FString("Authority");
		break;
	case ROLE_MAX:
		break;
	}
	FString RemoteRoleString = FString::Printf(TEXT("Remote Role : %s"), *Role);
	SetDisplayText(RemoteRoleString);
}

void UOverheadWidget::NativeDestruct()
{
	RemoveFromParent();
	Super::NativeDestruct();
}

