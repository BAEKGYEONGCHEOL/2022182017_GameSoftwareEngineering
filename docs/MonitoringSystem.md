# 로컬 게임 모니터링 시스템

## 목적

게임을 실행한 PC의 웹 브라우저에서 NPC와 지역 상태를 확인한다. 대시보드는 게임 고유 문자열 프로토콜을 해석하지 않고 HTTP로 전달되는 JSON 문서만 수신한다.

## 접속 방법

1. 게임을 실행한다.
2. 같은 PC의 브라우저에서 `http://127.0.0.1:8787/`에 접속한다.
3. 대시보드는 `GET /api/state`를 0.1초마다 호출한다.

서버는 외부 네트워크 인터페이스가 아닌 루프백 주소 `127.0.0.1`에만 바인딩한다. 게임이 종료되면 서버도 함께 종료된다. 포트가 이미 사용 중이면 게임은 중단되지 않으며 모니터링 기능만 시작되지 않는다.

## HTTP API

### `GET /`

로컬 HTML 대시보드를 반환한다.

### `GET /api/state`

UTF-8 `application/json` 형식의 최신 게임 상태 스냅샷을 반환한다. 응답에는 `Cache-Control: no-store`를 적용한다.

```json
{
  "schemaVersion": 2,
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
      "mobTotal": 9,
      "mapAvailable": true,
      "bounds": { "minX": -64, "maxX": 64, "minY": -64, "maxY": 64 }
    }
  ],
  "player": {
    "visible": true,
    "region": "erebus",
    "position": { "x": 1.0, "y": -2.0 }
  },
  "npcs": [
    {
      "id": "erebus_survivor_0",
      "name": "경비대원",
      "region": "erebus",
      "state": "panic",
      "health": 2.0,
      "maxHealth": 3.0,
      "fear": 85.0,
      "armed": true,
      "squad": 1,
      "position": { "x": 4.5, "y": -8.0 }
    }
  ],
  "mobs": [
    {
      "id": "erebus_void_0",
      "region": "erebus",
      "health": 2,
      "maxHealth": 2,
      "position": { "x": 10.0, "y": 4.0 }
    }
  ],
  "objects": [
    {
      "id": "erebus_engine_0",
      "name": "보조 엔진",
      "region": "erebus",
      "type": "engine",
      "position": { "x": -32.0, "y": 18.0 }
    }
  ],
  "terrain": {
    "noctis": [[-21, -21, 2], [-20, -21, 2]],
    "erebus": [[0, 0, 2], [1, 0, 1]]
  }
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
- 현재 체력, 최대 체력과 공포 수치
- 무장 여부
- 분대 번호
- 현재 월드 좌표

대시보드의 체력 열은 `health / maxHealth` 숫자와 체력 비율 게이지를 함께 표시한다. 체력 비율에 따라 게이지 색상이 녹색, 주황색, 빨간색으로 바뀐다.

### 지역 미니맵

대시보드 하단에는 생성된 각 지역의 미니맵을 표시한다. `terrain`의 각 타일은 `[월드 x, 월드 y, 타일 종류]` 형식이며, 종류 `1`은 통로이고 `2`는 방 바닥이다. 실제 게임과 같은 `(x-y, (x+y)/2)` 아이소메트릭 투영을 적용하여 방과 통로를 마름모 타일로 그린다. 따라서 단순한 사각형 범위가 아니라 실제 이동 가능한 지형의 형태가 표시된다.

플레이어와 개체 좌표에도 동일한 아이소메트릭 투영을 적용하고 다음 색상의 작은 원으로 구분한다.

- 플레이어: 청록색
- 안정 또는 공포 상태 NPC: 녹색
- 공황 상태 NPC: 주황색
- 공허종: 보라색
- 현재 상호작용할 수 있는 장치, 문서 및 아이템: 노란색

사망한 NPC와 공허종은 상태 집계 및 NPC 표에는 반영되지만 미니맵에는 그리지 않는다. 아직 생성되지 않은 지역은 `mapAvailable: false`로 전달하여 미니맵에서 제외한다. 작동 완료된 엔진이나 획득한 아이템처럼 더 이상 상호작용할 수 없는 일회성 오브젝트 역시 미니맵에서 제외한다.

캔버스는 내부 해상도와 표시 종횡비를 동일하게 유지하므로 원형 표식이 창 너비에 따라 타원으로 눌리지 않는다. 브라우저는 0.1초 간격의 최신 좌표 사이를 `requestAnimationFrame`으로 보간하여 이동 표식을 화면 주사율에 맞춰 부드럽게 갱신한다.

## 업데이트와 스레드 규칙

- 게임 스레드는 0.1초마다 JSON 스냅샷을 생성한다.
- HTTP 서버 스레드는 게임 객체를 직접 읽지 않고 뮤텍스로 보호된 JSON 복사본만 읽는다.
- 브라우저 요청 처리 때문에 NPC AI, 입력 또는 렌더링 업데이트가 정지하지 않게 한다.
- `schemaVersion`은 JSON 필드의 호환되지 않는 변경이 있을 때 증가시킨다.
