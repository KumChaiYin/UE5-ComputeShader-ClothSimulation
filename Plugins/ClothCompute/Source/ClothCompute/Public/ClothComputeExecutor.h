// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RenderGraphDefinitions.h" // Added for FRDGPooledBuffer
#include "RenderGraphResources.h" // Add this to define FRDGPooledBuffer
#include "Engine/TextureRenderTarget2D.h"
#include "ClothComputeExecutor.generated.h"

class FRHIGPUBufferReadback;

UCLASS(BlueprintType, Blueprintable)
class CLOTHCOMPUTE_API UClothComputeExecutor : public UObject
{
	GENERATED_BODY()

public:
	// Expose the variables so the UI sliders can modify them
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClothCompute")
	float Multiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClothCompute")
	int32 ElementCount = 3;

	// Re-exposed for the Texture Bridge
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClothCompute")
	UTextureRenderTarget2D* OutputRenderTarget;

	// A static function we can call from anywhere to dispatch our Compute Shader
	UFUNCTION(BlueprintCallable, Category = "ClothCompute")
	void ExecuteTestComputeShader();

	UFUNCTION(BlueprintCallable, Category = "ClothCompute")
	void PrintBufferDataToLog();

	// RDG-native pooled buffer that survives between frames
	TRefCountPtr<FRDGPooledBuffer> PositionBuffer = nullptr;
	// ADD THIS: Track the size from the previous frame
	int32 PreviousElementCount = 0;

	FRHIGPUBufferReadback* GPUReadback = nullptr;
};
