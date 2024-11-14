// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ExtendedCharacter.generated.h"

class UExtendedMovementComponent;

UCLASS()
class PROJECT_AERO_API AExtendedCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AExtendedCharacter(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character|Extended")
	UExtendedMovementComponent* GetExtendedMovementComponent() const { return EMC; }

	UFUNCTION(BlueprintCallable, Category = "Character|Extended")
	void StartGlide();

	UFUNCTION(BlueprintCallable, Category = "Character|Extended")
	void StopGlide();

	UFUNCTION(BlueprintImplementableEvent)
	void OnGlideImpact(const FHitResult& Hit);

protected:
	UPROPERTY()	TObjectPtr<UExtendedMovementComponent> EMC{ nullptr };

private:

};
