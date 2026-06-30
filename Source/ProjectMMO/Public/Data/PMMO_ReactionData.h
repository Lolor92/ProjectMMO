#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "PMMO_ReactionData.generated.h"

class UAnimMontage;
class UGameplayEffect;

USTRUCT(BlueprintType, meta=(DisplayName="Predicted Reaction"))
struct FPMMO_ReactionDataEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reaction")
	FGameplayTag ReactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reaction")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reaction")
	float PlayRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reaction")
	FName StartSection = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reaction")
	float MinReplayInterval = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reaction")
	bool bCancelActiveAbilityOnCleanHit = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reaction", meta=(TitleProperty="GameplayEffectClass"))
	TArray<TSubclassOf<UGameplayEffect>> TargetEffects;
};

UCLASS()
class PROJECTMMO_API UPMMO_ReactionData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SyncPrediction|Reaction", meta=(TitleProperty="ReactionTag"))
	TArray<FPMMO_ReactionDataEntry> Reactions;

	UFUNCTION(BlueprintPure, Category="SyncPrediction|Reaction")
	const FPMMO_ReactionDataEntry& FindReactionChecked(FGameplayTag ReactionTag) const;

	UFUNCTION(BlueprintPure, Category="SyncPrediction|Reaction")
	bool FindReaction(FGameplayTag ReactionTag, FPMMO_ReactionDataEntry& OutReaction) const;

	const FPMMO_ReactionDataEntry* FindReactionPtr(FGameplayTag ReactionTag) const;
};
