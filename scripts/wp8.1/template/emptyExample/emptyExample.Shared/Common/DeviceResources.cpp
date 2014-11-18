#include "pch.h"
#include "DeviceResources.h"
#include "AngleHelper.h"
#include "ofConstants.h"
#include "ofAppRunner.h"

#include <windows.ui.xaml.media.dxinterop.h>
#include <stdexcept>

using namespace D2D1;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Platform;

namespace AngleApp
{
	// Constants used to calculate screen rotations.
	namespace ScreenRotation
	{
		// 0-degree Z-rotation
		static const XMFLOAT4X4 Rotation0(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
			);

		// 90-degree Z-rotation
		static const XMFLOAT4X4 Rotation90(
			0.0f, 1.0f, 0.0f, 0.0f,
			-1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
			);

		// 180-degree Z-rotation
		static const XMFLOAT4X4 Rotation180(
			-1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, -1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
			);

		// 270-degree Z-rotation
		static const XMFLOAT4X4 Rotation270(
			0.0f, -1.0f, 0.0f, 0.0f,
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
			);
	};

	// Constructor for DeviceResources.
	DeviceResources::DeviceResources() :
		m_outputSize(0.0f, 0.0f),
		m_logicalSize(0.0f, 0.0f),
		m_nativeOrientation(DisplayOrientations::None),
		m_currentOrientation(DisplayOrientations::None),
		m_dpi(-1.0f),
		m_compositionScaleX(1.0f),
		m_compositionScaleY(1.0f),
		m_deviceNotify(nullptr),
		m_eglContext(EGL_NO_CONTEXT),
		m_eglDisplay(EGL_NO_DISPLAY),
		m_eglSurface(EGL_NO_SURFACE),
		m_swapChainPanel(nullptr),
		m_bAngleInitialized(false)
	{

	}

	// Configures resources that don't depend on the Direct3D device.
	void DeviceResources::CreateDeviceIndependentResources()
	{

	}

	void DeviceResources::Release()
	{
		eglMakeCurrent(NULL, NULL, NULL, NULL);

		if (m_eglDisplay && m_eglContext)
		{
			eglDestroyContext(m_eglDisplay, m_eglContext);
			m_eglContext = nullptr;
		}

		if (m_eglDisplay && m_eglSurface)
		{
			eglDestroySurface(m_eglDisplay, m_eglSurface);
			m_eglSurface = nullptr;
		}

		if (m_eglDisplay)
		{
			eglTerminate(m_eglDisplay);
			m_eglDisplay = nullptr;
		}

		m_bAngleInitialized = false;
	}

	void DeviceResources::aquireContext()
	{
		if (!m_eglSurface)
			return;

		std::lock_guard<std::mutex> guard(m_mutex);
		if (!eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext)){
			throw std::runtime_error("DeviceResouces: failed to make EGL context current");
		}
	}

	void DeviceResources::releaseContext()
	{
		std::lock_guard<std::mutex> guard(m_mutex);
		if (!eglMakeCurrent(NULL, NULL, NULL, NULL)){
			throw std::runtime_error("DeviceResouces: failed to set EGL context to NULL");
		}
	}

	// Configures the Direct3D device, and stores handles to it and the device context.
	void DeviceResources::CreateDeviceResources()
	{
		const EGLint configAttributes[] =
		{
			EGL_RED_SIZE, 8,
			EGL_GREEN_SIZE, 8,
			EGL_BLUE_SIZE, 8,
			EGL_ALPHA_SIZE, 8,
			EGL_DEPTH_SIZE, 8,
			EGL_STENCIL_SIZE, 8,
			EGL_NONE
		};

		const EGLint displayAttributes[] =
		{
			// This can be used to configure D3D11. For example, EGL_PLATFORM_ANGLE_TYPE_D3D11_FL9_3_ANGLE could be used.
			// This would ask the graphics card to use D3D11 Feature Level 9_3 instead of Feature Level 11_0+.
			// On Windows Phone, this would allow the Phone Emulator to act more like the GPUs that are available on real Phone devices.
			EGL_PLATFORM_ANGLE_TYPE_ANGLE,
			EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
			EGL_NONE,
		};

		const EGLint contextAttributes[] =
		{
			EGL_CONTEXT_CLIENT_VERSION, 2,
			EGL_NONE
		};

		PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
		if (!eglGetPlatformDisplayEXT)
		{
			throw Exception::CreateException(E_FAIL, L"Failed to get function eglGetPlatformDisplayEXT");
		}

		m_eglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, displayAttributes);
		if (m_eglDisplay == EGL_NO_DISPLAY)
		{
			throw Exception::CreateException(E_FAIL, L"Failed to get default EGL display");
		}

		EGLint majorVersion, minorVersion;
		if (eglInitialize(m_eglDisplay, &majorVersion, &minorVersion) == EGL_FALSE)
		{
			throw Exception::CreateException(E_FAIL, L"Failed to initialize EGL");
		}

		EGLint numConfigs = 0;
		if ((eglChooseConfig(m_eglDisplay, configAttributes, &m_eglConfig, 1, &numConfigs) == EGL_FALSE) || (numConfigs == 0))
		{
			throw Exception::CreateException(E_FAIL, L"Failed to choose first EGLConfig");
		}

		m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttributes);
		if (m_eglContext == EGL_NO_CONTEXT)
		{
			throw Exception::CreateException(E_FAIL, L"Failed to create EGL context");
		}

		m_bAngleInitialized = true;
	}

	// These resources need to be recreated every time the window size is changed.
	void DeviceResources::CreateWindowSizeDependentResources()
	{
		// Calculate the necessary swap chain and render target size in pixels.
		m_outputSize.Width = m_logicalSize.Width * m_compositionScaleX;
		m_outputSize.Height = m_logicalSize.Height * m_compositionScaleY;

		// Prevent zero size DirectX content from being created.
		m_outputSize.Width = max(m_outputSize.Width, 1);
		m_outputSize.Height = max(m_outputSize.Height, 1);

		if (m_eglSurface == EGL_NO_SURFACE)
		{
			// Create a PropertySet and initialize with the EGLNativeWindowType.
			Windows::Foundation::Collections::PropertySet^ surfaceCreationProperties = ref new Windows::Foundation::Collections::PropertySet();
			surfaceCreationProperties->Insert(ref new String(EGLNativeWindowTypeProperty), m_swapChainPanel);

			m_eglSurface = eglCreateWindowSurface(m_eglDisplay, m_eglConfig, reinterpret_cast<IInspectable*>(surfaceCreationProperties), NULL);
			if (m_eglSurface == EGL_NO_SURFACE)
			{
				throw Exception::CreateException(E_FAIL, L"Failed to create EGL surface");
			}
		}
	}

	// This method is called when the XAML control is created (or re-created).
	void DeviceResources::SetSwapChainPanel(SwapChainPanel^ panel)
	{
		m_swapChainPanel = panel;
		DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();
		m_logicalSize = Windows::Foundation::Size(static_cast<float>(panel->ActualWidth), static_cast<float>(panel->ActualHeight));
		m_nativeOrientation = currentDisplayInformation->NativeOrientation;
		m_currentOrientation = currentDisplayInformation->CurrentOrientation;
		m_compositionScaleX = panel->CompositionScaleX;
		m_compositionScaleY = panel->CompositionScaleY;
		m_dpi = currentDisplayInformation->LogicalDpi;
		CreateDeviceIndependentResources();
		CreateDeviceResources();
	}

	// This method is called in the event handler for the SizeChanged event.
	void DeviceResources::SetLogicalSize(Windows::Foundation::Size logicalSize)
	{
		if (m_logicalSize != logicalSize)
		{
			m_logicalSize = logicalSize;
			CreateWindowSizeDependentResources();
		}
	}

	// This method is called in the event handler for the DpiChanged event.
	void DeviceResources::SetDpi(float dpi)
	{
		if (dpi != m_dpi)
		{
			m_dpi = dpi;
			CreateWindowSizeDependentResources();
		}
	}

	// This method is called in the event handler for the OrientationChanged event.
	void DeviceResources::SetCurrentOrientation(DisplayOrientations currentOrientation)
	{
		if (m_currentOrientation != currentOrientation)
		{
			m_currentOrientation = currentOrientation;
			CreateWindowSizeDependentResources();
		}
	}

	// This method is called in the event handler for the CompositionScaleChanged event.
	void DeviceResources::SetCompositionScale(float compositionScaleX, float compositionScaleY)
	{
		if (m_compositionScaleX != compositionScaleX ||
			m_compositionScaleY != compositionScaleY)
		{
			m_compositionScaleX = compositionScaleX;
			m_compositionScaleY = compositionScaleY;
			CreateWindowSizeDependentResources();
		}
	}

	// This method is called in the event handler for the DisplayContentsInvalidated event.
	void DeviceResources::ValidateDevice()
	{

	}

	// Recreate all device resources and set them back to the current state.
	void DeviceResources::HandleDeviceLost()
	{
		Release();

		if (m_deviceNotify != nullptr)
		{
			m_deviceNotify->OnDeviceLost();
		}

		CreateDeviceResources();
		CreateWindowSizeDependentResources();

		if (m_deviceNotify != nullptr)
		{
			m_deviceNotify->OnDeviceRestored();
		}
	}

	// Register our DeviceNotify to be informed on device lost and creation.
	void DeviceResources::RegisterDeviceNotify(IDeviceNotify* deviceNotify)
	{
		m_deviceNotify = deviceNotify;
	}

	// Call this method when the app suspends. It provides a hint to the driver that the app 
	// is entering an idle state and that temporary buffers can be reclaimed for use by other apps.
	void DeviceResources::Trim()
	{

	}

	// Present the contents of the swap chain to the screen.
	void DeviceResources::Present()
	{
		if (!m_bAngleInitialized)
			return;

		eglSwapBuffers(m_eglDisplay, m_eglSurface);
	}
}
