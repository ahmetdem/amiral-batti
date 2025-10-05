#include "GameLogic.h"
#include "GameState.h"

#include <enet/enet.h>
#include <cstdio>

int main() {
  if (enet_initialize() != 0) {
    std::fprintf(stderr, "Failed to initialise ENet\n");
    return 1;
  }

  MenuResult menu = ShowMainMenu();
  int result = 0;

  if (!menu.quit) {
    if (menu.isHost) {
      result = RunServer();
    } else {
      const char *address =
          menu.address.empty() ? "127.0.0.1" : menu.address.c_str();
      result = RunClient(address);
    }
  }

  enet_deinitialize();
  return result;
}

