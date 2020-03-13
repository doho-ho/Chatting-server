#include "stdafx.h"

Map::Map(const char *_mapConfigFileName, chatServer *_Server)
{
	const char *configData = loadFile(_mapConfigFileName);

	loadConfig(configData);

	CH = new Channel[channelSize];
	for (int Count = 0; Count < channelSize; ++Count)
	{
		CH[Count].setConfig(configData);
		CH[Count].setServer(_Server);
		CH[Count].setCHNum(Count);
	}
}

void Map::loadConfig(const char *_mapConfig)
{
	rapidjson::Document Doc;
	Doc.Parse(_mapConfig);

	rapidjson::Value &Val = Doc["MapConfig"];

	channelSize = Val["ChannelSize"].GetInt();
}

directionData Map::setDirection(int _prevCH, int _prevMapNo, int _prevXTile, int _prevYTile,
	int _destCH, int _destMapNo, int _destXTile, int _destYTile)
{
	directionData Data;
	Data.prevCH = _prevCH;
	Data.prevMapNo = _prevMapNo;
	Data.prevXTile = _prevXTile;
	Data.prevYTile = _prevYTile;
	Data.destCH = _destCH;
	Data.destMapNo = _destMapNo;
	Data.destXTile = _destXTile;
	Data.destYTile = _destYTile;

	return Data;
}

void Map::updateCH(player *_User, int _CH, int _mapNo, int _xTile, int _yTile)
{
	if (!_User) return;
	if (_CH > channelSize || _CH < -1) return;

	if (_User->getCH() == -1 && _CH == -1) return;

	directionData Data = setDirection(_User->getCH(), _User->getMapNo(), _User->getXTile(), _User->getYTile(),
		_CH, _mapNo, _xTile, _yTile);

	unsigned __int64 playerCode = _User->getPlayerCode();
	if (Data.prevCH != Data.destCH)
	{
		if (Data.prevCH != -1)
			CH[Data.prevCH].deleteMap(playerCode, Data.prevMapNo, Data.prevXTile, Data.prevYTile);

		if (Data.destCH != -1)
			CH[Data.destCH].insertMap(playerCode, Data.destMapNo, Data.destXTile, Data.destYTile);

		if (Data.prevCH != -1)
			CH[Data.prevCH].sendPacket(playerCode, Data.prevMapNo, Data, false);

		if (Data.destCH != -1)
			CH[Data.destCH].sendPacket(playerCode, Data.destMapNo, Data, true);

	}
	else
		CH[Data.destCH].updateMap(playerCode, Data);
}

void Map::sendChatAround(int _CH, int _mapNo, int _xTile, int _yTile, Sbuf *_buf)
{
	if (!_buf) return;
	if (_CH > channelSize || _CH < 0) return;
	CH[_CH].sendChatAround(_mapNo, _xTile, _yTile, _buf);
}

void Channel::setConfig(const char *_mapConfig)
{
	rapidjson::Document Doc;
	Doc.Parse(_mapConfig);

	rapidjson::Value &dataList = Doc["MapData"];
	assert(dataList.IsArry());
	mapCount = dataList.Size();
	
	Map = new tileMap[mapCount];

	for (rapidjson::SizeType i = 0; i < mapCount; i++)
	{
		Map[i].setConfig(dataList[i]["xTile"].GetInt(), dataList[i]["yTile"].GetInt());
		Map[i].setCHNum(chNum);
		Map[i].setMapNo(i);
	}
}

void Channel::setServer(chatServer *_Server)
{
	for (int i = 0; i < mapCount; ++i)
		Map[i].setServer(_Server);
}

void Channel::setCHNum(int _chNum)
{
	chNum = _chNum;
	for (int i = 0; i < mapCount; ++i)
		Map[i].setCHNum(_chNum);
}

void Channel::updateMap(unsigned __int64 _playerCode, directionData _Data)
{
	if (_Data.prevMapNo > mapCount || _Data.prevMapNo < -1) return;
	if (_Data.destMapNo > mapCount || _Data.destMapNo < -1) return;

	if (_Data.prevMapNo != _Data.destMapNo)
	{
		Map[_Data.prevMapNo].deleteTile(_playerCode, _Data.prevXTile, _Data.prevYTile);
		Map[_Data.destMapNo].insertTile(_playerCode, _Data.destXTile, _Data.destXTile);

		Map[_Data.prevMapNo].sendPacket(_playerCode, _Data, false);
		Map[_Data.destMapNo].sendPacket(_playerCode, _Data, true);
	}
	else
		Map[_Data.destMapNo].updateTile(_playerCode, _Data.prevXTile, _Data.prevYTile, _Data.destXTile, _Data.destYTile);
}

void Channel::deleteMap(unsigned __int64 _playerCode, int _mapNo, int _xTile, int _yTile)
{
	if (_mapNo > mapCount || _mapNo < 0) return;
	Map[_mapNo].deleteTile(_playerCode, _xTile, _yTile);
}

void Channel::insertMap(unsigned __int64 _playerCode, int _mapNo, int _xTile, int _yTile)
{
	if (_mapNo > mapCount || _mapNo < 0) return;
	Map[_mapNo].insertTile(_playerCode, _xTile, _yTile);
}

void Channel::sendChatAround(int _mapNo, int _xTile, int _yTile, Sbuf *_buf)
{
	if (_mapNo > mapCount || _mapNo < 0) return;
	Map[_mapNo].sendChatAround(_xTile, _yTile, _buf);
}

void Channel::sendPacket(unsigned __int64 _playerCode, int _mapNo, directionData _Data, bool _insertFlag)
{
	Map[_mapNo].sendPacket(_playerCode, _Data, _insertFlag);
}

tileMap::~tileMap()
{
	for (int i = 0; i < ySize; ++i)
	{
		for (int j = 0; j < xSize; ++j)
		{
			Map[i][j].Users.clear();
		}
		delete[] Map[i];
	}
	delete[] Map;
}

void tileMap::deletePlayer(unsigned __int64 _playerCode, int _xTile, int _yTile)
{
	if (_xTile == -1 || _yTile == -1) return;
	std::vector<unsigned __int64>::iterator Iter = Map[_yTile][_xTile].Users.begin();
	std::vector<unsigned __int64>::iterator endIter = Map[_yTile][_xTile].Users.end();
	for (Iter ; Iter != endIter;++Iter)
	{
		if (*Iter == _playerCode)
		{
			Map[_yTile][_xTile].Users.erase(Iter);
			break;
		}
	}
}

void tileMap::insertPlayer(unsigned __int64 _playerCode, int _xTile, int _yTile)
{
	if (_xTile == -1 || _yTile == -1) return;
	Map[_yTile][_xTile].Users.push_back(_playerCode);	
}

tileCollection tileMap::getAroundTile(int _xTile, int _yTile)
{
	tileCollection Collection;
	Collection.Count = 0;
	if (_xTile == -1 || _yTile == -1) 
		return Collection;

	int minX = 0, maxX = 0, minY = 0, maxY = 0;
	int Width = 0, Height = 0;
	int xTile, yTile;

	minX = max(0, _xTile - 2);
	maxX = min((xSize - 1), _xTile + 2);
	minY = max(0, _yTile - 2);
	maxY = min((ySize - 1), _yTile + 2);

	yTile = minY;
	xTile = minX;
	for (Height; Height < 5; Height++)
	{
		for (Width = 0; Width < 5; Width++)
		{
			Collection.Collection[Collection.Count][0] = yTile;
			Collection.Collection[Collection.Count][1] = xTile;
			Collection.Count++;
			xTile++;
			if (xTile > maxX)
				break;
		}
		yTile++;
		if (yTile > maxY)
			break;
		xTile = minX;
	}

	return Collection;
}

tileCollection tileMap::getCalTile(int _prevXTile, int _prevYTile, int _destXTile, int _destYTile, bool _insertFlag)
{
	//_insertFlag is false -> Delete mode
	tileCollection Base, Comparison;
	tileCollection retVal;
	retVal.Count = 0;
	if (_insertFlag)
	{
		Base = getAroundTile(_destXTile,_destYTile);
		Comparison = getAroundTile(_prevXTile, _prevYTile);
	}
	else
	{
		Base = getAroundTile(_prevXTile, _prevYTile);
		Comparison = getAroundTile(_destXTile, _destYTile);
	}
	bool findFlag = false;
	for (int i = 0; i < Base.Count; ++i)
	{
		findFlag = false;
		for (int j = 0; j < Comparison.Count; ++j)
		{
			if (Base.Collection[i] == Comparison.Collection[j])
			{
				findFlag = true;
				break;
			}
		}
		if (!findFlag)
		{
			retVal.Collection[retVal.Count] = Base.Collection[i];
			retVal.Count++;
		}
	}

	return retVal;
}

void tileMap::sendUpdateMsg(unsigned __int64 _playerCode, int _prevXTile, int _prevYTile, int _destXTile, int _destYTile)
{
	tileCollection deleteCollection = getCalTile(_prevXTile, _prevYTile, _destXTile, _destYTile, false);
	Sbuf *deleteBuf = Server->packet_deletePlayer(_playerCode);
	sendMsg(_playerCode, deleteCollection, false, deleteBuf);
	deleteBuf->Free();

	tileCollection insertCollection = getCalTile(_prevXTile, _prevYTile, _destXTile, _destYTile, true);
	Sbuf *insertBuf = Server->packet_createPlayer(_playerCode,chNum,mapNum,_destXTile,_destYTile);
	sendMsg(_playerCode, insertCollection, true, insertBuf);
	insertBuf->Free();
}

void tileMap::sendMsg(unsigned __int64 _playerCode, tileCollection _Collection, bool _insertFlag, Sbuf *_buf)
{
	// insertFlag is false -> deleteMode
	Sbuf *buf = nullptr;
	for (int Count = 0; Count < _Collection.Count; ++Count)
	{
		auto List = Map[_Collection.Collection[Count][0]][_Collection.Collection[Count][1]].Users;
		for (auto Iter : List)
		{
			if(!_insertFlag)
				buf = Server->packet_deletePlayer(Iter);
			else
				buf = Server->packet_createPlayer(Iter, chNum, mapNum, _Collection.Collection[Count][1], _Collection.Collection[Count][0]);
			Server->sendMsg(_playerCode, buf);
			buf->Free();
			if (Iter != _playerCode)
				Server->sendMsg(Iter, _buf);
		}
	}
}

// tileMap class public
void tileMap::updateTile(unsigned __int64 _playerCode, int _prevXTile, int _prevYTile, int _destXTile, int _destYTile)
{
	if (_prevXTile > xSize || _prevXTile < -1 || _destXTile > xSize || _destXTile < -1) return;
	if (_prevYTile > ySize || _prevYTile < -1 || _destYTile > xSize || _destYTile < -1) return;

	deletePlayer(_playerCode, _prevXTile, _prevYTile);
	insertPlayer(_playerCode, _destXTile, _destYTile);
	sendUpdateMsg(_playerCode, _prevXTile, _prevYTile, _destXTile, _destYTile);
}

void tileMap::deleteTile(unsigned __int64 _playerCode, int _xTile, int _yTile)
{
	deletePlayer(_playerCode, _xTile, _yTile);
}

void tileMap::insertTile(unsigned __int64 _playerCode, int _xTile, int _yTile)
{
	insertPlayer(_playerCode, _xTile, _yTile);
}

void tileMap::sendPacket(unsigned __int64 _playerCode, directionData _Data, bool _insertFlag)
{
	Sbuf *buf = nullptr;
	if (!_insertFlag)
	{
		tileCollection deleteCollection = getCalTile(_Data.prevXTile, _Data.prevYTile,-1,-1,false);
		buf = Server->packet_deletePlayer(_playerCode);
		sendMsg(_playerCode, deleteCollection, _insertFlag, buf);
	}
	else
	{
		tileCollection insertCollection = getCalTile(-1, -1, _Data.destXTile, _Data.destYTile, true);
		buf = Server->packet_createPlayer(_playerCode, chNum, mapNum, _Data.destXTile, _Data.destYTile);
		sendMsg(_playerCode, insertCollection, _insertFlag, buf);
	}
	buf->Free();
}

void tileMap::setConfig(int _xSize, int _ySize)
{
	xSize = _xSize;
	ySize = _ySize;

	Map = new Tile*[ySize];

	for (int i = 0; i < ySize; ++i)
		Map[i] = new Tile[xSize];
}

void tileMap::setServer(chatServer *_Server)
{
	Server = _Server;
}

void tileMap::setCHNum(int _chNum)
{
	chNum = _chNum;
}

void tileMap::setMapNo(int _mapNo)
{
	mapNum = _mapNo;
}

void tileMap::sendChatAround(int _xTile, int _yTile, Sbuf *_buf)
{
	if (_xTile > xSize || _xTile < 0 || _yTile > ySize || _yTile < 0) return;

	tileCollection Collection = getAroundTile(_xTile, _yTile);

	for (int Count = 0; Count < Collection.Count; ++Count)
	{
		for (auto Iter : Map[Collection.Collection[Count][0]][Collection.Collection[Count][1]].Users)
			Server->sendMsg(Iter, _buf);
	}
}
