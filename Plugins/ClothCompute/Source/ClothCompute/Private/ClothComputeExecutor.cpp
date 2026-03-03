// Fill out your copyright notice in the Description page of Project Settings.


#include "ClothComputeExecutor.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"
#include "RenderGraphResources.h"
#include "RHI.h"
#include "RHIResources.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "RHIGPUReadback.h"
#include "Async/Async.h"

// Declare a custom GPU Stat for the viewport
DECLARE_GPU_STAT_NAMED(Stat_ClothComputeDispatch, TEXT("Cloth Compute Dispatch"));

// 1. Define the Parameter Struct that matches the USF file
BEGIN_SHADER_PARAMETER_STRUCT(FTestComputeParameters, )
	SHADER_PARAMETER(float, Multiplier)
	SHADER_PARAMETER(int32, ElementCount)
	SHADER_PARAMETER_RDG_BUFFER_UAV(FRDGBufferUAVRef, OutputBuffer)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(FRDGTextureUAVRef, OutputTexture)
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
	UE_LOG(LogTemp, Warning, TEXT("C++ Function Triggered! ElementCount: %d"), this->ElementCount);

	// Read properties on the Game Thread safely
	float CurrentMultiplier = this->Multiplier;
	int32 CurrentElementCount = this->ElementCount;

	// We check if the size changed on the Game Thread, totally avoiding the GetDesc() error.
	bool bSizeChanged = (CurrentElementCount != PreviousElementCount);
	PreviousElementCount = CurrentElementCount; // Save for the next click

	if (!OutputRenderTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("OutputRenderTarget is null! Please assign it in Blueprint."));
		return;
	}

	FTextureRenderTargetResource* RTResource = OutputRenderTarget->GameThread_GetRenderTargetResource();

	// Enqueue the render command to run on the Render Thread
	ENQUEUE_RENDER_COMMAND(FExecuteTestCompute)(
		[this, CurrentMultiplier, CurrentElementCount, RTResource, bSizeChanged](FRHICommandListImmediate& RHICmdList)
		{
			// Initialize the RDG Builder
			FRDGBuilder GraphBuilder(RHICmdList);

			// --- A. Setup Persistent Buffer ---
			FRDGBufferRef RDGBuffer = nullptr;
			int32 ExpectedTotalBytes = CurrentElementCount * sizeof(FVector3f);

			// We now use our Game Thread boolean (bSizeChanged) instead of querying the GPU buffer!
			if (!PositionBuffer.IsValid() || bSizeChanged)
			{
				FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), CurrentElementCount);
				RDGBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("ComputeOutputBuffer"));
			}
			else
			{
				// Back to using GraphBuilder! Since we now have the correct headers, this will compile perfectly.
				RDGBuffer = GraphBuilder.RegisterExternalBuffer(PositionBuffer, TEXT("ComputeOutputBuffer"));
			}

			// Create the UAV so the shader can write to it
			FRDGBufferUAVRef BufferUAV = GraphBuilder.CreateUAV(RDGBuffer);

			// Setup the Texture Bridge
			FTextureRHIRef TextureRHI = RTResource->GetRenderTargetTexture();
			if (!TextureRHI) return;

			FRDGTextureRef RDGTexture = RegisterExternalTexture(GraphBuilder, TextureRHI, TEXT("ComputeOutputTexture"));
			FRDGTextureUAVRef TextureUAV = GraphBuilder.CreateUAV(RDGTexture);

			// --- B. Setup the Shader ---
			TShaderMapRef<FTestComputeCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FTestComputeParameters* PassParameters = GraphBuilder.AllocParameters<FTestComputeParameters>();
			
			PassParameters->Multiplier = CurrentMultiplier;
			PassParameters->ElementCount = CurrentElementCount;
			PassParameters->OutputBuffer = BufferUAV;
			PassParameters->OutputTexture = TextureUAV;

			// --- C. Dispatch the Shader ---
			// Divided by 64 threads per group = 1 group
			FIntVector GroupCount = FIntVector(
				FMath::DivideAndRoundUp(CurrentElementCount, 64),
				1,
				1
			);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteTestCompute 1D Buffer"),
				PassParameters,
				ERDGPassFlags::Compute,
				[ComputeShader, PassParameters, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					// Scope the exact dispatch time for 'stat gpu'
					SCOPED_GPU_STAT(RHICmdList, Stat_ClothComputeDispatch);

					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});

			// --- D. Extract Buffer ---
			// IMPORTANT: This tells RDG to save the buffer's data into our persistent 'PositionBuffer'
			// variable before destroying the graph. It also automatically transitions the buffer to an SRV state!
			GraphBuilder.QueueBufferExtraction(RDGBuffer, &PositionBuffer);

			// --- E. Setup GPU Readback ---
			if (!GPUReadback)
			{
				GPUReadback = new FRHIGPUBufferReadback(TEXT("ClothDataReadback"));
			}
			AddEnqueueCopyPass(GraphBuilder, GPUReadback, RDGBuffer, ExpectedTotalBytes);

			// Execute the graph
			GraphBuilder.Execute();
		});
}

void UClothComputeExecutor::PrintBufferDataToLog()
{
	// 1. Initial sanity check
	if (!GPUReadback)
	{
		UE_LOG(LogTemp, Warning, TEXT("GPU Readback has not been initialized. Run the Compute Shader first."));
		return;
	}

	// Capture these by value so they can be safely passed to the lambda
	int32 LocalTotalBytes = ElementCount * sizeof(FVector3f);
	int32 LocalElementCount = ElementCount;

	// 2. We check IsReady() on the GameThread
	if (GPUReadback->IsReady())
	{
		// 3. Queue the actual memory reading onto the Render Thread!
		ENQUEUE_RENDER_COMMAND(ReadbackBufferCommand)(
			[this, LocalTotalBytes, LocalElementCount](FRHICommandListImmediate& RHICmdList)
			{
				// 4. Safely Lock, Read, and Unlock ON THE RENDER THREAD
				const FVector3f* DataPtr = (const FVector3f*)GPUReadback->Lock(LocalTotalBytes);

				if (DataPtr)
				{
					// Note: Logging from the Render Thread is generally fine for debug purposes.
					UE_LOG(LogTemp, Warning, TEXT("--- Cloth Buffer Data Readback ---"));
					for (int32 i = 0; i < LocalElementCount; i++)
					{
						UE_LOG(LogTemp, Warning, TEXT("Buffer[%d]: X=%f, Y=%f, Z=%f"), i, DataPtr[i].X, DataPtr[i].Y, DataPtr[i].Z);
					}
					UE_LOG(LogTemp, Warning, TEXT("----------------------------------"));

					GPUReadback->Unlock();
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Readback IsReady() was true, but Lock() returned null."));
				}
			});
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GPU Readback is NOT ready yet. Please try again in a moment."));
	}
}