#include "CEncoder.h"

// [kyl] begin
using namespace DirectX;
// [kyl] end
		CEncoder::CEncoder()
			: m_bExiting(false)
			, m_targetTimestampNs(0)
		{
			m_encodeFinished.Set();
		}

		
			CEncoder::~CEncoder()
		{
			if (m_videoEncoder)
			{
				m_videoEncoder->Shutdown();
				m_videoEncoder.reset();
			}
		}

		void CEncoder::Initialize(std::shared_ptr<CD3DRender> d3dRender, std::shared_ptr<ClientConnection> listener, std::vector<ID3D11Texture2D*> *frames_vec, std::vector<uint64_t> *timeStamp) {
			// [SM] begin
			m_d3dRender = d3dRender;

			FFRData ffrData = FfrDataFromSettings();
			m_FrameRender = std::make_shared<FrameRender>(d3dRender);
			m_FrameRender->Startup(ffrData);

			uint32_t encoderWidth = Settings::Instance().m_renderWidth, encoderHeight = Settings::Instance().m_renderHeight;

			m_lock.lock();
			m_ffrData = ffrData;
			m_lock.unlock();
			this->ffrUpdate(ffrData);
			// [SM] end
			
			// m_FrameRender = std::make_shared<FrameRender>(d3dRender);
			// m_FrameRender->Startup();
			// uint32_t encoderWidth, encoderHeight;
			// m_FrameRender->GetEncodingResolution(&encoderWidth, &encoderHeight);

			// [kyl] begin
			// load qrcode
			for (int i = 0; i < 1000; i++) {
				wchar_t filepath[30];
				swprintf_s(filepath, L"qrcode/%d.png", i);
				auto img = std::make_unique<ScratchImage>();
				ID3D11Texture2D *tex;
				HRESULT hr = LoadFromWICFile(filepath, WIC_FLAGS_NONE, nullptr, *img);
				if (FAILED(hr)) {
					Info("Load qrcode fail");
				}
				else {
					hr = CreateTexture(d3dRender->GetDevice(), img->GetImages(), img->GetImageCount(), img->GetMetadata(), (ID3D11Resource**)(&tex));
					if (FAILED(hr)) {
						Info("create qrcode texture fail");
					}
					else {
						qrcodeTex.push_back(tex);
					}
				}
			}
			Info("qrcode size: %d", qrcodeTex.size());
			// [kyl] end

			Exception vceException;
			Exception nvencException;
#ifdef ALVR_GPL
			Exception swException;
#endif
			try {
				Debug("Try to use VideoEncoderVCE.\n");
				m_videoEncoder = std::make_shared<VideoEncoderVCE>(d3dRender, listener, encoderWidth, encoderHeight);
				m_videoEncoder->Initialize();
				return;
			}
			catch (Exception e) {
				vceException = e;
			}
			try {
				Debug("Try to use VideoEncoderNVENC.\n");
				m_videoEncoder = std::make_shared<VideoEncoderNVENC>(d3dRender, listener, encoderWidth, encoderHeight, frames_vec, timeStamp, qrcodeTex);
				m_videoEncoder->Initialize();
				return;
			}
			catch (Exception e) {
				nvencException = e;
			}
#ifdef ALVR_GPL
			try {
				Debug("Try to use VideoEncoderSW.\n");
				m_videoEncoder = std::make_shared<VideoEncoderSW>(d3dRender, listener, encoderWidth, encoderHeight);
				m_videoEncoder->Initialize();
				return;
			}
			catch (Exception e) {
				swException = e;
			}
			throw MakeException("All VideoEncoder are not available. VCE: %s, NVENC: %s, SW: %s", vceException.what(), nvencException.what(), swException.what());
#else
			throw MakeException("All VideoEncoder are not available. VCE: %s, NVENC: %s", vceException.what(), nvencException.what());
#endif
		}

		bool CEncoder::CopyToStaging(ID3D11Texture2D *pTexture[][2], vr::VRTextureBounds_t bounds[][2], int layerCount, bool recentering
			, uint64_t presentationTime, uint64_t targetTimestampNs, const std::string& message, const std::string& debugText)
		{
			m_presentationTime = presentationTime;
			m_targetTimestampNs = targetTimestampNs;

			// m_FrameRender->Startup();

			m_FrameRender->RenderFrame(pTexture, bounds, layerCount, recentering, message, debugText);
			return true;
		}

		void CEncoder::Run()
		{
			Debug("CEncoder: Start thread. Id=%d\n", GetCurrentThreadId());
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_MOST_URGENT);

			while (!m_bExiting)
			{
				Debug("CEncoder: Waiting for new frame...\n");

				m_newFrameReady.Wait();
				if (m_bExiting)
					break;

				// if (m_FrameRender->GetTexture())
				// {
				// 	m_videoEncoder->Transmit(m_FrameRender->GetTexture().Get(), m_presentationTime, m_targetTimestampNs, m_scheduler.CheckIDRInsertion());
				// }

				// [SM] begin
				uint32_t encodeWidth, encodeHeight;
				m_FrameRender->GetEncodingResolution(&encodeWidth, &encodeHeight);
				// Info("[FFR] encodeWidth = %d, encodeHeight = %d\n", encodeWidth, encodeHeight);
				if (m_FrameRender->GetTexture())
				{
					m_videoEncoder->Transmit(m_FrameRender->GetTexture().Get(), m_presentationTime, m_targetTimestampNs, m_scheduler.CheckIDRInsertion(),
						encodeWidth, encodeHeight
					);
				}
				
				m_lock.lock();
				if(memcmp(&m_ffrData, &m_ffrDataNext, sizeof(FFRData))) {
					FfrReconfigSend(
						m_targetTimestampNs,
						m_ffrDataNext.centerSizeX, m_ffrDataNext.centerSizeY,
						m_ffrDataNext.centerShiftX, m_ffrDataNext.centerShiftY,
						m_ffrDataNext.edgeRatioX, m_ffrDataNext.edgeRatioY
					);
					m_FrameRender = std::make_shared<FrameRender>(m_d3dRender);
					m_FrameRender->Startup(m_ffrDataNext);
					m_ffrData = m_ffrDataNext;
				}
				m_lock.unlock();
				// [SM] end

				m_encodeFinished.Set();
			}
		}

		void CEncoder::Stop()
		{
			m_bExiting = true;
			m_newFrameReady.Set();
			Join();
			m_FrameRender.reset();
		}

		void CEncoder::NewFrameReady()
		{
			Debug("New Frame Ready\n");
			m_encodeFinished.Reset();
			m_newFrameReady.Set();
		}

		void CEncoder::WaitForEncode()
		{
			m_encodeFinished.Wait();
		}

		void CEncoder::OnStreamStart() {
			m_scheduler.OnStreamStart();
		}

		void CEncoder::OnPacketLoss() {
			m_scheduler.OnPacketLoss();
		}

		void CEncoder::InsertIDR() {
			m_scheduler.InsertIDR();
		}

// [SM] begin
void CEncoder::ffrUpdate(FFRData ffrData) {
	/* purely update ffrdata to avoid unnecessary sync-up issue */
	ffrData.eyeWidth = m_ffrData.eyeWidth;
	ffrData.eyeHeight = m_ffrData.eyeHeight;
	m_lock.lock();
	m_ffrDataNext = ffrData;
	m_lock.unlock();
	Info("[FFR] ffrUpdate Called\n");
}
// [SM] end