#include "FFR.h"

#include "alvr_server/Settings.h"
#include "alvr_server/Utils.h"
#include "alvr_server/bindings.h"

using Microsoft::WRL::ComPtr;
using namespace d3d_render_utils;

// namespace {

	// struct FoveationVars {
	// 	uint32_t targetEyeWidth;
	// 	uint32_t targetEyeHeight;
	// 	uint32_t optimizedEyeWidth;
	// 	uint32_t optimizedEyeHeight;

	// 	float eyeWidthRatio;
	// 	float eyeHeightRatio;

	// 	float centerSizeX;
	// 	float centerSizeY;
	// 	float centerShiftX;
	// 	float centerShiftY;
	// 	float edgeRatioX;
	// 	float edgeRatioY;
	// };

	// FoveationVars CalculateFoveationVars() {
	// 	float targetEyeWidth = (float)Settings::Instance().m_renderWidth / 2;
	// 	float targetEyeHeight = (float)Settings::Instance().m_renderHeight;

	// 	float centerSizeX = (float)Settings::Instance().m_foveationCenterSizeX;
	// 	float centerSizeY = (float)Settings::Instance().m_foveationCenterSizeY;
	// 	float centerShiftX = (float)Settings::Instance().m_foveationCenterShiftX;
	// 	float centerShiftY = (float)Settings::Instance().m_foveationCenterShiftY;
	// 	float edgeRatioX = (float)Settings::Instance().m_foveationEdgeRatioX;
	// 	float edgeRatioY = (float)Settings::Instance().m_foveationEdgeRatioY;

	// 	float edgeSizeX = targetEyeWidth-centerSizeX*targetEyeWidth;
	// 	float edgeSizeY = targetEyeHeight-centerSizeY*targetEyeHeight;

	// 	float centerSizeXAligned = 1.-ceil(edgeSizeX/(edgeRatioX*2.))*(edgeRatioX*2.)/targetEyeWidth;
	// 	float centerSizeYAligned = 1.-ceil(edgeSizeY/(edgeRatioY*2.))*(edgeRatioY*2.)/targetEyeHeight;

	// 	float edgeSizeXAligned = targetEyeWidth-centerSizeXAligned*targetEyeWidth;
	// 	float edgeSizeYAligned = targetEyeHeight-centerSizeYAligned*targetEyeHeight;

	// 	float centerShiftXAligned = ceil(centerShiftX*edgeSizeXAligned/(edgeRatioX*2.))*(edgeRatioX*2.)/edgeSizeXAligned;
	// 	float centerShiftYAligned = ceil(centerShiftY*edgeSizeYAligned/(edgeRatioY*2.))*(edgeRatioY*2.)/edgeSizeYAligned;

	// 	float foveationScaleX = (centerSizeXAligned+(1.-centerSizeXAligned)/edgeRatioX);
	// 	float foveationScaleY = (centerSizeYAligned+(1.-centerSizeYAligned)/edgeRatioY);

	// 	float optimizedEyeWidth = foveationScaleX*targetEyeWidth;
	// 	float optimizedEyeHeight = foveationScaleY*targetEyeHeight;

	// 	// round the frame dimensions to a number of pixel multiple of 32 for the encoder
	// 	auto optimizedEyeWidthAligned = (uint32_t)ceil(optimizedEyeWidth / 32.f) * 32;
	// 	auto optimizedEyeHeightAligned = (uint32_t)ceil(optimizedEyeHeight / 32.f) * 32;

	// 	float eyeWidthRatioAligned = optimizedEyeWidth/optimizedEyeWidthAligned;
	// 	float eyeHeightRatioAligned = optimizedEyeHeight/optimizedEyeHeightAligned;

	// 	return { (uint32_t)targetEyeWidth, (uint32_t)targetEyeHeight, optimizedEyeWidthAligned, optimizedEyeHeightAligned,
	// 		eyeWidthRatioAligned, eyeHeightRatioAligned,
	// 		centerSizeXAligned, centerSizeYAligned, centerShiftXAligned, centerShiftYAligned, edgeRatioX, edgeRatioY };
	// }

// [SM] begin
FFRData FfrDataFromSettings() {
	FFRData ret;
	ret.eyeWidth = Settings::Instance().m_renderWidth;
	ret.eyeHeight = Settings::Instance().m_renderHeight;
	ret.centerSizeX = Settings::Instance().m_foveationCenterSizeX;
	ret.centerSizeY = Settings::Instance().m_foveationCenterSizeY;
	ret.centerShiftX = Settings::Instance().m_foveationCenterShiftX;
	ret.centerShiftY = Settings::Instance().m_foveationCenterShiftY;
	ret.edgeRatioX = Settings::Instance().m_foveationEdgeRatioX;
	ret.edgeRatioY = Settings::Instance().m_foveationEdgeRatioY;
	return ret;
}
FoveationVars CalculateFoveationVars(FFRData data) {
	float targetEyeWidth = data.eyeWidth / 2;
	float targetEyeHeight = data.eyeHeight;

	float centerSizeX = data.centerSizeX;
	float centerSizeY = data.centerSizeY;
	float centerShiftX = data.centerShiftX;
	float centerShiftY = data.centerShiftY;
	float edgeRatioX = data.edgeRatioX;
	float edgeRatioY = data.edgeRatioY;

	float edgeSizeX = targetEyeWidth-centerSizeX*targetEyeWidth;
	float edgeSizeY = targetEyeHeight-centerSizeY*targetEyeHeight;

	float centerSizeXAligned = 1.-ceil(edgeSizeX/(edgeRatioX*2.))*(edgeRatioX*2.)/targetEyeWidth;
	float centerSizeYAligned = 1.-ceil(edgeSizeY/(edgeRatioY*2.))*(edgeRatioY*2.)/targetEyeHeight;

	float edgeSizeXAligned = targetEyeWidth-centerSizeXAligned*targetEyeWidth;
	float edgeSizeYAligned = targetEyeHeight-centerSizeYAligned*targetEyeHeight;

	float centerShiftXAligned = ceil(centerShiftX*edgeSizeXAligned/(edgeRatioX*2.))*(edgeRatioX*2.)/edgeSizeXAligned;
	float centerShiftYAligned = ceil(centerShiftY*edgeSizeYAligned/(edgeRatioY*2.))*(edgeRatioY*2.)/edgeSizeYAligned;

	float foveationScaleX = (centerSizeXAligned+(1.-centerSizeXAligned)/edgeRatioX);
	float foveationScaleY = (centerSizeYAligned+(1.-centerSizeYAligned)/edgeRatioY);

	float optimizedEyeWidth = foveationScaleX*targetEyeWidth;
	float optimizedEyeHeight = foveationScaleY*targetEyeHeight;

	// round the frame dimensions to a number of pixel multiple of 32 for the encoder
	auto optimizedEyeWidthAligned = (uint32_t)ceil(optimizedEyeWidth / 32.f) * 32;
	auto optimizedEyeHeightAligned = (uint32_t)ceil(optimizedEyeHeight / 32.f) * 32;

	float eyeWidthRatioAligned = optimizedEyeWidth/optimizedEyeWidthAligned;
	float eyeHeightRatioAligned = optimizedEyeHeight/optimizedEyeHeightAligned;

	return {data.eyeWidth, data.eyeHeight, optimizedEyeWidthAligned, optimizedEyeHeightAligned,
		eyeWidthRatioAligned, eyeHeightRatioAligned,
		centerSizeXAligned, centerSizeYAligned, centerShiftXAligned, centerShiftYAligned, edgeRatioX, edgeRatioY };
}
	// [SM] end
// }


void FFR::GetOptimizedResolution(uint32_t* width, uint32_t* height) {
	// auto fovVars = CalculateFoveationVars();
	// [SM] begin
	auto fovVars = mFfrVars;
	// [SM] end
	*width = fovVars.optimizedEyeWidth * 2;
	*height = fovVars.optimizedEyeHeight;
}

FFR::FFR(ID3D11Device* device) : mDevice(device) {}

void FFR::Initialize(ID3D11Texture2D* compositionTexture, FFRData ffrData) {
	// auto fovVars = CalculateFoveationVars();

	// [SM] begin
	mFfrVars = CalculateFoveationVars(ffrData);
	auto fovVars = mFfrVars;
	// [SM] end

	ComPtr<ID3D11Buffer> foveatedRenderingBuffer = CreateBuffer(mDevice.Get(), fovVars);

	std::vector<uint8_t> quadShaderCSO(QUAD_SHADER_CSO_PTR, QUAD_SHADER_CSO_PTR + QUAD_SHADER_CSO_LEN);
	mQuadVertexShader = CreateVertexShader(mDevice.Get(), quadShaderCSO);

	mOptimizedTexture = CreateTexture(mDevice.Get(), fovVars.optimizedEyeWidth * 2,
		fovVars.optimizedEyeHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

	if (Settings::Instance().m_enableFoveatedRendering) {
		std::vector<uint8_t> compressAxisAlignedShaderCSO(COMPRESS_AXIS_ALIGNED_CSO_PTR, COMPRESS_AXIS_ALIGNED_CSO_PTR + COMPRESS_AXIS_ALIGNED_CSO_LEN);
		auto compressAxisAlignedPipeline = RenderPipeline(mDevice.Get());
		compressAxisAlignedPipeline.Initialize({ compositionTexture }, mQuadVertexShader.Get(),
			compressAxisAlignedShaderCSO, mOptimizedTexture.Get(), foveatedRenderingBuffer.Get());

		mPipelines.push_back(compressAxisAlignedPipeline);
	} else {
		mOptimizedTexture = compositionTexture;
	}
}

void FFR::Render() {
	for (auto &p : mPipelines) {
		p.Render();
	}
}

ID3D11Texture2D* FFR::GetOutputTexture() {
	return mOptimizedTexture.Get();
}