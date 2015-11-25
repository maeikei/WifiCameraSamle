// ActionCameraStreamingSample.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <winsock.h>


static const double pyramidScale = 1.1;

const static std::tuple<std::string, std::string,bool> constConnectRC[] =
{
	// rtsp of sjcam.
	std::make_tuple("192.168.1.", "rtsp://192.168.1.254/sjcam.mov",false),
	// udp of gopro.
	std::make_tuple("10.5.5.", "udp://10.5.5.9:8554",true)
};








static void faceDetectThread(void);
static void keepAliveThread(void);
static void restartStreamThread(void);
static void powerOnThread(void);
static std::vector<std::string> getIP(void);


static std::mutex mtxFace;
static std::vector<cv::Rect> gDetectedFaces = {};

static std::mutex mtxImage;
static cv::Mat gToBeDetectImage;

static std::mutex mtxCV;
static std::condition_variable cvImage;



#define DUMP_VAR(x) {\
	std::cout << #x << "=<" << x << ">" << std::endl;\
}

int _tmain(int argc, _TCHAR* argv[])
{
	cv::VideoCapture vcap;
	cv::Mat image;

// exception ...
//	std::thread tDetectFace(faceDetectThread);
	
	std::string videoStreamAddress;

	bool KeepAlive = false;
	while (true) {
		auto myIPs = getIP();
		bool hit = false;
		for (auto myIP : myIPs){
			//std::cout << myIP << std::endl;
			for (auto rc : constConnectRC) {
				auto rcAddress = std::get<0>(rc);
				auto prefixAddress = myIP.substr(0, rcAddress.size());
				std::cout << prefixAddress << std::endl;
				std::cout << rcAddress << std::endl;
				if (rcAddress == prefixAddress){
					std::cout << "match address" << myIP << std::endl;
					videoStreamAddress = std::get<1>(rc);
					hit = true;
					if (std::get<2>(rc)){
						KeepAlive = true;
					}
				}
			}
		}
		if (hit){
			break;
		}
		std::cout << "wait a connect to sjcam or gopro" << std::endl;
		Sleep(1000);
	}
	if (KeepAlive){
		std::thread tPowerOn(powerOnThread);
		tPowerOn.detach();
		std::this_thread::sleep_for(std::chrono::seconds(20));

		std::thread tRestartStream(restartStreamThread);
		tRestartStream.detach();
		std::thread tKeepAlive(keepAliveThread);
		tKeepAlive.detach();
	}

	std::cout << videoStreamAddress << std::endl;

	//open the video stream and make sure it's opened
	if (!vcap.open(videoStreamAddress)) {
		std::cout << "Error opening video stream or file" << std::endl;
		return -1;
	}


	for (;;) {
		if (!vcap.read(image)) {
			std::cout << "No frame" << std::endl;
			cv::waitKey();
		}
		DUMP_VAR(image.rows);
		DUMP_VAR(image.cols);
		static int detectCounter = 0;
		if (0 == detectCounter++ % 6)
		{
			std::lock_guard<std::mutex> lk(mtxImage);
			gToBeDetectImage = image.clone();
			cvImage.notify_all();
		}
		for (auto &face : gDetectedFaces)
		{
			cv::Point pt1(face.x, face.y);
			cv::Point pt2((face.x + face.height), (face.y + face.width));
			cv::rectangle(image, pt1, pt2, cv::Scalar(0, 255, 0), 2, 8, 0);
		}

		cv::imshow("Output Window", image);
		if (cv::waitKey(1) >= 0) break;
	}
	return 0;
}

const std::string cStrPrefxi("C:/workspace/windows/opencv/mybuild/install");
const std::string cStrFaceCascadeName(cStrPrefxi + "/share/OpenCV/haarcascades/haarcascade_frontalface_alt.xml");

void faceDetectThread(void)
{
	cv::CascadeClassifier faceCascade;
	std::cout << "cStrFaceCascadeName = <" << cStrFaceCascadeName  << ">" << std::endl;
	auto ret = faceCascade.load(cStrFaceCascadeName);
	if (false == ret)
	{
		std::cerr << "--(!)Error loading" << std::endl;
		exit(-1);
	};

	while (true){
		{
			std::unique_lock<std::mutex> lk(mtxCV);
			cvImage.wait(lk);
		}

		cv::Mat image;
		{
			std::lock_guard<std::mutex> lk(mtxImage);
			image = gToBeDetectImage.clone();
		}
		std::cout << "image.cols=<"  << image.cols << ">" << " image.rows=<" << image.rows << ">" << std::endl;

		std::vector<cv::Rect> faces = {};
		cv::Mat frame_gray;
		cvtColor(image, frame_gray, cv::COLOR_BGR2GRAY);
		equalizeHist(frame_gray, frame_gray);
		// Detect faces
		faceCascade.detectMultiScale(frame_gray, faces, pyramidScale, 2, 0 | cv::CASCADE_SCALE_IMAGE, cv::Size(12, 12));
		{
			std::lock_guard<std::mutex> lk(mtxFace);
			gDetectedFaces = faces;
		}
	}
}




static std::vector<std::string> getIP(void)
{
	WSAData wsaData;
	if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
		return {};
	}
	char ac[80];
	if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {
		std::cerr << "Error " << WSAGetLastError() <<
			" when getting local host name." << std::endl;
		WSACleanup();
		return{};
	}
	//std::cout << "Host name is " << ac << "." << std::endl;

	struct hostent *phe = gethostbyname(ac);
	if (phe == 0) {
		std::cerr << "Yow! Bad host lookup." << std::endl;
		WSACleanup();
		return{};
	}

	std::vector<std::string> ret;
	for (int i = 0; phe->h_addr_list[i] != 0; ++i) {
		struct in_addr addr;
		memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
		auto addStr = inet_ntoa(addr);
		//std::cout << "Address " << i << ": " << addStr << std::endl;
		ret.push_back(addStr);
	}

	WSACleanup();
	return ret;
}




/*
content of 
C:/workspace/windows/ActionCamera/ActionCameraStreamingSample/x64/Release/keepLive
"C:/workspace/windows/ActionCamera/ActionCameraStreamingSample/x64/Release/curl" "http://10.5.5.9/gp/gpControl/execute?p1=gpStream&c1=restart"
*/


const int iConstKeepAliveInterval = 2500;
const std::string iStrKeepLiveMessage = "_GPHD_:0:0:2:0.000000\n";
static void keepAliveThread(void)
{
	WSAData wsaData;
	SOCKET sock;
	struct sockaddr_in addr;
	WSAStartup(MAKEWORD(2, 0), &wsaData);

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(8554);
	addr.sin_addr.S_un.S_addr = inet_addr("10.5.5.9");

	while (true){
		sendto(sock, iStrKeepLiveMessage.c_str(), iStrKeepLiveMessage.size(), 0, (struct sockaddr *)&addr, sizeof(addr));
		std::this_thread::sleep_for(std::chrono::milliseconds(iConstKeepAliveInterval));
	}
	closesocket(sock);
	WSACleanup();
}

const int iConstRestartInterval = 60;
static void startStreamThread(void)
{
	std::string cmd("\"C:/workspace/windows/ActionCamera/ActionCameraStreamingSample/x64/Release/startGoProStream\"");
//	while (true)
	{
		std::cout << cmd << std::endl;
		system(cmd.c_str());
		std::this_thread::sleep_for(std::chrono::seconds(iConstRestartInterval));
	}
}


/*
read mac address by post a http request
"http://10.5.5.9/gp/gpControl"
*/

std::string constStrGoproWakeUpMac_("F4DD9E0C0AF1");
const int iConstPowerOnInterval = 120;
static void powerOnThread(void)
{
	WSAData wsaData;
	SOCKET sock;
	struct sockaddr_in addr;
	WSAStartup(MAKEWORD(2, 0), &wsaData);

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(9);
	addr.sin_addr.S_un.S_addr = inet_addr("10.5.5.255");

	char message[102] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	for (int j = 0; j < 16; j++) {
		int c = 6 + j * constStrGoproWakeUpMac_.size();
		for (int i = 0; i < constStrGoproWakeUpMac_.size(); ++i) {
			message[c + i] = constStrGoproWakeUpMac_[i];
		}
	}
	DUMP_VAR(message);
	while (true){
		sendto(sock, message, sizeof(message), 0, (struct sockaddr *)&addr, sizeof(addr));
		std::this_thread::sleep_for(std::chrono::seconds(iConstPowerOnInterval));
	}
	closesocket(sock);
	WSACleanup();
}


/*

{"version":3.0,"display_hints":[{"key":"GPCAMERA_GROUP_VIDEO","display_name":"Video Settings","settings":[{"setting_id":5,"widget_type":"select","precedence":1},{"setting_id":6,"widget_type":"select","precedence":1},{"setting_id":2,"widget_type":"select","precedence":1},{"setting_id":3,"widget_type":"select","precedence":1},{"setting_id":4,"widget_type":"select","precedence":1},{"setting_id":8,"widget_type":"select","precedence":1},{"setting_id":9,"widget_type":"toggle","precedence":1},{"setting_id":10,"widget_type":"toggle","precedence":1},{"setting_id":13,"widget_type":"select","precedence":1},{"setting_id":14,"widget_type":"select","precedence":1}],"commands":[]},{"key":"GPCAMERA_GROUP_PHOTO","display_name":"Photo Settings","settings":[{"setting_id":17,"widget_type":"select","precedence":1},{"setting_id":20,"widget_type":"toggle","precedence":1}],"commands":[]},{"key":"GPCAMERA_GROUP_MULTI_SHOT","display_name":"Multi-Shot Settings","settings":[{"setting_id":29,"widget_type":"select","precedence":1},{"setting_id":30,"widget_type":"select","precedence":1},{"setting_id":28,"widget_type":"select","precedence":1},{"setting_id":33,"widget_type":"toggle","precedence":1}],"commands":[]},{"key":"GPCAMERA_GROUP_SETUP","display_name":"Setup","settings":[{"setting_id":52,"widget_type":"select","precedence":1},{"setting_id":53,"widget_type":"select","precedence":1},{"setting_id":55,"widget_type":"select","precedence":1},{"setting_id":56,"widget_type":"select","precedence":1},{"setting_id":57,"widget_type":"select","precedence":1}],"commands":[{"command_key":"GPCAMERA_SET_DATE_AND_TIME_ID","precedence":1}]},{"key":"GPCAMERA_GROUP_DELETE_ID","display_name":"Delete","settings":[],"commands":[{"command_key":"GPCAMERA_DELETE_LAST_FILE_ID","precedence":1},{"command_key":"GPCAMERA_DELETE_ALL_FILES_ID","precedence":1}]},{"key":"GPCAMERA_GROUP_CAMERA_INFO","display_name":"Camera Info","settings":[],"commands":[{"command_key":"GPCAMERA_NETWORK_NAME_ID","precedence":1},{"command_key":"GPCAMERA_INFO_VERSION_ID","precedence":1},{"command_key":"GPCAMERA_LOCATE_ID","precedence":1}]},{"key":"GPCAMERA_GROUP_WIRELESS_CONTROLS","display_name":"Wireless Controls","settings":[],"commands":[{"command_key":"GPCAMERA_USE_CURRENT_WIRELESS_REMOTE_ID","precedence":1}]},{"key":"GPCAMERA_GROUP_CAMERA_STATUS","display_name":"Camera Status","settings":[],"commands":[{"command_key":"GPCAMERA_BATTERY_LEVEL_ID","precedence":1},{"command_key":"GPCAMERA_SDCARD_CAPACITY_ID","precedence":1}]}],"modes":[{"path_segment":"video","display_name":"Video","value":0,"settings":[{"path_segment":"default_sub_mode","display_name":"Camera Mode","id":1,"options":[{"display_name":"Video","value":0},{"display_name":"Looping","value":3}]},{"path_segment":"current_sub_mode","display_name":"Video Sub Mode","id":68,"options":[{"display_name":"Video","value":0},{"display_name":"Looping","value":3}]},{"path_segment":"resolution","display_name":"Resolution","id":2,"options":[{"display_name":"1440","value":7},{"display_name":"1080 SuperView","value":8},{"display_name":"1080","value":9},{"display_name":"960","value":10},{"display_name":"720 SuperView","value":11},{"display_name":"720","value":12},{"display_name":"WVGA","value":13}]},{"path_segment":"fps","display_name":"Frames Per Second","id":3,"options":[{"display_name":"240","value":0},{"display_name":"120","value":1},{"display_name":"100","value":2},{"display_name":"90","value":3},{"display_name":"80","value":4},{"display_name":"60","value":5},{"display_name":"50","value":6},{"display_name":"48","value":7},{"display_name":"30","value":8},{"display_name":"25","value":9},{"display_name":"24","value":10},{"display_name":"15","value":11},{"display_name":"12.5","value":12}]},{"path_segment":"fov","display_name":"Field Of View","id":4,"options":[{"display_name":"Wide","value":0},{"display_name":"Medium","value":1},{"display_name":"Narrow","value":2}]},{"path_segment":"timelapse_rate","display_name":"Interval","id":5,"options":[{"display_name":"0.5 Seconds","value":0},{"display_name":"1 Second","value":1},{"display_name":"2 Seconds","value":2},{"display_name":"5 Seconds","value":3},{"display_name":"10 Seconds","value":4},{"display_name":"30 Seconds","value":5},{"display_name":"60 Seconds","value":6}]},{"path_segment":"looping","display_name":"Interval","id":6,"options":[{"display_name":"Max","value":0},{"display_name":"5 Minutes","value":1},{"display_name":"20 Minutes","value":2},{"display_name":"60 Minutes","value":3},{"display_name":"120 Minutes","value":4}]},{"path_segment":"spot_meter","display_name":"Spot Meter","id":9,"options":[{"display_name":"ON","value":1},{"display_name":"OFF","value":0}]},{"path_segment":"low_light","display_name":"Low Light","id":8,"options":[{"display_name":"AUTO","value":1},{"display_name":"OFF","value":0}]},{"path_segment":"protune","display_name":"Protune","id":10,"options":[{"display_name":"ON","value":1},{"display_name":"OFF","value":0}]},{"path_segment":"protune_sharpness","display_name":"Sharpness","id":14,"options":[{"display_name":"OFF","value":3},{"display_name":"ON","value":4}]},{"path_segment":"protune_iso","display_name":"ISO Limit","id":13,"options":[{"display_name":"1600","value":1},{"display_name":"400","value":2}]}]},{"path_segment":"photo","display_name":"Photo","value":1,"settings":[{"path_segment":"default_sub_mode","display_name":"Default Photo Sub Mode","id":16,"options":[{"display_name":"Single","value":0}]},{"path_segment":"current_sub_mode","display_name":"Photo Sub Mode","id":69,"options":[{"display_name":"Single","value":0}]},{"path_segment":"resolution","display_name":"Megapixels","id":17,"options":[{"display_name":"8MP Wide","value":0},{"display_name":"5MP Med","value":3}]},{"path_segment":"exposure_time","display_name":"Shutter","id":19,"options":[{"display_name":"Auto","value":0},{"display_name":"2 Seconds","value":1},{"display_name":"5 Seconds","value":2},{"display_name":"10 Seconds","value":3},{"display_name":"15 Seconds","value":4},{"display_name":"20 Seconds","value":5},{"display_name":"30 Seconds","value":6}]},{"path_segment":"spot_meter","display_name":"Spot Meter","id":20,"options":[{"display_name":"ON","value":1},{"display_name":"OFF","value":0}]}]},{"path_segment":"multi_shot","display_name":"Multishot","value":2,"settings":[{"path_segment":"default_sub_mode","display_name":"Default Multi-Shot Sub Mode","id":27,"options":[{"display_name":"Time Lapse","value":1},{"display_name":"Burst","value":0}]},{"path_segment":"current_sub_mode","display_name":"Multi-Shot Sub Mode","id":70,"options":[{"display_name":"Time Lapse","value":1},{"display_name":"Burst","value":0}]},{"path_segment":"burst_rate","display_name":"Rate","id":29,"options":[{"display_name":"3 Photos / 1 Second","value":0},{"display_name":"5 Photos / 1 Second","value":1},{"display_name":"10 Photos / 1 Second","value":2},{"display_name":"10 Photos / 2 Seconds","value":3},{"display_name":"10 Photos / 3 Seconds","value":4},{"display_name":"30 Photos / 1 Second","value":5},{"display_name":"30 Photos / 2 Seconds","value":6},{"display_name":"30 Photos / 3 Seconds","value":7}]},{"path_segment":"timelapse_rate","display_name":"Interval","id":30,"options":[{"display_name":"1 Photo / 0.5 Sec","value":0},{"display_name":"1 Photo / 1 Sec","value":1},{"display_name":"1 Photo / 2 Sec","value":2},{"display_name":"1 Photo / 5 Sec","value":5},{"display_name":"1 Photo / 10 Sec","value":10},{"display_name":"1 Photo / 30 Sec","value":30},{"display_name":"1 Photo / 60 Sec","value":60}]},{"path_segment":"resolution","display_name":"Megapixels","id":28,"options":[{"display_name":"8MP Wide","value":0},{"display_name":"5MP Med","value":3}]},{"path_segment":"spot_meter","display_name":"Spot Meter","id":33,"options":[{"display_name":"ON","value":1},{"display_name":"OFF","value":0}]}]},{"path_segment":"setup","display_name":"Setup","value":5,"settings":[{"path_segment":"orientation","display_name":"Orientation","id":52,"options":[{"display_name":"Up","value":1},{"display_name":"Down","value":2},{"display_name":"Auto","value":0}]},{"path_segment":"default_app_mode","display_name":"Default Mode","id":53,"options":[{"display_name":"Video","value":0},{"display_name":"Looping Video","value":5},{"display_name":"Photo","value":1},{"display_name":"Burst","value":2},{"display_name":"Time Lapse","value":3}]},{"path_segment":"led","display_name":"LED Blink","id":55,"options":[{"display_name":"OFF","value":0},{"display_name":"ON","value":1}]},{"path_segment":"beep_volume","display_name":"Beeps","id":56,"options":[{"display_name":"100%","value":0},{"display_name":"70%","value":1},{"display_name":"40%","value":3},{"display_name":"OFF","value":2}]},{"path_segment":"video_format","display_name":"Video Format","id":57,"options":[{"display_name":"NTSC","value":0},{"display_name":"PAL","value":1}]},{"path_segment":"wireless_mode","display_name":"Wireless Mode","id":63,"options":[{"display_name":"OFF","value":0},{"display_name":"App","value":1},{"display_name":"RC","value":2},{"display_name":"Smart","value":4}]},{"path_segment":"stream_gop_size","display_name":"Secondary Stream GOP Size","id":60,"options":[{"display_name":"Default","value":0},{"display_name":"3","value":3},{"display_name":"4","value":4},{"display_name":"8","value":8},{"display_name":"15","value":15},{"display_name":"30","value":30}]},{"path_segment":"stream_idr_interval","display_name":"Secondary Stream IDR Interval","id":61,"options":[{"display_name":"Default","value":0},{"display_name":"1","value":1},{"display_name":"2","value":2},{"display_name":"4","value":4}]},{"path_segment":"stream_bit_rate","display_name":"Secondary Stream Bit Rate","id":62,"options":[{"display_name":"250 Kbps","value":250000},{"display_name":"400 Kbps","value":400000},{"display_name":"600 Kbps","value":600000},{"display_name":"700 Kbps","value":700000},{"display_name":"800 Kbps","value":800000},{"display_name":"1 Mbps","value":1000000},{"display_name":"1.2 Mbps","value":1200000},{"display_name":"1.6 Mbps","value":1600000},{"display_name":"2 Mbps","value":2000000},{"display_name":"2.4 Mbps","value":2400000}]},{"path_segment":"stream_window_size","display_name":"Secondary Stream Window Size","id":64,"options":[{"display_name":"Default","value":0},{"display_name":"240","value":1},{"display_name":"240 3:4 Subsample","value":2},{"display_name":"240 1:2 Subsample","value":3},{"display_name":"480","value":4},{"display_name":"480 3:4 Subsample","value":5},{"display_name":"480 1:2 Subsample","value":6}]}]}],"filters":[{"activated_by":[{"setting_id":10,"setting_value":1}],"blacklist":{"setting_id":2,"values":[1,2,3,4,5,6,8,11,12,13]}},{"activated_by":[{"setting_id":2,"setting_value":7},{"setting_id":57,"setting_value":0}],"blacklist":{"setting_id":3,"values":[0,1,2,3,4,5,6,7,9,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":9},{"setting_id":57,"setting_value":0}],"blacklist":{"setting_id":3,"values":[0,1,2,3,4,6,7,9,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":8},{"setting_id":57,"setting_value":0}],"blacklist":{"setting_id":3,"values":[0,1,2,3,4,5,6,9,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":10},{"setting_id":57,"setting_value":0},{"setting_id":10,"setting_value":0}],"blacklist":{"setting_id":3,"values":[0,1,2,3,4,6,7,9,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":10},{"setting_id":57,"setting_value":0},{"setting_id":10,"setting_value":1}],"blacklist":{"setting_id":3,"values":[0,1,2,3,4,6,7,8,9,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":12},{"setting_id":57,"setting_value":0}],"blacklist":{"setting_id":3,"values":[0,1,3,4,6,7,9,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":11},{"setting_id":57,"setting_value":0}],"blacklist":{"setting_id":3,"values":[0,1,2,3,4,6,7,9,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":13},{"setting_id":57,"setting_value":0}],"blacklist":{"setting_id":3,"values":[0,2,3,4,5,6,7,8,9,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":7},{"setting_id":57,"setting_value":1}],"blacklist":{"setting_id":3,"values":[0,1,2,3,4,5,6,7,8,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":9},{"setting_id":57,"setting_value":1}],"blacklist":{"setting_id":3,"values":[0,1,2,3,4,5,7,8,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":8},{"setting_id":57,"setting_value":1}],"blacklist":{"setting_id":3,"values":[0,1,2,3,4,5,6,8,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":10},{"setting_id":57,"setting_value":1},{"setting_id":10,"setting_value":0}],"blacklist":{"setting_id":3,"values":[0,1,2,3,4,5,7,8,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":10},{"setting_id":57,"setting_value":1},{"setting_id":10,"setting_value":1}],"blacklist":{"setting_id":3,"values":[0,1,2,3,4,5,7,8,9,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":12},{"setting_id":57,"setting_value":1}],"blacklist":{"setting_id":3,"values":[0,1,3,4,5,7,8,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":11},{"setting_id":57,"setting_value":1}],"blacklist":{"setting_id":3,"values":[0,1,2,3,4,5,7,8,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":13},{"setting_id":57,"setting_value":1}],"blacklist":{"setting_id":3,"values":[0,1,3,4,5,6,7,8,9,10,11,12]}},{"activated_by":[{"setting_id":2,"setting_value":7}],"blacklist":{"setting_id":4,"values":[1,2]}},{"activated_by":[{"setting_id":2,"setting_value":9},{"setting_id":10,"setting_value":0}],"blacklist":{"setting_id":4,"values":[2]}},{"activated_by":[{"setting_id":2,"setting_value":9},{"setting_id":10,"setting_value":1}],"blacklist":{"setting_id":4,"values":[1,2]}},{"activated_by":[{"setting_id":2,"setting_value":8}],"blacklist":{"setting_id":4,"values":[1,2]}},{"activated_by":[{"setting_id":2,"setting_value":10}],"blacklist":{"setting_id":4,"values":[1,2]}},{"activated_by":[{"setting_id":2,"setting_value":12}],"blacklist":{"setting_id":4,"values":[2]}},{"activated_by":[{"setting_id":2,"setting_value":11}],"blacklist":{"setting_id":4,"values":[1,2]}},{"activated_by":[{"setting_id":2,"setting_value":13}],"blacklist":{"setting_id":4,"values":[1,2]}},{"activated_by":[{"setting_id":68,"setting_value":0}],"blacklist":{"setting_id":5,"values":[0,1,2,3,4,5,6]}},{"activated_by":[{"setting_id":68,"setting_value":0}],"blacklist":{"setting_id":6,"values":[0,1,2,3,4]}},{"activated_by":[{"setting_id":68,"setting_value":1}],"blacklist":{"setting_id":6,"values":[0,1,2,3,4]}},{"activated_by":[{"setting_id":68,"setting_value":1}],"blacklist":{"setting_id":2,"values":[1,2,3,4,5,6,8,10,11,12,13]}},{"activated_by":[{"setting_id":68,"setting_value":1}],"blacklist":{"setting_id":3,"values":[0,1,2,3,4,5,6,7,10,11,12]}},{"activated_by":[{"setting_id":68,"setting_value":3}],"blacklist":{"setting_id":5,"values":[0,1,2,3,4,5,6]}},{"activated_by":[{"setting_id":68,"setting_value":1}],"blacklist":{"setting_id":8,"values":[0,1]}},{"activated_by":[{"setting_id":2,"setting_value":13}],"blacklist":{"setting_id":8,"values":[0,1]}},{"activated_by":[{"setting_id":3,"setting_value":8}],"blacklist":{"setting_id":8,"values":[0,1]}},{"activated_by":[{"setting_id":3,"setting_value":9}],"blacklist":{"setting_id":8,"values":[0,1]}},{"activated_by":[{"setting_id":3,"setting_value":10}],"blacklist":{"setting_id":8,"values":[0,1]}},{"activated_by":[{"setting_id":3,"setting_value":11}],"blacklist":{"setting_id":8,"values":[0,1]}},{"activated_by":[{"setting_id":3,"setting_value":12}],"blacklist":{"setting_id":8,"values":[0,1]}},{"activated_by":[{"setting_id":70,"setting_value":0}],"blacklist":{"setting_id":30,"values":[0,1,2,5,10,30,60]}},{"activated_by":[{"setting_id":70,"setting_value":0}],"blacklist":{"setting_id":29,"values":[4,5,6,7]}},{"activated_by":[{"setting_id":70,"setting_value":1}],"blacklist":{"setting_id":29,"values":[0,1,2,3,4,5,6,7]}},{"activated_by":[{"setting_id":68,"setting_value":3}],"blacklist":{"setting_id":10,"values":[0,1]}},{"activated_by":[{"setting_id":68,"setting_value":3}],"blacklist":{"setting_id":14,"values":[3,4]}},{"activated_by":[{"setting_id":68,"setting_value":3}],"blacklist":{"setting_id":13,"values":[0,1,2]}},{"activated_by":[{"setting_id":10,"setting_value":0}],"blacklist":{"setting_id":14,"values":[3,4]}},{"activated_by":[{"setting_id":10,"setting_value":0}],"blacklist":{"setting_id":13,"values":[0,1,2]}}],"commands":[{"key":"GPCAMERA_SHUTTER","display_name":"Start or stop capture","url":"/command/shutter","widget_type":"button"},{"key":"GPCAMERA_MODE","display_name":"Set Mode","url":"/command/mode","widget_type":"button"},{"key":"GPCAMERA_SUBMODE","display_name":"Set Mode and Sub-Mode","url":"/command/sub_mode","widget_type":"button"},{"key":"GPCAMERA_POWER_ID","display_name":"Power Off Camera","url":"/command/system/sleep","widget_type":"button"},{"key":"GPCAMERA_FWUPDATE_DOWNLOAD_START","display_name":"Notify start FW Update File Download","url":"/command/fwupdate/download/start","widget_type":"button"},{"key":"GPCAMERA_FWUPDATE_DOWNLOAD_DONE","display_name":"Notify completion of FW Update File Download","url":"/command/fwupdate/download/done","widget_type":"button"},{"key":"GPCAMERA_FWUPDATE_DOWNLOAD_CANCEL","display_name":"Cancel FW Update File Download","url":"/command/fwupdate/download/cancel","widget_type":"button"},{"key":"GPCAMERA_FACTORY_RESET","display_name":"Reset to Factory Defaults","url":"/command/system/factory/reset","widget_type":"button"},{"key":"GPCAMERA_SLEEP","display_name":"Power Saving Sleep Mode","url":"/command/system/sleep","widget_type":"button"},{"key":"GPCAMERA_USE_CURRENT_WIRELESS_REMOTE_ID","display_name":"Use with Current Wi-Fi Remote","url":"/setting/63/2","widget_type":"button"},{"key":"GPCAMERA_USE_NEW_WIRELESS_REMOTE_ID","display_name":"Use with New Wi-Fi Remote","url":"/command/wireless/rc/pair","widget_type":"button"},{"key":"GPCAMERA_CANCEL_PAIR_WIRELESS_REMOTE_ID","display_name":"Cancel pairing of new Wi-Fi Remote","url":"/command/wireless/rc/pair/cancel","widget_type":"button"},{"key":"GPCAMERA_CANCEL_PAIR_WIRELESS_ID","display_name":"Cancel pairing if already paired","url":"/command/wireless/pair/cancel","widget_type":"button"},{"key":"GPCAMERA_VIDEO_PROTUNE_RESET_TO_DEFAULT","display_name":"Reset Protune","url":"/command/video/protune/reset","widget_type":"button"},{"key":"GPCAMERA_MULTISHOT_PROTUNE_RESET_TO_DEFAULT","display_name":"Reset Protune","url":"/command/multi_shot/protune/reset","widget_type":"button"},{"key":"GPCAMERA_PHOTO_PROTUNE_RESET_TO_DEFAULT","display_name":"Reset Protune","url":"/command/photo/protune/reset","widget_type":"button"},{"key":"GPCAMERA_SET_DATE_AND_TIME_ID","display_name":"Set Date and Time","url":"/command/setup/date_time","widget_type":"button"},{"key":"GPCAMERA_DELETE_LAST_FILE_ID","display_name":"Delete Last File","url":"/command/storage/delete/last","widget_type":"button"},{"key":"GPCAMERA_DELETE_ALL_FILES_ID","display_name":"Delete All Files from SD Card","url":"/command/storage/delete/all","widget_type":"button"},{"key":"GPCAMERA_DELETE_FILE_ID","display_name":"Delete File","url":"/command/storage/delete","widget_type":"button"},{"key":"GPCAMERA_LOCATE_ID","display_name":"Locate Camera","url":"/command/system/locate","widget_type":"toggle","status_field":"system/camera_locate_active"},{"key":"GPCAMERA_NETWORK_NAME_ID","display_name":"Name","url":"/command/wireless/ap/ssid","widget_type":"text","min_length":8,"max_length":32,"regex":"^((?!goprohero$)[A-Za-z0-9_\\-@]+)$"},{"key":"GPCAMERA_AP_CONTROL","display_name":"Control Wi-Fi AP","url":"/command/wireless/ap/control","widget_type":"button"},{"key":"GPCAMERA_INFO_VERSION_ID","display_name":"Version","url":"camera_version","widget_type":"readonly"},{"key":"GPCAMERA_NETWORK_VERSION_ID","display_name":"Version","url":"bacpac_version","widget_type":"readonly"},{"key":"GPCAMERA_BATTERY_LEVEL_ID","display_name":"Battery Level","url":"camera_battery","widget_type":"readonly"},{"key":"GPCAMERA_SDCARD_CAPACITY_ID","display_name":"SD Card Capacity","url":"sd_card","widget_type":"child"},{"key":"GPCAMERA_TAG_MOMENT","display_name":"Tag Moment","url":"/command/storage/tag_moment","widget_type":"button"},{"key":"GPCAMERA_RC_PAIR","display_name":"Pair with Known RC","url":"/command/rc/pair","widget_type":"button"},{"key":"GPCAMERA_SSID_SCAN","display_name":"Start Wi-Fi SSID Scan","url":"/command/wireless/ssid/scan","widget_type":"button"},{"key":"GPCAMERA_SSID_LIST","display_name":"Wi-Fi SSID Scan Results","url":"/command/wireless/ssid/list","widget_type":"button"},{"key":"GPCAMERA_SSID_SELECT","display_name":"Connect to Wi-Fi SSID","url":"/command/wireless/ssid/select","widget_type":"button"},{"key":"GPCAMERA_SSID_DELETE","display_name":"Delete Wi-Fi SSID from Known List","url":"/command/wireless/ssid/delete","widget_type":"button"}],"status":{"groups":[{"group":"system","fields":[{"id":1,"name":"internal_battery_present"},{"id":2,"name":"internal_battery_level"},{"id":3,"name":"external_battery_present"},{"id":4,"name":"external_battery_level"},{"id":6,"name":"system_hot"},{"id":8,"name":"system_busy"},{"id":9,"name":"quick_capture_active"},{"id":10,"name":"encoding_active"},{"id":11,"name":"lcd_lock_active"},{"id":45,"name":"camera_locate_active"},{"id":57,"name":"current_time_msec"},{"id":60,"name":"next_poll_msec"},{"id":61,"name":"analytics_ready"},{"id":62,"name":"analytics_size"},{"id":63,"name":"in_contextual_menu"}]},{"group":"app","fields":[{"id":43,"name":"mode"},{"id":44,"name":"sub_mode"}]},{"group":"video","fields":[{"id":13,"name":"video_progress_counter"},{"id":46,"name":"video_protune_default"}]},{"group":"photo","fields":[{"id":47,"name":"photo_protune_default"}]},{"group":"multi_shot","fields":[{"id":48,"name":"multi_shot_protune_default"},{"id":49,"name":"multi_shot_count_down"}]},{"group":"broadcast","fields":[{"id":14,"name":"broadcast_progress_counter"},{"id":15,"name":"broadcast_viewers_count"},{"id":16,"name":"broadcast_bstatus"}]},{"group":"wireless","fields":[{"id":17,"name":"enable"},{"id":19,"name":"state","levels":["pair_status"]},{"id":20,"name":"type","levels":["pair_status"]},{"id":21,"name":"pair_time","levels":["pair_status"]},{"id":22,"name":"state","levels":["scan_status"]},{"id":23,"name":"scan_time_msec","levels":["scan_status"]},{"id":28,"name":"pairing"},{"id":26,"name":"remote_control_version"},{"id":27,"name":"remote_control_connected"},{"id":31,"name":"app_count"},{"id":24,"name":"provision_status"},{"id":29,"name":"wlan_ssid"},{"id":30,"name":"ap_ssid"},{"id":56,"name":"wifi_bars"}]},{"group":"stream","fields":[{"id":32,"name":"enable"},{"id":55,"name":"supported"}]},{"group":"storage","fields":[{"id":33,"name":"sd_status"},{"id":34,"name":"remaining_photos"},{"id":35,"name":"remaining_video_time"},{"id":36,"name":"num_group_photos"},{"id":37,"name":"num_group_videos"},{"id":38,"name":"num_total_photos"},{"id":39,"name":"num_total_videos"},{"id":54,"name":"remaining_space"},{"id":58,"name":"num_hilights"},{"id":59,"name":"last_hilight_time_msec"}]},{"group":"setup","fields":[{"id":40,"name":"date_time"}]},{"group":"fwupdate","fields":[{"id":41,"name":"ota_status"},{"id":42,"name":"download_cancel_request_pending"}]}]},"services":{"live_stream_start_v2":{"version":1,"description":"Start real-time A/V stream using LTP.","url":"/gp/gpControl/execute?p1=gpStream&a1=proto_v2&c1=restart"},"live_stream_stop_v2":{"version":1,"description":"Stop real-time A/V stream using LTP.","url":"/gp/gpControl/execute?p1=gpStream&c1=stop"},"analytics_file_get":{"version":1,"description":"Acquire the analytics file as an octet-stream.","url":"/gp/gpControl/analytics/get"},"analytics_file_clear":{"version":1,"description":"Remove analytics file and clear analytics log.","url":"/gp/gpControl/analytics/clear"},"live_stream_start":{"version":1,"description":"Start real-time A/V stream using LTP.","url":"/gp/gpControl/execute?p1=gpStream&c1=restart"},"live_stream_stop":{"version":1,"description":"Stop real-time A/V stream using LTP.","url":"/gp/gpControl/execute?p1=gpStream&c1=stop"},"media_list":{"version":1,"description":"Supports listing of media on SD card.","url":"/gp/gpMediaList"},"media_metadata":{"version":1,"description":"Supports extraction of metadata from a particular media file.","url":"/gp/gpMediaMetadata"},"platform_auth":{"version":1,"description":"Supports OAuth2 cross-client authorization.","url":"/gp/gpPlatformAuth"},"fw_update":{"version":1,"description":"Supports client-assisted Over-the-Air firmware updating.","url":"/gp/gpUpdate"},"supports_app_clipping":{"version":1,"description":"Supports mobile app clipping.","url":""}},
"info":{"model_number":16,"model_name":"HERO4 Session","firmware_version":"HX1.01.01.50","serial_number":"C3141324655445","board_type":"0x07","ap_mac":"F4DD9E0C0AF1","ap_ssid":"gocam","ap_has_default_credentials":"0","git_sha1":"ede89b1d29b484551a236b280cdb0f265ab99784"}}


*/
