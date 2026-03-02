// Fill out your copyright notice in the Description page of Project Settings.


#include "ClothComputeExecutor.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHI.h"
#include "RHIResources.h"

// Declare a custom GPU Stat for the viewport
DECLARE_GPU_STAT_NAMED(Stat_ClothComputeDispatch, TEXT("Cloth Compute Dispatch"));

// 1. Define the Parameter Struct that matches the USF file
BEGIN_SHADER_PARAMETER_STRUCT(FTestComputeParameters, )
	SHADER_PARAMETER(float, Multiplier)
	SHADER_PARAMETER(FIntPoint, TextureSize) // Pass resolution to shader for bounds checking
	SHADER_PARAMETER_RDG_TEXTURE_UAV(FRDGTextureUAVRef, OutputTexture) // Changed from Buffer to Texture
END_SHADER_PARAMETER_STRUCT()

// 2. Link the C++ class to the .usf file
class FTestComputeCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTestComputeCS);
	SHADER_USE_PARAMETER_STRUCT(FTestComputeCS, FGlobalShader);

	// connect the struct to the class for RDG Dispatch
	using FParameters = FTestComputeParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// 3. Map the virtual path to the MainCS entry point
IMPLEMENT_GLOBAL_SHADER(FTestComputeCS, "/ClothComputeShaders/Private/TestCompute.usf", "MainCS", SF_Compute);

// 4. Implement the Blueprint Function
void UClothComputeExecutor::ExecuteTestComputeShader()
{
	UE_LOG(LogTemp, Warning, TEXT("C++ Function Triggered! Multiplier: %f"), this->Multiplier);
	
	if (!OutputRenderTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("OutputRenderTarget is null! Please assign it in Blueprint."));
		return;
	}

	// Read properties on the Game Thread safely
	float CurrentMultiplier = this->Multiplier;

	// Safely grab the render target resource for the Render Thread
	FTextureRenderTargetResource* RTResource = OutputRenderTarget->GameThread_GetRenderTargetResource();

	// Enqueue the render command to run on the Render Thread
	ENQUEUE_RENDER_COMMAND(FExecuteTestCompute)(
		[CurrentMultiplier, RTResource](FRHICommandListImmediate& RHICmdList)
		{
			// Initialize the RDG Builder
			FRDGBuilder GraphBuilder(RHICmdList);

			// --- A. Extract RHI and Bind to RDG ---
			FTextureRHIRef TextureRHI = RTResource->GetRenderTargetTexture();
			if (!TextureRHI) return;

			// Register the external render target texture into the graph
			FRDGTextureRef RDGTexture = RegisterExternalTexture(GraphBuilder, TextureRHI, TEXT("ComputeOutputTexture"));

			// Create the UAV so the shader can write to it
			FRDGTextureUAVRef TextureUAV = GraphBuilder.CreateUAV(RDGTexture);

			// --- B. Setup the Shader ---
			TShaderMapRef<FTestComputeCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

			FTestComputeParameters* PassParameters = GraphBuilder.AllocParameters<FTestComputeParameters>();
			
			PassParameters->Multiplier = CurrentMultiplier;
			PassParameters->TextureSize = FIntPoint(TextureRHI->GetSizeX(), TextureRHI->GetSizeY());
			PassParameters->OutputTexture = TextureUAV;

			// --- C. Dispatch the Shader ---
			// Divided by 64 threads per group = 1 group
			FIntVector GroupCount = FIntVector(
				FMath::DivideAndRoundUp((int32)TextureRHI->GetSizeX(), 8),
				FMath::DivideAndRoundUp((int32)TextureRHI->GetSizeY(), 8),
				1
			);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteTestCompute 2D Texture"),
				PassParameters,
				ERDGPassFlags::Compute,
				[ComputeShader, PassParameters, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					// Scope the exact dispatch time for 'stat gpu'
					SCOPED_GPU_STAT(RHICmdList, Stat_ClothComputeDispatch);

					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});

			// Execute the graph
			GraphBuilder.Execute();
		});
}