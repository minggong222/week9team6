# 0_Main_Game

`__Crossy.scene`의 플레이어·카메라·조명·GameManager/HUD와 맵·차량·기차 풀링을 합친 씬입니다.

## 실행 흐름

- 시작 → 맵 41줄·차량 풀 100대 준비 → 25종 캐릭터 선택 → 확정 후 게임 시작.
- 캐릭터 선택 → 좌우 방향키 또는 A/D로 변경, Enter/Space로 확정. 선택 중에는 이동·교통 중지.
- 전진 → 최고 도달 행 갱신 → 뒤쪽 행과 그 행의 차량 회수 → 앞쪽 행 생성.
- 행 생성 → 도로는 차량 배치 / 잔디는 나무 배치 / 레일은 기차 편성 선택.
- 차량 풀은 택시·세단·픽업·긴 소방차·긴 쓰레기 수거차를 같은 비율로 사용합니다. 폭 2로 맞추며 원본 길이 비율과 최소 차량 간격을 유지합니다.
- 레일 앞쪽에 경고등을 12유닛 간격으로 5개 배치합니다. 기둥은 플레이어 이동 칸 사이에 놓습니다. 출발 2초 전부터 0.25초마다 빨간 렌즈와 빨간 조명을 점멸하며, 경고 시작 후 총 3초가 지나면 모두 끕니다. 최초 출발도 최소 2초의 경고 시간을 보장합니다.
- 경고등은 해당 행과 함께 회수·재사용합니다. 캐릭터 선택 중에는 타이머가 진행되지 않습니다.
- 충돌·뒤처짐 → GameManager가 사망·점수 UI·효과음을 처리. 사망 후에도 교통은 유지.
- R → 선택한 캐릭터 유지·점프 취소·플레이어 복귀 → 차량 반환·맵/기차 재배치 → 점수 초기화.

## 조절할 곳

`Assets/Scripts/LevelManager.lua` 상단에서 설정합니다.

| 설정 | 현재 값 |
|---|---|
| LaneSpacing | 이동 거리·행 간격 3 |
| RowsBehind / RowsAhead | 뒤 10줄 / 앞 30줄 유지 |
| DeathRowsBehind | 최고 도달 행보다 6줄 뒤 경계를 벗어나면 사망 |
| MapRoadChance / MapRailChance | 도로 60% / 레일 5% / 나머지 잔디 |
| MapTreeCountWeights | 나무 0~5개: 5 / 15 / 20 / 30 / 20 / 10 |
| MapPlayerHalfWidth | 좌우 이동 ±24 |
| MapMinLaneSpeed / MapMaxLaneSpeed | 도로별 차량 속도 8~20 |
| MinSpawnInterval / MaxSpawnInterval | 차량 생성 간격 0.25~2.5초 |
| MapTrainSpeed | 기차 속도 50 |
| MapTrainWaitMin / MapTrainWaitMax | 편성 통과 후 대기 3~6초 |
| MapTrainTypes | 고속열차·도시형·화물열차, 같은 확률 |
| MapTrainWarningTime | 출발 전 경고 2초 |
| MapTrainBlinkDuration | 경고 시작부터 전체 점멸 유지 시간 3초; 이 값만 줄이면 더 일찍 소등 |
| MapTrainBlinkInterval | 켜짐/꺼짐 간격 0.25초 |
| MapRailSignalSpacing | 경고등 간격 12유닛 |
| MapRailSignalOffsetX | 레일 중심보다 전방 반대쪽으로 1.4유닛 |
| MapRailSignalIntensity | 빨간 점광원 밝기 6, 그림자 비활성 |

설정 변경은 다음 Play부터 적용됩니다. 행 풀 크기를 바꾸면 씬 생성기도 다시 실행합니다.

실행 중에는 같은 Lua 월드에서 아래처럼 속도를 바꿀 수 있습니다.

```lua
_G.CrossyLevel.SetTrainSpeed(40)     -- 현재/이후 모든 레일
_G.CrossyLevel.SetTrainSpeed(60, 12) -- 현재 12행이 레일이면 그 행만
```

## 파일 역할

- `LevelManager.lua`: 맵 생성·회수, 차량 풀·배치, 기차 배치·이동·속도.
- `GameManager.lua`: 캐릭터 선택·프리뷰·카메라 전환, 점수, 공통 사망 UI·효과음, R키 재시작. 캐릭터는 가로·세로 최대 2 World Units로 맞춤. 무한 맵에는 고정 도착점 클리어를 적용하지 않음.
- `Player.lua`: WASD·방향키 입력·점프·방향. `StopMovement()`로 사망/재시작 중 이동 취소, `RefreshMeshBaseLocation()`으로 캐릭터 교체 후 점프 기준 위치 갱신.
- `Car.lua`: 차량 이동·충돌 검사·풀 반환.

## 씬 재생성 / 코드 검증

```powershell
node Tools/BuildCrossySignals.mjs
node Tools/GenerateMapTest.mjs --main
Source/ThirdParty/luajit/luajit.exe Tools/TestMainGame.lua
```

생성기는 기존 고정 맵을 섞지 않고 풀링용 맵을 구성하며, 플레이어·카메라·조명·HUD 설정을 유지합니다.
Lua 검증은 엔진 API 모의 환경에서 실행하며 에디터/PIE를 실행하지 않습니다.

경고등 원본은 [Kenney City Kit Roads 2.2](https://kenney.nl/assets/city-kit-roads)의 `traffic-light.obj`(CC0)입니다.
`Assets/Models/Crossy/Signals/`에 원본·팔레트·라이선스와 가공한 켜짐/꺼짐 메시를 보관합니다.
새 OBJ를 추가했으므로 이미 에디터가 열려 있다면 에디터를 재실행해 에셋을 가져옵니다. C++ 변경은 없습니다.
