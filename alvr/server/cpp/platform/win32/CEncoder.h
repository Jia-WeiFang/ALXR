#pragma once
#include "shared/d3drender.h"

#include "shared/threadtools.h"

#include <d3d11.h>
#include <wrl.h>
#include <map>
#include <d3d11_1.h>
#include <wincodec.h>
#include <wincodecsdk.h>
// [kyl] begin
#include "DirectXTex/DirectXTexP.h"
#include "DirectXTex/DirectXTex.h"
// [kyl] end
// [jw] begin
#include "alvr_server/alvr_server.h"
// [jw] end
#include "alvr_server/ClientConnection.h"
#include "alvr_server/Utils.h"
#include "FrameRender.h"
#include "VideoEncoder.h"
#include "VideoEncoderNVENC.h"
#include "VideoEncoderVCE.h"
#ifdef ALVR_GPL
	#include "VideoEncoderSW.h"
#endif
#include "alvr_server/IDRScheduler.h"
#include <queue>


	using Microsoft::WRL::ComPtr;

	//----------------------------------------------------------------------------
	// Blocks on reading backbuffer from gpu, so WaitForPresent can return
	// as soon as we know rendering made it this frame.  This step of the pipeline
	// should run about 3ms per frame.
	//----------------------------------------------------------------------------
	class CEncoder : public CThread
	{
	public:
		CEncoder();
		~CEncoder();

		void Initialize(std::shared_ptr<CD3DRender> d3dRender, std::shared_ptr<ClientConnection> listener, std::vector<ID3D11Texture2D*> *frame_vec, std::vector<uint64_t> *timeStamp);

		bool CopyToStaging(ID3D11Texture2D *pTexture[][2], vr::VRTextureBounds_t bounds[][2], int layerCount, bool recentering
			, uint64_t presentationTime, uint64_t targetTimestampNs, const std::string& message, const std::string& debugText);

		virtual void Run();

		virtual void Stop();

		void NewFrameReady();

		void WaitForEncode();

		void OnStreamStart();

		void OnPacketLoss();

		void InsertIDR();

		// [SM] begin
		void ffrUpdate(FFRData ffrData);
		// [SM] end

		// [kyl] begin
		std::vector<ID3D11Texture2D*> *frames_vec_ptr;
		std::vector<uint64_t> *timeStamp_ptr;
		std::vector<ID3D11Texture2D*> qrcodeTex_big;
		std::vector<ID3D11Texture2D*> qrcodeTex_small;
		// [kyl] end

	private:
		CThreadEvent m_newFrameReady, m_encodeFinished;
		std::shared_ptr<VideoEncoder> m_videoEncoder;
		bool m_bExiting;
		uint64_t m_presentationTime;
		uint64_t m_targetTimestampNs;

		std::shared_ptr<FrameRender> m_FrameRender;

		IDRScheduler m_scheduler;

		// [SM] begin
		std::shared_ptr<CD3DRender> m_d3dRender;
		FFRData m_ffrData;
		FFRData m_ffrDataNext;
		std::mutex m_lock;
		// [SM] end
	};

