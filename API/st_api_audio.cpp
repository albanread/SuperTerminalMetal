//
// st_api_audio.cpp
// SuperTerminal v2 - C API Audio Implementation
//
// Sound effects, music, and synthesis API functions
//

#include "superterminal_api.h"
#include "st_api_context.h"
#include "../Audio/AudioManager.h"
#include "../Cart/CartManager.h"
#include "../Audio/Voice/VoiceController.h"
#include <fstream>
#include <algorithm>

// For terminal tools that use VoiceController directly
using namespace SuperTerminal;
using namespace STApi;

// =============================================================================
// Audio API - Sound Effects
// =============================================================================

ST_API STSoundID st_sound_load(const char* path) {
    if (!path) {
        ST_SET_ERROR("Sound path is null");
        return -1;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", -1);

    // For now, we don't support loading external sound files
    // This would require implementing an asset loading system
    ST_SET_ERROR("External sound loading not yet implemented");
    return -1;
}

ST_API STSoundID st_sound_load_builtin(const char* name) {
    if (!name) {
        ST_SET_ERROR("Sound name is null");
        return -1;
    }

    ST_LOCK;

    // Register the builtin sound name and return a handle
    int32_t handle = ST_CONTEXT.registerSound(name);
    return handle;
}

ST_API void st_sound_play(STSoundID sound, float volume) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    std::string soundName = ST_CONTEXT.getSoundName(sound);
    if (soundName.empty()) {
        ST_SET_ERROR("Invalid sound ID");
        return;
    }

    // Map builtin names to SoundEffect enum
    SoundEffect effect = SoundEffect::Beep;

    if (soundName == "beep") effect = SoundEffect::Beep;
    else if (soundName == "bang") effect = SoundEffect::Bang;
    else if (soundName == "explode") effect = SoundEffect::Explode;
    else if (soundName == "big_explosion") effect = SoundEffect::BigExplosion;
    else if (soundName == "small_explosion") effect = SoundEffect::SmallExplosion;
    else if (soundName == "distant_explosion") effect = SoundEffect::DistantExplosion;
    else if (soundName == "metal_explosion") effect = SoundEffect::MetalExplosion;
    else if (soundName == "zap") effect = SoundEffect::Zap;
    else if (soundName == "coin") effect = SoundEffect::Coin;
    else if (soundName == "jump") effect = SoundEffect::Jump;
    else if (soundName == "powerup") effect = SoundEffect::PowerUp;
    else if (soundName == "hurt") effect = SoundEffect::Hurt;
    else if (soundName == "shoot") effect = SoundEffect::Shoot;
    else if (soundName == "click") effect = SoundEffect::Click;
    else if (soundName == "sweep_up") effect = SoundEffect::SweepUp;
    else if (soundName == "sweep_down") effect = SoundEffect::SweepDown;
    else if (soundName == "random_beep") effect = SoundEffect::RandomBeep;
    else if (soundName == "pickup") effect = SoundEffect::Pickup;
    else if (soundName == "blip") effect = SoundEffect::Blip;
    else {
        ST_SET_ERROR("Unknown builtin sound name");
        return;
    }

    ST_CONTEXT.audio()->playSoundEffect(effect, volume);
}

ST_API void st_sound_play_with_fade(uint32_t sound_id, float volume, float cap_duration) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    printf("[st_sound_play_with_fade] Playing sound ID %u, volume=%.2f, cap=%.2f\n", 
           sound_id, volume, cap_duration);

    // If cap_duration is negative, play without fade (full duration)
    if (cap_duration < 0.0f) {
        ST_CONTEXT.audio()->soundPlay(sound_id, volume, 0.0f);
        return;
    }

    // TODO: Implement fade-out logic
    // For now, just play the sound normally
    // Future: add fade-out parameter to soundPlay or create new method
    ST_CONTEXT.audio()->soundPlay(sound_id, volume, 0.0f);
    
    // Note: Full fade implementation requires:
    // 1. Track sound playback instances
    // 2. Schedule fade-out at cap_duration
    // 3. Apply volume envelope in audio rendering
    printf("[st_sound_play_with_fade] Note: Fade-out not yet implemented, playing full sound\n");
}

ST_API void st_sound_stop(STSoundID sound) {
    // TODO: Implement per-sound stop when we add sound instance tracking
    ST_SET_ERROR("Per-sound stop not yet implemented");
}

ST_API void st_sound_unload(STSoundID sound) {
    ST_LOCK;
    ST_CONTEXT.unregisterSound(sound);
}

// =============================================================================
// Audio API - Music
// =============================================================================

ST_API void st_music_play(const char* abc_notation) {
    if (!abc_notation) {
        ST_SET_ERROR("ABC notation string is null");
        return;
    }

    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->playMusic(abc_notation, 1.0f);
}

ST_API void st_play_abc(const char* abc_text) {
    if (!abc_text) {
        ST_SET_ERROR("ABC text string is null");
        return;
    }

    // Process escape sequences: convert \n to actual newlines
    std::string processed;
    processed.reserve(strlen(abc_text));
    
    for (const char* p = abc_text; *p != '\0'; ++p) {
        if (*p == '\\' && *(p + 1) != '\0') {
            // Handle escape sequences
            ++p;
            switch (*p) {
                case 'n':
                    processed += '\n';
                    break;
                case 't':
                    processed += '\t';
                    break;
                case 'r':
                    processed += '\r';
                    break;
                case '\\':
                    processed += '\\';
                    break;
                default:
                    // Unknown escape - keep the backslash and character
                    processed += '\\';
                    processed += *p;
                    break;
            }
        } else {
            processed += *p;
        }
    }

    // Call the regular music_play with the processed string
    st_music_play(processed.c_str());
}

ST_API void st_music_play_file(const char* path) {
    if (!path) {
        ST_SET_ERROR("Music file path is null");
        return;
    }

    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    std::string pathStr(path);
    printf("[st_music_play_file] Attempting to load: '%s'\n", path);

    // Priority 1: Check if we have an active cart and try to load from it
    auto cartManager = ST_CONTEXT.getCartManager();
    if (cartManager && cartManager->isCartActive()) {
        printf("[st_music_play_file] Cart is active, attempting cart load\n");
        auto loader = cartManager->getLoader();
        if (loader) {
            // First, try loading from data_files table (for files saved with scripts/ or music/ paths)
            CartDataFile dataFile;
            printf("[st_music_play_file] Trying loadDataFile with path: '%s'\n", pathStr.c_str());
            if (loader->loadDataFile(pathStr, dataFile)) {
                printf("[st_music_play_file] Successfully loaded from data_files, size: %zu bytes\n", dataFile.data.size());
                // Got file from cart data_files table - determine format from path
                std::string lowerPath = pathStr;
                std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

                if (lowerPath.find(".abc") != std::string::npos) {
                    // ABC notation file
                    printf("[st_music_play_file] Playing ABC file from cart\n");
                    std::string abcString(dataFile.data.begin(), dataFile.data.end());
                    ST_CONTEXT.audio()->playMusic(abcString.c_str(), 1.0f);
                    return;
                } else if (lowerPath.find(".sid") != std::string::npos) {
                    // SID file
                    printf("[st_music_play_file] Playing SID file from cart\n");
                    uint32_t sidId = ST_CONTEXT.audio()->sidLoadMemory(dataFile.data.data(), dataFile.data.size());
                    if (sidId > 0) {
                        ST_CONTEXT.audio()->sidPlay(sidId, 0, 1.0f);
                        return;
                    } else {
                        ST_SET_ERROR("Failed to load SID music from cart data file");
                        return;
                    }
                } else if (lowerPath.find(".vscript") != std::string::npos) {
                    printf("[st_music_play_file] Processing VoiceScript file from cart\n");
                    // VoiceScript file - write to temp file and load
                    std::string tempPath = "/tmp/voicescript_cart_" + pathStr;
                    // Replace slashes with underscores for temp filename
                    std::replace(tempPath.begin(), tempPath.end(), '/', '_');
                    printf("[st_music_play_file] Writing VoiceScript to temp file: %s\n", tempPath.c_str());
                    std::ofstream tempFile(tempPath, std::ios::binary);
                    if (tempFile.write(reinterpret_cast<const char*>(dataFile.data.data()),
                                      dataFile.data.size())) {
                        tempFile.close();
                        printf("[st_music_play_file] Loading VoiceScript from temp file\n");
                        if (ST_CONTEXT.audio()->voiceScriptLoad(tempPath.c_str())) {
                            // Extract filename without extension for script name
                            size_t lastSlash = pathStr.find_last_of("/\\");
                            size_t start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
                            size_t lastDot = pathStr.find_last_of('.');
                            size_t end = (lastDot == std::string::npos) ? pathStr.length() : lastDot;
                            std::string scriptName = pathStr.substr(start, end - start);

                            // Play the script at default 120 BPM
                            printf("[st_music_play_file] Playing VoiceScript '%s' at 120 BPM\n", scriptName.c_str());
                            ST_CONTEXT.audio()->voiceScriptPlay(scriptName, 120.0f);
                            return;
                        } else {
                            ST_SET_ERROR("Failed to load VoiceScript from cart data file");
                            return;
                        }
                    } else {
                        ST_SET_ERROR("Failed to write VoiceScript temp file from cart data file");
                        return;
                    }
                }
            } else {
                printf("[st_music_play_file] loadDataFile failed, trying loadMusic as fallback\n");
            }

            // If not found in data_files, try loading as music asset by name
            CartMusic cartMusic;
            if (loader->loadMusic(path, cartMusic)) {
                printf("[st_music_play_file] Successfully loaded from music table, format: %s\n", cartMusic.format.c_str());
                // Got music from cart - play it based on format
                if (cartMusic.format == "abc") {
                    // ABC notation - convert data to string and play
                    std::string abcString(cartMusic.data.begin(), cartMusic.data.end());
                    ST_CONTEXT.audio()->playMusic(abcString.c_str(), 1.0f);
                    return;
                } else if (cartMusic.format == "sid") {
                    // SID file - load and play from memory
                    uint32_t sidId = ST_CONTEXT.audio()->sidLoadMemory(cartMusic.data.data(), cartMusic.data.size());
                    if (sidId > 0) {
                        ST_CONTEXT.audio()->sidPlay(sidId, 0, 1.0f); // Play default subtune at full volume
                        return;
                    } else {
                        ST_SET_ERROR("Failed to load SID music from cart");
                        return;
                    }
                } else if (cartMusic.format == "vscript" || cartMusic.format == "voicescript") {
                    // VoiceScript file - write to temp file and load
                    std::string tempPath = "/tmp/voicescript_cart_" + std::string(path) + ".vscript";
                    std::ofstream tempFile(tempPath, std::ios::binary);
                    if (tempFile.write(reinterpret_cast<const char*>(cartMusic.data.data()),
                                      cartMusic.data.size())) {
                        tempFile.close();
                        if (ST_CONTEXT.audio()->voiceScriptLoad(tempPath.c_str())) {
                            // Extract filename without extension for script name
                            std::string pathStr(path);
                            size_t lastSlash = pathStr.find_last_of("/\\");
                            size_t start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
                            size_t lastDot = pathStr.find_last_of('.');
                            size_t end = (lastDot == std::string::npos) ? pathStr.length() : lastDot;
                            std::string scriptName = pathStr.substr(start, end - start);

                            // Play the script at default 120 BPM
                            ST_CONTEXT.audio()->voiceScriptPlay(scriptName, 120.0f);
                            return;
                        } else {
                            ST_SET_ERROR("Failed to load VoiceScript from cart");
                            return;
                        }
                    } else {
                        ST_SET_ERROR("Failed to write VoiceScript temp file from cart");
                        return;
                    }
                } else {
                    // Other tracker formats (MOD, XM, etc.)
                    // TODO: Implement tracker playback from memory
                    ST_SET_ERROR("Tracker format playback from cart not yet implemented");
                    return;
                }
            } else {
                printf("[st_music_play_file] loadMusic also failed - music not found in cart\n");
            }
        } else {
            printf("[st_music_play_file] Cart loader not available\n");
        }
    } else {
        printf("[st_music_play_file] No active cart, trying filesystem\n");
    }

    // Not in cart or cart not active - try loading from file system
    printf("[st_music_play_file] Attempting to load from filesystem: %s\n", path);
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        printf("[st_music_play_file] Failed to open file: %s\n", path);
        ST_SET_ERROR("Failed to open music file");
        return;
    }
    
    printf("[st_music_play_file] Successfully opened file from filesystem\n");

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        ST_SET_ERROR("Failed to read music file");
        return;
    }

    // Determine format from extension
    if (pathStr.find(".abc") != std::string::npos) {
        // ABC notation file
        printf("[st_music_play_file] Playing ABC file from filesystem\n");
        std::string abcString(buffer.begin(), buffer.end());
        ST_CONTEXT.audio()->playMusic(abcString.c_str(), 1.0f);
    } else if (pathStr.find(".sid") != std::string::npos) {
        // SID file - load from file system
        printf("[st_music_play_file] Playing SID file from filesystem\n");
        uint32_t sidId = ST_CONTEXT.audio()->sidLoadMemory(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());
        if (sidId > 0) {
            ST_CONTEXT.audio()->sidPlay(sidId, 0, 1.0f); // Play default subtune at full volume
        } else {
            ST_SET_ERROR("Failed to load SID music from file");
        }
    } else if (pathStr.find(".vscript") != std::string::npos) {
        printf("[st_music_play_file] Loading VoiceScript from filesystem\n");
        // VoiceScript file - load and play
        if (ST_CONTEXT.audio()->voiceScriptLoad(path)) {
            // Extract filename without extension for script name
            size_t lastSlash = pathStr.find_last_of("/\\");
            size_t start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
            size_t lastDot = pathStr.find_last_of('.');
            size_t end = (lastDot == std::string::npos) ? pathStr.length() : lastDot;
            std::string scriptName = pathStr.substr(start, end - start);

            // Play the script at default 120 BPM
            printf("[st_music_play_file] Playing VoiceScript '%s' from filesystem at 120 BPM\n", scriptName.c_str());
            ST_CONTEXT.audio()->voiceScriptPlay(scriptName, 120.0f);
        } else {
            ST_SET_ERROR("Failed to load VoiceScript from file");
        }
    } else {
        // Assume ABC notation
        std::string abcString(buffer.begin(), buffer.end());
        ST_CONTEXT.audio()->playMusic(abcString.c_str(), 1.0f);
    }
}

ST_API void st_music_play_file_with_format(const char* path, const char* format) {
    if (!path) {
        ST_SET_ERROR("Music file path is null");
        return;
    }
    
    if (!format) {
        ST_SET_ERROR("Format is null");
        return;
    }

    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    std::string pathStr(path);
    std::string formatStr(format);
    printf("[st_music_play_file_with_format] Attempting to load: '%s' as format: '%s'\n", path, format);

    // Try to load file data (from cart or filesystem)
    std::vector<uint8_t> fileData;
    bool foundData = false;
    
    // Priority 1: Check if we have an active cart
    auto cartManager = ST_CONTEXT.getCartManager();
    if (cartManager && cartManager->isCartActive()) {
        printf("[st_music_play_file_with_format] Cart is active, attempting cart load\n");
        auto loader = cartManager->getLoader();
        if (loader) {
            // Try data_files table
            CartDataFile dataFile;
            if (loader->loadDataFile(pathStr, dataFile)) {
                fileData = dataFile.data;
                foundData = true;
                printf("[st_music_play_file_with_format] Loaded from cart data_files, size: %zu bytes\n", fileData.size());
            }
            // Try music table
            if (!foundData) {
                CartMusic cartMusic;
                if (loader->loadMusic(path, cartMusic)) {
                    fileData = cartMusic.data;
                    foundData = true;
                    printf("[st_music_play_file_with_format] Loaded from cart music table, size: %zu bytes\n", fileData.size());
                }
            }
            // Try sound table
            if (!foundData) {
                CartSound cartSound;
                if (loader->loadSound(path, cartSound)) {
                    fileData = cartSound.data;
                    foundData = true;
                    printf("[st_music_play_file_with_format] Loaded from cart sound table, size: %zu bytes\n", fileData.size());
                }
            }
        }
    }
    
    // Priority 2: Try filesystem
    if (!foundData) {
        printf("[st_music_play_file_with_format] Attempting to load from filesystem: %s\n", path);
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            printf("[st_music_play_file_with_format] Failed to open file: %s\n", path);
            ST_SET_ERROR("Failed to open music file");
            return;
        }
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        fileData.resize(size);
        
        if (!file.read(reinterpret_cast<char*>(fileData.data()), size)) {
            printf("[st_music_play_file_with_format] Failed to read file: %s\n", path);
            ST_SET_ERROR("Failed to read music file");
            return;
        }
        foundData = true;
        printf("[st_music_play_file_with_format] Successfully loaded from filesystem, size: %zu bytes\n", fileData.size());
    }
    
    if (!foundData || fileData.empty()) {
        ST_SET_ERROR("Failed to load music file data");
        return;
    }
    
    // Play based on format override
    if (formatStr == "abc") {
        // ABC notation
        printf("[st_music_play_file_with_format] Playing as ABC notation\n");
        std::string abcString(fileData.begin(), fileData.end());
        ST_CONTEXT.audio()->playMusic(abcString.c_str(), 1.0f);
    } else if (formatStr == "sid") {
        // SID file
        printf("[st_music_play_file_with_format] Playing as SID\n");
        uint32_t sidId = ST_CONTEXT.audio()->sidLoadMemory(fileData.data(), fileData.size());
        if (sidId > 0) {
            ST_CONTEXT.audio()->sidPlay(sidId, 0, 1.0f);
        } else {
            ST_SET_ERROR("Failed to load SID music");
        }
    } else if (formatStr == "voicescript") {
        // VoiceScript
        printf("[st_music_play_file_with_format] Playing as VoiceScript\n");
        std::string vscriptContent(fileData.begin(), fileData.end());
        
        // Extract filename without extension for script name
        size_t lastSlash = pathStr.find_last_of("/\\");
        size_t start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
        size_t lastDot = pathStr.find_last_of('.');
        size_t end = (lastDot == std::string::npos) ? pathStr.length() : lastDot;
        std::string scriptName = pathStr.substr(start, end - start);
        
        std::string error;
        if (ST_CONTEXT.audio()->voiceScriptDefine(scriptName, vscriptContent, error)) {
            ST_CONTEXT.audio()->voiceScriptPlay(scriptName, 120.0f);
        } else {
            ST_SET_ERROR(("Failed to compile VoiceScript: " + error).c_str());
        }
    } else if (formatStr == "wav") {
        // WAV file - use musicLoadFile
        printf("[st_music_play_file_with_format] Playing as WAV\n");
        uint32_t musicId = ST_CONTEXT.audio()->musicLoadFile(pathStr);
        if (musicId > 0) {
            ST_CONTEXT.audio()->musicPlay(musicId, 1.0f);
        } else {
            ST_SET_ERROR("Failed to load WAV file");
        }
    } else {
        ST_SET_ERROR("Unknown format specified");
    }
}

ST_API bool st_music_render_to_wav(const char* path, const char* outputPath, const char* format, bool fastRender) {
    if (!path) {
        ST_SET_ERROR("Music file path is null");
        return false;
    }
    
    if (!outputPath) {
        ST_SET_ERROR("Output path is null");
        return false;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", false);

    std::string pathStr(path);
    std::string outputPathStr(outputPath);
    printf("[st_music_render_to_wav] Rendering: '%s' to '%s'\n", path, outputPath);

    // Determine format (from parameter or auto-detect)
    std::string formatStr;
    if (format && format[0] != '\0') {
        formatStr = format;
        printf("[st_music_render_to_wav] Using format override: %s\n", format);
    } else {
        // Auto-detect from extension
        std::string lowerPath = pathStr;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
        if (lowerPath.find(".abc") != std::string::npos) {
            formatStr = "abc";
        } else if (lowerPath.find(".sid") != std::string::npos) {
            formatStr = "sid";
        } else if (lowerPath.find(".vscript") != std::string::npos) {
            formatStr = "voicescript";
        } else {
            ST_SET_ERROR("Cannot auto-detect format from file extension");
            return false;
        }
        printf("[st_music_render_to_wav] Auto-detected format: %s\n", formatStr.c_str());
    }
    
    // Load file data (from cart or filesystem)
    std::vector<uint8_t> fileData;
    bool foundData = false;
    
    // Try cart first if active
    auto cartManager = ST_CONTEXT.getCartManager();
    if (cartManager && cartManager->isCartActive()) {
        printf("[st_music_render_to_wav] Cart is active, attempting cart load\n");
        auto loader = cartManager->getLoader();
        if (loader) {
            // Try data_files table
            CartDataFile dataFile;
            if (loader->loadDataFile(pathStr, dataFile)) {
                fileData = dataFile.data;
                foundData = true;
                printf("[st_music_render_to_wav] Loaded from cart data_files, size: %zu bytes\n", fileData.size());
            }
            // Try music table
            if (!foundData) {
                CartMusic cartMusic;
                if (loader->loadMusic(path, cartMusic)) {
                    fileData = cartMusic.data;
                    foundData = true;
                    printf("[st_music_render_to_wav] Loaded from cart music table, size: %zu bytes\n", fileData.size());
                }
            }
        }
    }
    
    // Try filesystem if not in cart
    if (!foundData) {
        printf("[st_music_render_to_wav] Attempting to load from filesystem: %s\n", path);
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            printf("[st_music_render_to_wav] Failed to open file: %s\n", path);
            ST_SET_ERROR("Failed to open music file");
            return false;
        }
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        fileData.resize(size);
        
        if (!file.read(reinterpret_cast<char*>(fileData.data()), size)) {
            printf("[st_music_render_to_wav] Failed to read file: %s\n", path);
            ST_SET_ERROR("Failed to read music file");
            return false;
        }
        foundData = true;
        printf("[st_music_render_to_wav] Successfully loaded from filesystem, size: %zu bytes\n", fileData.size());
    }
    
    if (!foundData || fileData.empty()) {
        ST_SET_ERROR("Failed to load music file data");
        return false;
    }
    
    // Render based on format
    bool success = false;
    
    if (formatStr == "voicescript") {
        // VoiceScript rendering
        printf("[st_music_render_to_wav] Rendering VoiceScript to WAV\n");
        std::string vscriptContent(fileData.begin(), fileData.end());
        
        // Use a temporary script name
        std::string tempScriptName = "temp_render_script";
        
        std::string error;
        if (!ST_CONTEXT.audio()->voiceScriptDefine(tempScriptName, vscriptContent, error)) {
            ST_SET_ERROR(("Failed to compile VoiceScript: " + error).c_str());
            return false;
        }
        
        // Render to WAV (10 seconds default, 48kHz, 120 BPM)
        success = ST_CONTEXT.audio()->voiceScriptRenderToWAV(tempScriptName, outputPathStr, 10.0f, 48000, 120.0f, fastRender);
        
        // Clean up
        ST_CONTEXT.audio()->voiceScriptRemove(tempScriptName);
        
        if (!success) {
            ST_SET_ERROR("Failed to render VoiceScript to WAV");
        }
    } else if (formatStr == "sid") {
        // SID rendering
        printf("[st_music_render_to_wav] Rendering SID to WAV\n");
        uint32_t sidId = ST_CONTEXT.audio()->sidLoadMemory(fileData.data(), fileData.size());
        if (sidId == 0) {
            ST_SET_ERROR("Failed to load SID file");
            return false;
        }
        
        // Render offline (180 seconds = 3 minutes, subtune 0)
        success = ST_CONTEXT.audio()->sidRenderToWAV(sidId, outputPathStr, 180.0f, 0);
        
        // Clean up
        ST_CONTEXT.audio()->sidFree(sidId);
        
        if (!success) {
            ST_SET_ERROR("Failed to render SID to WAV");
        }
    } else if (formatStr == "abc") {
        // ABC rendering
        printf("[st_music_render_to_wav] Rendering ABC to WAV\n");
        std::string abcString(fileData.begin(), fileData.end());
        
        // Render to WAV (0.0f = auto-detect duration)
        success = ST_CONTEXT.audio()->abcRenderToWAV(abcString, outputPathStr, 0.0f);
        
        if (!success) {
            ST_SET_ERROR("Failed to render ABC to WAV (note: ABC rendering is partial)");
        }
    } else {
        ST_SET_ERROR("Unsupported format for WAV rendering");
        return false;
    }
    
    if (success) {
        printf("[st_music_render_to_wav] Successfully rendered to: %s\n", outputPath);
    }
    
    return success;
}

ST_API uint32_t st_music_render_to_slot(const char* path, uint32_t slotNumber, const char* format, bool fastRender) {
    if (!path) {
        ST_SET_ERROR("Music file path is null");
        return 0;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    std::string pathStr(path);
    printf("[st_music_render_to_slot] Rendering: '%s' to slot %u\n", path, slotNumber);

    // Determine format (from parameter or auto-detect)
    std::string formatStr;
    if (format && format[0] != '\0') {
        formatStr = format;
        printf("[st_music_render_to_slot] Using format override: %s\n", format);
    } else {
        // Auto-detect from extension
        std::string lowerPath = pathStr;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
        if (lowerPath.find(".abc") != std::string::npos) {
            formatStr = "abc";
        } else if (lowerPath.find(".sid") != std::string::npos) {
            formatStr = "sid";
        } else if (lowerPath.find(".vscript") != std::string::npos) {
            formatStr = "voicescript";
        } else {
            ST_SET_ERROR("Cannot auto-detect format from file extension");
            return 0;
        }
        printf("[st_music_render_to_slot] Auto-detected format: %s\n", formatStr.c_str());
    }
    
    // Load file data (from cart or filesystem)
    std::vector<uint8_t> fileData;
    bool foundData = false;
    
    // Try cart first if active
    auto cartManager = ST_CONTEXT.getCartManager();
    if (cartManager && cartManager->isCartActive()) {
        printf("[st_music_render_to_slot] Cart is active, attempting cart load\n");
        auto loader = cartManager->getLoader();
        if (loader) {
            // Try data_files table
            CartDataFile dataFile;
            if (loader->loadDataFile(pathStr, dataFile)) {
                fileData = dataFile.data;
                foundData = true;
                printf("[st_music_render_to_slot] Loaded from cart data_files, size: %zu bytes\n", fileData.size());
            }
            // Try music table
            if (!foundData) {
                CartMusic cartMusic;
                if (loader->loadMusic(path, cartMusic)) {
                    fileData = cartMusic.data;
                    foundData = true;
                    printf("[st_music_render_to_slot] Loaded from cart music table, size: %zu bytes\n", fileData.size());
                }
            }
        }
    }
    
    // Try filesystem if not in cart
    if (!foundData) {
        printf("[st_music_render_to_slot] Attempting to load from filesystem: %s\n", path);
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            printf("[st_music_render_to_slot] Failed to open file: %s\n", path);
            ST_SET_ERROR("Failed to open music file");
            return 0;
        }
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        fileData.resize(size);
        
        if (!file.read(reinterpret_cast<char*>(fileData.data()), size)) {
            printf("[st_music_render_to_slot] Failed to read file: %s\n", path);
            ST_SET_ERROR("Failed to read music file");
            return 0;
        }
        foundData = true;
        printf("[st_music_render_to_slot] Successfully loaded from filesystem, size: %zu bytes\n", fileData.size());
    }
    
    if (!foundData || fileData.empty()) {
        ST_SET_ERROR("Failed to load music file data");
        return 0;
    }
    
    // Render based on format
    uint32_t resultSlot = 0;
    
    if (formatStr == "voicescript") {
        // VoiceScript rendering
        printf("[st_music_render_to_slot] Rendering VoiceScript to sound bank\n");
        std::string vscriptContent(fileData.begin(), fileData.end());
        
        // Use a temporary script name
        std::string tempScriptName = "temp_render_script_" + std::to_string(slotNumber);
        
        std::string error;
        if (!ST_CONTEXT.audio()->voiceScriptDefine(tempScriptName, vscriptContent, error)) {
            ST_SET_ERROR(("Failed to compile VoiceScript: " + error).c_str());
            return 0;
        }
        
        // Render to sound bank (10 seconds default, 48kHz, 120 BPM)
        resultSlot = ST_CONTEXT.audio()->voiceScriptSaveToBank(tempScriptName, 10.0f, 48000, 120.0f, fastRender);
        
        // Clean up
        ST_CONTEXT.audio()->voiceScriptRemove(tempScriptName);
        
        if (resultSlot == 0) {
            ST_SET_ERROR("Failed to render VoiceScript to sound bank");
        }
    } else if (formatStr == "sid") {
        // SID rendering - render directly to memory buffer
        printf("[st_music_render_to_slot] Rendering SID to sound bank (in memory)\n");
        uint32_t sidId = ST_CONTEXT.audio()->sidLoadMemory(fileData.data(), fileData.size());
        if (sidId == 0) {
            ST_SET_ERROR("Failed to load SID file");
            return 0;
        }
        
        // Render directly to sound bank (no temp files!)
        resultSlot = ST_CONTEXT.audio()->sidRenderToBank(sidId, 180.0f, 0, 48000, fastRender);
        
        // Clean up SID
        ST_CONTEXT.audio()->sidFree(sidId);
        
        if (resultSlot == 0) {
            ST_SET_ERROR("Failed to render SID to sound bank");
        }
    } else if (formatStr == "abc") {
        // ABC rendering - render directly to memory buffer
        printf("[st_music_render_to_slot] Rendering ABC to sound bank (in memory)\n");
        std::string abcString(fileData.begin(), fileData.end());
        
        // Render directly to sound bank (no temp files!)
        resultSlot = ST_CONTEXT.audio()->abcRenderToBank(abcString, 0.0f, 48000, fastRender);
        
        if (resultSlot == 0) {
            ST_SET_ERROR("Failed to render ABC to sound bank (note: ABC rendering is partial)");
        }
    } else {
        ST_SET_ERROR("Unsupported format for slot rendering");
        return 0;
    }
    
    if (resultSlot > 0) {
        printf("[st_music_render_to_slot] Successfully rendered to slot: %u\n", resultSlot);
    }
    
    return resultSlot;
}

ST_API void st_music_stop(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->stopMusic();
}

ST_API void st_music_pause(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->pauseMusic();
}

ST_API void st_music_resume(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->resumeMusic();
}

ST_API bool st_music_is_playing(void) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", false);

    return ST_CONTEXT.audio()->isMusicPlaying();
}

ST_API void st_music_set_volume(float volume) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->setMusicVolume(volume);
}

// =============================================================================
// Audio API - Synthesis
// =============================================================================

ST_API void st_synth_note(int note, float duration, float volume) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->playNote(note, duration, volume);
}

ST_API void st_synth_set_instrument(int instrument) {
    // TODO: Implement instrument selection when we add MIDI instrument switching
    ST_SET_ERROR("Synth instrument selection not yet implemented");
}

ST_API void st_synth_frequency(float frequency, float duration, float volume) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->playFrequency(frequency, duration, volume);
}

// =============================================================================
// Audio API - Sound Bank (ID-based Sound Management)
// =============================================================================

ST_API uint32_t st_sound_create_beep(float frequency, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateBeep(frequency, duration);
}

ST_API uint32_t st_sound_create_zap(float frequency, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateZap(frequency, duration);
}

ST_API uint32_t st_sound_create_explode(float size, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateExplode(size, duration);
}

ST_API uint32_t st_sound_create_coin(float pitch, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateCoin(pitch, duration);
}

ST_API uint32_t st_sound_create_jump(float power, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateJump(power, duration);
}

ST_API uint32_t st_sound_create_shoot(float power, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateShoot(power, duration);
}

ST_API uint32_t st_sound_create_click(float sharpness, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateClick(sharpness, duration);
}

ST_API uint32_t st_sound_create_blip(float pitch, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateBlip(pitch, duration);
}

ST_API uint32_t st_sound_create_pickup(float brightness, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreatePickup(brightness, duration);
}

ST_API uint32_t st_sound_create_powerup(float intensity, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreatePowerup(intensity, duration);
}

ST_API uint32_t st_sound_create_hurt(float severity, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateHurt(severity, duration);
}

ST_API uint32_t st_sound_create_sweep_up(float start_freq, float end_freq, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateSweepUp(start_freq, end_freq, duration);
}

ST_API uint32_t st_sound_create_sweep_down(float start_freq, float end_freq, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateSweepDown(start_freq, end_freq, duration);
}

ST_API uint32_t st_sound_create_big_explosion(float size, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateBigExplosion(size, duration);
}

ST_API uint32_t st_sound_create_small_explosion(float intensity, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateSmallExplosion(intensity, duration);
}

ST_API uint32_t st_sound_create_distant_explosion(float distance, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateDistantExplosion(distance, duration);
}

ST_API uint32_t st_sound_create_metal_explosion(float shrapnel, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateMetalExplosion(shrapnel, duration);
}

ST_API uint32_t st_sound_create_bang(float intensity, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateBang(intensity, duration);
}

ST_API uint32_t st_sound_create_random_beep(uint32_t seed, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateRandomBeep(seed, duration);
}

ST_API void st_sound_play_id(uint32_t sound_id, float volume, float pan) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->soundPlay(sound_id, volume, pan);
}

ST_API bool st_sound_free_id(uint32_t sound_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", false);

    return ST_CONTEXT.audio()->soundFree(sound_id);
}

ST_API void st_sound_free_all(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->soundFreeAll();
}

ST_API bool st_sound_exists(uint32_t sound_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", false);

    return ST_CONTEXT.audio()->soundExists(sound_id);
}

ST_API size_t st_sound_get_count(void) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundGetCount();
}

ST_API size_t st_sound_get_memory_usage(void) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundGetMemoryUsage();
}

// =============================================================================
// Audio API - Phase 3: Custom Synthesis
// =============================================================================

ST_API uint32_t st_sound_create_tone(float frequency, float duration, int waveform) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateTone(frequency, duration, waveform);
}

ST_API uint32_t st_sound_create_note(int note, float duration, int waveform,
                                      float attack, float decay, float sustain_level, float release) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateNote(note, duration, waveform, attack, decay, sustain_level, release);
}

ST_API uint32_t st_sound_create_noise(int noise_type, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateNoise(noise_type, duration);
}
// =============================================================================
// Audio API - Phase 4: Advanced Synthesis
// =============================================================================

ST_API uint32_t st_sound_create_fm(float carrier_freq, float modulator_freq,
                                    float mod_index, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateFM(carrier_freq, modulator_freq, mod_index, duration);
}

ST_API uint32_t st_sound_create_filtered_tone(float frequency, float duration, int waveform,
                                               int filter_type, float cutoff, float resonance) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateFilteredTone(frequency, duration, waveform,
                                                        filter_type, cutoff, resonance);
}

ST_API uint32_t st_sound_create_filtered_note(int note, float duration, int waveform,
                                               float attack, float decay, float sustain_level, float release,
                                               int filter_type, float cutoff, float resonance) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateFilteredNote(note, duration, waveform,
                                                        attack, decay, sustain_level, release,
                                                        filter_type, cutoff, resonance);
}

// =============================================================================
// Audio API - Phase 5: Effects Chain
// =============================================================================

ST_API uint32_t st_sound_create_with_reverb(float frequency, float duration, int waveform,
                                             float room_size, float damping, float wet) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateWithReverb(frequency, duration, waveform,
                                                      room_size, damping, wet);
}

ST_API uint32_t st_sound_create_with_delay(float frequency, float duration, int waveform,
                                            float delay_time, float feedback, float mix) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateWithDelay(frequency, duration, waveform,
                                                     delay_time, feedback, mix);
}

ST_API uint32_t st_sound_create_with_distortion(float frequency, float duration, int waveform,
                                                 float drive, float tone, float level) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->soundCreateWithDistortion(frequency, duration, waveform,
                                                          drive, tone, level);
}

// =============================================================================
// Audio API - Music Bank (ID-based Music Management)
// =============================================================================

ST_API uint32_t st_music_load_string(const char* abc_notation) {
    if (!abc_notation) {
        ST_SET_ERROR("ABC notation string is null");
        return 0;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->musicLoadString(abc_notation);
}

ST_API uint32_t st_music_load_file(const char* filename) {
    if (!filename) {
        ST_SET_ERROR("Music filename is null");
        return 0;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->musicLoadFile(filename);
}

ST_API void st_music_play_id(uint32_t music_id, float volume) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->musicPlay(music_id, volume);
}

ST_API bool st_music_exists(uint32_t music_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", false);

    return ST_CONTEXT.audio()->musicExists(music_id);
}

// Thread-local storage for string returns
static thread_local std::string g_music_title;
static thread_local std::string g_music_composer;
static thread_local std::string g_music_key;

ST_API const char* st_music_get_title(uint32_t music_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", "");

    g_music_title = ST_CONTEXT.audio()->musicGetTitle(music_id);
    return g_music_title.c_str();
}

ST_API const char* st_music_get_composer(uint32_t music_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", "");

    g_music_composer = ST_CONTEXT.audio()->musicGetComposer(music_id);
    return g_music_composer.c_str();
}

ST_API const char* st_music_get_key(uint32_t music_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", "");

    g_music_key = ST_CONTEXT.audio()->musicGetKey(music_id);
    return g_music_key.c_str();
}

ST_API float st_music_get_tempo(uint32_t music_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0.0f);

    return ST_CONTEXT.audio()->musicGetTempo(music_id);
}

ST_API bool st_music_free(uint32_t music_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", false);

    return ST_CONTEXT.audio()->musicFree(music_id);
}

ST_API void st_music_free_all(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->musicFreeAll();
}

ST_API uint32_t st_music_get_count(void) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return static_cast<uint32_t>(ST_CONTEXT.audio()->musicGetCount());
}

ST_API uint32_t st_music_get_memory(void) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return static_cast<uint32_t>(ST_CONTEXT.audio()->musicGetMemoryUsage());
}

// =============================================================================
// Audio API - SID Player (Commodore 64 Music)
// =============================================================================

// Thread-local storage for SID string returns
static thread_local std::string g_sid_title;
static thread_local std::string g_sid_author;
static thread_local std::string g_sid_copyright;

ST_API uint32_t st_sid_load_file(const char* filename) {
    printf("[st_api_audio] st_sid_load_file called with filename: %s\n", filename ? filename : "NULL");
    fflush(stdout);

    if (!filename) {
        ST_SET_ERROR("SID filename is null");
        printf("[st_api_audio] ERROR: filename is null\n");
        fflush(stdout);
        return 0;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    printf("[st_api_audio] Calling AudioManager->sidLoadFile...\n");
    fflush(stdout);
    uint32_t result = ST_CONTEXT.audio()->sidLoadFile(filename);
    printf("[st_api_audio] st_sid_load_file returning: %u\n", result);
    fflush(stdout);
    return result;
}

ST_API uint32_t st_sid_load_memory(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        ST_SET_ERROR("SID data is null or empty");
        return 0;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->sidLoadMemory(data, size);
}

ST_API void st_sid_play(uint32_t sid_id, int subtune, float volume) {
    printf("[st_api_audio] st_sid_play called with sid_id=%u, subtune=%d, volume=%.2f\n", sid_id, subtune, volume);
    fflush(stdout);

    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    printf("[st_api_audio] Calling AudioManager->sidPlay...\n");
    fflush(stdout);
    ST_CONTEXT.audio()->sidPlay(sid_id, subtune, volume);
    printf("[st_api_audio] st_sid_play returned\n");
    fflush(stdout);
}

ST_API void st_sid_stop(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->sidStop();
}

ST_API void st_sid_pause(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->sidPause();
}

ST_API void st_sid_resume(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->sidResume();
}

ST_API bool st_sid_is_playing(void) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", false);

    return ST_CONTEXT.audio()->sidIsPlaying();
}

ST_API void st_sid_set_volume(float volume) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->sidSetVolume(volume);
}

ST_API const char* st_sid_get_title(uint32_t sid_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", "");

    g_sid_title = ST_CONTEXT.audio()->sidGetTitle(sid_id);
    return g_sid_title.c_str();
}

ST_API const char* st_sid_get_author(uint32_t sid_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", "");

    g_sid_author = ST_CONTEXT.audio()->sidGetAuthor(sid_id);
    return g_sid_author.c_str();
}

ST_API const char* st_sid_get_copyright(uint32_t sid_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", "");

    g_sid_copyright = ST_CONTEXT.audio()->sidGetCopyright(sid_id);
    return g_sid_copyright.c_str();
}

ST_API int st_sid_get_subtune_count(uint32_t sid_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->sidGetSubtuneCount(sid_id);
}

ST_API int st_sid_get_default_subtune(uint32_t sid_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return ST_CONTEXT.audio()->sidGetDefaultSubtune(sid_id);
}

ST_API void st_sid_set_quality(int quality) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->sidSetQuality(quality);
}

ST_API void st_sid_set_chip_model(int model) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->sidSetChipModel(model);
}

ST_API void st_sid_set_speed(float speed) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->sidSetSpeed(speed);
}

ST_API void st_sid_set_max_sids(int maxSids) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->sidSetMaxSids(maxSids);
}

ST_API int st_sid_get_max_sids() {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 3);

    return ST_CONTEXT.audio()->sidGetMaxSids();
}

ST_API float st_sid_get_time() {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0.0f);

    return ST_CONTEXT.audio()->sidGetTime();
}

ST_API bool st_sid_free(uint32_t sid_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", false);

    return ST_CONTEXT.audio()->sidFree(sid_id);
}

ST_API void st_sid_free_all(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");

    ST_CONTEXT.audio()->sidFreeAll();
}

ST_API bool st_sid_exists(uint32_t sid_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", false);

    return ST_CONTEXT.audio()->sidExists(sid_id);
}

ST_API uint32_t st_sid_get_count(void) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return static_cast<uint32_t>(ST_CONTEXT.audio()->sidGetCount());
}

ST_API uint32_t st_sid_get_memory(void) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);

    return static_cast<uint32_t>(ST_CONTEXT.audio()->sidGetMemoryUsage());
}

// =============================================================================
// Voice Controller API
// =============================================================================


// =============================================================================
// Voice Controller API - Direct VoiceController Access
// =============================================================================

ST_API void st_voice_set_waveform(int voiceNum, int waveform) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetWaveform(voiceNum, waveform);
}

ST_API void st_voice_set_frequency(int voiceNum, float frequencyHz) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetFrequency(voiceNum, frequencyHz);
}

ST_API void st_voice_set_note(int voiceNum, int midiNote) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetNote(voiceNum, midiNote);
}

ST_API void st_voice_set_note_name(int voiceNum, const char* noteName) {
    if (!noteName) return;
    ST_LOCK;
    auto* vc = ST_CONTEXT.audio() ? ST_CONTEXT.audio()->getVoiceController() : nullptr;
    if (vc) {
        vc->setNoteName(voiceNum, noteName);
    }
}

ST_API void st_voice_set_envelope(int voiceNum, float attackMs, float decayMs, float sustainLevel, float releaseMs) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetEnvelope(voiceNum, attackMs, decayMs, sustainLevel, releaseMs);
}

ST_API void st_voice_set_gate(int voiceNum, int gateOn) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetGate(voiceNum, gateOn != 0);
}

ST_API void st_voice_set_volume(int voiceNum, float volume) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetVolume(voiceNum, volume);
}

ST_API void st_voice_set_pulse_width(int voiceNum, float pulseWidth) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetPulseWidth(voiceNum, pulseWidth);
}

ST_API void st_voice_set_filter_routing(int voiceNum, int enabled) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetFilterRouting(voiceNum, enabled != 0);
}

ST_API void st_voice_set_filter_type(int filterType) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetFilterType(filterType);
}

ST_API void st_voice_set_filter_cutoff(float cutoffHz) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetFilterCutoff(cutoffHz);
}

ST_API void st_voice_set_filter_resonance(float resonance) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetFilterResonance(resonance);
}

ST_API void st_voice_set_filter_enabled(int enabled) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetFilterEnabled(enabled != 0);
}

ST_API void st_voice_set_master_volume(float volume) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetMasterVolume(volume);
}

ST_API float st_voice_get_master_volume(void) {
    ST_LOCK;
    if (!ST_CONTEXT.audio()) return 0.0f;
    return ST_CONTEXT.audio()->voiceGetMasterVolume();
}

ST_API void st_voice_reset_all(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceResetAll();
}

ST_API int st_voice_get_active_count(void) {
    ST_LOCK;
    if (!ST_CONTEXT.audio()) return 0;
    return ST_CONTEXT.audio()->voiceGetActiveCount();
}

ST_API int st_voices_are_playing(void) {
    ST_LOCK;
    if (!ST_CONTEXT.audio()) return 0;
    return ST_CONTEXT.audio()->voicesArePlaying() ? 1 : 0;
}

ST_API void st_voice_direct(const char* destination) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    if (destination && strlen(destination) > 0) {
        ST_CONTEXT.audio()->voiceSetRenderMode(true, destination);
    } else {
        ST_CONTEXT.audio()->voiceSetRenderMode(false, "");
    }
}

ST_API uint32_t st_voice_direct_slot(int slotNum, float volume, float duration) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);
    return ST_CONTEXT.audio()->voiceRenderToSlot(slotNum, volume, duration);
}

ST_API void st_voice_set_pan(int voiceNum, float pan) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetPan(voiceNum, pan);
}

ST_API void st_voice_set_ring_mod(int voiceNum, int sourceVoice) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetRingMod(voiceNum, sourceVoice);
}

ST_API void st_voice_set_sync(int voiceNum, int sourceVoice) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetSync(voiceNum, sourceVoice);
}

ST_API void st_voice_set_portamento(int voiceNum, float time) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetPortamento(voiceNum, time);
}

ST_API void st_voice_set_detune(int voiceNum, float cents) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetDetune(voiceNum, cents);
}

ST_API void st_voice_set_delay_enable(int voiceNum, int enabled) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetDelayEnable(voiceNum, enabled != 0);
}

ST_API void st_voice_set_delay_time(int voiceNum, float time) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetDelayTime(voiceNum, time);
}

ST_API void st_voice_set_delay_feedback(int voiceNum, float feedback) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetDelayFeedback(voiceNum, feedback);
}

ST_API void st_voice_set_delay_mix(int voiceNum, float mix) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetDelayMix(voiceNum, mix);
}

ST_API void st_lfo_set_waveform(int lfoNum, int waveform) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->lfoSetWaveform(lfoNum, waveform);
}

ST_API void st_lfo_set_rate(int lfoNum, float rateHz) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->lfoSetRate(lfoNum, rateHz);
}

ST_API void st_lfo_reset(int lfoNum) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->lfoReset(lfoNum);
}

ST_API void st_lfo_to_pitch(int voiceNum, int lfoNum, float depthCents) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->lfoToPitch(voiceNum, lfoNum, depthCents);
}

ST_API void st_lfo_to_volume(int voiceNum, int lfoNum, float depth) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->lfoToVolume(voiceNum, lfoNum, depth);
}

ST_API void st_lfo_to_filter(int voiceNum, int lfoNum, float depthHz) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->lfoToFilter(voiceNum, lfoNum, depthHz);
}

ST_API void st_lfo_to_pulsewidth(int voiceNum, int lfoNum, float depth) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->lfoToPulseWidth(voiceNum, lfoNum, depth);
}

ST_API void st_voice_set_physical_model(int voiceNum, int modelType) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetPhysicalModel(voiceNum, modelType);
}

ST_API void st_voice_set_physical_damping(int voiceNum, float damping) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetPhysicalDamping(voiceNum, damping);
}

ST_API void st_voice_set_physical_brightness(int voiceNum, float brightness) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetPhysicalBrightness(voiceNum, brightness);
}

ST_API void st_voice_set_physical_excitation(int voiceNum, float excitation) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetPhysicalExcitation(voiceNum, excitation);
}

ST_API void st_voice_set_physical_resonance(int voiceNum, float resonance) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetPhysicalResonance(voiceNum, resonance);
}

ST_API void st_voice_set_physical_tension(int voiceNum, float tension) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetPhysicalTension(voiceNum, tension);
}

ST_API void st_voice_set_physical_pressure(int voiceNum, float pressure) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voiceSetPhysicalPressure(voiceNum, pressure);
}

ST_API void st_voice_physical_trigger(int voiceNum) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voicePhysicalTrigger(voiceNum);
}
// =============================================================================

ST_API void st_music_save_to_wav(const char* scriptName, const char* assetName, float duration) {
    if (!scriptName || !assetName) {
        ST_SET_ERROR("Script name or asset name is null");
        return;
    }

    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    
    auto cartManager = ST_CONTEXT.getCartManager();
    if (!cartManager) {
        ST_SET_ERROR("CartManager not available");
        return;
    }

    // Check if cart is active
    if (!cartManager->isCartActive()) {
        ST_SET_ERROR("No active cart - cannot save WAV asset");
        return;
    }

    // Render to temporary file
    std::string tempPath = std::string("/tmp/voicescript_render_") + assetName + ".wav";
    
    printf("[st_music_save_to_wav] Rendering '%s' to temp file: %s\n", scriptName, tempPath.c_str());
    
    if (!ST_CONTEXT.audio()->voiceScriptRenderToWAV(scriptName, tempPath, duration)) {
        ST_SET_ERROR("Failed to render VoiceScript to WAV");
        return;
    }

    // Add to cart as music asset
    printf("[st_music_save_to_wav] Adding WAV to cart as '%s'\n", assetName);
    
    auto result = cartManager->addMusicFromFile(tempPath, assetName);
    
    // Clean up temp file
    std::remove(tempPath.c_str());
    
    if (!result.success) {
        ST_SET_ERROR("Failed to add WAV to cart");
        return;
    }

    printf("[st_music_save_to_wav] Successfully saved '%s' to cart as '%s.wav'\n", 
           scriptName, assetName);
}

ST_API uint32_t st_vscript_save_to_bank(const char* scriptName, float duration) {
    if (!scriptName) {
        ST_SET_ERROR("Script name is null");
        return 0;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);
    
    printf("[st_vscript_save_to_bank] Rendering '%s' to sound bank (duration: %.1fs)\n", 
           scriptName, duration);
    
    uint32_t soundId = ST_CONTEXT.audio()->voiceScriptSaveToBank(scriptName, duration);
    
    if (soundId > 0) {
        printf("[st_vscript_save_to_bank] Successfully saved '%s' to sound bank with ID %u\n", 
               scriptName, soundId);
    } else {
        ST_SET_ERROR("Failed to render VoiceScript to sound bank");
    }
    
    return soundId;
}

// =============================================================================
// VOICES Timeline System API
// =============================================================================

ST_API void st_voices_start(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voicesStartRecording();
}

ST_API void st_voice_wait(float beats) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voicesAdvanceBeatCursor(beats);
}

ST_API void st_voices_set_tempo(float bpm) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voicesSetTempo(bpm);
}

ST_API void st_voices_end_slot(int slot, float volume) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voicesEndAndSaveToSlot(slot, volume);
}

ST_API uint32_t st_voices_next_slot(float volume) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.audio(), "AudioManager", 0);
    return ST_CONTEXT.audio()->voicesEndAndReturnSlot(volume);
}

ST_API void st_voices_end_play(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    ST_CONTEXT.audio()->voicesEndAndPlay();
}

ST_API void st_voices_end_save(const char* filename) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.audio(), "AudioManager");
    if (!filename) {
        ST_SET_ERROR("Filename cannot be null");
        return;
    }
    ST_CONTEXT.audio()->voicesEndAndSaveToWAV(filename);
}
