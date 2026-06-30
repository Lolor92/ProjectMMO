#include "Character/PMMO_PlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "PlayerState/PMMO_PlayerState.h"

APMMO_PlayerCharacter::APMMO_PlayerCharacter()
{
	SpringArm = CreateDefaultSubobject<USpringArmComponent>("Spring Arm");
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 750.f;
	SpringArm->SetRelativeLocation(FVector(0.f, 25.f, 50.f));
	SpringArm->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
	Camera->SetupAttachment(SpringArm);
	Camera->bUsePawnControlRotation = false;
}

void APMMO_PlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	// Server-side ASC initialization.
	InitializeAbilitySystem();
}

void APMMO_PlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	// Client-side ASC initialization.
	InitializeAbilitySystem();
}

void APMMO_PlayerCharacter::InitializeAbilitySystem()
{
	APMMO_PlayerState* PMMO_PlayerState = GetPlayerState<APMMO_PlayerState>();
	if (!PMMO_PlayerState) return;

	// Player characters use the PlayerState-owned ASC.
	AbilitySystemComponent = PMMO_PlayerState->GetAbilitySystemComponent();
	AttributeSet = PMMO_PlayerState->GetAttributeSet();

	if (!AbilitySystemComponent) return;

	AbilitySystemComponent->InitAbilityActorInfo(PMMO_PlayerState, this);
}
