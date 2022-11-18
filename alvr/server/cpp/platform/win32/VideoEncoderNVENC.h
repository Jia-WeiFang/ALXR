#pragma once

#include <memory>
#include "shared/d3drender.h"
#include "alvr_server/ClientConnection.h"
#include "VideoEncoder.h"
#include "NvEncoderD3D11.h"

// [kyl] begin
#include "DirectXTex/DirectXTexP.h"
#include "DirectXTex/DirectXTex.h"
#include "mutex.h"
// [kyl] end

// Video encoder for NVIDIA NvEnc.
class VideoEncoderNVENC : public VideoEncoder
{
public:
	VideoEncoderNVENC(std::shared_ptr<CD3DRender> pD3DRender
		, std::shared_ptr<ClientConnection> listener
		, int width, int height, std::vector<ID3D11Texture2D*> *frames_vec, std::vector<uint64_t> *timeStamp, std::vector<ID3D11Texture2D*> qrcodeTex);
	~VideoEncoderNVENC();

	void Initialize();
	void Shutdown();

	// void Transmit(ID3D11Texture2D *pTexture, uint64_t presentationTime, uint64_t targetTimestampNs, bool insertIDR);

	// [SM] begin
	void Transmit(ID3D11Texture2D *pTexture, uint64_t presentationTime, uint64_t targetTimestampNs, bool insertIDR, uint32_t encodeWidth, uint32_t encodeHeight);
	// [SM] end

	// [kyl] begin
	std::vector<ID3D11Texture2D*> *frames_vec_ptr;
	std::vector<uint64_t> *timeStamp_ptr;
	std::vector<ID3D11Texture2D*> qrcodeTex_ptr;
	int qrcode_cnt = 0;
	// [kyl] end

private:
	void FillEncodeConfig(NV_ENC_INITIALIZE_PARAMS &initializeParams, int refreshRate, int renderWidth, int renderHeight, uint64_t bitrateBits);


	std::ofstream fpOut;
	std::shared_ptr<NvEncoder> m_NvNecoder;

	std::shared_ptr<CD3DRender> m_pD3DRender;
	int m_nFrame;

	std::shared_ptr<ClientConnection> m_Listener;

	bool mSupportsReferenceFrameInvalidation = false;

	int m_codec;
	int m_refreshRate;
	int m_renderWidth;
	int m_renderHeight;
	int m_bitrateInMBits;

	// [kyl] begin
	std::fstream fs;
	// [kyl] end

};
