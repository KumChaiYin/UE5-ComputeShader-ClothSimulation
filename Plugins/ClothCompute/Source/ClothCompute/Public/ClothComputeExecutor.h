// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ClothComputeExecutor.generated.h"

/**
 * 
 */
UCLASS()
class CLOTHCOMPUTE_API UClothComputeExecutor : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// A static function we can call from anywhere to dispatch our Compute Shader
	UFUNCTION(BlueprintCallable, Category = "ClothCompute")
	static void ExecuteTestComputeShader();

};
