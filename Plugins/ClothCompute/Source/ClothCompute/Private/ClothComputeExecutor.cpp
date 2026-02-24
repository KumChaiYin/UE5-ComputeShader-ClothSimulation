// Fill out your copyright notice in the Description page of Project Settings.


#include "ClothComputeExecutor.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"

// 1. Define the Parameter Struct that matches the USF file
BEGIN_SHADER_PARAMETER_STRUCT(FTestComputeParameters, )
	SHADER_PARAMETER_RDG_BUFFER_UAV(FRDGBufferUAVRef<float>, ClothData)
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
	// Enqueue the render command to run on the Render Thread
	ENQUEUE_RENDER_COMMAND(FExecuteTestCompute)(
		[](FRHICommandListImmediate& RHICmdList)
		{
			// Initialize the RDG Builder
			FRDGBuilder GraphBuilder(RHICmdList);

			// --- A. Create a Temporary Buffer for Testing ---
			// We allocate an array of 64 floats (just for this test)
			TResourceArray<float> InputData;
			InputData.Init(1.5f, 64); // Fill it with the number 1.5

			// Create the RDG Buffer to hold our data on the GPU
			FRDGBufferRef Buffer = CreateStructuredBuffer(
				GraphBuilder,
				TEXT("TestDataBuffer"),
				sizeof(float),
				InputData.Num(),
				InputData.GetData(),
				sizeof(float) * InputData.Num()
			);

			// Create a UAV (Unordered Access View) so the compute shader can write to it
			FRDGBufferUAVRef BufferUAV = GraphBuilder.CreateUAV(Buffer);

			// --- B. Setup the Shader ---
			TShaderMapRef<FTestComputeCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

			FTestComputeParameters* PassParameters = GraphBuilder.AllocParameters<FTestComputeParameters>();
			PassParameters->ClothData = BufferUAV;

			// --- C. Dispatch the Shader ---
			// 64 elements total, divided by 64 threads per group = 1 group
			FIntVector GroupCount = FIntVector(1, 1, 1);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteTestCompute"),
				PassParameters,
				ERDGPassFlags::Compute,
				[ComputeShader, PassParameters, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});

			// Execute the graph
			GraphBuilder.Execute();
		});
}