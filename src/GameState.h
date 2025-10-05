// GameState.h
#pragma once

#include "raylib.h"
#include <cmath>
#include <string>

struct GameState {
  bool isClientConnected = false;
};

// one shared instance across all translation units
inline GameState gameState;

constexpr int kWindowSize = 600;
constexpr int kGridCols = 10;
constexpr int kGridRows = 10;
constexpr int kCellSize = kWindowSize / kGridCols;

struct MenuResult {
  bool quit;
  bool isHost;
  std::string address;
};

enum class Phase { Preparing, Transition, Battle, Finished };
enum class Turn { None, Server, Client };

enum class GameResult { None, Victory, Defeat };

inline MenuResult ShowMainMenu() {
  InitWindow(kWindowSize, kWindowSize, "Shared Grid - Main Menu");
  SetTargetFPS(60);

  MenuResult result{true, false, std::string{}};
  std::string ipText = "127.0.0.1";
  bool editingIp = false;
  bool selectionMade = false;

  Rectangle hostRect{static_cast<float>(kWindowSize / 2 - 140), 220.0f, 280.0f,
                     60.0f};
  Rectangle joinRect{static_cast<float>(kWindowSize / 2 - 140), 290.0f, 280.0f,
                     60.0f};
  Rectangle ipRect{static_cast<float>(kWindowSize / 2 - 160), 420.0f, 320.0f,
                   48.0f};

  while (!WindowShouldClose()) {
    Vector2 mouse = GetMousePosition();
    bool hostHover = CheckCollisionPointRec(mouse, hostRect);
    bool joinHover = CheckCollisionPointRec(mouse, joinRect);
    bool ipHover = CheckCollisionPointRec(mouse, ipRect);

    int key = GetCharPressed();
    while (key > 0) {
      if (editingIp && ipText.size() < 63) {
        if (key >= 32 && key <= 126) {
          ipText.push_back(static_cast<char>(key));
        }
      }
      key = GetCharPressed();
    }

    if (editingIp && IsKeyPressed(KEY_BACKSPACE) && !ipText.empty()) {
      ipText.pop_back();
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
      selectionMade = false;
      break;
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      if (hostHover) {
        result.quit = false;
        result.isHost = true;
        result.address.clear();
        selectionMade = true;
        break;
      }
      if (joinHover && !ipText.empty()) {
        result.quit = false;
        result.isHost = false;
        result.address = ipText;
        selectionMade = true;
        break;
      }
      if (ipHover) {
        editingIp = true;
      } else {
        editingIp = false;
      }
    }

    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Shared Grid Title
    const char *title = "Shared Grid";
    int titleWidth = MeasureText(title, 42);
    DrawText(title, kWindowSize / 2 - titleWidth / 2, 120, 42, DARKGRAY);
    DrawText("Choose how you want to play", 160, 170, 20, DARKGRAY);

    // Host Game Button
    Color hostColor = hostHover ? SKYBLUE : LIGHTGRAY;
    DrawRectangleRec(hostRect, hostColor);
    DrawRectangleLinesEx(hostRect, 2.0f, DARKGRAY);
    int hostTextWidth = MeasureText("Host Game", 24);
    DrawText(
        "Host Game",
        static_cast<int>(hostRect.x + hostRect.width / 2 - hostTextWidth / 2),
        static_cast<int>(hostRect.y + 18), 24, DARKGRAY);

    // Join Game Button
    Color joinColor = joinHover ? SKYBLUE : LIGHTGRAY;
    DrawRectangleRec(joinRect, joinColor);
    DrawRectangleLinesEx(joinRect, 2.0f, DARKGRAY);
    int joinTextWidth = MeasureText("Join Game", 24);
    DrawText(
        "Join Game",
        static_cast<int>(joinRect.x + joinRect.width / 2 - joinTextWidth / 2),
        static_cast<int>(joinRect.y + 18), 24, DARKGRAY);

    // Server Address Input
    DrawText("Server Address", static_cast<int>(ipRect.x),
             static_cast<int>(ipRect.y) - 28, 20, DARKGRAY);
    DrawRectangleRec(ipRect, editingIp ? Fade(SKYBLUE, 0.4f) : LIGHTGRAY);
    DrawRectangleLinesEx(ipRect, 2.0f, DARKGRAY);
    DrawText(ipText.c_str(), static_cast<int>(ipRect.x + 12),
             static_cast<int>(ipRect.y + 12), 24, DARKGRAY);

    if (editingIp) {
      int caretX =
          static_cast<int>(ipRect.x + 12 + MeasureText(ipText.c_str(), 24));
      DrawText("|", caretX, static_cast<int>(ipRect.y + 12), 24, DARKGRAY);
    }

    DrawText("Esc to quit", 20, kWindowSize - 40, 20, GRAY);

    EndDrawing();
  }

  if (!selectionMade) {
    result.quit = true;
  }

  CloseWindow();
  return result;
}

inline void ShowWaitingRoom(const char *msg) {
  BeginDrawing();
  ClearBackground(RAYWHITE);

  int textWidth = MeasureText(msg, 24);
  DrawText(msg, kWindowSize / 2 - textWidth / 2, kWindowSize / 2 - 12, 24,
           DARKGRAY);

  DrawCircle(kWindowSize / 2, kWindowSize / 2 + 60,
             10 + static_cast<int>(GetTime() * 4) % 10,
             SKYBLUE); // small pulse animation

  EndDrawing();
}

inline void DrawTransition(float timer) {
  BeginDrawing();
  ClearBackground(RAYWHITE); // white background for the transition

  const char *text = "BATTLE START!";
  int fontSize = 60;
  int textWidth = MeasureText(text, fontSize);

  float x = (kWindowSize - textWidth) / 2.0f;
  float y = kWindowSize / 2.0f - fontSize / 2.0f;

  // Main text: dark gray fade-in with pulse
  float pulse = 0.5f + 0.5f * sinf(timer * 3.0f);
  Color textColor = Fade(DARKGRAY, pulse);

  // “Shiny sweep” effect: a bright flash moving across the text
  float sweepX = fmodf(timer * 500.0f, kWindowSize + 250.0f) - 250.0f;
  Rectangle shineRect{sweepX, y - 40, 150, static_cast<float>(fontSize + 80)};

  // Draw text first
  DrawText(text, static_cast<int>(x), static_cast<int>(y), fontSize, textColor);

  // Add the "shing" flash
  DrawRectangleGradientEx(shineRect, Fade(WHITE, 0.0f), Fade(YELLOW, 0.5f),
                          Fade(YELLOW, 0.5f), Fade(WHITE, 0.0f));

  // Slight black overlay to create visual depth
  DrawRectangle(0, 0, kWindowSize, kWindowSize, Fade(BLACK, 0.1f));

  // Optional subtext
  const char *sub = "Get Ready to Fire!";
  int subFont = 24;
  int subWidth = MeasureText(sub, subFont);
  DrawText(sub, (kWindowSize - subWidth) / 2,
           static_cast<int>(y + fontSize + 30), subFont, GRAY);

  EndDrawing();
}

inline void DrawFinishedScreen(GameResult result, float timer) {
  BeginDrawing();
  ClearBackground(RAYWHITE);

  Color accentColor = (result == GameResult::Victory) ? DARKGREEN : MAROON;
  Color glowColor = Fade(accentColor, 0.12f + 0.08f * sinf(timer * 2.0f));
  DrawRectangle(0, 0, kWindowSize, kWindowSize, glowColor);

  const char *headline =
      (result == GameResult::Victory) ? "Victory!" : "Defeat";
  const char *subtext = (result == GameResult::Victory)
                            ? "You sank all enemy ships."
                            : "All of your ships have been sunk.";

  int headlineFont = 58;
  int headlineWidth = MeasureText(headline, headlineFont);
  int headlineX = (kWindowSize - headlineWidth) / 2;
  int headlineY = kWindowSize / 2 - 120;

  float pulse = 0.6f + 0.4f * sinf(timer * 3.0f);
  DrawText(headline, headlineX, headlineY, headlineFont,
           Fade(accentColor, pulse));

  int subFont = 26;
  int subWidth = MeasureText(subtext, subFont);
  DrawText(subtext, (kWindowSize - subWidth) / 2, headlineY + 80, subFont,
           DARKGRAY);

  const char *prompt = "Press Enter to exit the game";
  int promptFont = 22;
  int promptWidth = MeasureText(prompt, promptFont);
  int promptY = kWindowSize - 140;
  DrawText(prompt, (kWindowSize - promptWidth) / 2, promptY, promptFont,
           GRAY);

  // simple decorative orbs to match other screens' liveliness
  float orbRadius = 18.0f + 6.0f * sinf(timer * 1.5f);
  DrawCircle(kWindowSize / 2 - 150, headlineY + 120, orbRadius,
             Fade(SKYBLUE, 0.4f));
  DrawCircle(kWindowSize / 2 + 150, headlineY + 40, orbRadius,
             Fade(SKYBLUE, 0.3f));

  EndDrawing();
}
