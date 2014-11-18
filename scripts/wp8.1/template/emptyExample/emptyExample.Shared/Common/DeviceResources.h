#pragma once

#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "EGL/eglplatform.h"
#include "GLES2/gl2.h"
#include "GLES2/gl2ext.h"
#include <angle_windowsstore.h>
#include <mutex>

namespace AngleApp
{
	// Provides an interface for an application that owns DeviceResources to be notified of the device being lost or created.
	interface IDeviceNotify
	{
		virtual void OnDeviceLost() = 0;
		virtual void OnDeviceRestored() = 0;
	};

	// Controls all the DirectX device resources.
	class DeviceResources
	{
	public:
		DeviceResources();
		void SetSwapChainPanel(Windows::UI::Xaml::Controls::SwapChainPanel^ panel);
		void SetLogicalSize(Windows::Foundation::Size logicalSize);
		void SetCurrentOrientation(Windows::Graphics::Display::DisplayOrientations currentOrientation);
		void SetDpi(float dpi);
		void SetCompositionScale(float compositionScaleX, float compositionScaleY);
		void ValidateDevice();
		void HandleDeviceLost();
		void RegisterDeviceNotify(IDeviceNotify* deviceNotify);
		void Trim();
		void Present();

		// Device Accessors.
		Windows::Foundation::Size GetOutputSize() const					{ return m_outputSize; }
		Windows::Foundation::Size GetLogicalSize() const				{ return m_logicalSize; }

		float GetDpi() const                          { return m_compositionScaleX; }
		void aquireContext();
		void releaseContext();

	private:
		void CreateDeviceIndependentResources();
		void CreateDeviceResources();
		void CreateWindowSizeDependentResources();
		void Release();

		// Cached reference to the XAML panel.
		Windows::UI::Xaml::Controls::SwapChainPanel^    m_swapChainPanel;

		// Cached device properties.
		Windows::Foundation::Size						m_outputSize;
		Windows::Foundation::Size						m_logicalSize;
		Windows::Graphics::Display::DisplayOrientations	m_nativeOrientation;
		Windows::Graphics::Display::DisplayOrientations	m_currentOrientation;
		float											m_dpi;
		float											m_compositionScaleX;
		float											m_compositionScaleY;

		// EGL stuff
		bool m_bAngleInitialized;
		EGLConfig m_eglConfig;
		EGLDisplay m_eglDisplay;
		EGLContext m_eglContext;
		EGLSurface m_eglSurface;

		// The IDeviceNotify can be held directly as it owns the DeviceResources.
		IDeviceNotify* m_deviceNotify;
		std::mutex m_mutex;
	};
}
