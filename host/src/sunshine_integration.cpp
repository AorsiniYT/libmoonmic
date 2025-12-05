/**
 * @file sunshine_integration.cpp
 * @brief Simplified Sunshine integration implementation
 */

#include "sunshine_integration.h"
#include "config.h"
#include <iostream>

namespace moonmic {

SunshineIntegration::SunshineIntegration(Config& config) 
    : config_(config) {
    std::cout << "[Sunshine] Integration initialized (using config only)" << std::endl;
}

void SunshineIntegration::reload() {
    // Nothing to reload - we just read from config
    std::cout << "[Sunshine] Status: " << (isPaired() ? "Paired" : "Not Paired") << std::endl;
}

bool SunshineIntegration::isPaired() const {
    return config_.sunshine.paired;
}

} // namespace moonmic
