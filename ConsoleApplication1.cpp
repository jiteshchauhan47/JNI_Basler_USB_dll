// ConsoleApplication1.cpp : Defines the entry point for the console application.
//
//Author : JMC (Arvces Technologies)
//#define USE_OPENCV

#include "stdafx.h"
#include <stdio.h>
#include <conio.h>
#include <Windows.h>

#include "arvces_usbBasler_usb_basler.h"
#define NUM_OF_BUFFERS 100
// Include files to use the PYLON API // Settings to use Basler USB cameras
#define USE_USB
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbCamera.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/PylonGUI.h>
#include <deque>
#include <Windows.h>
#include <thread>


#ifdef USE_OPENCV
//Include Opencv Stuff
#include <opencv\cv.h>
#include <opencv\highgui.h>
#endif

using namespace Pylon;
//using namespace Basler_UsbCameraParams;
using namespace GenApi;
#ifdef USE_OPENCV
using namespace cv;
#endif

BOOL InitCameraBasler(long lCameraSerial);
BOOL DeInitCameraBasler();
BOOL SetExposureBasler(float fExpTimeMS, float fGain = 1.0f, float fBrightness = 1.0f, float fContrast = 1.0f);
BOOL SetCameraAOI(int nOffsetX, int nOffsetY, int nWidth, int nHeight);
BOOL SetCameraTriggerMode(int nTriggerMode, int nTriggerSource, int nTriggerActivation);
BOOL GrabCameraFrame(int nArrMode = 0);//0 = char array and 1 = int array for image transfer
int64_t Adjust(int64_t val, int64_t minimum, int64_t maximum, int64_t inc);
BOOL ImageLiveStartBasler(uint32_t nNoOfImages = 50000000, int nArrMode = 0);
BOOL ImageLiveStopBasler();
BOOL SetImageOrientation(int nOrientationMode);
DWORD WINAPI GrabandMaintainQue(LPVOID lpParameter);
void GrabinThread();

// Basler Camera specific code
Pylon::CBaslerUsbInstantCamera m_CameraBasler;

BOOL m_bCamAvailable = FALSE;
BOOL m_bLive = FALSE;
BOOL m_bThreadLive = TRUE;
int nImageWidth, nImageHeight;
std::string m_strCamera1Serial;
bool m_bValidCam = false;
unsigned char *arrImage_data;
int *arrIntImage_data;
float fExposureValue = 105;
BOOL bUseOpencvSaveImage = false;

BOOL bSaveImageinQue = true;
int * Image;
int m_nWidth;
int m_nHeight;
std::deque <int*> m_dequeIntImage_Data;

#ifdef USE_OPENCV
IplImage* m_img = NULL;
#endif

int main()
{
//	printf("Starting Basler Engine\n");
	/*
	if (InitCameraBasler())
	{
		printf("Grabbing Image\n");
		GrabCameraFrame();
		printf("Grabbed. Done");
	}
	*/
//	_getch();
    return 0;
}

BOOL InitCameraBasler(long lCameraSerial)
{
	PylonInitialize();
	// Get the transport layer factory
	CTlFactory& TlFactory = CTlFactory::GetInstance();
	// Get all attached cameras and exit the application if no camera is found
	
	
	// Automagically call PylonInitialize and PylonTerminate to ensure the pylon runtime system
	// is initialized during the lifetime of this object.
	Pylon::PylonAutoInitTerm autoInitTerm;

	DeviceInfoList_t lstDevices;
	try
	{
		TlFactory.EnumerateDevices(lstDevices);
		printf("Enumerating Cameras\n");
	}
	catch (GenICam::GenericException &e)
	{
		// Error handling
		std::string strErr;
		strErr = e.GetDescription();
		printf("Enumerating Cameras Failed : %s\n",strErr);
	}
	Pylon::CDeviceInfo deviceInfo;
	int nDeviceID = 0;
	int nNumDevices = (int)lstDevices.size();
	printf("Number of Cameras Found : %d\n", nNumDevices);
	int nNumTries = 0;
	if (lstDevices.empty())// To be commented out finally
	{
		printf("Camera Not Detected! Turn ON the Camera Power/check Cable and retry!\n");
		//m_pFrame->SetCameraStatus("Camera is not available");
		printf("InitCameraBasler Done\n");
		return FALSE;
	}
	else
	{
		m_bCamAvailable = TRUE;
		for (int i = 0; i < lstDevices.size(); i++)
		{
			deviceInfo = lstDevices.at(i);
			if ((atoi(deviceInfo.GetSerialNumber()) == lCameraSerial))
			{
				printf("Found Cam with desired Serial : %d , at ID : %d \n",lCameraSerial, i);
				nDeviceID = i;
				break;
			}
		}

		//m_nNumCameras = nNumDevices;
	}

	int i = 0;
	// Prepare cameras for grabbing
	try
	{
		printf("Trying to attach device at ID : %d \n", nDeviceID);
		
		m_CameraBasler.Attach(TlFactory.CreateDevice(lstDevices[nDeviceID]));
				
		if (!m_CameraBasler.IsUsb())
		{
			m_CameraBasler.DetachDevice();
			printf("USB cam Not Found\n");
		}
		else
			printf("USB cam Found\n");
		
		m_CameraBasler.Open();
		printf(" USB InitCameraBasler->Open\n");

		m_strCamera1Serial = m_CameraBasler.GetDeviceInfo().GetSerialNumber();
		printf(" USB CAM Serial No : %s\n", m_strCamera1Serial);
	
		if (atoi(m_strCamera1Serial.data()) == lCameraSerial)
			m_bValidCam = true;

		if (m_bValidCam)
		{
			printf("....Setting Default Params....\n");

			printf(" USB Trying to set exposure\n");
			SetExposureBasler(fExposureValue);
			printf(" USB Exposure Set\n");

			//set Max AOI
			printf("USB Setting AOI\n");
			SetCameraAOI(0,0,100,100);

			printf("USB Setting Trigger Mode\n");
			SetCameraTriggerMode(1,1,0);

			printf("\n");
		}
		else
			printf("Invalid Camera");

	}
	catch (GenICam::GenericException &e)
	{
		// Error handling
		//CString strError = e.GetDescription();
		//AfxMessageBox(strError);
		printf("InitCameraBasler Exception Caught \n");
		if (m_CameraBasler.IsOpen())
			m_CameraBasler.Close();
		if (m_CameraBasler.IsPylonDeviceAttached())
			m_CameraBasler.DetachDevice();
		m_bCamAvailable = FALSE;
	}// End of Pylon specific code
	printf("InitCameraBasler Done \n");

	return m_bCamAvailable;
} // InitCameraBasler()

BOOL DeInitCameraBasler()
{
	m_bThreadLive = FALSE;

	//Image = NULL;
	printf("Before Image Deleting...");
	delete[]Image;
	printf("Image* deleted.");

	if (m_CameraBasler.IsOpen())
		m_CameraBasler.Close();
	if (m_CameraBasler.IsPylonDeviceAttached())
		m_CameraBasler.DetachDevice();

	if (arrImage_data != NULL)
		delete[]arrImage_data;

	if (arrIntImage_data != NULL)
		delete[]arrIntImage_data;

	PylonTerminate();
#ifdef USE_OPENCV
	if (m_img != NULL)
		cvReleaseImage(&m_img);
#endif

	printf("DeInitCameraBasler Done \n");
	return TRUE;
}



BOOL SetExposureBasler(float fExpTimeMS, float fGain, float fBrightness, float fContrast)
{
	try
	{
		if (m_bValidCam)
		{
			if (!m_CameraBasler.IsPylonDeviceAttached())
				m_CameraBasler.Attach(CTlFactory::GetInstance().CreateFirstDevice());
			if (!m_CameraBasler.IsOpen())
				m_CameraBasler.Open();

			// Set Exposure
			m_CameraBasler.AcquisitionMode.SetValue(Basler_UsbCameraParams::AcquisitionMode_Continuous);
			m_CameraBasler.ExposureAuto.SetValue(Basler_UsbCameraParams::ExposureAuto_Off);
			m_CameraBasler.ExposureTime.SetValue(fExpTimeMS);
			double e = m_CameraBasler.ExposureTime.GetValue();
			printf("Setting Exposure: %3.3f \n", e);

			//Set Gain
			m_CameraBasler.GainAuto.SetValue(Basler_UsbCameraParams::GainAuto_Off);
			m_CameraBasler.Gain.SetValue(m_CameraBasler.Gain.GetMin());
			e = m_CameraBasler.Gain.GetValue();
			printf("Setting Gain: %3.3f \n", e);

			//Set Pixel format
			m_CameraBasler.PixelFormat.SetValue(Basler_UsbCameraParams::PixelFormat_Mono8);
			printf("Setting Pixel Format to Mono8\n");
		}
	}
	catch (GenICam::GenericException &e)
	{
		// Error handling
		//CString strError = e.GetDescription();
		//AfxMessageBox(strError);
		printf("USB-SetExposureBasler Exception Caught \n");
		if (m_CameraBasler.IsOpen())
			m_CameraBasler.Close();
		if (m_CameraBasler.IsPylonDeviceAttached())
			m_CameraBasler.DetachDevice();
		m_bCamAvailable = FALSE;
		return FALSE;
	}
	return TRUE;
}//SetExposureBaslerUSB()


BOOL SetCameraAOI(int nOffsetX, int nOffsetY, int nWidth, int nHeight)
{
	BOOL bRetVal = TRUE;
	try
	{
		if (m_bValidCam)
		{
			if (arrImage_data != NULL)
				delete[]arrImage_data;
			if (arrIntImage_data != NULL)
				delete[]arrIntImage_data;
			arrImage_data = (unsigned char*)calloc(sizeof(unsigned char), nWidth * nHeight);
			arrIntImage_data = (int*)calloc(sizeof(int), nWidth * nHeight);
			m_nWidth = nWidth;
			m_nHeight = nHeight;
			Image = (int*)calloc(sizeof(int), m_nWidth * m_nHeight);
			if (!m_CameraBasler.IsPylonDeviceAttached())
				m_CameraBasler.Attach(CTlFactory::GetInstance().CreateFirstDevice());
			if (!m_CameraBasler.IsOpen())
				m_CameraBasler.Open();
			// Set the AOI:

			// Get the integer nodes describing the AOI.

			CIntegerPtr offsetX(m_CameraBasler.OffsetX.GetNode());
			CIntegerPtr offsetY(m_CameraBasler.OffsetY.GetNode());
			CIntegerPtr width(m_CameraBasler.Width.GetNode());
			CIntegerPtr height(m_CameraBasler.Height.GetNode());

			// On some cameras the offsets are read-only,
			// so we check whether we can write a value. Otherwise, we would get an exception.
			// GenApi has some convenience predicates to check this easily.
			if (IsWritable(offsetX))
			{
				nOffsetX = nOffsetX < 0 ? 0 : nOffsetX;
				offsetX->SetValue(nOffsetX);
			}
			if (IsWritable(offsetY))
			{
				nOffsetY = nOffsetY < 0 ? 0 : nOffsetY;
				offsetY->SetValue(nOffsetY);
			}

			// Some properties have restrictions. Use GetInc/GetMin/GetMax to make sure you set a valid value.
			int64_t newWidth = nWidth;// width->GetMax();
			newWidth = Adjust(newWidth, width->GetMin(), width->GetMax(), width->GetInc());

			int64_t newHeight = nHeight;// height->GetMax();
			newHeight = Adjust(newHeight, height->GetMin(), height->GetMax(), height->GetInc());

			width->SetValue(newWidth);
			height->SetValue(newHeight);

			nImageHeight = newHeight;
			nImageWidth = newWidth;

			printf("Set Width : %d, Height : %d \n", newWidth, newHeight);
#ifdef USE_OPENCV	
			if (bUseOpencvSaveImage)
			{
				if (m_img != NULL)
					cvReleaseImage(&m_img);

				m_img = cvCreateImage(cvSize(newWidth, newHeight), 8, 1);
				cvSetZero(m_img);
			}
#endif
		}
	}
	catch (GenICam::GenericException &e)
	{
		// Error handling
		//CString strError = e.GetDescription();
		//AfxMessageBox(strError);
		printf("USB-SetCameraAOI Exception Caught \n");
		if (m_CameraBasler.IsOpen())
			m_CameraBasler.Close();
		if (m_CameraBasler.IsPylonDeviceAttached())
			m_CameraBasler.DetachDevice();
		m_bCamAvailable = FALSE;
		bRetVal = FALSE;
	}
	return bRetVal;
}

BOOL SetCameraTriggerMode(int nTriggerMode, int nTriggerSource, int nTriggerActivation)
{
	try
	{
		if (m_bValidCam)
		{
			if (!m_CameraBasler.IsPylonDeviceAttached())
				m_CameraBasler.Attach(CTlFactory::GetInstance().CreateFirstDevice());
			if (!m_CameraBasler.IsOpen())
				m_CameraBasler.Open();
			//Set Trigger Mode

			m_CameraBasler.TriggerMode.SetValue((Basler_UsbCameraParams::TriggerModeEnums)nTriggerMode);
			int nTriggerAMode = m_CameraBasler.TriggerMode.GetValue();
			printf("Trigger Mode Set to : %d \n", nTriggerAMode);

			m_CameraBasler.AcquisitionMode.SetValue(Basler_UsbCameraParams::AcquisitionMode_SingleFrame);
			int nAcMode = (int)m_CameraBasler.AcquisitionMode.GetValue();
			printf("Acquisition Mode Set to : %d \n", nAcMode);

			m_CameraBasler.TriggerSource.SetValue((Basler_UsbCameraParams::TriggerSourceEnums)nTriggerSource);
			int nTriggerSourceMode = (int)m_CameraBasler.TriggerSource.GetValue();
			printf("Trigger SourceMode Set to : %d, requested was : %d \n", nTriggerSourceMode, nTriggerSource);

			m_CameraBasler.TriggerActivation.SetValue((Basler_UsbCameraParams::TriggerActivationEnums)nTriggerActivation);
			int nTriggerActMode = (int)m_CameraBasler.TriggerActivation.GetValue();
			printf("Trigger ActivationMode Set to : %d \n", nTriggerActMode);

		}
	}
	catch (GenICam::GenericException &e)
	{
		// Error handling
		//CString strError = e.GetDescription();
		//AfxMessageBox(strError);
		printf("USB-SetCameraTriggerMode Exception Caught \n");
		if (m_CameraBasler.IsOpen())
			m_CameraBasler.Close();
		if (m_CameraBasler.IsPylonDeviceAttached())
			m_CameraBasler.DetachDevice();
		m_bCamAvailable = FALSE;
		return FALSE;
	}
	return TRUE;
}

BOOL SetImageOrientation(int nOrientationMode)
{
	if (m_bValidCam)
	{
		;
		
	}
	return TRUE;
}

DWORD WINAPI GrabandMaintainQue(LPVOID lpParameter)
{
	while (m_bThreadLive)
	{
		printf("Running in Thread\n");
		GrabCameraFrame(1);
		//		m_dequeIntImage_Data.push_back(arrIntImage_data); //as done in ImageLiveSTart
		printf("Image Grabbed in Thread. And Pushed\n");
	}
	return 0;
}

void GrabinThread()
{
	while (m_bThreadLive)
	{
		printf("Running in Thread\n");
		GrabCameraFrame(1);
		//	m_dequeIntImage_Data.push_back(arrIntImage_data); //as done in ImageLiveStart
		printf("Image Grabbed in Thread. And Pushed\n");
	}
}

BOOL GrabCameraFrame(int nArrMode)
{
	if (m_bValidCam)
	{
		ImageLiveStartBasler(1, nArrMode);
		ImageLiveStopBasler();
	}
	return TRUE;
}

// Adjust value to make it comply with range and increment passed.
//
// The parameter's minimum and maximum are always considered as valid values.
// If the increment is larger than one, the returned value will be: min + (n * inc).
// If the value doesn't meet these criteria, it will be rounded down to ensure compliance.


int64_t Adjust(int64_t val, int64_t minimum, int64_t maximum, int64_t inc)
{
	// Check the input parameters.
	if (inc <= 0)
	{
		// Negative increments are invalid.
		throw LOGICAL_ERROR_EXCEPTION("Unexpected increment %d", inc);
	}
	if (minimum > maximum)
	{
		// Minimum must not be bigger than or equal to the maximum.
		throw LOGICAL_ERROR_EXCEPTION("minimum bigger than maximum.");
	}

	// Check the lower bound.
	if (val < minimum)
	{
		return minimum;
	}

	// Check the upper bound.
	if (val > maximum)
	{
		return maximum;
	}

	// Check the increment.
	if (inc == 1)
	{
		// Special case: all values are valid.
		return val;
	}
	else
	{
		// The value must be min + (n * inc).
		// Due to the integer division, the value will be rounded down.
		return minimum + (((val - minimum) / inc) * inc);
	}


}

BOOL ImageLiveStartBasler(uint32_t nNoOfImages, int nArrMode)
{
	try
	{
		m_bLive = TRUE;
		if (!m_CameraBasler.IsPylonDeviceAttached())
			m_CameraBasler.Attach(CTlFactory::GetInstance().CreateFirstDevice());
		if (!m_CameraBasler.IsOpen())
			m_CameraBasler.Open();

		const uint32_t c_countOfImagesToGrab = nNoOfImages;
		CGrabResultPtr ptrGrabResult;
		m_CameraBasler.MaxNumBuffer = 50;
		m_CameraBasler.StartGrabbing(c_countOfImagesToGrab, GrabStrategy_OneByOne);//GrabStrategy_LatestImageOnly

		//printf("Grabbing Image at Camera. \n");
		while (m_CameraBasler.IsGrabbing() && m_bLive)
		{
			//printf("Waiting for RetrieveResult. Timeout set to INFINITE\n");
			printf("Waiting for Image\n");
			m_CameraBasler.RetrieveResult(INFINITE, ptrGrabResult, TimeoutHandling_ThrowException);
			//printf("Grabbed Image Buffer received. \n");
			if (ptrGrabResult->GrabSucceeded())
			{
				//printf("Grab Succeeded. Setting up data \n");
				unsigned char *pImageBuffer = (unsigned char *)ptrGrabResult->GetBuffer();
				int nW = ptrGrabResult->GetWidth();
				int nH = ptrGrabResult->GetHeight();
				//printf("Image Data Copied. Pixel 1: %d \n",pImageBuffer[0]);
				//printf("Image Data Copied. Pixel 100: %d \n", pImageBuffer[100]);
				for (int n = 0; n < nW*nH; n++)
				{
#ifdef USE_OPENCV
					if (bUseOpencvSaveImage)
					{
						m_img->imageData[n] = pImageBuffer[n];
					}
#endif
					if (nArrMode == 0)
						arrImage_data[n] = pImageBuffer[n];
					else if (nArrMode == 1)
						arrIntImage_data[n] = pImageBuffer[n];
				}
				m_dequeIntImage_Data.push_back(arrIntImage_data);//added 20.1.19s
				//printf("Data Copied \n");
			}
			else
			{
				printf("Error Encountered while Grabbing Image at Camera. \n");
				return FALSE;
			}
		}
#ifdef USE_OPENCV
		if (bUseOpencvSaveImage)
			cvSaveImage("Usb_Basler.bmp", m_img);
#endif
		m_CameraBasler.StopGrabbing();
		printf("USB-Image Grabbed\n");
	}
	catch (GenICam::GenericException &e)
	{
		// Error handling
		//CString strError = e.GetDescription();
		//AfxMessageBox(strError);
		printf("USB-ImageLiveStartBasler Exception Caught \n");
		if (m_CameraBasler.IsOpen())
			m_CameraBasler.Close();
		if (m_CameraBasler.IsPylonDeviceAttached())
			m_CameraBasler.DetachDevice();
		m_bCamAvailable = FALSE;
		return FALSE;
	}
	return TRUE;
}//ImageLiveStartBasler()

 //////////////////////////////////////////////////////////////////////////////////////////////////////////
 //Funtion Name			: ImageLiveStopBasler()
 //Function Description  : Image streaming has already been stopped in ImageLiveStartBasler()  
 //						    
 //	    					
 //Input					:  
 //Return				:  
 //////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL ImageLiveStopBasler()
{
	m_bLive = FALSE;
	return TRUE;
}//ImageLiveStop()

JNIEXPORT jint JNICALL Java_arvces_usbBasler_usb_1basler_InitCamera(JNIEnv *env, jclass type, jlong lCamSerial)
{
	int nRetVal = InitCameraBasler(lCamSerial);	

	return nRetVal;
}

JNIEXPORT void JNICALL Java_arvces_usbBasler_usb_1basler_DeInitCamera(JNIEnv *env, jclass type)
{
	DeInitCameraBasler();
}

JNIEXPORT void JNICALL Java_arvces_usbBasler_usb_1basler_SetExposure(JNIEnv *env, jclass type, jfloat fexposure)
{
	fExposureValue = fexposure < 105 ? 105 : fexposure;
	
	SetExposureBasler(fExposureValue);
}

JNIEXPORT void JNICALL Java_arvces_usbBasler_usb_1basler_SetAOI(JNIEnv *env, jclass type, jint offX, jint offY, jint width, jint height)
{
	
	SetCameraAOI((int)offX, (int)offY, (int)width, (int)height);
}

JNIEXPORT jcharArray JNICALL Java_arvces_usbBasler_usb_1basler_GetBaslerImageinArr(JNIEnv * env, jclass type, jboolean bSaveImageOpencv)
{
	bUseOpencvSaveImage = bSaveImageOpencv;

	jcharArray jretarr = env->NewCharArray(nImageWidth * nImageHeight);
	GrabCameraFrame(0);
	env->SetCharArrayRegion(jretarr, 0, nImageWidth * nImageHeight, (jchar*)arrImage_data);
	return jretarr;
}

JNIEXPORT jintArray JNICALL Java_arvces_usbBasler_usb_1basler_GetBaslerImageinArrIntArr(JNIEnv * env, jclass type, jboolean bSaveImageOpencv)
{
#ifdef USE_OPENCV
	bUseOpencvSaveImage = bSaveImageOpencv;
#endif
	jintArray jretarr = env->NewIntArray(nImageWidth * nImageHeight);
	//GrabCameraFrame(1);
	//Image = (int*)calloc(sizeof(int), m_nWidth * m_nHeight);
	if (m_dequeIntImage_Data.size() > 0)
	{
		printf("Number of Images in Q: %d", m_dequeIntImage_Data.size());
		Image = m_dequeIntImage_Data.front();
		m_dequeIntImage_Data.pop_front();

		printf("Image Grabbed(Popped..!) now setting in Int Array /n/n\n");
	}
	else
		printf("Size = 0 , Default Image Set /n");


	env->SetIntArrayRegion(jretarr, 0, nImageWidth * nImageHeight, (jint*)Image);
	printf("After SetIntArray.....");

	printf("Before returning..");
	
	//printf("Finished with Grab. Now prepare data in jIntArray \n");
	//env->SetIntArrayRegion(jretarr, 0, nImageWidth * nImageHeight, (jint*)arrIntImage_data);
	return jretarr;
}


JNIEXPORT void JNICALL Java_arvces_usbBasler_usb_1basler_SetTriggerMode(JNIEnv *env, jclass type, jint nTriggerMode, jint nTriggerSource, jint nTriggerActivation)
{
	int o = nTriggerMode;
	int n = nTriggerSource;
	int m = nTriggerActivation;
	printf("nTriggerMode requested : %d\n", o);
	SetCameraTriggerMode(o, n, m);
}

JNIEXPORT void JNICALL Java_arvces_usbBasler_usb_1basler_SetOrientation(JNIEnv *env, jclass type, jint nOrientMode)
{
	int n = nOrientMode;
	printf("OrientMode requested : %d\n",n);
	printf("Option to be Set\n");

	std::thread t1(GrabinThread);
	printf("\n Thread Startted");
	if (n == 0)
		SetThreadPriority(t1.native_handle(), THREAD_PRIORITY_NORMAL);
	else if (n == 1)
		SetThreadPriority(t1.native_handle(), THREAD_PRIORITY_HIGHEST);
	else if (n == 2)
		SetThreadPriority(t1.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
	else
		SetThreadPriority(t1.native_handle(), THREAD_PRIORITY_NORMAL);
	t1.detach();
	printf("\nThread detached");
}

