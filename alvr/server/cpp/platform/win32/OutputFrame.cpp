// [kyl] begin
#include "OutputFrame.h"
#include <stdio.h>
#include <time.h>

// using namespace std;
using namespace DirectX;
int frame_count = 0;
std::mutex lock;


		OutputFrame::OutputFrame()
			: m_bExiting(false)
			, m_targetTimestampNs(0)
		{
			m_encodeFinished.Set();
		}

		
		OutputFrame::~OutputFrame()
		{
			if (m_videoEncoder)
			{
				m_videoEncoder->Shutdown();
				m_videoEncoder.reset();
			}
		}

		void OutputFrame::Initialize(std::shared_ptr<CD3DRender> d3dRender, std::vector<ID3D11Texture2D*> *frames_vec, std::vector<uint64_t> *timeStamp) {
            frames_vec_ptr = frames_vec;
			timeStamp_ptr = timeStamp;
			m_pD3DRender = d3dRender;
		}

		void OutputFrame::Run()
		{
			int now_frame = 0;
			int round = 0;
			int value = 0;
			time_t starttime, endtime;
			bool flag = false;
			// lock.lock();
            while (!clientShutDown || !(*frames_vec_ptr).empty())
            {
				if (!flag)
				{
					starttime = time(NULL);
					flag = true;
				}
				// lock.unlock();
				lock.lock();
                if (!(*frames_vec_ptr).empty() && !(*timeStamp_ptr).empty()) {
					ID3D11Texture2D *texture = frames_vec_ptr->at(0);
					timeStamp = timeStamp_ptr->at(0);
					(*frames_vec_ptr).erase((*frames_vec_ptr).begin());
					(*timeStamp_ptr).erase((*timeStamp_ptr).begin());
					
					ScratchImage img;
					HRESULT hr = CaptureTexture(m_pD3DRender->GetDevice(), m_pD3DRender->GetContext(), texture, img);
					if (FAILED(hr)) {
						Info("[kyl] capture fail");
						lock.unlock();
					}
					else {
						round = (int)(timeStamp/1000);
						value = (int)(timeStamp%1000);
						now_frame = frame_count;
						frame_count++;
						swprintf_s(filepath, L"frames/%lld_%lld.png", round, value);
						lock.unlock();
						hr = SaveToWICFile(img.GetImages(), img.GetImageCount(), WIC_FLAGS_NONE, GUID_ContainerFormatPng, filepath, &GUID_WICPixelFormat24bppBGR);
						if (FAILED(hr)) {
							Info("[kyl] failed to save image");
						}	
					}
					texture->Release();
					img.Release();
                }
				else {
					lock.unlock();
				}
				// lock.lock();
            }
			endtime = time(NULL);
			double diff = difftime(endtime, starttime);

			Info("[kyl] OutputFrame time: %f.", diff);
			Info("[kyl] OutputFrame count: %d.", frame_count);
			Info("[kyl] Output done.");
		}

		void OutputFrame::Stop()
		{
			Info("stop OutputFrame thread");
			Join();
		}
// [kyl] end