//
// ParticleSystem.h
// SuperTerminal v2.0 Framework
//
// Particle system for sprite explosions and visual effects.
// Provides both C++ and C API for cross-language compatibility.
//
// Features:
// - Sprite explosion effects (radial, directional, custom)
// - Physics simulation (gravity, velocity, drag)
// - Particle lifecycle management (spawn, update, fade, destroy)
// - Efficient rendering integration with Metal
// - Thread-safe API for use from script threads
//
// Created by SuperTerminal Project
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef SUPERTERMINAL_PARTICLE_SYSTEM_H
#define SUPERTERMINAL_PARTICLE_SYSTEM_H

#include <cstdint>
#include <vector>
#include <mutex>
#include <memory>
#include <chrono>

namespace SuperTerminal {

// Forward declarations
class SpriteManager;

// =============================================================================
// Explosion Mode
// =============================================================================

/// Particle rendering mode
enum class ParticleMode {
    POINT_SPRITE,      // Circular point sprites (v2 style) - good for pure explosions
    SPRITE_FRAGMENT    // Textured sprite fragments (v1 style) - sprite shattering effect
};

// =============================================================================
// Explosion Configuration
// =============================================================================

/// Configuration for particle explosions - exposes all parameters for customization
struct ExplosionConfig {
    // Particle count
    uint16_t particleCount;        // Number of particles to create (1-500)
    
    // Physics parameters
    float explosionForce;          // Initial velocity magnitude (50-1000)
    float forceVariation;          // Velocity randomization (0.0-1.0, default 0.5)
    float gravityStrength;         // Gravity strength (0-500, default 100)
    float dragCoefficient;         // Air resistance (0.9-0.999, default 0.98)
    
    // Lifetime parameters
    float fadeTime;                // Base lifetime in seconds (0.5-10.0)
    float lifetimeVariation;       // Lifetime randomization (0.0-1.0, default 0.3)
    
    // Visual parameters
    float fragmentSizeMin;         // Minimum particle size (0.1-5.0)
    float fragmentSizeMax;         // Maximum particle size (0.1-5.0)
    float scaleMultiplier;         // Rendering scale (1.0-50.0, default 12.0)
    
    // Rotation parameters
    float rotationSpeedMin;        // Minimum angular velocity in rad/s (-10 to 10)
    float rotationSpeedMax;        // Maximum angular velocity in rad/s (-10 to 10)
    
    // Directional bias (for directional explosions)
    float directionX;              // Horizontal force bias (-1.0 to 1.0)
    float directionY;              // Vertical force bias (-1.0 to 1.0)
    
    // Rendering mode
    ParticleMode mode;             // Point sprite or sprite fragment
    
    // Constructor with sensible defaults
    ExplosionConfig() 
        : particleCount(50)
        , explosionForce(200.0f)
        , forceVariation(0.5f)
        , gravityStrength(100.0f)
        , dragCoefficient(0.98f)
        , fadeTime(4.0f)
        , lifetimeVariation(0.3f)
        , fragmentSizeMin(0.5f)
        , fragmentSizeMax(1.5f)
        , scaleMultiplier(12.0f)
        , rotationSpeedMin(-2.0f)
        , rotationSpeedMax(2.0f)
        , directionX(0.0f)
        , directionY(0.0f)
        , mode(ParticleMode::SPRITE_FRAGMENT)
    {}
    
    // Preset configurations
    static ExplosionConfig basicExplosion();
    static ExplosionConfig massiveBlast();
    static ExplosionConfig gentleDispersal();
    static ExplosionConfig rapidBurst();
    static ExplosionConfig slowMotion();
    static ExplosionConfig confetti();
};

// =============================================================================
// Particle Structure
// =============================================================================

/// Individual particle data
struct Particle {
    float x, y;                // Position
    float vx, vy;              // Velocity
    float life;                // Remaining lifetime (seconds)
    float maxLife;             // Initial lifetime
    uint32_t color;            // RGBA color
    float size;                // Particle size (radius)
    bool active;               // Is this particle alive?
    
    // Texture coordinates (for sprite fragment mode)
    float texCoordMinX, texCoordMinY;
    float texCoordMaxX, texCoordMaxY;
    
    // Rotation
    float rotation;            // Current rotation in radians
    float angularVelocity;     // Rotation speed
    
    // Rendering mode
    ParticleMode mode;
    uint16_t sourceSprite;     // Source sprite ID (for fragment mode)
    float scaleMultiplier;     // Rendering scale multiplier
    
    Particle()
        : x(0), y(0)
        , vx(0), vy(0)
        , life(0), maxLife(0)
        , color(0xFFFFFFFF)
        , size(1.0f)
        , active(false)
        , texCoordMinX(0), texCoordMinY(0)
        , texCoordMaxX(1), texCoordMaxY(1)
        , rotation(0), angularVelocity(0)
        , mode(ParticleMode::POINT_SPRITE)
        , sourceSprite(0)
        , scaleMultiplier(12.0f)
    {}
};

// =============================================================================
// Particle System Class
// =============================================================================

/// Manages particle creation, physics, and lifecycle
class ParticleSystem {
public:
    /// Constructor
    ParticleSystem();
    
    /// Destructor
    ~ParticleSystem();
    
    // Disable copy, allow move
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;
    ParticleSystem(ParticleSystem&&) noexcept = default;
    ParticleSystem& operator=(ParticleSystem&&) noexcept = default;
    
    // =========================================================================
    // Initialization & Shutdown
    // =========================================================================
    
    /// Initialize the particle system
    /// @param maxParticles Maximum number of concurrent particles
    /// @return true if initialization succeeded
    bool initialize(uint32_t maxParticles = 2048);
    
    /// Check if system is initialized
    bool isInitialized() const { return m_initialized; }
    
    /// Shutdown and free resources
    void shutdown();
    
    /// Set sprite manager reference (required for sprite-based explosions)
    /// @param spriteManager Pointer to sprite manager instance
    void setSpriteManager(SpriteManager* spriteManager);
    
    // =========================================================================
    // Particle Creation (Coordinate-based)
    // =========================================================================
    
    /// Create a radial explosion from a sprite position
    /// @param x Center X position
    /// @param y Center Y position
    /// @param particleCount Number of particles to spawn
    /// @param color Base color (can be 0 for sprite sampling)
    /// @param force Explosion force (velocity magnitude)
    /// @param gravity Gravity strength
    /// @param fadeTime Particle lifetime in seconds
    /// @return true if particles were created
    bool explode(float x, float y, uint16_t particleCount, uint32_t color,
                float force = 200.0f, float gravity = 100.0f, float fadeTime = 2.0f);
    
    /// Create a directional explosion
    /// @param x Center X position
    /// @param y Center Y position
    /// @param particleCount Number of particles to spawn
    /// @param color Base color
    /// @param forceX Horizontal force
    /// @param forceY Vertical force
    /// @param gravity Gravity strength
    /// @param fadeTime Particle lifetime in seconds
    /// @return true if particles were created
    bool explodeDirectional(float x, float y, uint16_t particleCount, uint32_t color,
                           float forceX, float forceY, float gravity = 100.0f, float fadeTime = 2.0f);
    
    // =========================================================================
    // Particle Creation (Sprite-based - samples sprite texture for colors)
    // =========================================================================
    
    /// Create a radial explosion from a sprite (samples texture colors)
    /// @param spriteId Sprite ID to explode
    /// @param particleCount Number of particles to spawn
    /// @param force Explosion force (velocity magnitude)
    /// @param gravity Gravity strength
    /// @param fadeTime Particle lifetime in seconds
    /// @param mode Particle rendering mode (point sprites or sprite fragments)
    /// @return true if particles were created
    bool explodeSprite(uint16_t spriteId, uint16_t particleCount,
                      float force = 200.0f, float gravity = 100.0f, float fadeTime = 2.0f,
                      ParticleMode mode = ParticleMode::POINT_SPRITE);
    
    /// Create a directional explosion from a sprite (samples texture colors)
    /// @param spriteId Sprite ID to explode
    /// @param particleCount Number of particles to spawn
    /// @param forceX Horizontal force
    /// @param forceY Vertical force
    /// @param gravity Gravity strength
    /// @param fadeTime Particle lifetime in seconds
    /// @param mode Particle rendering mode (point sprites or sprite fragments)
    /// @return true if particles were created
    bool explodeSpriteDirectional(uint16_t spriteId, uint16_t particleCount,
                                 float forceX, float forceY, float gravity = 100.0f, float fadeTime = 2.0f,
                                 ParticleMode mode = ParticleMode::POINT_SPRITE);
    
    /// Create a fully customizable explosion from a sprite (advanced)
    /// @param spriteId Sprite ID to explode
    /// @param config Explosion configuration with all parameters
    /// @return true if particles were created
    bool explodeSpriteCustom(uint16_t spriteId, const ExplosionConfig& config);
    
    /// Spawn a single particle (advanced usage)
    /// @return Pointer to particle or nullptr if pool full
    Particle* spawnParticle(float x, float y, float vx, float vy, 
                           uint32_t color, float size, float lifetime);
    
    // =========================================================================
    // Simulation & Rendering
    // =========================================================================
    
    /// Update particle physics
    /// @param deltaTime Time step in seconds
    void update(float deltaTime);
    
    /// Get all active particles for rendering
    /// @return Vector of active particles (read-only access)
    const std::vector<Particle>& getActiveParticles() const;
    
    /// Get particle count
    uint32_t getActiveCount() const { return m_activeCount; }
    
    /// Get total particles created since initialization
    uint64_t getTotalCreated() const { return m_totalCreated; }
    
    // =========================================================================
    // System Control
    // =========================================================================
    
    /// Clear all particles
    void clear();
    
    /// Pause particle simulation
    void pause() { m_paused = true; }
    
    /// Resume particle simulation
    void resume() { m_paused = false; }
    
    /// Check if paused
    bool isPaused() const { return m_paused; }
    
    /// Set time scale (slow motion / fast forward)
    /// @param scale Time scale multiplier (0.1 to 5.0)
    void setTimeScale(float scale);
    
    /// Set world bounds for particle culling
    /// @param width World width in pixels
    /// @param height World height in pixels
    void setWorldBounds(float width, float height);
    
    /// Enable or disable the particle system
    /// @param enabled true to enable, false to disable
    void setEnabled(bool enabled) { m_enabled = enabled; }
    
    /// Check if enabled
    bool isEnabled() const { return m_enabled; }
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    /// Set global gravity
    void setGravity(float gravity) { m_globalGravity = gravity; }
    
    /// Get global gravity
    float getGravity() const { return m_globalGravity; }
    
    /// Set global drag coefficient
    void setDrag(float drag) { m_drag = drag; }
    
    /// Get global drag
    float getDrag() const { return m_drag; }
    
    /// Dump statistics to console
    void dumpStats() const;
    
private:
    // Particle pool
    std::vector<Particle> m_particles;
    uint32_t m_maxParticles;
    uint32_t m_activeCount;
    uint64_t m_totalCreated;
    
    // System state
    bool m_initialized;
    bool m_enabled;
    bool m_paused;
    float m_timeScale;
    
    // Physics parameters
    float m_globalGravity;
    float m_drag;
    float m_worldWidth;
    float m_worldHeight;
    
    // Explosion timing tracking
    bool m_explosionActive;
    float m_explosionSimTime;
    int m_explosionUpdateCount;
    std::chrono::steady_clock::time_point m_explosionStartTime;
    
    // Sprite manager reference (for sprite-based explosions)
    SpriteManager* m_spriteManager;
    
    // Thread safety
    mutable std::mutex m_mutex;
    
    // Helper methods
    std::vector<uint32_t> sampleSpriteColors(uint16_t spriteId, uint16_t sampleCount);
    void generateFragmentTexCoords(Particle& particle, float fragmentSize);
};

} // namespace SuperTerminal

// =============================================================================
// C API (for scripting languages and FFI)
// =============================================================================

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize the particle system (called by framework)
bool st_particle_system_initialize(uint32_t maxParticles);

/// Shutdown the particle system (called by framework)
void st_particle_system_shutdown();

/// Check if particle system is ready
bool st_particle_system_is_ready();

/// Set sprite manager reference (required for sprite-based explosions)
void st_particle_system_set_sprite_manager(void* spriteManager);

/// Create sprite explosion at position (coordinate-based)
bool st_sprite_explode(float x, float y, uint16_t particleCount, uint32_t color);

/// Create advanced sprite explosion (coordinate-based)
bool st_sprite_explode_advanced(float x, float y, uint16_t particleCount, uint32_t color,
                               float force, float gravity, float fadeTime);

/// Create directional sprite explosion (coordinate-based)
bool st_sprite_explode_directional(float x, float y, uint16_t particleCount, uint32_t color,
                                   float forceX, float forceY);

// =========================================================================
// Sprite-based explosion API (v1 compatible - samples sprite textures)
// =========================================================================

/// Create sprite explosion by sprite ID (samples sprite texture for colors)
bool sprite_explode(uint16_t spriteId, uint16_t particleCount);

/// Create advanced sprite explosion by sprite ID
bool sprite_explode_advanced(uint16_t spriteId, uint16_t particleCount,
                            float explosionForce, float gravity, float fadeTime);

/// Create directional sprite explosion by sprite ID
bool sprite_explode_directional(uint16_t spriteId, uint16_t particleCount,
                               float forceX, float forceY);

/// Create sprite explosion with custom size multiplier
bool sprite_explode_size(uint16_t spriteId, uint16_t particleCount, float sizeMultiplier);

/// Create custom explosion with full parameter control
bool sprite_explode_custom(uint16_t spriteId, const SuperTerminal::ExplosionConfig* config);

/// Clear all particles
void st_particle_clear();

/// Pause particle simulation
void st_particle_pause();

/// Resume particle simulation
void st_particle_resume();

/// Set time scale
void st_particle_set_time_scale(float scale);

/// Set world bounds
void st_particle_set_world_bounds(float width, float height);

/// Enable or disable particles
void st_particle_set_enabled(bool enabled);

/// Get active particle count
uint32_t st_particle_get_active_count();

/// Get total particles created
uint64_t st_particle_get_total_created();

/// Dump statistics
void st_particle_dump_stats();

/// Update particle system (called by framework each frame)
void st_particle_system_update(float deltaTime);

/// Get particle data for rendering
/// @param outParticles Pointer to receive particle array
/// @param outCount Pointer to receive active particle count
/// @return true if particles are available
bool st_particle_get_render_data(const void** outParticles, uint32_t* outCount);

#ifdef __cplusplus
}
#endif

#endif // SUPERTERMINAL_PARTICLE_SYSTEM_H