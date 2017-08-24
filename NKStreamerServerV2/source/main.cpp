#include <glog/config.h>
#include <evpp/tcp_server.h>
#include <evpp/buffer.h>
#include <evpp/tcp_conn.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>


#include <turbojpeg.h>
#include <webp/encode.h>


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

#define FPS_24 41;
#define FPS_30 33;
#define FPS_60 16;

/// Creates a bitmask from a bit number.
#define BIT(n) (1U<<(n))
enum
{
	KEY_A = BIT(0),       ///< A
	KEY_B = BIT(1),       ///< B
	KEY_SELECT = BIT(2),       ///< Select
	KEY_START = BIT(3),       ///< Start
	KEY_DRIGHT = BIT(4),       ///< D-Pad Right
	KEY_DLEFT = BIT(5),       ///< D-Pad Left
	KEY_DUP = BIT(6),       ///< D-Pad Up
	KEY_DDOWN = BIT(7),       ///< D-Pad Down
	KEY_R = BIT(8),       ///< R
	KEY_L = BIT(9),       ///< L
	KEY_X = BIT(10),      ///< X
	KEY_Y = BIT(11),      ///< Y
	KEY_ZL = BIT(14),      ///< ZL (New 3DS only)
	KEY_ZR = BIT(15),      ///< ZR (New 3DS only)
	KEY_TOUCH = BIT(20),      ///< Touch (Not actually provided by HID)
	KEY_CSTICK_RIGHT = BIT(24), ///< C-Stick Right (New 3DS only)
	KEY_CSTICK_LEFT = BIT(25), ///< C-Stick Left (New 3DS only)
	KEY_CSTICK_UP = BIT(26), ///< C-Stick Up (New 3DS only)
	KEY_CSTICK_DOWN = BIT(27), ///< C-Stick Down (New 3DS only)
	KEY_CPAD_RIGHT = BIT(28),   ///< Circle Pad Right
	KEY_CPAD_LEFT = BIT(29),   ///< Circle Pad Left
	KEY_CPAD_UP = BIT(30),   ///< Circle Pad Up
	KEY_CPAD_DOWN = BIT(31),   ///< Circle Pad Down

							   // Generic catch-all directions
							   KEY_UP = KEY_DUP | KEY_CPAD_UP,    ///< D-Pad Up or Circle Pad Up
							   KEY_DOWN = KEY_DDOWN | KEY_CPAD_DOWN,  ///< D-Pad Down or Circle Pad Down
							   KEY_LEFT = KEY_DLEFT | KEY_CPAD_LEFT,  ///< D-Pad Left or Circle Pad Left
							   KEY_RIGHT = KEY_DRIGHT | KEY_CPAD_RIGHT, ///< D-Pad Right or Circle Pad Right
};

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
int streamFPS = FPS_60;

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
bool splitFrameMode = true;

//-------------------------------------------------
// Mouse speed
//-------------------------------------------------
int mouseSpeed = 15;
int cpadDeadZone = 15;
int cpadMin = 0;
int cpadMax = 156;

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
bool isSentProfile = false;

std::mutex msgMutex;
Message* receivedMessage = nullptr;
Message* receivedMessageMain = nullptr;

SL::Screen_Capture::ScreenCaptureManager framgrabber;
int connectionCount;
std::vector<ClientConnection> conns;
ClientConnection mainConn;

std::map<int, std::vector<char*>> framePieceCached;
std::map<int, int> framePieceState;
int framePieceNormal = 0;
int framePieceLast = 0;
char totalConn;
//=================================================================================
// KEY MAPPING
//=================================================================================
struct MappingProfile
{
	std::string name = "";
	std::map<uint8_t, FakeInput::Key> mappings;
	bool mouseSupport = false;
};

static std::string currentProfile = "";
static std::map<std::string, MappingProfile> MappingProfiles;
static std::vector<std::string> ProfilesHolder;
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
	// encryption
	//-----------------------------------------------------------------------------
	cv::Mat rawImg = cv::Mat(cv::Size(Width(img), Height(img)), CV_8UC3, imgbuffer);
	cv::Mat scaledImg;
	cv::resize(rawImg, scaledImg, cv::Size(400, 240));
	//---------------------------------
	// JPEG
	long unsigned int imgSize = 0;
	unsigned char* _compressedImage = NULL;
	tjhandle _jpegCompressor = tjInitCompress();
	tjCompress2(_jpegCompressor, scaledImg.data, 400, 0, 240, TJPF_RGB, &_compressedImage, &imgSize, TJSAMP_444, imageQuality, TJFLAG_FASTDCT);
	if(imgSize == 0)
	{
		free(imgbuffer);
		tjDestroy(_jpegCompressor);
		tjFree(_compressedImage);
		scaledImg.release();
		rawImg.release();
		return;
	}
	//-----------------------------------------------------------------------------
	// build Message
	//-----------------------------------------------------------------------------
	// header size = 1 + 4 + 4 = 9 -> 1 byte code + 4 bytes content size + 4 bytes frame
	int totalSize = imgSize + 9;
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
	memcpy(data, _compressedImage, imgSize);
	//std::cout << "[Static Frame] index: " << currentFrame << " MSG size: " << totalSize << std::endl;
	//-----------------------------------------------------------------------------
	// Send message to all client
	//-----------------------------------------------------------------------------
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
	delete msg;
	//-----------------------------------------------------------------------------
	free(imgbuffer);
	tjDestroy(_jpegCompressor);
	tjFree(_compressedImage);
	scaledImg.release();
	rawImg.release();
}

//-----------------------------------------------------------------------------
// return False if frame not found or not finish
// return True if frame sent and erased
bool CleanFrameCached(int frameIndex)
{
	if (framePieceState.find(frameIndex) != framePieceState.end())
	{
		if (framePieceState[frameIndex] == totalConn)
		{
			for (char i = 0; i < totalConn; i++)
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
	for (char i = 0; i < totalConn; ++i)
	{
		//-----------------------------------------------------------------------------
		// build Message
		//-----------------------------------------------------------------------------
		// header size = 1 + 4 + 4 + 2 = 11 -> 1 byte code + 4 bytes content size + 4 bytes frame + 1 byte total part + 1 byte part index
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
	tjDestroy(_jpegCompressor);
	tjFree(_compressedImage);
	scaledImg.release();
	rawImg.release();
}

void ProcessInput(const Message* message, ClientConnection* clientConnect)
{
	int cursor = 0;
	//----------------------------------
	// down event
	uint32_t downEvent = (uint32_t)message->Content[cursor] |
		(uint32_t)message->Content[cursor + 1] << 8 |
		(uint32_t)message->Content[cursor + 2] << 16 |
		(uint32_t)message->Content[cursor + 3] << 24;
	cursor += 4;
	//----------------------------------
	// up event
	uint32_t upEvent = (uint32_t)message->Content[cursor] |
		(uint32_t)message->Content[cursor + 1] << 8 |
		(uint32_t)message->Content[cursor + 2] << 16 |
		(uint32_t)message->Content[cursor + 3] << 24;
	cursor += 4;
	//----------------------------------------------------------------
	for (uint8_t i = 0; i < 32; ++i)
	{
		if (downEvent & BIT(i))
		{
			if (MappingProfiles[currentProfile].mouseSupport && (i == 14 || i == 15))
			{
				if (i == 14)
					FakeInput::Mouse::pressButton(FakeInput::Mouse_Left);
				else
					FakeInput::Mouse::pressButton(FakeInput::Mouse_Right);
			}
			else
			{
				if (MappingProfiles[currentProfile].mappings.find(i) != MappingProfiles[currentProfile].mappings.end())
					FakeInput::Keyboard::pressKey(MappingProfiles[currentProfile].mappings[i]);
			}
		}
		if (upEvent & BIT(i))
		{
			if (MappingProfiles[currentProfile].mouseSupport && (i == 14 || i == 15))
			{
				if (i == 14)
					FakeInput::Mouse::releaseButton(FakeInput::Mouse_Left);
				else
					FakeInput::Mouse::releaseButton(FakeInput::Mouse_Right);
			}
			else {
				if (MappingProfiles[currentProfile].mappings.find(i) != MappingProfiles[currentProfile].mappings.end())
					FakeInput::Keyboard::releaseKey(MappingProfiles[currentProfile].mappings[i]);
			}
		}
	}

	if (MappingProfiles[currentProfile].mouseSupport) {
		//----------------------------------
		// circle pad X
		uint8_t cPadX = (uint8_t)message->Content[cursor];
		uint8_t cPadX_Dir = (uint8_t)message->Content[cursor + 1];
		cursor += 2;
		//----------------------------------
		// circle pad Y
		uint8_t cPadY = (uint8_t)message->Content[cursor];
		uint8_t cPadY_Dir = (uint8_t)message->Content[cursor + 1];
		cursor += 2;

		int finalDX = cPadX;
		int finalDY = cPadY;
		//----------------------------------------------------------------
		int mouseX = 0;
		int mouseY = 0;
		if (finalDX > cpadDeadZone)
		{
			int16_t moveX = finalDX - cpadDeadZone;
			if (moveX < 0) moveX = 0;
			float deltaX = float(moveX) / float(cpadMax - cpadDeadZone);
			int dir = cPadX_Dir > 0 ? 1 : -1;
			mouseX = deltaX * mouseSpeed * dir;
		}
		if (finalDY > cpadDeadZone)
		{
			int16_t moveY = finalDY - cpadDeadZone;
			if (moveY < 0) moveY = 0;
			float deltaY = float(moveY) / float(cpadMax - cpadDeadZone);
			int dir = cPadY_Dir > 0 ? -1 : 1;
			mouseY = deltaY * mouseSpeed * dir;
		}
		if (mouseX != 0 || mouseY != 0)
		{
			FakeInput::Mouse::move(mouseX, mouseY);
		}
	}
}

void ProcessMessage(const Message* message, ClientConnection* clientConnect)
{
	if (message == nullptr) return;

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
	case OPT_VIDEO_QUALITY_PACKET:
	{
		std::cout << "Client request to change video quality." << std::endl;
		int state = message->GetFirstInt();
		//----------------------------------
		std::cout << "Video quality from " << imageQuality << " to " << state << std::endl;
		imageQuality = state;
		//----------------------------------
		break;
	}
	case OPT_STREAM_MODE_PACKET:
	{
		std::cout << "Client request to change stream mode." << std::endl;
		int state = message->GetFirstInt();
		//----------------------------------
		if (state == 1)
		{
			std::cout << "Stream mode: Video Stream" << std::endl;
			needWaitForReceived = false;
			streamFPS = FPS_30;
			framgrabber.setFrameChangeInterval(std::chrono::milliseconds(streamFPS));//100 ms
		}
		else
		{
			std::cout << "Stream mode: Game Stream" << std::endl;
			needWaitForReceived = true;
			streamFPS = FPS_60;
			framgrabber.setFrameChangeInterval(std::chrono::milliseconds(streamFPS));//100 ms
		}
		//----------------------------------
		break;
	}
	case OPT_FPS_MODE_PACKET:
	{
		std::cout << "Client request to change fps mode." << std::endl;
		int state = message->GetFirstInt();
		//----------------------------------
		if (state == 0) streamFPS = FPS_60;
		if (state == 1) streamFPS = FPS_30;
		if (state == 2) streamFPS = FPS_24;
		framgrabber.setFrameChangeInterval(std::chrono::milliseconds(streamFPS));//100 ms
																				 //----------------------------------
		break;
	}
	case OPT_CHANGE_INPUT_PROFILE:
	{
		int newInput = message->GetFirstInt();
		if (newInput >= 0 && newInput < ProfilesHolder.size())
		{
			currentProfile = ProfilesHolder[newInput];
			std::cout << "Client request change input profile to: " << currentProfile << std::endl;
		}
		break;
	}
	case INPUT_PACKET_FRAME:
	{
		ProcessInput(message, clientConnect);
		break;
	}
	}
}

void ProcessData(const char* buffer, size_t lenght, ClientConnection* clientConnect)
{
	if (receivedMessage == nullptr) receivedMessage = new Message();
	int cutOffset = receivedMessage->ReadMessageFromData(buffer, lenght);
	if (cutOffset >= 0)
	{
		// Process new message
		ProcessMessage(receivedMessage, clientConnect);
		if (receivedMessage != nullptr) {
			delete receivedMessage;
			receivedMessage = nullptr;
		}
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
}
void ProcessMainData(const char* buffer, size_t lenght, ClientConnection* clientConnect)
{
	if (receivedMessageMain == nullptr) receivedMessageMain = new Message();
	int cutOffset = receivedMessageMain->ReadMessageFromData(buffer, lenght);
	if (cutOffset >= 0)
	{
		// Process new message
		ProcessMessage(receivedMessageMain, clientConnect);
		if (receivedMessageMain != nullptr) {
			delete receivedMessageMain;
			receivedMessageMain = nullptr;
		}
		if (cutOffset > 0)
		{
			// continue process by buffer.
			int sizeLeft = lenght - cutOffset;
			char* bufferLeft = (char*)malloc(sizeLeft);
			memcpy(bufferLeft, buffer + cutOffset, sizeLeft);

			ProcessMainData(bufferLeft, sizeLeft, clientConnect);

			free(bufferLeft);
		}
	}
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

		if (!serverCfg.lookupValue("mouse_speed", mouseSpeed) || mouseSpeed == -1) mouseSpeed = 15;
		if (!serverCfg.lookupValue("circle_pad_deadzone", cpadDeadZone) || cpadDeadZone == -1) cpadDeadZone = 15;

		const Setting& root = serverCfg.getRoot();
		//===============================
		// load input profiles
		const Setting& inputProfiles = root["input"];
		int inputCount = inputProfiles.getLength();
		for (int i = 0; i < inputCount; ++i)
		{
			MappingProfile profile;

			std::string inputName;
			std::string btn_A, btn_B, btn_X, btn_Y;
			std::string btn_DPAD_UP, btn_DPAD_DOWN, btn_DPAD_LEFT, btn_DPAD_RIGHT;
			std::string btn_L, btn_R, btn_ZL, btn_ZR;
			std::string btn_START, btn_SELECT;
			bool mouseSupport;

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
				inputProfiles[i].lookupValue("btn_START", btn_START) &&
				inputProfiles[i].lookupValue("btn_SELECT", btn_SELECT) &&
				inputProfiles[i].lookupValue("mouse_support", mouseSupport)))
			{
				continue;
			}

			profile.name = inputName;
			profile.mouseSupport = mouseSupport;
			profile.mappings[0] = KeyMapping[btn_A];
			profile.mappings[1] = KeyMapping[btn_B];
			profile.mappings[10] = KeyMapping[btn_X];
			profile.mappings[11] = KeyMapping[btn_Y];
			profile.mappings[9] = KeyMapping[btn_L];
			profile.mappings[8] = KeyMapping[btn_R];
			profile.mappings[6] = KeyMapping[btn_DPAD_UP];
			profile.mappings[7] = KeyMapping[btn_DPAD_DOWN];
			profile.mappings[5] = KeyMapping[btn_DPAD_LEFT];
			profile.mappings[4] = KeyMapping[btn_DPAD_RIGHT];
			profile.mappings[3] = KeyMapping[btn_START];
			profile.mappings[2] = KeyMapping[btn_SELECT];

			// if not using cpad as mouse then must set it
			if (!mouseSupport)
			{
				profile.mappings[30] = KeyMapping[btn_DPAD_UP];
				profile.mappings[31] = KeyMapping[btn_DPAD_DOWN];
				profile.mappings[29] = KeyMapping[btn_DPAD_LEFT];
				profile.mappings[28] = KeyMapping[btn_DPAD_RIGHT];
				profile.mappings[14] = KeyMapping[btn_ZL];
				profile.mappings[15] = KeyMapping[btn_ZR];
			}

			ProfilesHolder.push_back(inputName);
			MappingProfiles[inputName] = profile;
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
			for (int i = 0; i < conns.size(); i++)
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
	framgrabber.pause();

	std::cout << "Select monitor: " << cfgMonitorIndex << std::endl;
	std::cout << "Init Screen Capture : Successfully" << std::endl;
	//===========================================================================
	// Socket server part
	//===========================================================================

	std::string addr = "0.0.0.0:" + std::to_string(cfgPort);
	std::cout << "Running on address: " << addr << std::endl;
	evpp::EventLoop loop;
	evpp::TCPServer server(&loop, addr, "NKStreamerServer", cfgThreadNum);
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
		//-------------------------------
		// conn is piece worker
		if (idx > -1)
		{
			//std::cout << "Recv Data On Thread ID: " << std::this_thread::get_id() << std::endl;
			//===========================================================================
			msgMutex.lock();
			ProcessData(msg->data(), msg->length(), &conns[idx]);
			msgMutex.unlock();
			// need reset to empty msg after using.
			msg->Reset();
		}
		else
		{
			msgMutex.lock();
			// conn is main
			ProcessMainData(msg->data(), msg->length(), &mainConn);
			msgMutex.unlock();
			msg->Reset();
		}
	});
	server.SetConnectionCallback([&](const evpp::TCPConnPtr& conn)
	{
		if (conn->IsConnected())
		{
			if(mainConn.conn == nullptr)
			{
				mainConn.conn = conn;
				mainConn.received = true;
				mainConn.isMainConnect = true;
				//--------------------------------------

				if (!isSentProfile)
				{
					isSentProfile = true;
					//--------------------------------------
					int totalSize = 5;
					static std::map<std::string, MappingProfile>::iterator iter;
					for (iter = MappingProfiles.begin(); iter != MappingProfiles.end(); ++iter)
						totalSize += iter->first.size() + 1;
					//--------------------------------------
					Message* msg = new Message();
					msg->MessageCode = OPT_RECEIVE_INPUT_PROFILE;
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

					int cursor = 0;
					for (iter = MappingProfiles.begin(); iter != MappingProfiles.end(); ++iter)
					{
						uint8_t cs = iter->first.size();
						*(data + cursor) = cs;
						memcpy(data + cursor + 1, iter->first.c_str(), cs);
						cursor += cs + 1;
					}
					conn->Send(msgContent, totalSize);
				}
			}else
			{
				ClientConnection client;
				client.conn = conn;
				client.received = true;
				client.isMainConnect = false;
				conns.push_back(client);
			}
			
		}
		else
		{
			//------------------------------
			// stop stream when no connection
			if (conns.size() > 0)
			{
				std::vector<ClientConnection>().swap(conns);
				isSentProfile = false;
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
