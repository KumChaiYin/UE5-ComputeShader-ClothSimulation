// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ClothComputeExecutor.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class CLOTHCOMPUTE_API UClothComputeExecutor : public UObject
{
	GENERATED_BODY()

public:
	// Expose the variables so the UI sliders can modify them
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClothCompute")
	float Multiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClothCompute")
	int32 ElementCount = 256; // Using int32 for Blueprint compatibility

	// A static function we can call from anywhere to dispatch our Compute Shader
	UFUNCTION(BlueprintCallable, Category = "ClothCompute")
	void ExecuteTestComputeShader();

};
