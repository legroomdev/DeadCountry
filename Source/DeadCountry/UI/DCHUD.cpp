#include "UI/DCHUD.h"
#include "UI/DCHUDWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

void ADCHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();
	if (PC && PC->IsLocalController())
	{
		Widget = CreateWidget<UDCHUDWidget>(PC, UDCHUDWidget::StaticClass());
		if (Widget)
		{
			Widget->AddToViewport(0);
		}
	}
}
