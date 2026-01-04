//
// LuaFormatterWrapper.cpp
// SuperTerminal Framework - Lua Code Formatter Wrapper Implementation
//
// Lightweight implementation using simple_lua_formatter
//

#include "LuaFormatterWrapper.h"
#include "simple_lua_format.h"
#include <iostream>

namespace SuperTerminal {

// Static member initialization
std::string LuaFormatterWrapper::lastError = "";

std::optional<std::string> LuaFormatterWrapper::format(
    const std::string& sourceCode,
    const LuaFormatterConfig& config)
{
    try {
        // Create formatter options from config
        SimpleLuaFormatter::FormatConfig options;
        
        // Map the config fields from SuperTerminal to SimpleLuaFormatter
        options.indentWidth = config.indentWidth;
        options.useTab = config.useTab;
        options.columnLimit = config.columnLimit;
        options.alignArgs = config.alignArgs;
        options.spacesAroundOperators = config.spacesAroundEqualsInField;
        options.spacesAfterCommas = true;
        options.keepBlankLines = true;
        
        // Format the code
        auto result = SimpleLuaFormatter::formatLua(sourceCode, options);
        
        if (!result) {
            lastError = SimpleLuaFormatter::getLastError();
            std::cerr << "LuaFormatterWrapper error: " << lastError << std::endl;
            return std::nullopt;
        }
        
        // Clear any previous error
        lastError.clear();
        
        return result;
    }
    catch (const std::exception& e) {
        lastError = std::string("Formatting error: ") + e.what();
        std::cerr << "LuaFormatterWrapper error: " << lastError << std::endl;
        return std::nullopt;
    }
    catch (...) {
        lastError = "Unknown formatting error occurred";
        std::cerr << "LuaFormatterWrapper: " << lastError << std::endl;
        return std::nullopt;
    }
}

std::optional<std::string> LuaFormatterWrapper::formatWithDefaults(const std::string& sourceCode)
{
    return format(sourceCode, LuaFormatterConfig());
}

bool LuaFormatterWrapper::isAvailable()
{
    // Simple formatter is always available (no external dependencies)
    return true;
}

std::string LuaFormatterWrapper::getLastError()
{
    return lastError;
}

} // namespace SuperTerminal