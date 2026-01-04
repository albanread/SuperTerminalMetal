//
// LuaFormatterWrapper.h
// SuperTerminal Framework - Lua Code Formatter Wrapper
//
// Provides a simple C++ interface to the LuaFormatter library
// for formatting Lua source code from within the editor.
//

#pragma once

#include <string>
#include <optional>

namespace SuperTerminal {

// Configuration options for Lua formatting
struct LuaFormatterConfig {
    int columnLimit = 80;
    int indentWidth = 4;
    int tabWidth = 4;
    int continuationIndentWidth = 4;
    int spacesBeforeCall = 1;
    int columnTableLimit = 0;
    char tableSep = ',';
    bool useTab = false;
    bool keepSimpleControlBlockOneLine = true;
    bool keepSimpleFunctionOneLine = true;
    bool alignArgs = true;
    bool breakAfterFunctionCallLp = false;
    bool breakBeforeFunctionCallRp = false;
    bool alignParameter = true;
    bool chopDownParameter = false;
    bool breakAfterFunctionDefLp = false;
    bool breakBeforeFunctionDefRp = false;
    bool alignTableField = true;
    bool breakAfterTableLb = true;
    bool breakBeforeTableRb = true;
    bool chopDownTable = false;
    bool chopDownKvTable = true;
    bool extraSepAtTableEnd = false;
    bool breakAfterOperator = true;
    bool doubleQuoteToSingleQuote = false;
    bool singleQuoteToDoubleQuote = false;
    bool spacesInsideFunctionDefParens = false;
    bool spacesInsideFunctionCallParens = false;
    bool spacesInsideTableBraces = false;
    bool spacesAroundEqualsInField = true;
    
    // Line separator: "input", "os", "lf", "crlf", "cr"
    std::string lineSeparator = "input";
    
    // Default constructor with reasonable defaults
    LuaFormatterConfig() = default;
};

class LuaFormatterWrapper {
public:
    // Format Lua source code with the given configuration
    // Returns formatted code on success, or std::nullopt on error
    static std::optional<std::string> format(
        const std::string& sourceCode,
        const LuaFormatterConfig& config = LuaFormatterConfig()
    );
    
    // Format Lua source code with default configuration
    // Returns formatted code on success, or std::nullopt on error
    static std::optional<std::string> formatWithDefaults(const std::string& sourceCode);
    
    // Check if the formatter is available (library linked properly)
    static bool isAvailable();
    
    // Get last error message (if formatting failed)
    static std::string getLastError();
    
private:
    static std::string lastError;
};

} // namespace SuperTerminal