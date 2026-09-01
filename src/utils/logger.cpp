module;
#include <chrono>
#include <print>
export module logger;

constexpr std::string RED = "\033[31m";
constexpr std::string YELLOW = "\033[33m";
constexpr std::string WHITE = "\033[37m";

export enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

constexpr std::string_view to_string(LogLevel c) {
    switch (c) {
        case LogLevel::DEBUG:     return "Debug";
        case LogLevel::INFO:      return "Info";
        case LogLevel::WARNING:   return "Warning";
        case LogLevel::ERROR:     return "Error";
        case LogLevel::CRITICAL:  return "Critical";


    }
    return "Unknown";
}

constexpr std::string_view to_color(LogLevel c){
    switch (c) {
        case LogLevel::DEBUG:     return WHITE;
        case LogLevel::INFO:      return WHITE;
        case LogLevel::WARNING:   return YELLOW;
        case LogLevel::ERROR:     return RED;
        case LogLevel::CRITICAL:  return RED;


    }
    return "Unknown";

}

export class Logger {
  public:
    Logger() {}

    auto log(LogLevel level, const std::string& msg) -> void
    {
        auto current_time = std::chrono::system_clock::now();
        // Format: YYYY-MM-DD HR:MINUTE:SECOND:MS Message 
        // Color is the first parameter
        std::println("{}[{}] {}: {}", to_color(level), to_string(level), current_time, msg);
        // Set color to white after printing
        std::print(WHITE);

        return;
    }

    private:

};