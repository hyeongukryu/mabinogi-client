

#define NOMINMAX
#include <Windows.h>
#include <tchar.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include "Inc/CommonStates.h"
#include "Inc/DDSTextureLoader.h"
#include "Inc/Effects.h"
#include "Inc/GeometricPrimitive.h"
#include "Inc/Model.h"
#include "Inc/PrimitiveBatch.h"
#include "Inc/ScreenGrab.h"
#include "Inc/SpriteBatch.h"
#include "Inc/SpriteFont.h"
#include "Inc/VertexTypes.h"
#include "Inc\WICTextureLoader.h"
#include "resource.h"
#include <iostream>
#include <fstream>

using namespace DirectX;
HINSTANCE                           g_hInst = nullptr;
HWND                                g_hWnd = nullptr;
D3D_DRIVER_TYPE                     g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL                   g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*                       g_pd3dDevice = nullptr;
ID3D11DeviceContext*                g_pImmediateContext = nullptr;
IDXGISwapChain*                     g_pSwapChain = nullptr;
ID3D11RenderTargetView*             g_pRenderTargetView = nullptr;
ID3D11Texture2D*                    g_pDepthStencil = nullptr;
ID3D11DepthStencilView*             g_pDepthStencilView = nullptr;

ID3D11ShaderResourceView*           g_pTextureRV1 = nullptr;
ID3D11ShaderResourceView*           g_pTextureRV2 = nullptr;

std::unique_ptr<CommonStates>                           g_States;
std::unique_ptr<SpriteBatch>                            g_Sprites;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
void Card();
void Crash();
void Render();

#include <mmdeviceapi.h>
#include <endpointvolume.h> 

void Mute()
{
	HRESULT hr;
	CoInitialize(NULL);
	IMMDeviceEnumerator *deviceEnumerator = NULL;
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (LPVOID *)&deviceEnumerator);
	IMMDevice *defaultDevice = NULL; 

	hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
	deviceEnumerator->Release();
	deviceEnumerator = NULL; 

	IAudioEndpointVolume *endpointVolume = NULL;
	hr = defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, NULL, (LPVOID *)&endpointVolume);
	defaultDevice->Release();
	defaultDevice = NULL; 

	// -------------------------
	endpointVolume->SetMute(TRUE, NULL);
	endpointVolume->Release(); 

	CoUninitialize();
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow )
{
	UNREFERENCED_PARAMETER( hPrevInstance );
	UNREFERENCED_PARAMETER( lpCmdLine );

	Card();

	if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
		return 0;

	Mute();

	if( FAILED( InitDevice() ) )
	{
		CleanupDevice();
		return 0;
	}

	// Main message loop
	MSG msg = {0};
	while( WM_QUIT != msg.message )
	{
		if( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		else
		{
			Render();
		}
	}

	CleanupDevice();

	return ( int )msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow )
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof( WNDCLASSEX );
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon( hInstance, ( LPCTSTR )IDI_ICON1 );
	wcex.hCursor = LoadCursor( nullptr, IDC_ARROW );
	wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"ClientWindowClass";
	wcex.hIconSm = LoadIcon( wcex.hInstance, ( LPCTSTR )IDI_ICON1 );
	if( !RegisterClassEx( &wcex ) )
		return E_FAIL;

	// Create window
	g_hInst = hInstance;
	RECT rc = { 0, 0, 1024, 768 };
	AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
	g_hWnd = CreateWindow( L"ClientWindowClass", L"마비노기", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		GetSystemMetrics(SM_CXSCREEN)/2 - 512, GetSystemMetrics(SM_CYSCREEN)/2 - 384, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
		nullptr );
	if( !g_hWnd )
		return E_FAIL;

	ShowWindow( g_hWnd, nCmdShow );

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect( g_hWnd, &rc );
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE( driverTypes );

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE( featureLevels );

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory( &sd, sizeof( sd ) );
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_CENTERED;
	sd.OutputWindow = g_hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ )
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDeviceAndSwapChain( nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );
		if( SUCCEEDED( hr ) )
			break;
	}
	if( FAILED( hr ) )
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = g_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&pBackBuffer );
	if( FAILED( hr ) )
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView( pBackBuffer, nullptr, &g_pRenderTargetView );
	pBackBuffer->Release();
	if( FAILED( hr ) )
		return hr;

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory( &descDepth, sizeof(descDepth) );
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_pd3dDevice->CreateTexture2D( &descDepth, nullptr, &g_pDepthStencil );
	if( FAILED( hr ) )
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory( &descDSV, sizeof(descDSV) );
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pd3dDevice->CreateDepthStencilView( g_pDepthStencil, &descDSV, &g_pDepthStencilView );
	if( FAILED( hr ) )
		return hr;

	g_pImmediateContext->OMSetRenderTargets( 1, &g_pRenderTargetView, g_pDepthStencilView );

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports( 1, &vp );

	// Create DirectXTK objects
	g_States.reset( new CommonStates( g_pd3dDevice ) );
	g_Sprites.reset( new SpriteBatch( g_pImmediateContext ) );

	// Load the Texture
	std::ifstream res0_file("res0.mintismintmint", std::ifstream::binary);
	std::vector<char> res0((std::istreambuf_iterator<char>(res0_file)), std::istreambuf_iterator<char>());
	std::reverse(res0.begin(), res0.end());
	if (res0.size() != 9978)
		return E_FAIL;
	hr = CreateWICTextureFromMemory( g_pd3dDevice, g_pImmediateContext, (const uint8_t *)&res0[0], res0.size(), nullptr, &g_pTextureRV1 );
	if( FAILED( hr ) )
		return hr;

	std::ifstream res1_file("res1.mintismintmint", std::ifstream::binary);
	std::vector<char> res1((std::istreambuf_iterator<char>(res1_file)), std::istreambuf_iterator<char>());
	std::reverse(res1.begin(), res1.end());
	if (res1.size() != 25194)
		return E_FAIL;
	hr = CreateWICTextureFromMemory( g_pd3dDevice, g_pImmediateContext, (const uint8_t *)&res1[0], res1.size(), nullptr, &g_pTextureRV2 );
	if( FAILED( hr ) )
		return hr;

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
	if( g_pImmediateContext ) g_pImmediateContext->ClearState();

	if( g_pTextureRV1 ) g_pTextureRV1->Release();
	if( g_pTextureRV2 ) g_pTextureRV2->Release();

	if( g_pDepthStencilView ) g_pDepthStencilView->Release();
	if( g_pDepthStencil ) g_pDepthStencil->Release();
	if( g_pRenderTargetView ) g_pRenderTargetView->Release();
	if( g_pSwapChain ) g_pSwapChain->Release();
	if( g_pImmediateContext ) g_pImmediateContext->Release();
	if( g_pd3dDevice ) g_pd3dDevice->Release();
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch( message )
	{
	case WM_PAINT:
		hdc = BeginPaint( hWnd, &ps );
		EndPaint( hWnd, &ps );
		break;

	case WM_DESTROY:
		PostQuitMessage( 0 );
		break;

	default:
		return DefWindowProc( hWnd, message, wParam, lParam );
	}

	return 0;
}

void Draw(float f)
{	
	g_Sprites->Begin();

	const float timeLine[] = {2, 3, 4, 5, 6, 7, 8, 8.5};

	if (f < timeLine[0])
	{
		// blank
	}
	else if (f < timeLine[1])
	{
		// nexon
		g_Sprites->Draw(g_pTextureRV1, XMFLOAT2(0, 0), Colors::White);
	}
	else if (f < timeLine[2])
	{
		// nexon out
		g_Sprites->Draw(g_pTextureRV1, XMFLOAT2(0, 0), Colors::White * (1.0f - (f - timeLine[1])));
	}
	else if (f < timeLine[3])
	{
		// blank
	}
	else if (f < timeLine[4])
	{
		// grb in
		g_Sprites->Draw(g_pTextureRV2, XMFLOAT2(0, 0), Colors::White * (f - timeLine[3]));
	}
	else if (f < timeLine[5])
	{
		// grb
		g_Sprites->Draw(g_pTextureRV2, XMFLOAT2(0, 0), Colors::White);
	}
	else if (f < timeLine[6])
	{
		// grb out
		g_Sprites->Draw(g_pTextureRV2, XMFLOAT2(0, 0), Colors::White * (1.0f - (f - timeLine[5])));
	}
	else if (f < timeLine[7])
	{
		// blank
	}
	else 
	{
		Crash();
	}

	g_Sprites->End();
}


//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
	// Update our time
	static float t = 0.0f;
	if( g_driverType == D3D_DRIVER_TYPE_REFERENCE )
	{
		t += ( float )XM_PI * 0.0125f;
	}
	else
	{
		static uint64_t dwTimeStart = 0;
		uint64_t dwTimeCur = GetTickCount64();
		if( dwTimeStart == 0 )
			dwTimeStart = dwTimeCur;
		t = ( dwTimeCur - dwTimeStart ) / 1000.0f;
	}

	//
	// Clear the back buffer
	//
	g_pImmediateContext->ClearRenderTargetView( g_pRenderTargetView, Colors::White );

	//
	// Clear the depth buffer to 1.0 (max depth)
	//
	g_pImmediateContext->ClearDepthStencilView( g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0 );

	Draw(t);

	//
	// Present our back buffer to our front buffer
	//
	g_pSwapChain->Present( 0, 0 );
}

void Card()
{
	int result = MessageBox(NULL,
		_T("고객님의 시스템에 설치되어있는 그래픽 카드는\n마비노기가 동작을 보장하는 카드가 아닙니다.\n이 그래픽 카드는 마비노기를 실행하는데 필요한\n최소한의 중요한 기능 몇 가지를 지원하지 않고 있으며\n실행을 계속할 경우 게임이 정상적으로 실행이 되지 않는 등의\n문제가 발생할 가능성이 있습니다.\n\n계속 실행하시겠습니까?\n"),
		_T("주의해주세요!"),
		MB_YESNO);

	if (result != IDYES)
	{
		exit(0);
	}
}

void Crash()
{
	int result = MessageBox(NULL,
		_T("불편을 끼쳐드려 대단히 죄송합니다.\n\n핵쉴드와 관련된 문제가 발생했습니다.\n\n오류내용 : 보호 API에 대한 후킹 행위가 감지되었습니다.\n\n함수명 :\n모듈명 :\n\n오류코드 : 0x00010301\n\n발생한 문제의 오류코드가 클립보드에 복사되었습니다.\n이 코드를 보안센터의 공지사항에서 검색해보신 후 (ctrl+v)\n해당 문제에 대한 조치를 받으시기 바랍니다.\n\n보안센터에서 해결이 힘든 경우에는\n버그 리포트 게시판의 오류보고도우미를 통해 리포트해주시기 바랍니다.\n\n[확인] 버튼을 누르면 마비노기 보안센터로 이동합니다.\n현재 실행되고 있는 클라이언트는 종료됩니다.\n\n\n\n"),
		_T("핵쉴드 오류"), MB_OKCANCEL);

	if (result == IDOK)
	{
		ShellExecute(NULL, NULL, _T("http://mabinogi.nexon.com/C3/News/security/main.asp"), NULL, NULL, NULL);
	}

	exit(0);
}
