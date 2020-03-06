#pragma once

struct positionData
{
	int CH, mapNo, xTile, yTile;
};

class player
{
private:
	unsigned __int64		playerCode;

	chatProtocol::chatClientType Type;

	int		CH, mapNo;
	int		xTile, yTile;

	volatile LONG useCount = 0;

public:
	unsigned __int64 getPlayerCode() { return playerCode; }
	chatProtocol::chatClientType getType() { return Type; }
	int getCH() { return CH; }
	int getMapNo() { return mapNo; }
	int getXTile() { return xTile; }
	int getYTile() { return yTile; }
	void	getPosition(int &_xTile, int &_yTile) { _xTile = xTile; _yTile = yTile; }

	void setType(chatProtocol::chatClientType _Type) { Type = _Type; }
	void setTile(int _xTile, int _yTile) { xTile = _xTile; yTile = _yTile; }
	void setCH(int _CH) { CH = _CH; }
	void setMapNo(int _mapNo) { mapNo = _mapNo; }
	void setPlayerCode(unsigned __int64 _playerCode) { playerCode = _playerCode; }
	void setInit() { playerCode = -1; CH = -1; mapNo = -1; xTile = -1; yTile = -1; }

	void acquireUseCount(void) { useCount += 1; }
	void releaseUseCount(void) { useCount -= 1; }
	void checkUseCount(void) { while (useCount != 0) { Sleep(1); } }	// spin lock
};

class chatServer : protected IOCPServer
{
private:
	//Network soruce
	char IP[16];
	unsigned short Port;
	unsigned short workerThreadCount;
	bool	nagleOpt;
	unsigned int maxClient;

	// Monitor config file name
	std::string monitorJsonName;
	std::string mapJsonName;

	monitorClient *Monitor;

	//Encryption key
	BYTE Code, Key1, Key2;

	// Control value
	bool	terminateFlag;

	std::map<unsigned __int64, player*> *playerList;
	memoryPool<player> playerPool;
	Map *map;

	SRWLOCK playerListLock;

private:
	void loadConfigData(const char *_configData);

	// Logic proccess function
	void	recv_connectReq(unsigned __int64 _index, Sbuf *_buf);
	void	recv_playerMove(unsigned __int64 _index, Sbuf *_buf);
	void	recv_playerChMove(unsigned __int64 _index, Sbuf *_buf);
	void	recv_Chatting(unsigned __int64 _index, Sbuf *_buf);
	void	recv_dataReq(unsigned __int64 _Index);

	void	proc_Connect(unsigned __int64 _playerCode, short _clientType);
	void	proc_Move(player *_User, int _destX, int _destY);
	void	proc_chMove(player *_User, int _destCH);
	void	proc_Chatting(player *_User, WCHAR *_data, int _dataSize);
	void	proc_dataReq(unsigned __int64 _playerCode);

	Sbuf* packet_Login(char _Result, unsigned __int64 _playerCode);
	Sbuf* packet_dataRes(unsigned __int64 _playerCode);
	Sbuf*	 packet_moveResult(unsigned __int64 _playerCode, int _xPos = -1, int _yPos = -1);
	Sbuf*	 packet_chMoveResult(unsigned __int64 _playerCode, int _CH);
	Sbuf* packet_Chatting(unsigned __int64 _playerCode, WCHAR *_data, int _dataSize);


	player*	get_playerData(unsigned __int64 _playerCode);
	positionData	get_Position();
	void	set_Position(player* _User, positionData _Data);

public:
	chatServer(const char *_configData);
	~chatServer();

public:
	static unsigned __stdcall monitorThread(LPVOID _data);

	void sendMsg(unsigned __int64 _index, Sbuf *_buf);

	Sbuf* packet_createPlayer(unsigned __int64 _playerCode, int _CH, int _mapNo, int _xTile, int _yTile);
	Sbuf* packet_deletePlayer(unsigned __int64 _Index);

public:
	void OnClientJoin(unsigned __int64 _index);							// accept -> 접속처리 완료 후 호출
	void OnClientLeave(unsigned __int64 _index);						// disconnect 후 호출
	bool OnConnectionRequest(char *_ip, unsigned int _port);	// accept 후 [false : 클라이언트 거부 / true : 접속 허용]
	void OnRecv(unsigned __int64 _index, Sbuf *_buf);				// 수신 완료 후
	void OnError(int _errorCode, WCHAR *_string);						// 오류메세지 전송
};