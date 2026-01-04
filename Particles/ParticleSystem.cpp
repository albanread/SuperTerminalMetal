//
// ParticleSystem.cpp
// SuperTerminal v2.0 Framework
//
// Particle system implementation for sprite explosions and visual effects.
//
// Created by SuperTerminal Project
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "ParticleSystem.h"
#include "../Display/SpriteManager.h"
#include "../API/st_api_context.h"
#include <cmath>
#include <random>
#include <iostream>
#include <algorithm>

namespace SuperTerminal {

// =============================================================================
// Explosion Configuration Presets
// =============================================================================

ExplosionConfig ExplosionConfig::basicExplosion() {
    ExplosionConfig config;
    config.particleCount = 48;
    config.explosionForce = 200.0f;
    config.fadeTime = 2.0f;
    config.fragmentSizeMin = 0.5f;
    config.fragmentSizeMax = 1.5f;
    config.scaleMultiplier = 12.0f;
    return config;
}

ExplosionConfig ExplosionConfig::massiveBlast() {
    ExplosionConfig config;
    config.particleCount = 128;
    config.explosionForce = 350.0f;
    config.gravityStrength = 80.0f;
    config.fadeTime = 3.0f;
    config.fragmentSizeMin = 0.3f;
    config.fragmentSizeMax = 2.0f;
    config.scaleMultiplier = 15.0f;
    config.rotationSpeedMin = -4.0f;
    config.rotationSpeedMax = 4.0f;
    return config;
}

ExplosionConfig ExplosionConfig::gentleDispersal() {
    ExplosionConfig config;
    config.particleCount = 64;
    config.explosionForce = 120.0f;
    config.gravityStrength = 40.0f;
    config.fadeTime = 5.0f;
    config.fragmentSizeMin = 0.8f;
    config.fragmentSizeMax = 1.8f;
    config.scaleMultiplier = 10.0f;
    config.rotationSpeedMin = -1.0f;
    config.rotationSpeedMax = 1.0f;
    return config;
}

ExplosionConfig ExplosionConfig::rapidBurst() {
    ExplosionConfig config;
    config.particleCount = 32;
    config.explosionForce = 400.0f;
    config.gravityStrength = 200.0f;
    config.fadeTime = 1.0f;
    config.fragmentSizeMin = 0.3f;
    config.fragmentSizeMax = 0.8f;
    config.scaleMultiplier = 8.0f;
    config.rotationSpeedMin = -5.0f;
    config.rotationSpeedMax = 5.0f;
    return config;
}

ExplosionConfig ExplosionConfig::slowMotion() {
    ExplosionConfig config;
    config.particleCount = 60;
    config.explosionForce = 80.0f;
    config.gravityStrength = 30.0f;
    config.dragCoefficient = 0.95f;
    config.fadeTime = 8.0f;
    config.fragmentSizeMin = 0.8f;
    config.fragmentSizeMax = 2.0f;
    config.scaleMultiplier = 14.0f;
    config.rotationSpeedMin = -0.5f;
    config.rotationSpeedMax = 0.5f;
    return config;
}

ExplosionConfig ExplosionConfig::confetti() {
    ExplosionConfig config;
    config.particleCount = 100;
    config.explosionForce = 250.0f;
    config.gravityStrength = 150.0f;
    config.fadeTime = 4.0f;
    config.fragmentSizeMin = 0.2f;
    config.fragmentSizeMax = 0.6f;
    config.scaleMultiplier = 10.0f;
    config.rotationSpeedMin = -8.0f;
    config.rotationSpeedMax = 8.0f;
    config.mode = ParticleMode::POINT_SPRITE;
    return config;
}

// =============================================================================
// Random Number Generation
// =============================================================================

static std::random_device g_randomDevice;
static std::mt19937 g_randomEngine(g_randomDevice());

static float randomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(g_randomEngine);
}

// =============================================================================
// ParticleSystem Implementation
// =============================================================================

ParticleSystem::ParticleSystem()
    : m_maxParticles(0)
    , m_activeCount(0)
    , m_totalCreated(0)
    , m_initialized(false)
    , m_enabled(true)
    , m_paused(false)
    , m_timeScale(1.0f)
    , m_globalGravity(100.0f)
    , m_drag(0.98f)
    , m_worldWidth(1920.0f)
    , m_worldHeight(1080.0f)
    , m_explosionActive(false)
    , m_explosionSimTime(0.0f)
    , m_explosionUpdateCount(0)
    , m_spriteManager(nullptr)
{
}

ParticleSystem::~ParticleSystem() {
    shutdown();
}

bool ParticleSystem::initialize(uint32_t maxParticles) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        std::cerr << "ParticleSystem: Already initialized\n";
        return true;
    }
    
    m_maxParticles = maxParticles;
    m_particles.resize(maxParticles);
    m_activeCount = 0;
    m_totalCreated = 0;
    m_initialized = true;
    
    std::cout << "ParticleSystem: Initialized with " << maxParticles << " max particles\n";
    return true;
}

void ParticleSystem::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        return;
    }
    
    m_particles.clear();
    m_activeCount = 0;
    m_initialized = false;
    
    std::cout << "ParticleSystem: Shutdown complete\n";
}

void ParticleSystem::setSpriteManager(SpriteManager* spriteManager) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_spriteManager = spriteManager;
    if (spriteManager) {
        std::cout << "ParticleSystem: SpriteManager reference set\n";
    }
}

// =============================================================================
// Particle Creation (Coordinate-based)
// =============================================================================

bool ParticleSystem::explode(float x, float y, uint16_t particleCount, uint32_t color,
                             float force, float gravity, float fadeTime) {
    if (!m_initialized || !m_enabled) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    uint16_t spawned = 0;
    
    // Create particles in a radial pattern
    for (uint16_t i = 0; i < particleCount; ++i) {
        // Find inactive particle slot
        uint32_t index = 0;
        bool found = false;
        for (uint32_t j = 0; j < m_maxParticles; ++j) {
            if (!m_particles[j].active) {
                index = j;
                found = true;
                break;
            }
        }
        
        if (!found) {
            break; // Particle pool full
        }
        
        // Calculate random angle and velocity
        float angle = randomFloat(0.0f, 2.0f * M_PI);
        float speed = randomFloat(force * 0.5f, force);
        
        float vx = std::cos(angle) * speed;
        float vy = std::sin(angle) * speed;
        
        // Randomize particle properties
        float size = randomFloat(1.0f, 3.0f);
        float lifetime = randomFloat(fadeTime * 0.7f, fadeTime * 1.3f);
        
        // Vary color slightly
        uint8_t r = (color >> 24) & 0xFF;
        uint8_t g = (color >> 16) & 0xFF;
        uint8_t b = (color >> 8) & 0xFF;
        uint8_t a = color & 0xFF;
        
        r = std::min(255, std::max(0, (int)r + (int)randomFloat(-20, 20)));
        g = std::min(255, std::max(0, (int)g + (int)randomFloat(-20, 20)));
        b = std::min(255, std::max(0, (int)b + (int)randomFloat(-20, 20)));
        
        uint32_t particleColor = (r << 24) | (g << 16) | (b << 8) | a;
        
        // Initialize particle
        Particle& p = m_particles[index];
        p.x = x + randomFloat(-2.0f, 2.0f);
        p.y = y + randomFloat(-2.0f, 2.0f);
        p.vx = vx;
        p.vy = vy;
        p.life = lifetime;
        p.maxLife = lifetime;
        p.color = particleColor;
        p.size = size;
        p.active = true;
        
        spawned++;
        m_activeCount++;
        m_totalCreated++;
    }
    
    return spawned > 0;
}

bool ParticleSystem::explodeDirectional(float x, float y, uint16_t particleCount, uint32_t color,
                                       float forceX, float forceY, float gravity, float fadeTime) {
    if (!m_initialized || !m_enabled) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    uint16_t spawned = 0;
    
    // Create particles with directional bias
    for (uint16_t i = 0; i < particleCount; ++i) {
        // Find inactive particle slot
        uint32_t index = 0;
        bool found = false;
        for (uint32_t j = 0; j < m_maxParticles; ++j) {
            if (!m_particles[j].active) {
                index = j;
                found = true;
                break;
            }
        }
        
        if (!found) {
            break;
        }
        
        // Add randomness to force direction
        float spreadAngle = randomFloat(-0.5f, 0.5f); // +/- ~30 degrees
        float vx = forceX * randomFloat(0.7f, 1.3f) + std::cos(spreadAngle) * 50.0f;
        float vy = forceY * randomFloat(0.7f, 1.3f) + std::sin(spreadAngle) * 50.0f;
        
        float size = randomFloat(1.0f, 3.0f);
        float lifetime = randomFloat(fadeTime * 0.7f, fadeTime * 1.3f);
        
        // Vary color
        uint8_t r = (color >> 24) & 0xFF;
        uint8_t g = (color >> 16) & 0xFF;
        uint8_t b = (color >> 8) & 0xFF;
        uint8_t a = color & 0xFF;
        
        r = std::min(255, std::max(0, (int)r + (int)randomFloat(-20, 20)));
        g = std::min(255, std::max(0, (int)g + (int)randomFloat(-20, 20)));
        b = std::min(255, std::max(0, (int)b + (int)randomFloat(-20, 20)));
        
        uint32_t particleColor = (r << 24) | (g << 16) | (b << 8) | a;
        
        // Initialize particle
        Particle& p = m_particles[index];
        p.x = x + randomFloat(-2.0f, 2.0f);
        p.y = y + randomFloat(-2.0f, 2.0f);
        p.vx = vx;
        p.vy = vy;
        p.life = lifetime;
        p.maxLife = lifetime;
        p.color = particleColor;
        p.size = size;
        p.active = true;
        
        spawned++;
        m_activeCount++;
        m_totalCreated++;
    }
    
    return spawned > 0;
}

Particle* ParticleSystem::spawnParticle(float x, float y, float vx, float vy,
                                       uint32_t color, float size, float lifetime) {
    if (!m_initialized || !m_enabled) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Find inactive particle
    for (uint32_t i = 0; i < m_maxParticles; ++i) {
        if (!m_particles[i].active) {
            Particle& p = m_particles[i];
            p.x = x;
            p.y = y;
            p.vx = vx;
            p.vy = vy;
            p.life = lifetime;
            p.maxLife = lifetime;
            p.color = color;
            p.size = size;
            p.active = true;
            
            m_activeCount++;
            m_totalCreated++;
            
            return &p;
        }
    }
    
    return nullptr; // Pool full
}

// =============================================================================
// Particle Creation (Sprite-based - samples sprite texture)
// =============================================================================

bool ParticleSystem::explodeSprite(uint16_t spriteId, uint16_t particleCount,
                                   float force, float gravity, float fadeTime,
                                   ParticleMode mode) {
    std::cout << "ParticleSystem::explodeSprite: Called with spriteId=" << spriteId 
              << ", count=" << particleCount << ", mode=" << (int)mode << std::endl;
    
    std::cout << "ParticleSystem: Checking m_initialized=" << m_initialized 
              << ", m_enabled=" << m_enabled << std::endl;
    
    if (!m_initialized) {
        std::cerr << "ParticleSystem: FAILED - Not initialized\n";
        return false;
    }
    if (!m_enabled) {
        std::cerr << "ParticleSystem: FAILED - Not enabled\n";
        return false;
    }
    if (!m_spriteManager) {
        std::cerr << "ParticleSystem: FAILED - SpriteManager not set\n";
        return false;
    }
    
    std::cout << "ParticleSystem: Checking if sprite " << spriteId << " is loaded...\n";
    
    // Check if sprite exists and is loaded
    if (!m_spriteManager->isSpriteLoaded(spriteId)) {
        std::cerr << "ParticleSystem: FAILED - Sprite " << spriteId << " not loaded\n";
        return false;
    }
    
    std::cout << "ParticleSystem: Sprite " << spriteId << " is loaded, getting position...\n";
    
    // Get sprite position
    float spriteX = 0.0f, spriteY = 0.0f;
    m_spriteManager->getSpritePosition(spriteId, &spriteX, &spriteY);
    
    std::cout << "ParticleSystem: Sprite position: (" << spriteX << ", " << spriteY << ")\n";
    std::cout << "ParticleSystem: World bounds: " << m_worldWidth << "x" << m_worldHeight << "\n";
    std::cout << "ParticleSystem: NOTE - Sprites use 1920x1080 logical space, particles use actual viewport coordinates!\n";
    
    // Sample colors from sprite texture
    std::cout << "ParticleSystem: About to sample colors for sprite " << spriteId << std::endl;
    std::vector<uint32_t> colors = sampleSpriteColors(spriteId, particleCount);
    
    std::cout << "ParticleSystem: Sampled " << colors.size() << " colors\n";
    
    if (colors.empty()) {
        std::cerr << "ParticleSystem: FAILED - Failed to sample sprite colors for sprite " << spriteId << "\n";
        std::cout << "ParticleSystem: Falling back to white particles...\n";
        // Fall back to white particles
        return explode(spriteX, spriteY, particleCount, 0xFFFFFFFF, force, gravity, fadeTime);
    }
    
    // HIDE THE SPRITE before creating particles
    std::cout << "ParticleSystem: Hiding sprite " << spriteId << " before explosion\n";
    m_spriteManager->hideSprite(spriteId);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    uint16_t spawned = 0;
    
    std::cout << "ParticleSystem: Starting particle creation loop (count=" << particleCount 
              << ", activeCount=" << m_activeCount << ", maxParticles=" << m_maxParticles << ")\n";
    
    // Create particles in a radial pattern with sampled colors
    for (uint16_t i = 0; i < particleCount; ++i) {
        // Find inactive particle slot
        uint32_t index = 0;
        bool found = false;
        for (uint32_t j = 0; j < m_maxParticles; ++j) {
            if (!m_particles[j].active) {
                index = j;
                found = true;
                break;
            }
        }
        
        if (!found) {
            std::cerr << "ParticleSystem: Particle pool full after spawning " << spawned << " particles\n";
            break; // Particle pool full
        }
        
        // Calculate random angle and velocity
        float angle = randomFloat(0.0f, 2.0f * M_PI);
        float speed = randomFloat(force * 0.5f, force * 1.0f);
        
        float vx = std::cos(angle) * speed;
        float vy = std::sin(angle) * speed;
        
        // Randomize particle properties (smaller fragments)
        float size = randomFloat(0.5f, 1.5f);
        float lifetime = randomFloat(fadeTime * 0.7f, fadeTime * 1.3f);
        
        // Use sampled color
        uint32_t particleColor = colors[i % colors.size()];
        
        // Initialize particle
        Particle& p = m_particles[index];
        p.x = spriteX + randomFloat(-2.0f, 2.0f);
        p.y = spriteY + randomFloat(-2.0f, 2.0f);
        p.vx = vx;
        p.vy = vy;
        p.life = lifetime;
        p.maxLife = lifetime;
        p.color = particleColor;
        p.size = size;
        p.active = true;
        p.mode = mode;
        p.sourceSprite = spriteId;
        p.scaleMultiplier = 12.0f;
        
        // Initialize rotation
        p.rotation = randomFloat(0.0f, 2.0f * M_PI);
        p.angularVelocity = randomFloat(-2.0f, 2.0f);
        
        // Generate texture coordinates for sprite fragment mode
        if (mode == ParticleMode::SPRITE_FRAGMENT) {
            generateFragmentTexCoords(p, size * 0.05f);
        }
        
        spawned++;
        m_activeCount++;
        m_totalCreated++;
    }
    
    std::cout << "ParticleSystem: Exploded sprite " << spriteId 
              << " at (" << spriteX << ", " << spriteY 
              << ") with " << spawned << " particles\n";
    
    // Debug: Print first few particles
    if (spawned > 0) {
        std::cout << "ParticleSystem: First 3 particles:\n";
        int debugCount = 0;
        for (uint32_t i = 0; i < m_maxParticles && debugCount < 3; ++i) {
            if (m_particles[i].active && m_particles[i].sourceSprite == spriteId) {
                const auto& p = m_particles[i];
                std::cout << "  Particle " << debugCount << ": pos=(" << p.x << ", " << p.y 
                          << ") vel=(" << p.vx << ", " << p.vy << ") size=" << p.size 
                          << " life=" << p.life << "/" << p.maxLife 
                          << " color=0x" << std::hex << p.color << std::dec << "\n";
                debugCount++;
            }
        }
    }
    
    // Start timing this explosion
    if (spawned > 0 && !m_explosionActive) {
        m_explosionActive = true;
        m_explosionSimTime = 0.0f;
        m_explosionUpdateCount = 0;
        m_explosionStartTime = std::chrono::steady_clock::now();
        std::cout << "[TIMING] Explosion started - tracking timing..." << std::endl;
    }
    
    return spawned > 0;
}

bool ParticleSystem::explodeSpriteDirectional(uint16_t spriteId, uint16_t particleCount,
                                              float forceX, float forceY, float gravity, float fadeTime,
                                              ParticleMode mode) {
    if (!m_initialized || !m_enabled || !m_spriteManager) {
        if (!m_spriteManager) {
            std::cerr << "ParticleSystem: Cannot explode sprite - SpriteManager not set\n";
        }
        return false;
    }
    
    // Check if sprite exists and is loaded
    if (!m_spriteManager->isSpriteLoaded(spriteId)) {
        std::cerr << "ParticleSystem: Sprite " << spriteId << " not loaded\n";
        return false;
    }
    
    // Get sprite position
    float spriteX = 0.0f, spriteY = 0.0f;
    m_spriteManager->getSpritePosition(spriteId, &spriteX, &spriteY);
    
    // Sample colors from sprite texture
    std::vector<uint32_t> colors = sampleSpriteColors(spriteId, particleCount);
    
    if (colors.empty()) {
        std::cerr << "ParticleSystem: Failed to sample sprite colors for sprite " << spriteId << "\n";
        // Fall back to white particles
        return explodeDirectional(spriteX, spriteY, particleCount, 0xFFFFFFFF, forceX, forceY, gravity, fadeTime);
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    uint16_t spawned = 0;
    
    // Create particles with directional bias and sampled colors
    for (uint16_t i = 0; i < particleCount; ++i) {
        // Find inactive particle slot
        uint32_t index = 0;
        bool found = false;
        for (uint32_t j = 0; j < m_maxParticles; ++j) {
            if (!m_particles[j].active) {
                index = j;
                found = true;
                break;
            }
        }
        
        if (!found) {
            break;
        }
        
        // Add randomness to force direction
        float spreadAngle = randomFloat(-0.5f, 0.5f); // +/- ~30 degrees
        float vx = forceX * randomFloat(0.7f, 1.3f) + std::cos(spreadAngle) * 50.0f;
        float vy = forceY * randomFloat(0.7f, 1.3f) + std::sin(spreadAngle) * 50.0f;
        
        float size = randomFloat(1.5f, 3.5f);
        float lifetime = randomFloat(fadeTime * 0.7f, fadeTime * 1.3f);
        
        // Use sampled color
        uint32_t particleColor = colors[i % colors.size()];
        
        // Initialize particle
        Particle& p = m_particles[index];
        p.x = spriteX + randomFloat(-2.0f, 2.0f);
        p.y = spriteY + randomFloat(-2.0f, 2.0f);
        p.vx = vx;
        p.vy = vy;
        p.life = lifetime;
        p.maxLife = lifetime;
        p.color = particleColor;
        p.size = size;
        p.active = true;
        p.mode = mode;
        p.sourceSprite = spriteId;
        p.scaleMultiplier = 12.0f;
        
        // Initialize rotation
        p.rotation = randomFloat(0.0f, 2.0f * M_PI);
        p.angularVelocity = randomFloat(-2.0f, 2.0f);
        
        // Generate texture coordinates for sprite fragment mode
        if (mode == ParticleMode::SPRITE_FRAGMENT) {
            generateFragmentTexCoords(p, size * 0.05f);
        }
        
        spawned++;
        m_activeCount++;
        m_totalCreated++;
    }
    
    std::cout << "ParticleSystem: Exploded sprite " << spriteId 
              << " directionally at (" << spriteX << ", " << spriteY 
              << ") with " << spawned << " particles\n";
    
    return spawned > 0;
}

bool ParticleSystem::explodeSpriteCustom(uint16_t spriteId, const ExplosionConfig& config) {
    std::cout << "ParticleSystem::explodeSpriteCustom: Called with spriteId=" << spriteId 
              << ", count=" << config.particleCount << std::endl;
    
    if (!m_initialized) {
        std::cerr << "ParticleSystem: Not initialized\n";
        return false;
    }
    if (!m_enabled) {
        std::cerr << "ParticleSystem: Not enabled\n";
        return false;
    }
    if (!m_spriteManager) {
        std::cerr << "ParticleSystem: Cannot explode sprite - SpriteManager not set\n";
        return false;
    }
    
    // Check if sprite exists and is loaded
    if (!m_spriteManager->isSpriteLoaded(spriteId)) {
        std::cerr << "ParticleSystem: Sprite " << spriteId << " not loaded\n";
        return false;
    }
    
    // Get sprite position
    float spriteX = 0.0f, spriteY = 0.0f;
    m_spriteManager->getSpritePosition(spriteId, &spriteX, &spriteY);
    
    // Sample colors from sprite texture
    std::vector<uint32_t> colors = sampleSpriteColors(spriteId, config.particleCount);
    
    if (colors.empty()) {
        std::cerr << "ParticleSystem: Failed to sample sprite colors for sprite " << spriteId << "\n";
        return explode(spriteX, spriteY, config.particleCount, 0xFFFFFFFF, 
                      config.explosionForce, config.gravityStrength, config.fadeTime);
    }
    
    // HIDE THE SPRITE before creating particles
    std::cout << "ParticleSystem: Hiding sprite " << spriteId << " before explosion\n";
    m_spriteManager->hideSprite(spriteId);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    uint16_t spawned = 0;
    
    // Create particles with custom configuration
    for (uint16_t i = 0; i < config.particleCount; ++i) {
        // Find inactive particle slot
        uint32_t index = 0;
        bool found = false;
        for (uint32_t j = 0; j < m_maxParticles; ++j) {
            if (!m_particles[j].active) {
                index = j;
                found = true;
                break;
            }
        }
        
        if (!found) {
            break; // Particle pool full
        }
        
        // Calculate explosion direction based on config
        float angle = randomFloat(0.0f, 2.0f * M_PI);
        float forceMin = config.explosionForce * (1.0f - config.forceVariation);
        float forceMax = config.explosionForce * (1.0f + config.forceVariation);
        float speed = randomFloat(forceMin, forceMax);
        
        float vx = std::cos(angle) * speed;
        float vy = std::sin(angle) * speed;
        
        // Apply directional bias
        vx += config.directionX * config.explosionForce * 0.5f;
        vy += config.directionY * config.explosionForce * 0.5f;
        
        // Randomize particle properties
        float size = randomFloat(config.fragmentSizeMin, config.fragmentSizeMax);
        float lifetimeMin = config.fadeTime * (1.0f - config.lifetimeVariation);
        float lifetimeMax = config.fadeTime * (1.0f + config.lifetimeVariation);
        float lifetime = randomFloat(lifetimeMin, lifetimeMax);
        
        // DEBUG: Log first particle's lifetime
        if (i == 0) {
            std::cout << "ParticleSystem: First particle lifetime=" << lifetime 
                      << "s (fadeTime=" << config.fadeTime << "s)" << std::endl;
        }
        
        // Use sampled color
        uint32_t particleColor = colors[i % colors.size()];
        
        // Initialize particle
        Particle& p = m_particles[index];
        p.x = spriteX + randomFloat(-2.0f, 2.0f);
        p.y = spriteY + randomFloat(-2.0f, 2.0f);
        p.vx = vx;
        p.vy = vy;
        p.life = lifetime;
        p.maxLife = lifetime;
        p.color = particleColor;
        p.size = size;
        p.active = true;
        p.mode = config.mode;
        p.sourceSprite = spriteId;
        p.scaleMultiplier = config.scaleMultiplier;
        
        // Initialize rotation
        p.rotation = randomFloat(0.0f, 2.0f * M_PI);
        p.angularVelocity = randomFloat(config.rotationSpeedMin, config.rotationSpeedMax);
        
        // Generate texture coordinates for sprite fragment mode
        if (config.mode == ParticleMode::SPRITE_FRAGMENT) {
            generateFragmentTexCoords(p, size * 0.05f);
        }
        
        spawned++;
        m_activeCount++;
        m_totalCreated++;
    }
    
    std::cout << "ParticleSystem: Exploded sprite " << spriteId 
              << " at (" << spriteX << ", " << spriteY 
              << ") with " << spawned << " particles (custom config)\n";
    
    return spawned > 0;
}

std::vector<uint32_t> ParticleSystem::sampleSpriteColors(uint16_t spriteId, uint16_t sampleCount) {
    std::vector<uint32_t> colors;
    
    if (!m_spriteManager) {
        return colors;
    }
    
    // Get sprite dimensions
    int width = 0, height = 0;
    m_spriteManager->getSpriteSize(spriteId, &width, &height);
    
    std::cout << "ParticleSystem: Sprite " << spriteId << " dimensions: " << width << "x" << height << std::endl;
    
    if (width <= 0 || height <= 0) {
        std::cout << "ParticleSystem: Invalid sprite dimensions, returning empty colors" << std::endl;
        return colors;
    }
    
    colors.reserve(sampleCount);
    
    // Check if sprite is indexed - if so, sample from its palette
    bool isIndexed = m_spriteManager->isSpriteIndexed(spriteId);
    std::cout << "ParticleSystem: Sprite " << spriteId << " isIndexed=" << isIndexed << std::endl;
    
    if (isIndexed) {
        uint8_t palette[64];  // 16 colors × 4 bytes (RGBA)
        
        bool gotPalette = m_spriteManager->getSpritePalette(spriteId, palette);
        std::cout << "ParticleSystem: getSpritePalette returned " << gotPalette << std::endl;
        
        if (gotPalette) {
            std::cout << "ParticleSystem: Sampling from indexed sprite palette" << std::endl;
            
            // Debug: Print first 4 palette colors
            for (int i = 0; i < 4; i++) {
                int offset = i * 4;
                std::cout << "  Palette[" << i << "]: R=" << (int)palette[offset]
                         << " G=" << (int)palette[offset+1]
                         << " B=" << (int)palette[offset+2]
                         << " A=" << (int)palette[offset+3] << std::endl;
            }
            
            // Sample from the sprite's actual palette (skip index 0 which is typically transparent)
            for (uint16_t i = 0; i < sampleCount; ++i) {
                // Use palette indices 1-15 (avoiding 0 which is usually transparent)
                int paletteIndex = 1 + (i % 15);
                int offset = paletteIndex * 4;
                
                uint8_t r = palette[offset + 0];
                uint8_t g = palette[offset + 1];
                uint8_t b = palette[offset + 2];
                uint8_t a = palette[offset + 3];
                
                uint32_t color = (r << 24) | (g << 16) | (b << 8) | a;
                colors.push_back(color);
            }
            
            return colors;
        } else {
            std::cout << "ParticleSystem: getSpritePalette failed for indexed sprite" << std::endl;
        }
    } else {
        std::cout << "ParticleSystem: Sprite is not indexed, using fallback colors" << std::endl;
    }
    
    // Fallback: For RGB sprites or if palette reading fails, use generic explosion colors
    // This matches the existing behavior for non-indexed sprites
    std::vector<uint32_t> baseColors = {
        0xFF6B35FF,  // Orange-red
        0xFFB85CFF,  // Light orange
        0xFFE156FF,  // Yellow
        0xFF4444FF,  // Red
        0xFFAA00FF,  // Amber
        0xFFFFAAFF,  // Light yellow
        0xFFCC88FF,  // Peach
        0xFF8844FF,  // Dark orange
    };
    
    // Sample from base colors with variations
    for (uint16_t i = 0; i < sampleCount; ++i) {
        uint32_t baseColor = baseColors[i % baseColors.size()];
        
        // Extract RGBA components
        uint8_t r = (baseColor >> 24) & 0xFF;
        uint8_t g = (baseColor >> 16) & 0xFF;
        uint8_t b = (baseColor >> 8) & 0xFF;
        uint8_t a = baseColor & 0xFF;
        
        // Add random variation
        r = std::min(255, std::max(0, (int)r + (int)randomFloat(-30, 30)));
        g = std::min(255, std::max(0, (int)g + (int)randomFloat(-30, 30)));
        b = std::min(255, std::max(0, (int)b + (int)randomFloat(-30, 30)));
        
        uint32_t sampledColor = (r << 24) | (g << 16) | (b << 8) | a;
        colors.push_back(sampledColor);
    }
    
    return colors;
}

void ParticleSystem::generateFragmentTexCoords(Particle& particle, float fragmentSize) {
    // Generate random texture coordinate center
    float centerX = randomFloat(0.0f, 1.0f);
    float centerY = randomFloat(0.0f, 1.0f);
    
    // Create a small quad around the center
    float halfSize = fragmentSize * 0.5f;
    particle.texCoordMinX = std::max(0.0f, centerX - halfSize);
    particle.texCoordMinY = std::max(0.0f, centerY - halfSize);
    particle.texCoordMaxX = std::min(1.0f, centerX + halfSize);
    particle.texCoordMaxY = std::min(1.0f, centerY + halfSize);
}

// =============================================================================
// Simulation & Rendering
// =============================================================================

void ParticleSystem::update(float deltaTime) {
    if (!m_initialized || !m_enabled || m_paused) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Use actual deltaTime for frame-rate independent animation
    float dt = deltaTime * m_timeScale;
    
    // Track explosion timing
    if (m_explosionActive) {
        m_explosionSimTime += dt;
        m_explosionUpdateCount++;
    }
    
    // Update all active particles
    for (uint32_t i = 0; i < m_maxParticles; ++i) {
        Particle& p = m_particles[i];
        
        if (!p.active) {
            continue;
        }
        
        // Update lifetime
        p.life -= dt;
        
        if (p.life <= 0.0f) {
            p.active = false;
            m_activeCount--;
            continue;
        }
        
        // Apply gravity
        p.vy += m_globalGravity * dt;
        
        // Apply drag (frame-rate independent using exponential decay)
        // Convert per-frame drag to per-second drag: drag^(dt * 60)
        float dragFactor = std::pow(m_drag, dt * 60.0f);
        p.vx *= dragFactor;
        p.vy *= dragFactor;
        
        // Update position
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        
        // Update rotation
        p.rotation += p.angularVelocity * dt;
        
        // Fade alpha based on remaining lifetime (use 255 as base, not current alpha!)
        float lifeRatio = p.life / p.maxLife;
        uint8_t alpha = (uint8_t)(255.0f * lifeRatio);
        p.color = (p.color & 0xFFFFFF00) | alpha;
        
        // Cull particles outside world bounds (with margin)
        float margin = 100.0f;
        if (p.x < -margin || p.x > m_worldWidth + margin ||
            p.y < -margin || p.y > m_worldHeight + margin) {
            p.active = false;
            m_activeCount--;
        }
    }
    
    // Check if explosion just finished
    if (m_explosionActive && m_activeCount == 0) {
        auto endTime = std::chrono::steady_clock::now();
        auto realElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - m_explosionStartTime).count();
        
        // Optional: Log timing results (can be disabled in production)
        // std::cout << "[Explosion] Completed in " << (realElapsed / 1000.0f) 
        //           << "s real time, " << m_explosionSimTime << "s sim time\n";
        
        m_explosionActive = false;
    }
}

const std::vector<Particle>& ParticleSystem::getActiveParticles() const {
    return m_particles;
}

// =============================================================================
// System Control
// =============================================================================

void ParticleSystem::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& p : m_particles) {
        p.active = false;
    }
    
    m_activeCount = 0;
}

void ParticleSystem::setTimeScale(float scale) {
    m_timeScale = std::max(0.1f, std::min(5.0f, scale));
}

void ParticleSystem::setWorldBounds(float width, float height) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_worldWidth = width;
    m_worldHeight = height;
}

void ParticleSystem::dumpStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::cout << "\n=== Particle System Statistics ===\n";
    std::cout << "Initialized: " << (m_initialized ? "Yes" : "No") << "\n";
    std::cout << "Enabled: " << (m_enabled ? "Yes" : "No") << "\n";
    std::cout << "Paused: " << (m_paused ? "Yes" : "No") << "\n";
    std::cout << "Max Particles: " << m_maxParticles << "\n";
    std::cout << "Active Particles: " << m_activeCount << "\n";
    std::cout << "Total Created: " << m_totalCreated << "\n";
    std::cout << "Time Scale: " << m_timeScale << "\n";
    std::cout << "Gravity: " << m_globalGravity << "\n";
    std::cout << "Drag: " << m_drag << "\n";
    std::cout << "World Bounds: " << m_worldWidth << "x" << m_worldHeight << "\n";
    std::cout << "===================================\n\n";
}

} // namespace SuperTerminal

// =============================================================================
// C API Implementation
// =============================================================================

using namespace SuperTerminal;

// Global particle system instance
static std::unique_ptr<ParticleSystem> g_particleSystem = nullptr;

bool st_particle_system_initialize(uint32_t maxParticles) {
    if (g_particleSystem) {
        return true; // Already initialized
    }
    
    g_particleSystem = std::make_unique<ParticleSystem>();
    return g_particleSystem->initialize(maxParticles);
}

void st_particle_system_shutdown() {
    if (g_particleSystem) {
        g_particleSystem->shutdown();
        g_particleSystem.reset();
    }
}

bool st_particle_system_is_ready() {
    return g_particleSystem && g_particleSystem->isInitialized();
}

bool st_sprite_explode(float x, float y, uint16_t particleCount, uint32_t color) {
    if (!g_particleSystem) {
        return false;
    }
    return g_particleSystem->explode(x, y, particleCount, color);
}

bool st_sprite_explode_advanced(float x, float y, uint16_t particleCount, uint32_t color,
                               float force, float gravity, float fadeTime) {
    if (!g_particleSystem) {
        return false;
    }
    return g_particleSystem->explode(x, y, particleCount, color, force, gravity, fadeTime);
}

bool st_sprite_explode_directional(float x, float y, uint16_t particleCount, uint32_t color,
                                   float forceX, float forceY) {
    if (!g_particleSystem) {
        return false;
    }
    return g_particleSystem->explodeDirectional(x, y, particleCount, color, forceX, forceY);
}

void st_particle_clear() {
    if (g_particleSystem) {
        g_particleSystem->clear();
    }
}

void st_particle_pause() {
    if (g_particleSystem) {
        g_particleSystem->pause();
    }
}

void st_particle_resume() {
    if (g_particleSystem) {
        g_particleSystem->resume();
    }
}

void st_particle_set_time_scale(float scale) {
    if (g_particleSystem) {
        g_particleSystem->setTimeScale(scale);
    }
}

void st_particle_set_world_bounds(float width, float height) {
    if (g_particleSystem) {
        g_particleSystem->setWorldBounds(width, height);
    }
}

void st_particle_set_enabled(bool enabled) {
    if (g_particleSystem) {
        g_particleSystem->setEnabled(enabled);
    }
}

uint32_t st_particle_get_active_count() {
    if (g_particleSystem) {
        return g_particleSystem->getActiveCount();
    }
    return 0;
}

uint64_t st_particle_get_total_created() {
    if (g_particleSystem) {
        return g_particleSystem->getTotalCreated();
    }
    return 0;
}

void st_particle_dump_stats() {
    if (g_particleSystem) {
        g_particleSystem->dumpStats();
    }
}

extern "C" void st_particle_system_update(float deltaTime) {
    if (g_particleSystem) {
        static int callCount = 0;
        static auto lastSecond = std::chrono::steady_clock::now();
        static int callsThisSecond = 0;
        
        callsThisSecond++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSecond).count();
        
        if (elapsed >= 1000) {
            // Optional: Log update rate for diagnostics
            // std::cout << "st_particle_system_update: " << callsThisSecond << " calls/sec\n";
            callsThisSecond = 0;
            lastSecond = now;
        }
        
        callCount++;
        g_particleSystem->update(deltaTime);
    }
}

extern "C" bool st_particle_get_render_data(const void** outParticles, uint32_t* outCount) {
    if (!g_particleSystem || !outParticles || !outCount) {
        return false;
    }
    
    const auto& particles = g_particleSystem->getActiveParticles();
    *outParticles = particles.data();
    *outCount = g_particleSystem->getActiveCount();
    return true;
}

extern "C" void st_particle_system_set_sprite_manager(void* spriteManager) {
    if (g_particleSystem) {
        g_particleSystem->setSpriteManager(static_cast<SuperTerminal::SpriteManager*>(spriteManager));
    }
}

// =============================================================================
// Sprite-based explosion API (v1 compatible)
// =============================================================================

extern "C" bool sprite_explode(uint16_t spriteHandle, uint16_t particleCount) {
    std::cout << "[sprite_explode C API] Called with spriteHandle=" << spriteHandle 
              << ", particleCount=" << particleCount << std::endl;
    
    if (!g_particleSystem) {
        std::cerr << "[sprite_explode C API] ERROR: g_particleSystem is NULL!" << std::endl;
        return false;
    }
    
    // Convert handle to actual sprite ID
    int spriteId = ST_CONTEXT.getSpriteId(spriteHandle);
    if (spriteId < 0) {
        std::cerr << "[sprite_explode C API] ERROR: Invalid sprite handle " << spriteHandle << std::endl;
        return false;
    }
    
    std::cout << "[sprite_explode C API] Converted handle " << spriteHandle 
              << " to sprite ID " << spriteId << std::endl;
    std::cout << "[sprite_explode C API] g_particleSystem is valid, calling explodeSprite..." << std::endl;
    
    // Use POINT_SPRITE mode for colored particle explosions
    bool result = g_particleSystem->explodeSprite(static_cast<uint16_t>(spriteId), particleCount, 200.0f, 100.0f, 4.0f,
                                          SuperTerminal::ParticleMode::POINT_SPRITE);
    
    std::cout << "[sprite_explode C API] explodeSprite returned: " << (result ? "true" : "false") << std::endl;
    return result;
}

extern "C" bool sprite_explode_advanced(uint16_t spriteHandle, uint16_t particleCount,
                                       float explosionForce, float gravity, float fadeTime) {
    if (!g_particleSystem) {
        return false;
    }
    
    // Convert handle to actual sprite ID
    int spriteId = ST_CONTEXT.getSpriteId(spriteHandle);
    if (spriteId < 0) {
        return false;
    }
    
    // Use POINT_SPRITE mode for colored particle explosions
    return g_particleSystem->explodeSprite(static_cast<uint16_t>(spriteId), particleCount, explosionForce, gravity, fadeTime,
                                          SuperTerminal::ParticleMode::POINT_SPRITE);
}

extern "C" bool sprite_explode_directional(uint16_t spriteHandle, uint16_t particleCount,
                                          float forceX, float forceY) {
    if (!g_particleSystem) {
        return false;
    }
    
    // Convert handle to actual sprite ID
    int spriteId = ST_CONTEXT.getSpriteId(spriteHandle);
    if (spriteId < 0) {
        return false;
    }
    
    // Use POINT_SPRITE mode for colored particle explosions
    return g_particleSystem->explodeSpriteDirectional(static_cast<uint16_t>(spriteId), particleCount, forceX, forceY,
                                                      100.0f, 4.0f, SuperTerminal::ParticleMode::POINT_SPRITE);
}

extern "C" bool sprite_explode_size(uint16_t spriteHandle, uint16_t particleCount, float sizeMultiplier) {
    if (!g_particleSystem) {
        return false;
    }
    
    // Convert handle to actual sprite ID
    int spriteId = ST_CONTEXT.getSpriteId(spriteHandle);
    if (spriteId < 0) {
        return false;
    }
    
    // Create custom config with specified size
    SuperTerminal::ExplosionConfig config;
    config.particleCount = particleCount;
    config.explosionForce = 200.0f;
    config.gravityStrength = 100.0f;
    config.fadeTime = 4.0f;
    config.fragmentSizeMin = 0.5f;
    config.fragmentSizeMax = 1.5f;
    config.scaleMultiplier = sizeMultiplier;
    config.mode = SuperTerminal::ParticleMode::POINT_SPRITE;
    
    return g_particleSystem->explodeSpriteCustom(static_cast<uint16_t>(spriteId), config);
}

extern "C" bool sprite_explode_custom(uint16_t spriteId, const SuperTerminal::ExplosionConfig* config) {
    if (!g_particleSystem || !config) {
        return false;
    }
    return g_particleSystem->explodeSpriteCustom(spriteId, *config);
}