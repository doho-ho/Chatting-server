# Chatting-server
IOCP기반 멀티스레드 채팅 서버
1. boost library - lockfreequeue, lockfreestack 사용
2. rapidjson 이용하여 config data load

#2020.03.06 소스코드 업로드

# 2020.03.12
- SRWLOCK -> std::recursive_mutex로 수정
  1. tileMap의 SRWLOCK 데드락 문제 발생
    원인 : 채팅 데이터 전송 -> Tile 서치 및 전송 -> 채팅 전송 대상이 disconnect 대기중 -> disconnect 처리 중 Map에서 제거 : tileLock deadLock
  2. chatServer의 playerListLock 데드락 문제 발생
    원인 : Disconnect -> Map에서 제거 -> 맵의 유저들에게 disconnect 패킷 전송 알림 -> 패킷 받을 대상이 disconnect 대기 -> disconnect -> playerListLock deadLock
  해결 : 스레드에서 이미 획득한 Lock을 다시 획득하려고 시도 -> recursive_mutex를 이용해 이미 획득한 락을 재 획득 시도할 때 추가 락 없이 이용
  단점 : SRWLOCK에 비해 속도가 느림
