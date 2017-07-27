#include <glog/config.h>
#include <evpp/tcp_server.h>
#include <evpp/buffer.h>
#include <evpp/tcp_conn.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <turbojpeg.h>

#include "ScreenCapture.h"
#include "Message.h"
#include <libconfig.h++>
#include <fakeinput.hpp>


#include <winsock2.h>
#include <stdio.h>

#include <lz4.h>

using namespace libconfig;

#define SIZE_3DS_RGB 400*3*240
#define SIZE_3DS_RGB565 400*2*240

//-------------------------------------------------
// CONFIGURATION VARIABLES
// should not be const to be edit via client on runtime
//-------------------------------------------------

//-------------------------------------------------
// Frame quality ( JPEG )
//-------------------------------------------------
int imageQuality = 40;

//-------------------------------------------------
// Capture FPS
//-------------------------------------------------
int streamFPS = 16;

//-------------------------------------------------
// this option only good for play game ( that need mininum delay )
// but for movie should be false for smoother
//-------------------------------------------------
bool needWaitForReceived = true;

//-------------------------------------------------
// When "needWaitForReceived" to avoid lag too long and save time when transfer then
// we only wait "WAIT_FRAME" if client still not received we start sending frame.
//-------------------------------------------------
int maxFrameToWait = INT_MAX;

//-------------------------------------------------
// Split frame mode
//-------------------------------------------------
bool splitFrameMode = false;

//-------------------------------------------------
// Private variables
//-------------------------------------------------

struct ClientConnection
{
	evpp::TCPConnPtr conn;
	bool received;
	bool isMainConnect;
	// for split frame mode
	int currentFrame;
	char pieceIndex;
};


bool isStreamingDesktop = false;
int currentActiveConnect = -1;
int currentFrame = 0;
int currentWaitFrame = 0;

Message* receivedMessage = nullptr;
std::mutex msgMutex;

SL::Screen_Capture::ScreenCaptureManager framgrabber;
int connectionCount;
std::vector<ClientConnection> conns;

std::map<int, std::vector<char*>> framePieceCached;
std::map<int, int> framePieceState;
int framePieceNormal = 0;
int framePieceLast = 0;
char totalConn;
//=================================================================================
// KEY MAPPING
//=================================================================================
static std::string currentProfile = "";
static std::map<std::string, std::map<char, FakeInput::Key>> MappingProfiles;
static std::map<std::string, FakeInput::Key> KeyMapping;

void MapKey()
{
	KeyMapping["A"] = FakeInput::Key_A;
	KeyMapping["B"] = FakeInput::Key_B;
	KeyMapping["C"] = FakeInput::Key_C;
	KeyMapping["D"] = FakeInput::Key_D;
	KeyMapping["E"] = FakeInput::Key_E;
	KeyMapping["F"] = FakeInput::Key_F;
	KeyMapping["G"] = FakeInput::Key_G;
	KeyMapping["H"] = FakeInput::Key_H;
	KeyMapping["I"] = FakeInput::Key_I;
	KeyMapping["J"] = FakeInput::Key_J;
	KeyMapping["K"] = FakeInput::Key_K;
	KeyMapping["L"] = FakeInput::Key_L;
	KeyMapping["M"] = FakeInput::Key_M;
	KeyMapping["N"] = FakeInput::Key_N;
	KeyMapping["O"] = FakeInput::Key_O;
	KeyMapping["P"] = FakeInput::Key_P;
	KeyMapping["Q"] = FakeInput::Key_Q;
	KeyMapping["R"] = FakeInput::Key_R;
	KeyMapping["S"] = FakeInput::Key_S;
	KeyMapping["T"] = FakeInput::Key_T;
	KeyMapping["U"] = FakeInput::Key_U;
	KeyMapping["V"] = FakeInput::Key_V;
	KeyMapping["W"] = FakeInput::Key_W;
	KeyMapping["X"] = FakeInput::Key_X;
	KeyMapping["Y"] = FakeInput::Key_Y;
	KeyMapping["Z"] = FakeInput::Key_Z;
	KeyMapping["0"] = FakeInput::Key_0;
	KeyMapping["1"] = FakeInput::Key_1;
	KeyMapping["2"] = FakeInput::Key_2;
	KeyMapping["3"] = FakeInput::Key_3;
	KeyMapping["4"] = FakeInput::Key_4;
	KeyMapping["5"] = FakeInput::Key_5;
	KeyMapping["6"] = FakeInput::Key_6;
	KeyMapping["7"] = FakeInput::Key_7;
	KeyMapping["8"] = FakeInput::Key_8;
	KeyMapping["9"] = FakeInput::Key_9;
	KeyMapping["F1"] = FakeInput::Key_F1;
	KeyMapping["F2"] = FakeInput::Key_F2;
	KeyMapping["F3"] = FakeInput::Key_F3;
	KeyMapping["F4"] = FakeInput::Key_F4;
	KeyMapping["F5"] = FakeInput::Key_F5;
	KeyMapping["F6"] = FakeInput::Key_F6;
	KeyMapping["F7"] = FakeInput::Key_F7;
	KeyMapping["F8"] = FakeInput::Key_F8;
	KeyMapping["F9"] = FakeInput::Key_F9;
	KeyMapping["F10"] = FakeInput::Key_F10;
	KeyMapping["F11"] = FakeInput::Key_F11;
	KeyMapping["F12"] = FakeInput::Key_F12;
	KeyMapping["Escape"] = FakeInput::Key_Escape;
	KeyMapping["Space"] = FakeInput::Key_Space;
	KeyMapping["Return"] = FakeInput::Key_Return;
	KeyMapping["Backspace"] = FakeInput::Key_Backspace;
	KeyMapping["Tab"] = FakeInput::Key_Tab;
	KeyMapping["Shift_L"] = FakeInput::Key_Shift_L;
	KeyMapping["Shift_R"] = FakeInput::Key_Shift_R;
	KeyMapping["Control_L"] = FakeInput::Key_Control_L;
	KeyMapping["Control_R"] = FakeInput::Key_Control_R;
	KeyMapping["Alt_L"] = FakeInput::Key_Alt_L;
	KeyMapping["Alt_R"] = FakeInput::Key_Alt_R;
	KeyMapping["CapsLock"] = FakeInput::Key_CapsLock;
	KeyMapping["NumLock"] = FakeInput::Key_NumLock;
	KeyMapping["ScrollLock"] = FakeInput::Key_ScrollLock;
	KeyMapping["PrintScreen"] = FakeInput::Key_PrintScreen;
	KeyMapping["Insert"] = FakeInput::Key_Insert;
	KeyMapping["Delete"] = FakeInput::Key_Delete;
	KeyMapping["PageUP"] = FakeInput::Key_PageUP;
	KeyMapping["PageDown"] = FakeInput::Key_PageDown;
	KeyMapping["Home"] = FakeInput::Key_Home;
	KeyMapping["End"] = FakeInput::Key_End;
	KeyMapping["Left"] = FakeInput::Key_Left;
	KeyMapping["Right"] = FakeInput::Key_Right;
	KeyMapping["Up"] = FakeInput::Key_Up;
	KeyMapping["Down"] = FakeInput::Key_Down;
	KeyMapping["Numpad0"] = FakeInput::Key_Numpad0;
	KeyMapping["Numpad1"] = FakeInput::Key_Numpad1;
	KeyMapping["Numpad2"] = FakeInput::Key_Numpad2;
	KeyMapping["Numpad3"] = FakeInput::Key_Numpad3;
	KeyMapping["Numpad4"] = FakeInput::Key_Numpad4;
	KeyMapping["Numpad5"] = FakeInput::Key_Numpad5;
	KeyMapping["Numpad6"] = FakeInput::Key_Numpad6;
	KeyMapping["Numpad7"] = FakeInput::Key_Numpad7;
	KeyMapping["Numpad8"] = FakeInput::Key_Numpad8;
	KeyMapping["Numpad9"] = FakeInput::Key_Numpad9;
	KeyMapping["NumpadAdd"] = FakeInput::Key_NumpadAdd;
	KeyMapping["NumpadSubtract"] = FakeInput::Key_NumpadSubtract;
	KeyMapping["NumpadMultiply"] = FakeInput::Key_NumpadMultiply;
	KeyMapping["NumpadDivide"] = FakeInput::Key_NumpadDivide;
	KeyMapping["NumpadDecimal"] = FakeInput::Key_NumpadDecimal;
	KeyMapping["NumpadEnter"] = FakeInput::Key_NumpadEnter;
}

void SendRawImageDataToClient(const SL::Screen_Capture::Image& img, evpp::TCPConnPtr conn3ds)
{
	char* imgbuffer = (char*)malloc(SIZE_3DS_RGB);
	ExtractAndConvertToBGR_3DS(img, imgbuffer);
	//-----------------------------------------------------------------------------
	// build Message
	//-----------------------------------------------------------------------------
	char* imgCompressed = (char*)malloc(SIZE_3DS_RGB);
	int compressedSize = LZ4_compress_default(imgbuffer, imgCompressed, SIZE_3DS_RGB, SIZE_3DS_RGB);
	if (compressedSize > 0)
	{
		int totalSize = compressedSize + 5;
		Message* msg = new Message();
		msg->MessageCode = 30;
		msg->ContentSize = totalSize;
		char* msgContent = (char*)malloc(totalSize);
		//--------------------------------------
		char* data = msgContent;
		*data++ = msg->MessageCode;
		*data++ = msg->ContentSize;
		*data++ = msg->ContentSize >> 8;
		*data++ = msg->ContentSize >> 16;
		*data++ = msg->ContentSize >> 24;
		memcpy(data, imgCompressed, compressedSize);
		//-----------------------------------------------------------------------------
		// Send message to all client
		//-----------------------------------------------------------------------------
		if (conn3ds != nullptr) conn3ds->Send(msgContent, totalSize);
		//-----------------------------------------------------------------------------
		// free msg data
		//-----------------------------------------------------------------------------
		free(msgContent);
		free(msg);
	}
	//-----------------------------------------------------------------------------
	free(imgCompressed);
	free(imgbuffer);
}

void SendImageDataToClient(const SL::Screen_Capture::Image& img, evpp::TCPConnPtr conn3ds)
{
	if (conn3ds == nullptr) return;
	//-----------------------------------------------------------------------------
	// Extract image data
	//-----------------------------------------------------------------------------
	auto size = RowStride(img) * Height(img);
	char* imgbuffer = (char*)malloc(size);
	int result = ExtractAndConvertToRGB(img, imgbuffer);
	if (result == -1)
	{
		free(imgbuffer);
		return;
	}
	//-----------------------------------------------------------------------------
	// encrypt to JPEG small
	//-----------------------------------------------------------------------------
	cv::Mat rawImg = cv::Mat(cv::Size(Width(img), Height(img)), CV_8UC3, imgbuffer);
	cv::Mat scaledImg;
	cv::resize(rawImg, scaledImg, cv::Size(400, 240));
	long unsigned int _jpegSize = 0;
	unsigned char* _compressedImage = NULL;
	tjhandle _jpegCompressor = tjInitCompress();
	tjCompress2(_jpegCompressor, scaledImg.data, 400, 0, 240, TJPF_RGB, &_compressedImage, &_jpegSize, TJSAMP_444, imageQuality, TJFLAG_FASTDCT);
	//-----------------------------------------------------------------------------
	// build Message
	//-----------------------------------------------------------------------------
	// header size = 1 + 4 + 4 = 9 -> 1 byte code + 4 bytes content size + 4 bytes frame
	int totalSize = _jpegSize + 9;
	Message* msg = new Message();
	msg->MessageCode = IMAGE_PACKET;
	msg->ContentSize = totalSize;
	char* msgContent = (char*)malloc(totalSize);
	//--------------------------------------
	char* data = msgContent;
	*data++ = msg->MessageCode;
	//--------------------------------------
	// 4 bytes for content size
	*data++ = msg->ContentSize;
	*data++ = msg->ContentSize >> 8;
	*data++ = msg->ContentSize >> 16;
	*data++ = msg->ContentSize >> 24;
	//--------------------------------------
	// 4 bytes for frame index
	//--------------------------------------
	*data++ = currentFrame;
	*data++ = currentFrame >> 8;
	*data++ = currentFrame >> 16;
	*data++ = currentFrame >> 24;
	//--------------------------------------
	memcpy(data, _compressedImage, _jpegSize);
	std::cout << "Frame Index: " << currentFrame << " MSG size: " << totalSize << std::endl;
	//-----------------------------------------------------------------------------
	// Send message to all client
	//-----------------------------------------------------------------------------
	//if (conn3ds != nullptr) conn3ds->Send(msgContent, totalSize);
	conn3ds->Send(msgContent, totalSize);
	//-----------------------------------------------------------------------------
	// advance to next frame or recircle back to 0 when reach max.
	currentFrame++;
	if (currentFrame >= INT32_MAX - 1)
		currentFrame = 0;
	//-----------------------------------------------------------------------------
	// free msg data
	//-----------------------------------------------------------------------------
	free(msgContent);
	msg->Release();
	delete msg;
	//-----------------------------------------------------------------------------
	free(imgbuffer);
	tjDestroy(_jpegCompressor);
	tjFree(_compressedImage);
}

//-----------------------------------------------------------------------------
// return False if frame not found or not finish
// return True if frame sent and erased
bool CleanFrameCached(int frameIndex)
{
	if(framePieceState.find(frameIndex) != framePieceState.end())
	{
		if(framePieceState[frameIndex] == totalConn)
		{
			for(char i = 0; i < totalConn; i++)
			{
				free(framePieceCached[frameIndex][i]);
			}
			std::vector<char*>().swap(framePieceCached[frameIndex]);
			framePieceCached.erase(frameIndex);
			framePieceState.erase(frameIndex);
			return true;
		}
		return false;
	}
	return false;
}

int PopOutOnePiece(int frameIndex)
{
	if (framePieceState.find(frameIndex) != framePieceState.end())
	{
		if (framePieceState[frameIndex] == totalConn) return -1;
		int ret = framePieceState[frameIndex];
		framePieceState[frameIndex]++;
		return ret;
	} 
	return -1;
}

void GetFramePieces(const SL::Screen_Capture::Image& img)
{
	//-----------------------------------------------------------------------------
	// advance to next frame or recircle back to 0 when reach max.
	currentFrame++;
	if (currentFrame >= INT32_MAX - 1)
		currentFrame = 0;

	//-----------------------------------------------------------------------------
	// Extract image data
	//-----------------------------------------------------------------------------
	auto size = RowStride(img) * Height(img);
	char* imgbuffer = (char*)malloc(size);
	int result = ExtractAndConvertToRGB(img, imgbuffer);
	if (result == -1)
	{
		free(imgbuffer);
		return;
	}
	//-----------------------------------------------------------------------------
	// encrypt to JPEG small
	//-----------------------------------------------------------------------------
	cv::Mat rawImg = cv::Mat(cv::Size(Width(img), Height(img)), CV_8UC3, imgbuffer);
	cv::Mat scaledImg;
	cv::resize(rawImg, scaledImg, cv::Size(400, 240));
	long unsigned int _jpegSize = 0;
	unsigned char* _compressedImage = NULL;
	tjhandle _jpegCompressor = tjInitCompress();
	tjCompress2(_jpegCompressor, scaledImg.data, 400, 0, 240, TJPF_RGB, &_compressedImage, &_jpegSize, TJSAMP_444, imageQuality, TJFLAG_FASTDCT);

	//-----------------------------------------------------------------------------
	// collect split information
	//-----------------------------------------------------------------------------
	framePieceNormal = _jpegSize / totalConn;
	framePieceLast = _jpegSize - (framePieceNormal * totalConn) + framePieceNormal;

	std::vector<char*> frameCached;
	for(char i = 0; i < totalConn; ++i)
	{
		//-----------------------------------------------------------------------------
		// build Message
		//-----------------------------------------------------------------------------
		// header size = 1 + 4 + 4 = 9 -> 1 byte code + 4 bytes content size + 4 bytes frame
		int totalSize = 11;
		if (i == totalConn - 1) totalSize += framePieceLast;
		else totalSize += framePieceNormal;
		//--------------------------------------
		Message* msg = new Message();
		msg->MessageCode = IMAGE_PACKET;
		msg->ContentSize = totalSize;
		char* msgContent = (char*)malloc(sizeof(char) * totalSize);
		//--------------------------------------
		char* data = msgContent;
		*data++ = msg->MessageCode;
		//--------------------------------------
		// 4 bytes for content size
		*data++ = msg->ContentSize;
		*data++ = msg->ContentSize >> 8;
		*data++ = msg->ContentSize >> 16;
		*data++ = msg->ContentSize >> 24;
		//--------------------------------------
		// 4 bytes for frame index
		//--------------------------------------
		*data++ = currentFrame;
		*data++ = currentFrame >> 8;
		*data++ = currentFrame >> 16;
		*data++ = currentFrame >> 24;
		//--------------------------------------
		// 1 byte for total part
		//--------------------------------------
		*data++ = totalConn;
		//--------------------------------------
		// 1 byte for part index
		//--------------------------------------
		*data++ = i;
		//--------------------------------------
		if (i == totalConn - 1) memcpy(data, _compressedImage + (i*framePieceNormal), framePieceLast);
		else memcpy(data, _compressedImage + (i*framePieceNormal), framePieceNormal);
		//--------------------------------------
		// Store the msg content of this frame.
		frameCached.push_back(msgContent);
	}
	//--------------------------------------
	framePieceCached[currentFrame] = frameCached;
	framePieceState[currentFrame] = 0;
	//-----------------------------------------------------------------------------
	free(imgbuffer);
}

void ProcessInput(const Message* message, ClientConnection* clientConnect)
{
	//------------------------------------------------------------
	// input code from 70 to 85 is normal input mapping
	if (message->MessageCode >= char(70) && message->MessageCode <= char(85))
	{
		char state = message->GetFirstByte();
		if (state == char(1))
		{
			FakeInput::Keyboard::pressKey(MappingProfiles[currentProfile][message->MessageCode]);
		}
		else
		{
			FakeInput::Keyboard::releaseKey(MappingProfiles[currentProfile][message->MessageCode]);
		}
	}
	else if (message->MessageCode > char(85) && message->MessageCode <= char(90))
	{
		//-----------------------------------------------------
		// input code from 86 to 90 is for circle pad

		std::cout << "Circle pad input is not implement yet!" << std::endl;
	}
}

void ProcessMessage(const Message* message, ClientConnection* clientConnect)
{
	switch (message->MessageCode)
	{
	case START_STREAM_PACKET:
		{
		std::cout << "Client start streaming" << std::endl;
			totalConn = conns.size();
			//---------------------------------
			// Client what start the stream is main
			//---------------------------------
			clientConnect->isMainConnect = true;
			isStreamingDesktop = true;
			currentFrame = 0;
			if (framgrabber.isPaused()) framgrabber.resume();
			break;
		}
	case STOP_STREAM_PACKET:
		{
		std::cout << "Client stop streaming" << std::endl;
			//---------------------------------
			// Only main client can stop stream
			//---------------------------------
			if (!clientConnect->isMainConnect) return;
			isStreamingDesktop = false;
			framgrabber.pause();
			break;
		}
	case IMAGE_RECEIVED_PACKET:
		{
		
			clientConnect->received = true;
			break;
		}
	case OPTION_PACKET:
		{
			break;
		}
	default:
		{
			ProcessInput(message, clientConnect);
			break;
		}
	}
}

void ProcessData(const char* buffer, size_t lenght, ClientConnection* clientConnect)
{
	msgMutex.lock();
	if (receivedMessage == nullptr) receivedMessage = new Message();
	int cutOffset = receivedMessage->ReadMessageFromData(buffer, lenght);
	if (cutOffset >= 0)
	{
		// Process new message
		ProcessMessage(receivedMessage, clientConnect);

		delete receivedMessage;
		receivedMessage = nullptr;

		if (cutOffset > 0)
		{
			// continue process by buffer.
			int sizeLeft = lenght - cutOffset;
			char* bufferLeft = (char*)malloc(sizeLeft);
			memcpy(bufferLeft, buffer + cutOffset, sizeLeft);

			ProcessData(bufferLeft, sizeLeft, clientConnect);

			free(bufferLeft);
		}
	}
	msgMutex.unlock();
}

void GetIPAddress()
{
	// Get local host name
	char szHostName[128] = "";

	if (::gethostname(szHostName, sizeof(szHostName)))
	{
		// Error handling -> call 'WSAGetLastError()'
	}
	// Get local IP addresses
	struct sockaddr_in SocketAddress;
	struct hostent* pHost = 0;
	pHost = ::gethostbyname(szHostName);
	if (!pHost)
	{
		// Error handling -> call 'WSAGetLastError()'
	}
	char aszIPAddresses[10][16]; // maximum of ten IP addresses
	for (int iCnt = 0; ((pHost->h_addr_list[iCnt]) && (iCnt < 10)); ++iCnt)
	{
		memcpy(&SocketAddress.sin_addr, pHost->h_addr_list[iCnt], pHost->h_length);
		strcpy(aszIPAddresses[iCnt], inet_ntoa(SocketAddress.sin_addr));

		std::cout << "IP: " << aszIPAddresses[iCnt] << std::endl;
	}
}

int main(int argc, char** argv)
{
	google::InitGoogleLogging("NKStreamerServer");
	google::SetCommandLineOption("GLOG_minloglevel", "2");

	//===========================================================================
	// Config
	//===========================================================================
	MapKey();
	Config serverCfg;
	try
	{
		serverCfg.readFile("server.cfg");
	}
	catch (const FileIOException& fioex)
	{
		std::cout << "Config file not found: server.cfg. Please make sure it on root folder." << std::endl;
		return (EXIT_FAILURE);
	}
	catch (const ParseException& pex)
	{
		std::cout << "Config file corrupted." << std::endl;
		return (EXIT_FAILURE);
	}
	//===============================
	int cfgPort = 1234;
	int cfgMonitorIndex = -1;
	int cfgThreadNum = -1;
	//===============================
	try
	{
		if (!serverCfg.lookupValue("port", cfgPort)) cfgPort = 3;
		if (!serverCfg.lookupValue("monitor", cfgMonitorIndex) || cfgMonitorIndex == -1) cfgMonitorIndex = 0;
		if (!serverCfg.lookupValue("thread_num", cfgThreadNum) || cfgThreadNum == -1) cfgThreadNum = 3;

		const Setting& root = serverCfg.getRoot();
		//===============================
		// load input profiles
		const Setting& inputProfiles = root["input"];
		int inputCount = inputProfiles.getLength();
		for (int i = 0; i < inputCount; ++i)
		{
			std::string inputName;
			std::string btn_A, btn_B, btn_X, btn_Y;
			std::string btn_DPAD_UP, btn_DPAD_DOWN, btn_DPAD_LEFT, btn_DPAD_RIGHT;
			std::string btn_L, btn_R, btn_ZL, btn_ZR;
			std::string btn_START, btn_SELECT;

			if (!(inputProfiles[i].lookupValue("name", inputName) &&
				inputProfiles[i].lookupValue("btn_A", btn_A) &&
				inputProfiles[i].lookupValue("btn_B", btn_B) &&
				inputProfiles[i].lookupValue("btn_X", btn_X) &&
				inputProfiles[i].lookupValue("btn_Y", btn_Y) &&
				inputProfiles[i].lookupValue("btn_DPAD_UP", btn_DPAD_UP) &&
				inputProfiles[i].lookupValue("btn_DPAD_DOWN", btn_DPAD_DOWN) &&
				inputProfiles[i].lookupValue("btn_DPAD_LEFT", btn_DPAD_LEFT) &&
				inputProfiles[i].lookupValue("btn_DPAD_RIGHT", btn_DPAD_RIGHT) &&
				inputProfiles[i].lookupValue("btn_L", btn_L) &&
				inputProfiles[i].lookupValue("btn_R", btn_R) &&
				inputProfiles[i].lookupValue("btn_ZL", btn_ZL) &&
				inputProfiles[i].lookupValue("btn_ZR", btn_ZR) &&
				inputProfiles[i].lookupValue("btn_START", btn_START) &&
				inputProfiles[i].lookupValue("btn_SELECT", btn_SELECT)))
			{
				continue;
			}

			std::map<char, FakeInput::Key> profileMap;
			profileMap[INPUT_PACKET_A] = KeyMapping[btn_A];
			profileMap[INPUT_PACKET_B] = KeyMapping[btn_B];
			profileMap[INPUT_PACKET_X] = KeyMapping[btn_X];
			profileMap[INPUT_PACKET_Y] = KeyMapping[btn_Y];
			profileMap[INPUT_PACKET_L] = KeyMapping[btn_L];
			profileMap[INPUT_PACKET_R] = KeyMapping[btn_R];
			profileMap[INPUT_PACKET_LZ] = KeyMapping[btn_ZL];
			profileMap[INPUT_PACKET_RZ] = KeyMapping[btn_ZR];
			profileMap[INPUT_PACKET_UP_D] = KeyMapping[btn_DPAD_UP];
			profileMap[INPUT_PACKET_DOWN_D] = KeyMapping[btn_DPAD_DOWN];
			profileMap[INPUT_PACKET_LEFT_D] = KeyMapping[btn_DPAD_LEFT];
			profileMap[INPUT_PACKET_RIGHT_D] = KeyMapping[btn_DPAD_RIGHT];
			profileMap[INPUT_PACKET_START] = KeyMapping[btn_START];
			profileMap[INPUT_PACKET_SELECT] = KeyMapping[btn_SELECT];

			MappingProfiles[inputName] = profileMap;
			if (i == 0) currentProfile = inputName;
		}
	}
	catch (const SettingNotFoundException& nfex)
	{
		std::cout << "Config file corrupted." << std::endl;
		return (EXIT_FAILURE);
	}

	if (currentProfile == "")
	{
		std::cout << "Need at least 1 input profile in config." << std::endl;
		return (EXIT_FAILURE);
	}

	//===========================================================================
	framgrabber = SL::Screen_Capture::CreateScreeCapture([&cfgMonitorIndex]()
			{
				auto mons = SL::Screen_Capture::GetMonitors();
				std::vector<SL::Screen_Capture::Monitor> selectedMonitor = std::vector<SL::Screen_Capture::Monitor>();
				if (cfgMonitorIndex >= mons.size()) cfgMonitorIndex = 0;
				selectedMonitor.push_back(mons[cfgMonitorIndex]);
				return selectedMonitor;
			}).onNewFrame([&](const SL::Screen_Capture::Image& img, const SL::Screen_Capture::Monitor& monitor)
				{
				if (conns.size() == 0) return;
				if (!isStreamingDesktop) return;
				//===========================================================================
				// check for active connect valid
				if (currentActiveConnect == -1)
				{
					if (conns.size() > 0) currentActiveConnect = 0;
					else return;
				}
				if (currentActiveConnect >= conns.size()) currentActiveConnect = 0;
				//===========================================================================
				// Split frame mode
				if (splitFrameMode)
				{
					// loop in each conn and check
					for (int i = 0; i < totalConn; i++)
					{
						// send this part of
						if (conns[i].received)
						{
							int currentPieceIdx = PopOutOnePiece(currentFrame);
							if (currentPieceIdx == -1) {
								CleanFrameCached(currentFrame);
								GetFramePieces(img);
								std::cout << "[New Frame] Frame: " << currentFrame << std::endl;
								currentPieceIdx = PopOutOnePiece(currentFrame);
							}
						
							conns[i].received = false;
							conns[i].currentFrame = currentFrame;
							conns[i].pieceIndex = currentPieceIdx;
							int size = (currentPieceIdx == totalConn - 1 ? framePieceLast : framePieceNormal) + 11;
							conns[i].conn->Send(framePieceCached[currentFrame][currentPieceIdx], size);
							std::cout << "[Peak] piece index: " << currentPieceIdx << " in frame: " << currentFrame << " size: " << size << std::endl;
						}
					}
					return;
				}
				//===========================================================================
				// Normal mode
				if (needWaitForReceived)
				{
					if (conns[currentActiveConnect].received)
					{
						conns[currentActiveConnect].received = false;
						SendImageDataToClient(img, conns[currentActiveConnect].conn);
						currentActiveConnect++;
					}
					else
					{
						currentWaitFrame++;
						if (currentWaitFrame > maxFrameToWait)
						{
							conns[currentActiveConnect].received = false;
							SendImageDataToClient(img, conns[currentActiveConnect].conn);
							currentActiveConnect++;
							currentWaitFrame = 0;
						}
					}
				}
				else
				{
					SendImageDataToClient(img, conns[currentActiveConnect].conn);
					currentActiveConnect++;
				}

				}).start_capturing();
	framgrabber.setFrameChangeInterval(std::chrono::milliseconds(streamFPS));//100 ms
	//framgrabber.pause();

	std::cout << "Select monitor: " << cfgMonitorIndex << std::endl;
	std::cout << "Init Screen Capture : Successfully" << std::endl;
	//===========================================================================
	// Socket server part
	//===========================================================================

	std::string addr = "0.0.0.0:" + std::to_string(cfgPort);
	std::cout << "Running on address: " << addr << std::endl;
	evpp::EventLoop loop;
	evpp::TCPServer server(&loop, addr, "NKStreamerServer", 6);
	server.SetMessageCallback([&](const evpp::TCPConnPtr& conn, evpp::Buffer* msg)
		{
			int idx = -1;
			for (int i = 0; i < conns.size(); i++)
			{
				if (conns[i].conn->name().compare(conn->name()) == 0)
				{
					idx = i;
					break;
				}
			}
			if (idx > -1)
			{
				//std::cout << "Recevied data: " << msg->length() << std::endl;
				//===========================================================================
				ProcessData(msg->data(), msg->length(), &conns[idx]);
				// need reset to empty msg after using.
				msg->Reset();
			}
			else
			{
				LOG_ERROR << "Unknow client : " << conn->name() << " send.";
			}
		});
	server.SetConnectionCallback([&](const evpp::TCPConnPtr& conn)
		{
			if (conn->IsConnected())
			{
				std::cout << ">> New client connected." << std::endl;

				//LOG_ERROR << "New Client : " << conn->name() << " Connected";
				//------------------------------
				// new connection
				ClientConnection client;
				client.conn = conn;
				client.received = true;
				client.isMainConnect = false;
				conns.push_back(client);
			}
			else
			{
				std::cout << ">> client disconnected." << std::endl;
				//LOG_ERROR << "Client : " << conn->name() << " Disconnected";
				//------------------------------
				// client disconect
				int idx = -1;
				for (int i = 0; i < conns.size(); i++)
				{
					if (conns[i].conn->name().compare(conn->name()) == 0)
					{
						idx = i;
						break;
					}
				}
				conns.erase(conns.begin() + idx);
				//------------------------------
				// stop stream when no connection
				if (conns.size() == 0)
				{
					isStreamingDesktop = false;
					framgrabber.pause();
				}
			}
		});
	server.Init();
	server.Start();

	std::cout << "Init Server : Successfully" << std::endl;
	std::cout << "-------------------------------------------" << std::endl;
	std::cout << "Please use one of those IP in your 3DS client to connect to server:" << std::endl;
	std::cout << "(normally it should be the last one)" << std::endl;
	std::cout << "-------------------------------------------" << std::endl;
	GetIPAddress();
	std::cout << "-------------------------------------------" << std::endl;
	std::cout << "Wait for connection..." << std::endl;

	loop.Run();

	return 0;
}

#include "winmain-inl.h"
