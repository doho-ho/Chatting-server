#pragma once

class chatServer;
class player;

struct tileCollection
{
	int Count;
	std::array<std::array<int, 2>, 25> Collection; // Y Tile, X Tile
};

struct directionData
{
	int prevCH, destCH;
	int prevMapNo, destMapNo;
	int prevXTile, destXTile;
	int prevYTile, destYTile;
};

struct Tile
{
	std::vector<unsigned __int64> Users;
};

class tileMap
{
private:
	Tile **Map;
	
	chatServer *Server;
	int xSize, ySize;
	int mapNum;
	int chNum;

private:
	void deletePlayer(unsigned __int64 _playerCode, int _xTile, int _yTile);
	void insertPlayer(unsigned __int64 _playerCode, int _xTile, int _yTile);
	
	tileCollection getAroundTile(int _xTile, int _yTile);
	tileCollection getCalTile(int _prevXTile, int _prevYTile, int _destXTile, int _destYTile, bool _insertFlag);

	void sendUpdateMsg(unsigned __int64 _playerCode, int _prevXTile, int _prevYTile, int _destXTile, int _destYTile);
	void sendMsg(unsigned __int64 _playerCode, tileCollection _Collection, bool _insertFlag, Sbuf *_buf);

public:
	~tileMap();
	void updateTile(unsigned __int64 _playerCode, int _prevXTile, int _prevYTile, int _destXTile, int _destYTile);
	void deleteTile(unsigned __int64 _playerCode, int _xTile, int _yTile);
	void insertTile(unsigned __int64 _playerCode, int _xTile, int _yTile);

	void sendPacket(unsigned __int64 _playerCode, directionData _Data, bool _insertFlag);

	void setConfig(int _xSize, int _ySize);
	void setServer(chatServer *_Server);
	void setCHNum(int _chNum);
	void setMapNo(int _mapNo);

	void sendChatAround(int _xTile, int _yTile, Sbuf *_buf);
};

class Channel
{
private:
	tileMap *Map;

	int mapCount;
	int chNum;
public:
	Channel() {};
	~Channel() { delete[] Map; };

	void setConfig(const char *_mapConfig);
	void setServer(chatServer *_Server);
	void setCHNum(int _chNum);

	void updateMap(unsigned __int64 _playerCode, directionData _Data);
	void deleteMap(unsigned __int64 _playerCode, int _mapNo, int _xTile, int _yTile);
	void insertMap(unsigned __int64 _playerCode, int _mapNo, int _xTile, int _yTile);

	void sendChatAround(int _mapNo, int _xTile, int _yTile, Sbuf *_buf);
	void sendPacket(unsigned __int64 _playerCode, int _mapNo, directionData _Data, bool _insertFlag);

	void acquireLock(int _mapNo);
	void releaseLock(int _mapNo);

	
};

class Map
{
private:
	Channel *CH;

	int channelSize;

private:
	void loadConfig(const char *_mapConfig);

	directionData setDirection(int _prevCH, int _prevMapNo, int _prevXTile, int _prevYTile,
		int _destCH, int _destMapNo, int _destXTile, int _destYTile);
public:
	Map(const char *_mapConfigFileName, chatServer *_Server);
	~Map() { delete[] CH; };

	void updateCH(player *_User, int _CH, int _mapNo, int _xTile, int _yTile);
	void sendChatAround(int _CH, int _mapNo, int _xTile, int _yTile, Sbuf *_buf);
};