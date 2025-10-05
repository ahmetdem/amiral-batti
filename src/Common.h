#pragma once

#include <cstdint>
#include <iostream>
#include <vector>

inline void printShipLocations(const std::vector<std::uint8_t> &shipLocations) {
  std::cout << "shipLocationsServer contents (" << shipLocations.size()
            << " elements): ";

  for (std::uint8_t value : shipLocations) {
    std::cout << static_cast<int>(value)
              << " "; // convert to int for readable output
  }

  std::cout << std::endl;
}
