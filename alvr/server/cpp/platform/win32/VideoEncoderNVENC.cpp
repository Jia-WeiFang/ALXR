// [kyl] begin
#include <string>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
// [kyl] end
#include <d3d11.h>

#include "VideoEncoderNVENC.h"
#include "NvCodecUtils.h"
#include "alvr_server/nvencoderclioptions.h"

#include "alvr_server/Statistics.h"
#include "alvr_server/Logger.h"
#include "alvr_server/Settings.h"
#include "alvr_server/Utils.h"

// [kyl] begin
using namespace DirectX;
// using namespace std;
#include "alvr_server/alvr_server.h"
#include "alvr_server/bindings.h"
bool captureTriggerValue = false;
void captureTrigger(
    bool _captureTriggerValue
){
	captureTriggerValue = !captureTriggerValue;
}
// [kyl] end
// [jw] begin
std::ofstream outfile;
// [jw] end

VideoEncoderNVENC::VideoEncoderNVENC(std::shared_ptr<CD3DRender> pD3DRender
	, std::shared_ptr<ClientConnection> listener
	, int width, int height, std::vector<ID3D11Texture2D*> *frames_vec, std::vector<uint64_t> *timeStamp, std::vector<ID3D11Texture2D*> qrcodeTex)
	: m_pD3DRender(pD3DRender)
	, m_nFrame(0)
	, m_Listener(listener)
	, m_codec(Settings::Instance().m_codec)
	, m_refreshRate(Settings::Instance().m_refreshRate)
	, m_renderWidth(width)
	, m_renderHeight(height)
	, m_bitrateInMBits(Settings::Instance().mEncodeBitrateMBs)
	, frames_vec_ptr(frames_vec)
	, timeStamp_ptr(timeStamp)
	, qrcodeTex_ptr(qrcodeTex)
{
	
}

VideoEncoderNVENC::~VideoEncoderNVENC()
{}

void VideoEncoderNVENC::Initialize()
{
	//
	// Initialize Encoder
	//

	NV_ENC_BUFFER_FORMAT format = NV_ENC_BUFFER_FORMAT_ABGR;
	
	if (Settings::Instance().m_use10bitEncoder) {
		format = NV_ENC_BUFFER_FORMAT_ABGR10;
	}

	Debug("Initializing CNvEncoder. Width=%d Height=%d Format=%d\n", m_renderWidth, m_renderHeight, format);

	try {
		m_NvNecoder = std::make_shared<NvEncoderD3D11>(m_pD3DRender->GetDevice(), m_renderWidth, m_renderHeight, format, 0);
	}
	catch (NVENCException e) {
		throw MakeException("NvEnc NvEncoderD3D11 failed. Code=%d %hs\n", e.getErrorCode(), e.what());
	}

	NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
	NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
	initializeParams.encodeConfig = &encodeConfig;

	FillEncodeConfig(initializeParams, m_refreshRate, m_renderWidth, m_renderHeight, m_bitrateInMBits * 1'000'000);
	   

	try {
		m_NvNecoder->CreateEncoder(&initializeParams);
	}
	catch (NVENCException e) {
		if (e.getErrorCode() == NV_ENC_ERR_INVALID_PARAM) {
			throw MakeException("This GPU does not support H.265 encoding. (NvEncoderCuda NV_ENC_ERR_INVALID_PARAM)");
		}
		throw MakeException("NvEnc CreateEncoder failed. Code=%d %hs", e.getErrorCode(), e.what());
	}

	// [jw] begin
	outfile.open("foveatedParams.csv", std::ios::out);
	outfile << "targetTimestampNs" << "," << "qrcode_cnt" << ","
			<< "centerSizeX" << "," << "centerSizeY" << ","
			<< "centerShiftX" << "," << "centerShiftY" << ","
			<< "edgeRatioX" << "," << "edgeRatioY" << std::endl;
	outfile.close();
	// [jw] end

	// [kyl] begin
	if(remove("encodeVideo.264") != 0)
		Info("Error deleting H264 file");
  	else
		Info("H264 file successfully deleted");
	// [kyl] end

	Debug("CNvEncoder is successfully initialized.\n");
}

void VideoEncoderNVENC::Shutdown()
{
	std::vector<std::vector<uint8_t>> vPacket;
	if(m_NvNecoder)
		m_NvNecoder->EndEncode(vPacket);

	for (std::vector<uint8_t> &packet : vPacket)
	{
		if (fpOut) {
			fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
		}
	}
	if (m_NvNecoder) {
		m_NvNecoder->DestroyEncoder();
		m_NvNecoder.reset();
	}

	Debug("CNvEncoder::Shutdown\n");

	if (fpOut) {
		fpOut.close();
	}
}

// [SM] begin
void VideoEncoderNVENC::Transmit(ID3D11Texture2D *pTexture, uint64_t presentationTime, uint64_t targetTimestampNs, bool insertIDR, uint32_t encodeWidth, uint32_t encodeHeight)
{
	bool ffrChange = encodeWidth != m_NvNecoder->GetEncodeWidth() || encodeHeight != m_NvNecoder->GetEncodeHeight();
	if (m_Listener) {
		if (m_Listener->GetStatistics()->CheckBitrateUpdated() || ffrChange) {
			m_bitrateInMBits = m_Listener->GetStatistics()->GetBitrate();
			NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
			NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
			initializeParams.encodeConfig = &encodeConfig;
			// [jw] change refresh rate, resolution
			FillEncodeConfig(initializeParams, m_refreshRate, encodeWidth, encodeHeight, m_bitrateInMBits * 1'000'000);

			NV_ENC_RECONFIGURE_PARAMS reconfigureParams = { NV_ENC_RECONFIGURE_PARAMS_VER };
			reconfigureParams.reInitEncodeParams = initializeParams;
			reconfigureParams.forceIDR = ffrChange;

			// Info("[NVENC] Reconfigure called!\n");
			#include "NvEncoder.h"
			try {
				m_NvNecoder->Reconfigure(&reconfigureParams);
			}
			catch (const NVENCException &e){
				Error("[NVENC] Reconfigure throws \"%s\"\n", e.what());
			}
			// Info("[NVENC] Reconfigure returned!\n");
		}
	}

	std::vector<std::vector<uint8_t>> vPacket;

	const NvEncInputFrame* encoderInputFrame = m_NvNecoder->GetNextInputFrame();

	ID3D11Texture2D *pInputTexture = reinterpret_cast<ID3D11Texture2D*>(encoderInputFrame->inputPtr);
	// m_pD3DRender->GetContext()->CopyResource(pInputTexture, pTexture);

	// [SM] begin
	// Info("[jw] Width: %d", encodeWidth);
	// Info("[jw] Height: %d", encodeHeight);
	D3D11_BOX box;
	box.left = 0, box.right = encodeWidth;
	box.top = 0,  box.bottom = encodeHeight;
	box.front = 0, box.back = 1;
	m_pD3DRender->GetContext()->CopySubresourceRegion(
		pInputTexture, 0,
		0, 0, 0,
		pTexture, 0, &box
	);
	// [SM] end

	// [kyl] begin
	if (!clientShutDown && captureTriggerValue) {
		// [jw] begin save foveated config
		outfile.open("foveatedParams.csv", std::ios::out | std::ios::app);
		outfile << targetTimestampNs << "," << qrcode_cnt << "," 
			<< ffrData_cur.centerSizeX << "," << ffrData_cur.centerSizeY << ","
			<< ffrData_cur.centerShiftX << "," << ffrData_cur.centerShiftY << ","
			<< ffrData_cur.edgeRatioX << "," << ffrData_cur.edgeRatioY << std::endl;
		outfile.close();
		// [jw] end

		// paste QRcode
		box.left = 0, box.right = 32;
		box.top = 0,  box.bottom = 32;
		box.front = 0, box.back = 1;
		m_pD3DRender->GetContext()->CopySubresourceRegion(
			pInputTexture, 0,
			encodeWidth*2/3, encodeHeight/2, 0,
			qrcodeTex_ptr[qrcode_cnt%1000], 0, &box
		);
		qrcode_cnt += 1;
	}

		// m_pD3DRender->GetContext()->CopySubresourceRegion(
		// 	pInputTexture, 0,
		// 	encodeWidth/20, encodeHeight*8/10, 0,
		// 	qrcodeTex_ptr[qrcode_round%1000], 0, &box
		// );
		// if (qrcode_cnt % 1000 == 0)
		// 	qrcode_round += 1;

	// 	ID3D11Texture2D *bufferTexture;
	// 	ScratchImage img;
	// 	HRESULT hr = CaptureTexture(m_pD3DRender->GetDevice(), m_pD3DRender->GetContext(), pInputTexture, img);
	// 	if (FAILED(hr)) {
	// 		Info("copy texture fail");	
	// 	}
	// 	else {
	// 		hr = CreateTexture(m_pD3DRender->GetDevice(), img.GetImages(), img.GetImageCount(), img.GetMetadata(), (ID3D11Resource**)(&bufferTexture));
	// 		if (FAILED(hr)) {
	// 			Info("create buffer texture fail");	
	// 		}
	// 		else {
	// 			lock.lock();
	// 			(*frames_vec_ptr).push_back(bufferTexture); // save frames for capture
	// 			(*timeStamp_ptr).push_back(qrcode_cnt); // save qrcode index
	// 			qrcode_cnt += 1;
	// 			// if (qrcode_cnt % 1000 == 0)
	// 			// 	qrcode_round += 1;
	// 			lock.unlock();
	// 		}
	// 	}
	// }
	// [kyl] end

	// [jw] begin 
	// outfile.open("foveatedParams.csv", std::ios::out | std::ios::app);
	// outfile << targetTimestampNs << "," << qrcode_cnt << "," 
	// 		<< ffrData_cur.centerSizeX << "," << ffrData_cur.centerSizeY << ","
	// 		<< ffrData_cur.centerShiftX << "," << ffrData_cur.centerShiftY << ","
	// 		<< ffrData_cur.edgeRatioX << "," << ffrData_cur.edgeRatioY << std::endl;
	// outfile.close();
	// [jw] end

	NV_ENC_PIC_PARAMS picParams = {};
	if (insertIDR) {
		Debug("Inserting IDR frame.\n");
		picParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR;
	}
	m_NvNecoder->EncodeFrame(vPacket, &picParams);

	if (m_Listener) {
		m_Listener->GetStatistics()->EncodeOutput(GetTimestampUs() - presentationTime);
	}

	// [kyl] begin
	fs.open("encodeVideo.264", std::fstream::out |  std::fstream::binary | std::fstream::app);

	m_nFrame += (int)vPacket.size();
	for (std::vector<uint8_t> &packet : vPacket)
	{
		if (!clientShutDown) {
			if (fs) {
				fs.write(reinterpret_cast<char*>(packet.data()), packet.size());
			}
		}
		if (fpOut) {
			fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
		}
		if (m_Listener) {
			m_Listener->SendVideo(packet.data(), (int)packet.size(), targetTimestampNs);
		}
	}

	fs.close();
	// [kyl] end

	// m_nFrame += (int)vPacket.size();
	// for (std::vector<uint8_t> &packet : vPacket)
	// {
	// 	if (fpOut) {
	// 		fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
	// 	}
	// 	if (m_Listener) {
	// 		m_Listener->SendVideo(packet.data(), (int)packet.size(), targetTimestampNs);
	// 	}
	// }
}
// [SM] end

// void VideoEncoderNVENC::saveH264(ID3D11Texture2D *pTexture, uint64_t presentationTime, uint64_t targetTimestampNs, bool insertIDR)
// {
// 	if (m_Listener) {
// 		if (m_Listener->GetStatistics()->CheckBitrateUpdated()) {
// 			m_bitrateInMBits = m_Listener->GetStatistics()->GetBitrate();
// 			NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
// 			NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
// 			initializeParams.encodeConfig = &encodeConfig;
// 			FillEncodeConfig(initializeParams, m_refreshRate, m_renderWidth, m_renderHeight, m_bitrateInMBits * 1'000'000);
// 			NV_ENC_RECONFIGURE_PARAMS reconfigureParams = { NV_ENC_RECONFIGURE_PARAMS_VER };
// 			reconfigureParams.reInitEncodeParams = initializeParams;
// 			m_NvNecoder->Reconfigure(&reconfigureParams);
// 		}
// 	}

// 	std::vector<std::vector<uint8_t>> vPacket;

// 	// const NvEncInputFrame* encoderInputFrame = m_NvNecoder->GetNextInputFrame();

// 	ID3D11Texture2D *pInputTexture; // = reinterpret_cast<ID3D11Texture2D*>(encoderInputFrame->inputPtr);
// 	m_pD3DRender->GetContext()->CopyResource(pInputTexture, pTexture);

// 	NV_ENC_PIC_PARAMS picParams = {};
// 	if (insertIDR) {
// 		Debug("Inserting IDR frame.\n");
// 		picParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR;
// 	}
// 	m_NvNecoder->EncodeFrame(vPacket, &picParams);

// 	if (m_Listener) {
// 		m_Listener->GetStatistics()->EncodeOutput(GetTimestampUs() - presentationTime);
// 	}

// 	// [kyl] begin
// 	fs.open("encodeVideo.264", std::fstream::out |  std::fstream::binary | std::fstream::app);

// 	m_nFrame += (int)vPacket.size();
// 	for (std::vector<uint8_t> &packet : vPacket)
// 	{
// 		if (!clientShutDown) {
// 			if (fs) {
// 				fs.write(reinterpret_cast<char*>(packet.data()), packet.size());
// 			}
// 		}
// 		if (fpOut) {
// 			fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
// 		}
// 	}

// 	fs.close();
// 	// [kyl] end

// 	// m_nFrame += (int)vPacket.size();
// 	// for (std::vector<uint8_t> &packet : vPacket)
// 	// {
// 	// 	if (fpOut) {
// 	// 		fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
// 	// 	}
// 	// 	if (m_Listener) {
// 	// 		m_Listener->SendVideo(packet.data(), (int)packet.size(), targetTimestampNs);
// 	// 	}
// 	// }
// }

void VideoEncoderNVENC::FillEncodeConfig(NV_ENC_INITIALIZE_PARAMS &initializeParams, int refreshRate, int renderWidth, int renderHeight, uint64_t bitrateBits)
{
	auto &encodeConfig = *initializeParams.encodeConfig;
	GUID EncoderGUID = m_codec == ALVR_CODEC_H264 ? NV_ENC_CODEC_H264_GUID : NV_ENC_CODEC_HEVC_GUID;

	// According to the docment, NVIDIA Video Encoder (NVENC) Interface 8.1,
	// following configrations are recommended for low latency application:
	// 1. Low-latency high quality preset
	// 2. Rate control mode = CBR
	// 3. Very low VBV buffer size(single frame)
	// 4. No B Frames
	// 5. Infinite GOP length
	// 6. Long term reference pictures
	// 7. Intra refresh
	// 8. Adaptive quantization(AQ) enabled

	m_NvNecoder->CreateDefaultEncoderParams(&initializeParams, EncoderGUID, NV_ENC_PRESET_LOW_LATENCY_HQ_GUID);

	initializeParams.encodeWidth = initializeParams.darWidth = renderWidth;
	initializeParams.encodeHeight = initializeParams.darHeight = renderHeight;
	initializeParams.frameRateNum = refreshRate;
	initializeParams.frameRateDen = 1;

	// Use reference frame invalidation to faster recovery from frame loss if supported.
	mSupportsReferenceFrameInvalidation = m_NvNecoder->GetCapabilityValue(EncoderGUID, NV_ENC_CAPS_SUPPORT_REF_PIC_INVALIDATION);
	bool supportsIntraRefresh = m_NvNecoder->GetCapabilityValue(EncoderGUID, NV_ENC_CAPS_SUPPORT_INTRA_REFRESH);
	Debug("VideoEncoderNVENC: SupportsReferenceFrameInvalidation: %d\n", mSupportsReferenceFrameInvalidation);
	Debug("VideoEncoderNVENC: SupportsIntraRefresh: %d\n", supportsIntraRefresh);

	// 16 is recommended when using reference frame invalidation. But it has caused bad visual quality.
	// Now, use 0 (use default).
	int maxNumRefFrames = 0;

	if (m_codec == ALVR_CODEC_H264) {
		auto &config = encodeConfig.encodeCodecConfig.h264Config;
		config.repeatSPSPPS = 1;
		//if (supportsIntraRefresh) {
		//	config.enableIntraRefresh = 1;
		//	// Do intra refresh every 10sec.
		//	config.intraRefreshPeriod = refreshRate * 10;
		//	config.intraRefreshCnt = refreshRate;
		//}
		config.maxNumRefFrames = maxNumRefFrames;
		config.idrPeriod = NVENC_INFINITE_GOPLENGTH;
	}
	else {
		auto &config = encodeConfig.encodeCodecConfig.hevcConfig;
		config.repeatSPSPPS = 1;
		//if (supportsIntraRefresh) {
		//	config.enableIntraRefresh = 1;
		//	// Do intra refresh every 10sec.
		//	config.intraRefreshPeriod = refreshRate * 10;
		//	config.intraRefreshCnt = refreshRate;
		//}
		config.maxNumRefFramesInDPB = maxNumRefFrames;
		config.idrPeriod = NVENC_INFINITE_GOPLENGTH;
	}

	// According to the document, NVIDIA Video Encoder Interface 5.0,
	// following configrations are recommended for low latency application:
	// 1. NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP rate control mode.
	// 2. Set vbvBufferSize and vbvInitialDelay to maxFrameSize.
	// 3. Inifinite GOP length.
	// NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP also assures maximum frame size,
	// which introduces lower transport latency and fewer packet losses.

	// Disable automatic IDR insertion by NVENC. We need to manually insert IDR when packet is dropped
	// if don't use reference frame invalidation.
	encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
	encodeConfig.frameIntervalP = 1;

	// NV_ENC_PARAMS_RC_CBR_HQ is equivalent to NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP.
	//encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;// NV_ENC_PARAMS_RC_CBR_HQ;
	encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
	uint32_t maxFrameSize = static_cast<uint32_t>(bitrateBits / refreshRate);
	Debug("VideoEncoderNVENC: maxFrameSize=%d bits\n", maxFrameSize);
	encodeConfig.rcParams.vbvBufferSize = maxFrameSize;
	encodeConfig.rcParams.vbvInitialDelay = maxFrameSize;
	encodeConfig.rcParams.maxBitRate = static_cast<uint32_t>(bitrateBits);
	encodeConfig.rcParams.averageBitRate = static_cast<uint32_t>(bitrateBits);

	if (Settings::Instance().m_use10bitEncoder) {
		encodeConfig.rcParams.enableAQ = 1;
		encodeConfig.encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 = 2;
	}
}