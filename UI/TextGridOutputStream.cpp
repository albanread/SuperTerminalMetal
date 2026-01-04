//
// TextGridOutputStream.cpp
// SuperTerminal Framework - Text Grid Output Stream
//
// Implementation of scrolling text output for TextGrid.
//

#include "TextGridOutputStream.h"
#include "../Display/TextGrid.h"
#include <algorithm>

namespace SuperTerminal {

// Default colors (white on black)
static const uint32_t DEFAULT_FG_COLOR = 0xFFFFFFFF;
static const uint32_t DEFAULT_BG_COLOR = 0xFF000000;

// Default scrollback buffer size (lines)
static const size_t DEFAULT_MAX_LINES = 1000;

TextGridOutputStream::TextGridOutputStream(std::shared_ptr<TextGrid> textGrid,
                                           int topRow, int bottomRow)
    : textGrid_(textGrid)
    , topRow_(topRow)
    , bottomRow_(bottomRow)
    , foregroundColor_(DEFAULT_FG_COLOR)
    , backgroundColor_(DEFAULT_BG_COLOR)
    , currentCol_(0)
    , currentRow_(0)
    , autoScroll_(true)
    , scrollOffset_(0)
    , maxLines_(DEFAULT_MAX_LINES)
{
    // Start with one empty line
    lines_.push_back("");
}

TextGridOutputStream::~TextGridOutputStream() {
}

void TextGridOutputStream::print(const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (text.empty()) {
        return;
    }

    // Process the text character by character
    for (char ch : text) {
        if (ch == '\n') {
            // Newline - move to next line
            addLine("");
            currentCol_ = 0;
        } else if (ch == '\r') {
            // Carriage return - move to start of line
            currentCol_ = 0;
        } else if (ch == '\t') {
            // Tab - advance to next tab stop (every 8 chars)
            int nextTab = ((currentCol_ / 8) + 1) * 8;
            int spaces = nextTab - currentCol_;
            for (int i = 0; i < spaces; i++) {
                ensureCurrentLine();
                std::string& currentLine = lines_.back();
                if (currentCol_ >= getWidth()) {
                    // Wrap to next line
                    addLine("");
                    currentCol_ = 0;
                } else {
                    currentLine.push_back(' ');
                    currentCol_++;
                }
            }
        } else {
            // Regular character
            ensureCurrentLine();
            std::string& currentLine = lines_.back();

            // Check if we need to wrap
            if (currentCol_ >= getWidth()) {
                addLine("");
                currentCol_ = 0;
            }

            // Insert character at current position
            if (currentCol_ >= static_cast<int>(currentLine.length())) {
                currentLine.append(currentCol_ - currentLine.length(), ' ');
                currentLine.push_back(ch);
            } else {
                currentLine[currentCol_] = ch;
            }
            currentCol_++;
        }
    }

    // Trim buffer if needed
    trimBuffer();
}

void TextGridOutputStream::println(const std::string& text) {
    print(text);
    println();
}

void TextGridOutputStream::println() {
    std::lock_guard<std::mutex> lock(mutex_);
    addLine("");
    currentCol_ = 0;
}

void TextGridOutputStream::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    lines_.clear();
    lines_.push_back("");
    currentCol_ = 0;
    currentRow_ = 0;
    scrollOffset_ = 0;
}

void TextGridOutputStream::scroll() {
    std::lock_guard<std::mutex> lock(mutex_);
    addLine("");
}

std::pair<int, int> TextGridOutputStream::getCursor() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::make_pair(currentCol_, currentRow_);
}

void TextGridOutputStream::setCursor(int x, int y) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Clamp to valid range
    int width = getWidth();
    int height = getRows();

    currentCol_ = std::max(0, std::min(x, width - 1));
    currentRow_ = std::max(0, std::min(y, height - 1));

    // Ensure we have enough lines
    int neededLines = currentRow_ + 1;
    while (static_cast<int>(lines_.size()) < neededLines) {
        lines_.push_back("");
    }
}

void TextGridOutputStream::home() {
    std::lock_guard<std::mutex> lock(mutex_);
    currentCol_ = 0;
    currentRow_ = 0;
}

void TextGridOutputStream::setColors(uint32_t foreground, uint32_t background) {
    std::lock_guard<std::mutex> lock(mutex_);
    foregroundColor_ = foreground;
    backgroundColor_ = background;
}

int TextGridOutputStream::getWidth() const {
    if (!textGrid_) {
        return 80;  // Default width
    }
    return textGrid_->getWidth();
}

void TextGridOutputStream::render() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!textGrid_) {
        return;
    }

    int height = getRows();
    int width = getWidth();

    // Calculate which lines to display based on scroll position
    int totalLines = static_cast<int>(lines_.size());
    int startLine = totalLines - height - scrollOffset_;
    if (startLine < 0) {
        startLine = 0;
    }

    // Render each row
    for (int row = 0; row < height; row++) {
        int lineIndex = startLine + row;

        // Clear the row
        for (int x = 0; x < width; x++) {
            textGrid_->putChar(x, topRow_ + row, ' ', foregroundColor_, backgroundColor_);
        }

        // Render line if it exists
        if (lineIndex >= 0 && lineIndex < totalLines) {
            const std::string& line = lines_[lineIndex];
            int x = 0;
            for (size_t i = 0; i < line.length() && x < width; i++) {
                char ch = line[i];
                textGrid_->putChar(x, topRow_ + row, ch, foregroundColor_, backgroundColor_);
                x++;
            }
        }
    }
}

void TextGridOutputStream::setScrollPosition(int offset) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int maxOffset = static_cast<int>(lines_.size()) - getRows();
    if (maxOffset < 0) {
        maxOffset = 0;
    }
    
    scrollOffset_ = std::max(0, std::min(offset, maxOffset));
}

void TextGridOutputStream::scrollUp(int lines) {
    std::lock_guard<std::mutex> lock(mutex_);
    scrollOffset_ += lines;
    
    int maxOffset = static_cast<int>(lines_.size()) - getRows();
    if (maxOffset < 0) {
        maxOffset = 0;
    }
    
    if (scrollOffset_ > maxOffset) {
        scrollOffset_ = maxOffset;
    }
}

void TextGridOutputStream::scrollDown(int lines) {
    std::lock_guard<std::mutex> lock(mutex_);
    scrollOffset_ -= lines;
    
    if (scrollOffset_ < 0) {
        scrollOffset_ = 0;
    }
}

void TextGridOutputStream::addLine(const std::string& line) {
    lines_.push_back(line);
    
    // If auto-scroll is enabled, reset scroll offset
    if (autoScroll_) {
        scrollOffset_ = 0;
    }
    
    trimBuffer();
}

void TextGridOutputStream::wrapAndAddLine(const std::string& text) {
    // Not currently used, but available for future word-wrapping
    addLine(text);
}

void TextGridOutputStream::ensureCurrentLine() {
    if (lines_.empty()) {
        lines_.push_back("");
    }
}

void TextGridOutputStream::trimBuffer() {
    // Keep buffer size under limit
    while (lines_.size() > maxLines_) {
        lines_.pop_front();
    }
}

std::string TextGridOutputStream::getCurrentLine() const {
    if (lines_.empty()) {
        return "";
    }
    return lines_.back();
}

void TextGridOutputStream::updateCurrentLine(const std::string& newContent) {
    if (!lines_.empty()) {
        lines_.back() = newContent;
    }
}

} // namespace SuperTerminal
