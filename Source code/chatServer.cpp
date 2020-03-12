#include "stdafx.h"

chatServer::chatServer(const char *_configData)
{
	loadConfigData(_configData);

	Monitor = new monitorClient(monitorJsonName.c_str());

	setEncodeKey(Code, Key1, Key2);
	map = new Map(mapJsonName.c_str(), this);

	terminateFlag = false;

	playerList = new std::map<unsigned __int64, player*>;

	HANDLE hThread;
	hThread = (HANDLE)_beginthreadex(NULL, 0, monitorThread, (LPVOID)this, 0, 0);
	CloseHandle(hThread);
	Start(IP, Port, workerThreadCount, nagleOpt, maxClient);
}

chatServer::~chatServer()
{
	Stop();

	delete Monitor;
	delete playerList;
}

void chatServer::loadConfigData(const char *_configData)
{
	rapidjson::Document Doc;
	Doc.Parse(_configData);

	rapidjson::Value &Val = Doc["Chat_Server"];

	strcpy_s(IP, 16, Val["Server_ip"].GetString());
	Port = Val["Server_port"].GetUint();
	workerThreadCount = Val["Workerthread_count"].GetUint();
	maxClient = Val["Max_client_count"].GetUint();
	nagleOpt = Val["Nagle_option"].GetBool();

	rapidjson::Value &Key = Doc["Encryption_key"];
	assert(arry.IsArry());
	Code = (char)Key[0].GetUint();
	Key1 = (char)Key[1].GetUint();
	Key2 = (char)Key[2].GetUint();

	rapidjson::Value &monitorName = Doc["MonitorFileName"];
	monitorJsonName = monitorName.GetString();

	rapidjson::Value &mapName = Doc["MapFileName"];
	mapJsonName = mapName.GetString();
}

void chatServer::acquireLock()
{
	playerListLock.lock();
}

void chatServer::releaseLock()
{
	playerListLock.unlock();
}

unsigned __stdcall chatServer::monitorThread(LPVOID _data)
{
	chatServer *server = (chatServer*)_data;

	SYSTEMTIME stNowTime;
	GetLocalTime(&stNowTime);
	char startTime[100];
	std::snprintf(startTime, 100, "Start Time : %d.%d.%d %02d:%02d:%02d", stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay,
		stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);

	while (!server->terminateFlag)
	{

		printf("---------------------------------------------------\n");
		printf(" Chatting Server (Ver1.0)\n");
		printf("---------------------------------------------------\n");
		printf(" Start time : %s \n", startTime);
		printf(" Pause : P | Stop : S | Quit : Q\n");
		printf("---------------------------------------------------\n");
		printf("[Config]--------------------------------------------\n");
		printf("   Limit client count : %d  \n", server->maxClient);
		printf("[Server]------------------------------------------\n");
		printf("   Accept Total : %d  | Accept TPS : %d\n", server->getAcceptTotal(), server->getAcceptTPS());
		printf("   Recv TPS : %d  | Send TPS %d\n", server->getRecvTPS(), server->getSendTPS());
		printf("[Contents]-----------------------------------------\n");
		printf("   playerList count : %d  |  Now connection : %d \n", server->playerList->size(), server->GetClientCount());
		printf("   playerPool Alloc : %d  /  used : %d\n", server->playerPool.getAllocCount(), server->playerPool.getUsedCount());
		printf("   Sbuf Alloc : %d  |  Used : %d\n", Sbuf::pool->getAllocCount(), Sbuf::pool->getUsedCount());
		printf("\n\n");
		server->TPS();
		server->Monitor->sendData(3, server->getAcceptTPS(), server->getRecvTPS(), server->getSendTPS());
		server->setTPS();

		Sleep(1000);
	}

	return 0;
}
// Recv function
void chatServer::recv_connectReq(unsigned __int64 _index, Sbuf *_buf)
{
	short	clientType;

	*_buf >> clientType;

	proc_Connect(_index, clientType);
}

void chatServer::recv_playerMove(unsigned __int64 _index, Sbuf *_buf)
{
	int	xPos, yPos;

	*_buf >> xPos;
	*_buf >> yPos;

	player *User = get_playerData(_index);
	if (!User)
		clientShutdown(_index);
	proc_Move(User, xPos, yPos);
	User->releaseUseCount();
}

void chatServer::recv_playerChMove(unsigned __int64 _index, Sbuf *_buf)
{
	char	prevCH, destCH;

	*_buf >> destCH;

	player *User = get_playerData(_index);
	if (!User)
		clientShutdown(_index);
	proc_chMove(User, destCH);
	User->releaseUseCount();
}

void chatServer::recv_Chatting(unsigned __int64 _index, Sbuf *_buf)
{
	WCHAR Data[50];
	int dataSize = 0;

	unsigned __int64 playerCode;
	*_buf >> playerCode;
	*_buf >> dataSize;

	if (dataSize > 100 || dataSize <= 0)
		clientShutdown(_index);

	_buf->pop((char*)&Data, dataSize);


	player *User = get_playerData(_index);
	if (!User)
	{
		clientShutdown(_index);
		return;
	}
	if (User->getPlayerCode() != playerCode)
	{
		clientShutdown(_index);
		User->releaseUseCount();
		return;
	}
	proc_Chatting(User, Data, dataSize);
	User->releaseUseCount();
}

void chatServer::recv_dataReq(unsigned __int64 _Index)
{
	proc_dataReq(_Index);
}

void chatServer::proc_Connect(unsigned __int64 _playerCode, short _clientType)
{
	char Result = false;
	player *User = get_playerData(_playerCode);
	if (User)
	{
		Result = true;
		User->setType((chatProtocol::chatClientType)_clientType);
		User->releaseUseCount();
	}
	Sbuf *buf = packet_Login(Result, _playerCode);
	SendPacket(_playerCode, buf);
	buf->Free();
}

void chatServer::proc_Move(player *_User, int _destX, int _destY)
{
	map->updateCH(_User, _User->getCH(), _User->getMapNo(), _destX, _destY);
	_User->setTile(_destX, _destY);
	Sbuf *buf = NULL;
	buf = packet_moveResult(_User->getPlayerCode(), _User->getXTile(), _User->getYTile());
	map->sendChatAround(_User->getCH(), _User->getMapNo(),_User->getXTile(), _User->getYTile(), buf);
	buf->Free();
}

void chatServer::proc_chMove(player *_User, int _destCH)
{
	map->updateCH(_User, _destCH, _User->getMapNo(), _User->getXTile(), _User->getYTile());
	_User->setCH(_destCH);
	Sbuf *buf = NULL;
	buf = packet_chMoveResult(_User->getPlayerCode(), _User->getCH());
	SendPacket(_User->getPlayerCode(), buf);
	buf->Free();
}

void chatServer::proc_Chatting(player *_User, WCHAR *_data, int _dataSize)
{
	Sbuf *buf = packet_Chatting(_User->getPlayerCode(), _data, _dataSize);
	map->sendChatAround(_User->getCH(), _User->getMapNo(), _User->getXTile(), _User->getYTile(), buf);
	buf->Free();
}

void chatServer::proc_dataReq(unsigned __int64 _playerCode)
{
	player *User = get_playerData(_playerCode);
	if (User)
	{
		Sbuf *buf = packet_dataRes(_playerCode);
		SendPacket(_playerCode, buf);
		buf->Free();
		positionData Data = get_Position();
		map->updateCH(User, Data.CH, Data.mapNo, Data.xTile, Data.yTile);
		set_Position(User, Data);
		User->releaseUseCount();
	}
}

// Packet function

Sbuf* chatServer::packet_Login(char _Result, unsigned __int64 _playerCode)
{
	// short		Type;
	//	char		Result;			1 : 허락, 0 : 거절
	// unsigned __int64	playerCode;
	Sbuf *buf = Sbuf::Alloc();
	*buf << (short)chatProtocol::Protocol::s2c_Login_Res;
	*buf << _Result;
	*buf << _playerCode;

	return buf;
}

Sbuf* chatServer::packet_dataRes(unsigned __int64 _playerCode)
{
	// short		Type;
	//	char		Result;			1 : 허락, 0 : 거절
	// unsigned __int64	playerCode;
	Sbuf *buf = Sbuf::Alloc();
	*buf << (short)chatProtocol::Protocol::s2c_playerData_Res;
	*buf << _playerCode;

	return buf;
}

Sbuf* chatServer::packet_moveResult(unsigned __int64 _playerCode, int _xPos, int _yPos)
{
	Sbuf *buf = Sbuf::Alloc();
	*buf << (short)chatProtocol::Protocol::s2c_playerMove;
	*buf << _playerCode;
	*buf << _xPos;
	*buf << _yPos;
	return buf;
}

Sbuf* chatServer::packet_chMoveResult(unsigned __int64 _playerCode, int _CH)
{
	Sbuf *buf = Sbuf::Alloc();
	*buf << (short)chatProtocol::Protocol::s2c_playerCHChange;
	*buf << _playerCode;
	*buf << _CH;

	return buf;
}

Sbuf* chatServer::packet_Chatting(unsigned __int64 _playerCode, WCHAR *data, int _dataSize)
{
	Sbuf *buf = Sbuf::Alloc();

	*buf << (short)chatProtocol::Protocol::s2c_Chatting;
	*buf << _playerCode;
	*buf << _dataSize;
	buf->push((char*)data, _dataSize);
	return buf;
}

Sbuf* chatServer::packet_createPlayer(unsigned __int64 _playerCode, int _CH, int _mapNo, int _xTile, int _yTile)
{
	// unsigned __int64		playerCode			Player 구분 코드 (sessionKey)
	//	bool						controlFlag;			0 : 다른 플레이어 캐릭터, 1 : 직접 컨트롤 하는 캐릭터
	//	int							chNumber;				채널 번호
	//	int							xPos, yPos;			Tile 좌표

	Sbuf *buf = Sbuf::Alloc();
	*buf << (short)chatProtocol::Protocol::s2c_createPlayer;
	*buf << _playerCode;
	*buf << _CH;
	*buf << _mapNo;
	*buf << _xTile;
	*buf << _yTile;

	return buf;
}

Sbuf* chatServer::packet_deletePlayer(unsigned __int64 _Index)
{
	Sbuf *buf = Sbuf::Alloc();
	*buf << (short)chatProtocol::Protocol::s2c_deletePlayer;
	*buf << _Index;
	return buf;
}

player* chatServer::get_playerData(unsigned __int64 _playerCode)
{
	player* retPtr = nullptr;
	acquireLock();
	std::map<unsigned __int64, player*>::iterator Iter = playerList->find(_playerCode);
	if (Iter == playerList->end())
		retPtr = nullptr;
	else
	{
		retPtr = Iter->second;
		retPtr->acquireUseCount();
	}
	releaseLock();
	return retPtr;
}

positionData chatServer::get_Position()
{
	positionData Data;

	Data.CH = rand() % 1;
	Data.mapNo = 0;
	Data.xTile = rand() % 30;
	Data.yTile = rand() % 30;

	return Data;
}

void chatServer::set_Position(player *_User, positionData _Data)
{
	_User->setCH(_Data.CH);
	_User->setMapNo(_Data.mapNo);
	_User->setTile(_Data.xTile, _Data.yTile);
}

void chatServer::sendMsg(unsigned __int64 _index, Sbuf *_buf)
{
	SendPacket(_index, _buf);
}

void chatServer::TPS()
{
	Monitor->TPS();
}

// Virtual function
void chatServer::OnClientJoin(unsigned __int64 _index)
{
	player *User = playerPool.Alloc();
	User->setInit();
	User->setPlayerCode(_index);

	acquireLock();
	playerList->insert(std::pair<unsigned __int64, player*>(_index, User));
	releaseLock();
}

void chatServer::OnClientLeave(unsigned __int64 _index)
{
	acquireLock();
	std::map<unsigned __int64, player*>::iterator Iter = playerList->find(_index);
	if (Iter != playerList->end())
	{
		player *User = Iter->second;
		Iter->second->checkUseCount();
		map->updateCH(User, -1, -1, -1, -1);
		User->setInit();
		playerList->erase(Iter);
		playerPool.Free(User);
	}
	releaseLock();
}

bool chatServer::OnConnectionRequest(char *_ip, unsigned int _port)
{

	return true;
}

void chatServer::OnRecv(unsigned __int64 _index, Sbuf *_buf)
{
	short Type;
	*_buf >> Type;
	switch (Type)
	{
	case chatProtocol::c2s_Login_Req:
		recv_connectReq(_index, _buf);
		break;
	case chatProtocol::c2s_playerData_Req:
		recv_dataReq(_index);
		break;
	case chatProtocol::c2s_playerMove:
		recv_playerMove(_index, _buf);
		break;
	case chatProtocol::c2s_playerCHChange:
		recv_playerChMove(_index, _buf);
		break;
	case chatProtocol::c2s_Chatting:
		recv_Chatting(_index, _buf);
		break;
	default:
		break;
	}
}

void chatServer::OnError(int _errorCode, WCHAR *_string)
{
	printf("Error code : %d] %s\n", _errorCode, _string);
}

