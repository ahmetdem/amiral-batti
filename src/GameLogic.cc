#include "GameLogic.h"

#include "GameState.h"
#include "raylib.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <enet/enet.h>

namespace {

constexpr enet_uint8 kChannel = 0;
constexpr enet_uint16 kServerPort = 7777;

static_assert(kWindowSize % kGridCols == 0,
              "Window size must be divisible by grid cols");
static_assert(kWindowSize % kGridRows == 0,
              "Window size must be divisible by grid rows");

enum class MessageType : std::uint8_t {
  CellRequest = 1,
  CellUpdate = 2,
  GridSnapshot = 3,
  FinishedPreparing = 4,
  TurnUpdate = 5
};

enum class CellState : std::uint8_t { Empty = 0, Ship, Hit, Miss };

constexpr int kCellCount = kGridCols * kGridRows;
using Grid = std::array<CellState, kCellCount>;

struct Ship {
  int length;
  bool isHorizontal;
};

#pragma pack(push, 1)
struct CellRequestMessage {
  std::uint8_t type;
  std::uint16_t x;
  std::uint16_t y;
};

struct CellUpdateMessage {
  std::uint8_t type;
  std::uint16_t x;
  std::uint16_t y;
  CellState filled;
};

struct GridSnapshotMessage {
  std::uint8_t type;
  std::uint16_t width;
  std::uint16_t height;
  std::uint8_t cells[kCellCount];
};

struct FinishedPreparingMessage {
  std::uint8_t type;
  std::uint8_t finished;
};

struct TurnUpdateMessage {
  std::uint8_t type;
  std::uint8_t currentTurn; // 0 = server, 1 = client
};
#pragma pack(pop)

int CellIndex(int x, int y) { return y * kGridCols + x; }

void ResetGrid(Grid &grid, CellState state = CellState::Empty) {
  std::fill(grid.begin(), grid.end(), state);
}

bool CanPlaceShip(const Grid &grid, int x, int y, int length,
                  bool isHorizontal) {
  for (int i = 0; i < length; ++i) {
    int cx = x + (isHorizontal ? i : 0);
    int cy = y + (isHorizontal ? 0 : i);

    if (cx < 0 || cx >= kGridCols || cy < 0 || cy >= kGridRows) {
      return false;
    }
    if (grid[CellIndex(cx, cy)] == CellState::Ship) {
      return false;
    }
  }
  return true;
}

bool ApplyFill(Grid &grid, int x, int y, int length, bool isHorizontal) {
  if (!CanPlaceShip(grid, x, y, length, isHorizontal)) {
    return false;
  }
  for (int i = 0; i < length; ++i) {
    int cx = x + (isHorizontal ? i : 0);
    int cy = y + (isHorizontal ? 0 : i);
    grid[CellIndex(cx, cy)] = CellState::Ship;
  }
  return true;
}

void ApplyHover(const Grid &grid, int shipLength, bool isHorizontal) {
  Vector2 mousePos = GetMousePosition();
  int cellX = static_cast<int>(mousePos.x) / kCellSize;
  int cellY = static_cast<int>(mousePos.y) / kCellSize;

  if (cellX < 0 || cellX >= kGridCols || cellY < 0 || cellY >= kGridRows) {
    return;
  }

  bool canPlace = CanPlaceShip(grid, cellX, cellY, shipLength, isHorizontal);
  Color hoverColor = canPlace ? Fade(SKYBLUE, 0.4f) : Fade(RED, 0.4f);

  for (int i = 0; i < shipLength; ++i) {
    int hoverX = cellX + (isHorizontal ? i : 0);
    int hoverY = cellY + (isHorizontal ? 0 : i);

    if (hoverX < 0 || hoverX >= kGridCols || hoverY < 0 ||
        hoverY >= kGridRows) {
      break;
    }

    Rectangle cellRect{static_cast<float>(hoverX * kCellSize),
                       static_cast<float>(hoverY * kCellSize),
                       static_cast<float>(kCellSize),
                       static_cast<float>(kCellSize)};
    DrawRectangleRec(cellRect, hoverColor);
  }
}

void DrawGrid(const Grid &grid, const std::string &headline) {
  BeginDrawing();
  ClearBackground(RAYWHITE);

  for (int y = 0; y < kGridRows; ++y) {
    for (int x = 0; x < kGridCols; ++x) {
      Rectangle cellRect{
          static_cast<float>(x * kCellSize), static_cast<float>(y * kCellSize),
          static_cast<float>(kCellSize), static_cast<float>(kCellSize)};
      DrawRectangleLinesEx(cellRect, 1.0f, LIGHTGRAY);

      CellState state = grid[CellIndex(x, y)];
      Color fillColor = RAYWHITE;
      switch (state) {
      case CellState::Ship:
        fillColor = SKYBLUE;
        break;
      case CellState::Hit:
        fillColor = RED;
        break;
      case CellState::Miss:
        fillColor = LIGHTGRAY;
        break;
      case CellState::Empty:
      default:
        break;
      }

      if (state != CellState::Empty) {
        DrawRectangle(x * kCellSize + 1, y * kCellSize + 1, kCellSize - 2,
                      kCellSize - 2, fillColor);
      }
    }
  }

  DrawText(headline.c_str(), 20, 20, 22, DARKGRAY);
  EndDrawing();
}

std::vector<Ship> CreateFleet() {
  return {{4, true}, {3, true}, {3, true}, {2, true}, {2, true},
          {2, true}, {1, true}, {1, true}, {1, true}, {1, true}};
}

void RecordShipCells(std::vector<std::uint8_t> &locations, int x, int y,
                     const Ship &ship) {
  for (int i = 0; i < ship.length; ++i) {
    int cx = x + (ship.isHorizontal ? i : 0);
    int cy = y + (ship.isHorizontal ? 0 : i);
    locations.push_back(static_cast<std::uint8_t>(CellIndex(cx, cy)));
  }
}

void BroadcastCellUpdate(ENetHost *host, int x, int y, CellState filled) {
  CellUpdateMessage msg{static_cast<std::uint8_t>(MessageType::CellUpdate),
                        static_cast<std::uint16_t>(x),
                        static_cast<std::uint16_t>(y), filled};
  ENetPacket *packet =
      enet_packet_create(&msg, sizeof(msg), ENET_PACKET_FLAG_RELIABLE);
  enet_host_broadcast(host, kChannel, packet);
  enet_host_flush(host);
}

void SendGridSnapshot(ENetPeer *peer, const Grid &grid) {
  if (!peer || peer->state != ENET_PEER_STATE_CONNECTED) {
    return;
  }

  GridSnapshotMessage msg{};
  msg.type = static_cast<std::uint8_t>(MessageType::GridSnapshot);
  msg.width = static_cast<std::uint16_t>(kGridCols);
  msg.height = static_cast<std::uint16_t>(kGridRows);

  for (int i = 0; i < kCellCount; ++i) {
    msg.cells[i] = static_cast<std::uint8_t>(grid[i]);
  }

  ENetPacket *packet =
      enet_packet_create(&msg, sizeof(msg), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(peer, kChannel, packet);
  enet_host_flush(peer->host);
}

} // namespace

int RunServer() {
  ENetAddress address{};
  address.host = ENET_HOST_ANY;
  address.port = kServerPort;

  ENetHost *host = enet_host_create(&address, 32, 1, 0, 0);
  if (!host) {
    std::fprintf(stderr, "Failed to create ENet server host\n");
    return 1;
  }

  ENetPeer *connectedPeer = nullptr;

  InitWindow(kWindowSize, kWindowSize, "ENet Server - Shared Grid");
  SetTargetFPS(60);

  Grid playerGrid{};
  Grid enemyGrid{};

  std::vector<std::uint8_t> shipLocations;
  shipLocations.reserve(20);

  std::vector<Ship> ships = CreateFleet();

  Turn currentTurn = Turn::Server;
  int currentShipIndex = 0;
  int hittedShipCount = 0;
  int enemyHitCount = 0;

  bool serverFinishedPreparing = false;
  bool clientFinishedPreparing = false;

  bool resetGrid = false;

  std::string headline = "Server: Preparing Phase";

  Phase currentPhase = Phase::Preparing;
  float transitionTimer = 0.0f;
  float finishedTimer = 0.0f;
  GameResult outcome = GameResult::None;
  bool exitRequested = false;

  while (!WindowShouldClose()) {
    ENetEvent event;
    while (enet_host_service(host, &event, 0) > 0) {
      switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT:
        std::printf("Client connected: %x:%u\n", event.peer->address.host,
                    event.peer->address.port);
        connectedPeer = event.peer;
        gameState.isClientConnected = true;
        SendGridSnapshot(event.peer, playerGrid);
        break;
      case ENET_EVENT_TYPE_DISCONNECT:
        std::printf("Client disconnected\n");
        if (connectedPeer == event.peer) {
          connectedPeer = nullptr;
        }
        event.peer->data = nullptr;
        break;
      case ENET_EVENT_TYPE_RECEIVE: {
        if (event.packet->dataLength < 1) {
          enet_packet_destroy(event.packet);
          break;
        }

        const auto messageType =
            static_cast<MessageType>(event.packet->data[0]);

        switch (messageType) {
        case MessageType::CellUpdate:
          if (event.packet->dataLength == sizeof(CellUpdateMessage)) {
            const auto *msg =
                reinterpret_cast<const CellUpdateMessage *>(event.packet->data);
            int x = static_cast<int>(msg->x);
            int y = static_cast<int>(msg->y);

            if (x >= 0 && x < kGridCols && y >= 0 && y < kGridRows) {
              int index = CellIndex(x, y);
              if (currentTurn == Turn::Server) {
                CellState previous = enemyGrid[index];
                enemyGrid[index] = msg->filled;

                if (msg->filled == CellState::Hit &&
                    previous != CellState::Hit &&
                    currentPhase != Phase::Finished &&
                    outcome != GameResult::Defeat) {
                  ++enemyHitCount;
                  if (enemyHitCount >= 20) {
                    outcome = GameResult::Victory;
                    currentPhase = Phase::Finished;
                    finishedTimer = 0.0f;
                    currentTurn = Turn::None;
                  }
                }
              } else {
                playerGrid[index] = msg->filled;
              }
            }
          }
          break;
        case MessageType::CellRequest:
          if (event.packet->dataLength == sizeof(CellRequestMessage)) {
            const auto *msg = reinterpret_cast<const CellRequestMessage *>(
                event.packet->data);
            int x = static_cast<int>(msg->x);
            int y = static_cast<int>(msg->y);

            if (x < 0 || x >= kGridCols || y < 0 || y >= kGridRows) {
              break;
            }

            int index = CellIndex(x, y);
            bool isHit = std::find(shipLocations.begin(), shipLocations.end(),
                                   index) != shipLocations.end();

            CellState result = isHit ? CellState::Hit : CellState::Miss;
            playerGrid[index] = result;

            if (isHit) {
              // Server Ship Has Been Hitted
              hittedShipCount++;

              if (hittedShipCount >= 20 && currentPhase != Phase::Finished) {
                outcome = GameResult::Defeat;
                currentPhase = Phase::Finished;
                finishedTimer = 0.0f;
                currentTurn = Turn::None;
              }
            }

            BroadcastCellUpdate(host, x, y, result);

            if (!isHit) {
              std::uint8_t nextTurn = (currentTurn == Turn::Server) ? 1 : 0;
              currentTurn = (nextTurn == 0) ? Turn::Server : Turn::Client;
              TurnUpdateMessage turnMsg{
                  static_cast<std::uint8_t>(MessageType::TurnUpdate), nextTurn};
              ENetPacket *turnPacket = enet_packet_create(
                  &turnMsg, sizeof(turnMsg), ENET_PACKET_FLAG_RELIABLE);
              enet_host_broadcast(host, kChannel, turnPacket);
              enet_host_flush(host);
            }
          }
          break;
        case MessageType::TurnUpdate:
          if (event.packet->dataLength == sizeof(TurnUpdateMessage)) {
            const auto *msg =
                reinterpret_cast<const TurnUpdateMessage *>(event.packet->data);
            currentTurn = (msg->currentTurn == 0) ? Turn::Server : Turn::Client;

            ENetPacket *fwdPacket = enet_packet_create(
                msg, sizeof(TurnUpdateMessage), ENET_PACKET_FLAG_RELIABLE);
            enet_host_broadcast(host, kChannel, fwdPacket);
            enet_host_flush(host);
          }
          break;
        case MessageType::FinishedPreparing:
          if (event.packet->dataLength == sizeof(FinishedPreparingMessage)) {
            const auto *msg =
                reinterpret_cast<const FinishedPreparingMessage *>(
                    event.packet->data);
            if (msg->finished == 1) {
              clientFinishedPreparing = true;
            }
          }
          break;
        default:
          break;
        }

        enet_packet_destroy(event.packet);
        break;
      }
      case ENET_EVENT_TYPE_NONE:
      default:
        break;
      }
    }

    if (!gameState.isClientConnected) {
      ShowWaitingRoom("Waiting for client to connect...");
      continue;
    }

    if (serverFinishedPreparing && !clientFinishedPreparing) {
      ShowWaitingRoom("Waiting for other player to finish...");
      continue;
    }

    float delta = GetFrameTime();

    switch (currentPhase) {
    case Phase::Preparing:
      if (static_cast<size_t>(currentShipIndex) < ships.size()) {
        Ship &ship = ships[currentShipIndex];

        ApplyHover(playerGrid, ship.length, ship.isHorizontal);

        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
          ship.isHorizontal = !ship.isHorizontal;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
          Vector2 mousePos = GetMousePosition();
          int cellX = static_cast<int>(mousePos.x) / kCellSize;
          int cellY = static_cast<int>(mousePos.y) / kCellSize;

          if (ApplyFill(playerGrid, cellX, cellY, ship.length,
                        ship.isHorizontal)) {
            RecordShipCells(shipLocations, cellX, cellY, ship);
            ++currentShipIndex;
          }
        }
      } else if (!serverFinishedPreparing && connectedPeer) {
        FinishedPreparingMessage msg{
            static_cast<std::uint8_t>(MessageType::FinishedPreparing), 1};
        ENetPacket *packet =
            enet_packet_create(&msg, sizeof(msg), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(connectedPeer, kChannel, packet);
        enet_host_flush(host);
        serverFinishedPreparing = true;
      }

      DrawGrid(playerGrid, headline);
      break;

    case Phase::Transition:
      transitionTimer += delta;
      DrawTransition(transitionTimer);

      if (transitionTimer > 3.0f) {
        currentPhase = Phase::Battle;
        currentTurn = Turn::Server;

        TurnUpdateMessage turnMsg{
            static_cast<std::uint8_t>(MessageType::TurnUpdate), 0};
        ENetPacket *turnPacket = enet_packet_create(&turnMsg, sizeof(turnMsg),
                                                    ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(host, kChannel, turnPacket);
        enet_host_flush(host);
      }
      break;

    case Phase::Battle:
      headline = "Battle Phase!";
      if (!resetGrid) {
        ResetGrid(enemyGrid);
        resetGrid = true;
      }

      if (currentTurn != Turn::Server) {
        DrawGrid(playerGrid, "Enemy's Turn - Your Ships");
        break;
      }

      DrawGrid(enemyGrid, "Your Turn (Server)");
      ApplyHover(enemyGrid, 1, true);

      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && connectedPeer) {
        Vector2 mousePos = GetMousePosition();
        int cellX = static_cast<int>(mousePos.x) / kCellSize;
        int cellY = static_cast<int>(mousePos.y) / kCellSize;

        CellRequestMessage msg{
            static_cast<std::uint8_t>(MessageType::CellRequest),
            static_cast<std::uint16_t>(cellX),
            static_cast<std::uint16_t>(cellY)};
        ENetPacket *packet =
            enet_packet_create(&msg, sizeof(msg), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(connectedPeer, kChannel, packet);
        enet_host_flush(host);
      }
      break;

    case Phase::Finished: {
      finishedTimer += delta;
      GameResult screenResult =
          (outcome == GameResult::None) ? GameResult::Defeat : outcome;
      DrawFinishedScreen(screenResult, finishedTimer);

      if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
          IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        exitRequested = true;
      }
      break;
    }
    }

    if (exitRequested) {
      break;
    }

    if (clientFinishedPreparing && serverFinishedPreparing &&
        currentPhase == Phase::Preparing) {
      currentPhase = Phase::Transition;
      transitionTimer = 0.0f;
    }
  }

  enet_host_flush(host);
  enet_host_destroy(host);
  CloseWindow();
  return 0;
}

int RunClient(const char *hostName) {
  ENetHost *client = enet_host_create(nullptr, 1, 1, 0, 0);
  if (!client) {
    std::fprintf(stderr, "Failed to create ENet client host\n");
    return 1;
  }

  ENetAddress address{};
  enet_address_set_host(&address, hostName);
  address.port = kServerPort;

  ENetPeer *peer = enet_host_connect(client, &address, 1, 0);
  if (!peer) {
    std::fprintf(stderr, "Failed to initiate connection to %s:%u\n", hostName,
                 kServerPort);
    enet_host_destroy(client);
    return 1;
  }

  ENetEvent event;
  if (enet_host_service(client, &event, 5000) <= 0 ||
      event.type != ENET_EVENT_TYPE_CONNECT) {
    std::fprintf(stderr, "Connection to %s timed out\n", hostName);
    enet_peer_reset(peer);
    enet_host_destroy(client);
    return 1;
  }

  InitWindow(kWindowSize, kWindowSize, "ENet Client - Shared Grid");
  SetTargetFPS(60);

  Grid playerGrid{};
  Grid enemyGrid{};
  std::vector<std::uint8_t> shipLocations;
  shipLocations.reserve(20);
  std::vector<Ship> ships = CreateFleet();

  Turn currentTurn = Turn::None;
  int currentShipIndex = 0;
  int hittedShipCount = 0;
  int enemyHitCount = 0;

  std::string headline = "Client: Preparing Phase";

  Phase currentPhase = Phase::Preparing;
  float transitionTimer = 0.0f;
  float finishedTimer = 0.0f;
  GameResult outcome = GameResult::None;
  bool exitRequested = false;

  bool connectionActive = true;
  bool clientFinishedPreparing = false;
  bool serverFinishedPreparing = false;
  bool resetGrid = false;

  while (!WindowShouldClose() && connectionActive) {
    while (enet_host_service(client, &event, 0) > 0) {
      switch (event.type) {
      case ENET_EVENT_TYPE_RECEIVE: {
        if (event.packet->dataLength < 1) {
          enet_packet_destroy(event.packet);
          break;
        }

        const auto messageType =
            static_cast<MessageType>(event.packet->data[0]);

        switch (messageType) {
        case MessageType::CellUpdate:
          if (event.packet->dataLength == sizeof(CellUpdateMessage)) {
            const auto *msg =
                reinterpret_cast<const CellUpdateMessage *>(event.packet->data);
            int x = static_cast<int>(msg->x);
            int y = static_cast<int>(msg->y);

            if (x >= 0 && x < kGridCols && y >= 0 && y < kGridRows) {
              int index = CellIndex(x, y);
              if (currentTurn == Turn::Client) {
                CellState previous = enemyGrid[index];
                enemyGrid[index] = msg->filled;

                if (msg->filled == CellState::Hit &&
                    previous != CellState::Hit &&
                    currentPhase != Phase::Finished &&
                    outcome != GameResult::Defeat) {
                  ++enemyHitCount;
                  if (enemyHitCount >= 20) {
                    outcome = GameResult::Victory;
                    currentPhase = Phase::Finished;
                    finishedTimer = 0.0f;
                    currentTurn = Turn::None;
                  }
                }
              } else {
                playerGrid[index] = msg->filled;
              }
            }
          }
          break;
        case MessageType::GridSnapshot:
          if (event.packet->dataLength == sizeof(GridSnapshotMessage)) {
            const auto *msg = reinterpret_cast<const GridSnapshotMessage *>(
                event.packet->data);

            if (msg->width == kGridCols && msg->height == kGridRows) {
              for (int i = 0; i < kCellCount; ++i) {
                playerGrid[i] = static_cast<CellState>(msg->cells[i]);
              }
            }
          }
          break;
        case MessageType::FinishedPreparing:
          if (event.packet->dataLength == sizeof(FinishedPreparingMessage)) {
            const auto *msg =
                reinterpret_cast<const FinishedPreparingMessage *>(
                    event.packet->data);
            if (msg->finished == 1) {
              serverFinishedPreparing = true;
            }
          }
          break;
        case MessageType::TurnUpdate:
          if (event.packet->dataLength == sizeof(TurnUpdateMessage)) {
            const auto *msg =
                reinterpret_cast<const TurnUpdateMessage *>(event.packet->data);
            currentTurn = (msg->currentTurn == 0) ? Turn::Server : Turn::Client;
          }
          break;
        case MessageType::CellRequest:
          if (event.packet->dataLength == sizeof(CellRequestMessage)) {
            const auto *msg = reinterpret_cast<const CellRequestMessage *>(
                event.packet->data);

            int x = static_cast<int>(msg->x);
            int y = static_cast<int>(msg->y);

            if (x < 0 || x >= kGridCols || y < 0 || y >= kGridRows) {
              break;
            }

            int index = CellIndex(x, y);
            bool isHit = std::find(shipLocations.begin(), shipLocations.end(),
                                   index) != shipLocations.end();

            if (isHit) {
              // Client Ship Has Been Hitted
              hittedShipCount++;

              if (hittedShipCount >= 20 && currentPhase != Phase::Finished) {
                outcome = GameResult::Defeat;
                currentPhase = Phase::Finished;
                finishedTimer = 0.0f;
                currentTurn = Turn::None;
              }
            }

            CellState result = isHit ? CellState::Hit : CellState::Miss;
            playerGrid[index] = result;

            CellUpdateMessage updateMsg{
                static_cast<std::uint8_t>(MessageType::CellUpdate),
                static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y),
                result};
            ENetPacket *packet = enet_packet_create(
                &updateMsg, sizeof(updateMsg), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(peer, kChannel, packet);
            enet_host_flush(client);

            if (!isHit) {
              TurnUpdateMessage turnMsg{
                  static_cast<std::uint8_t>(MessageType::TurnUpdate), 1};
              ENetPacket *turnPacket = enet_packet_create(
                  &turnMsg, sizeof(turnMsg), ENET_PACKET_FLAG_RELIABLE);
              enet_peer_send(peer, kChannel, turnPacket);
              enet_host_flush(client);
            }
          }
          break;
        default:
          break;
        }

        enet_packet_destroy(event.packet);
        break;
      }
      case ENET_EVENT_TYPE_DISCONNECT:
        std::printf("Disconnected from server\n");
        connectionActive = false;
        break;
      case ENET_EVENT_TYPE_NONE:
      case ENET_EVENT_TYPE_CONNECT:
      default:
        break;
      }
    }

    if (peer->state != ENET_PEER_STATE_CONNECTED) {
      continue;
    }

    if (currentPhase != Phase::Finished && clientFinishedPreparing &&
        !serverFinishedPreparing) {
      ShowWaitingRoom("Waiting for other player to finish...");
      continue;
    }

    float delta = GetFrameTime();

    switch (currentPhase) {
    case Phase::Preparing:
      if (static_cast<size_t>(currentShipIndex) < ships.size()) {
        Ship &ship = ships[currentShipIndex];

        ApplyHover(playerGrid, ship.length, ship.isHorizontal);

        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
          ship.isHorizontal = !ship.isHorizontal;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
          Vector2 mousePos = GetMousePosition();
          int cellX = static_cast<int>(mousePos.x) / kCellSize;
          int cellY = static_cast<int>(mousePos.y) / kCellSize;

          if (ApplyFill(playerGrid, cellX, cellY, ship.length,
                        ship.isHorizontal)) {
            RecordShipCells(shipLocations, cellX, cellY, ship);
            ++currentShipIndex;
          }
        }
      } else if (!clientFinishedPreparing) {
        FinishedPreparingMessage msg{
            static_cast<std::uint8_t>(MessageType::FinishedPreparing), 1};
        ENetPacket *packet =
            enet_packet_create(&msg, sizeof(msg), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer, kChannel, packet);
        enet_host_flush(client);
        clientFinishedPreparing = true;
      }

      DrawGrid(playerGrid, headline);
      break;

    case Phase::Transition:
      transitionTimer += delta;
      DrawTransition(transitionTimer);

      if (transitionTimer > 3.0f) {
        currentPhase = Phase::Battle;
      }
      break;

    case Phase::Battle:
      headline = "Battle Phase!";
      if (!resetGrid) {
        ResetGrid(enemyGrid);
        resetGrid = true;
      }

      if (currentTurn != Turn::Client) {
        DrawGrid(playerGrid, "Waiting for opponent...");
        break;
      }

      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mousePos = GetMousePosition();
        int cellX = static_cast<int>(mousePos.x) / kCellSize;
        int cellY = static_cast<int>(mousePos.y) / kCellSize;

        CellRequestMessage msg{
            static_cast<std::uint8_t>(MessageType::CellRequest),
            static_cast<std::uint16_t>(cellX),
            static_cast<std::uint16_t>(cellY)};
        ENetPacket *packet =
            enet_packet_create(&msg, sizeof(msg), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer, kChannel, packet);
        enet_host_flush(client);
      }

      DrawGrid(enemyGrid, "Your Turn (Client)");
      ApplyHover(enemyGrid, 1, true);

      break;

    case Phase::Finished: {
      finishedTimer += delta;
      GameResult screenResult =
          (outcome == GameResult::None) ? GameResult::Defeat : outcome;
      DrawFinishedScreen(screenResult, finishedTimer);

      if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
          IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        exitRequested = true;
      }
      break;
    }
    }

    if (exitRequested) {
      break;
    }

    if (clientFinishedPreparing && serverFinishedPreparing &&
        currentPhase == Phase::Preparing) {
      currentPhase = Phase::Transition;
      transitionTimer = 0.0f;
    }
  }

  if (connectionActive) {
    enet_peer_disconnect(peer, 0);
    while (enet_host_service(client, &event, 3000) > 0) {
      if (event.type == ENET_EVENT_TYPE_RECEIVE) {
        enet_packet_destroy(event.packet);
      } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
        break;
      }
    }
  }

  enet_host_destroy(client);
  CloseWindow();
  return 0;
}
