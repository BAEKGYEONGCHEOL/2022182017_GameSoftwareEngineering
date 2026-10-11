# 로컬 게임 모니터링 시스템

## 목적

게임을 실행한 PC의 웹 브라우저에서 NPC와 지역 상태를 확인한다. 대시보드는 게임 고유 문자열 프로토콜을 해석하지 않고 HTTP로 전달되는 JSON 문서만 수신한다.

## 접속 방법

1. 게임을 실행한다.
2. 같은 PC의 브라우저에서 `http://127.0.0.1:8787/`에 접속한다.
3. 대시보드는 `GET /api/state`를 0.5초마다 호출한다.

서버는 외부 네트워크 인터페이스가 아닌 루프백 주소 `127.0.0.1`에만 바인딩한다. 게임이 종료되면 서버도 함께 종료된다. 포트가 이미 사용 중이면 게임은 중단되지 않으며 모니터링 기능만 시작되지 않는다.

## HTTP API

### `GET /`

로컬 HTML 대시보드를 반환한다.

### `GET /api/state`

UTF-8 `application/json` 형식의 최신 게임 상태 스냅샷을 반환한다. 응답에는 `Cache-Control: no-store`를 적용한다.

```json
{
  "schemaVersion": 1,
  "sequence": 42,
  "gameMode": "erebus_level_1",
  "currentRegion": "erebus",
  "regions": [
    {
      "id": "erebus",
      "name": "수송선 에레보스",
      "status": "current_contested",
      "npcAlive": 10,
      "npcTotal": 12,
      "mobAlive": 7,
      "mobTotal": 9
    }
  ],
  "npcs": [
    {
      "id": "erebus_survivor_0",
      "name": "경비대원",
      "region": "erebus",
      "state": "panic",
      "health": 2.0,
      "fear": 85.0,
      "armed": true,
      "squad": 1,
      "position": { "x": 4.5, "y": -8.0 }
    }
  ]
}
```

## 제공하는 정보

### 지역 집계

현재 구현에서 향후 마을로 확장할 수 있는 지역 단위는 다음 세 곳이다.

- 연구선 노크티스
- 수송선 에레보스
- 거대 우주 정거장

각 지역은 전체 상태 코드, 생존 NPC 수와 전체 NPC 수, 살아 있는 몹 수와 전체 생성 몹 수를 제공한다.

지역 상태 코드는 `unvisited`, `emergency`, `secured`, `contested`를 기본으로 사용하며 현재 플레이어가 있는 지역에는 `current_` 접두사를 붙인다.

### NPC 상태

모든 생성 NPC는 다음 정보를 제공한다.

- 영구적으로 구별할 수 있는 ID
- 역할 또는 이름
- 소속 지역
- 상태: `stable`, `frightened`, `panic`, `dead`
- 현재 체력과 공포 수치
- 무장 여부
- 분대 번호
- 현재 월드 좌표

## 업데이트와 스레드 규칙

- 게임 스레드는 0.25초마다 JSON 스냅샷을 생성한다.
- HTTP 서버 스레드는 게임 객체를 직접 읽지 않고 뮤텍스로 보호된 JSON 복사본만 읽는다.
- 브라우저 요청 처리 때문에 NPC AI, 입력 또는 렌더링 업데이트가 정지하지 않게 한다.
- `schemaVersion`은 JSON 필드의 호환되지 않는 변경이 있을 때 증가시킨다.
