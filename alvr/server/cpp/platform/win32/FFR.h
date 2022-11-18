#pragma once

#include "d3d-render-utils/RenderPipeline.h"

// [SM] begin
// from client
struct FFRData {
    uint32_t eyeWidth;
    uint32_t eyeHeight;
    float centerSizeX;
    float centerSizeY;
    float centerShiftX;
    float centerShiftY;
    float edgeRatioX;
    float edgeRatioY;
};
struct FoveationVars {
		uint32_t targetEyeWidth;
		uint32_t targetEyeHeight;
		uint32_t optimizedEyeWidth;
		uint32_t optimizedEyeHeight;

		float eyeWidthRatio;
		float eyeHeightRatio;

		float centerSizeX;
		float centerSizeY;
		float centerShiftX;
		float centerShiftY;
		float edgeRatioX;
		float edgeRatioY;
	};
FFRData FfrDataFromSettings();
FoveationVars CalculateFoveationVars(FFRData data);
// [SM] end

class FFR
{
public:
	FFR(ID3D11Device* device);
	void Initialize(ID3D11Texture2D* compositionTexture, FFRData ffrData);
	void Render();
	void GetOptimizedResolution(uint32_t* width, uint32_t* height);
	ID3D11Texture2D* GetOutputTexture();

private:
	Microsoft::WRL::ComPtr<ID3D11Device> mDevice;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> mOptimizedTexture;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> mQuadVertexShader;

	std::vector<d3d_render_utils::RenderPipeline> mPipelines;

	// [SM] begin
	FoveationVars mFfrVars;
	// [SM] end
};

