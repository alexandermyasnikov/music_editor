
#include <cstdarg>
#include <chrono>



#define LOGGER(name, indent)       Logger logger(indent, name, __FILE__, __PRETTY_FUNCTION__, __LINE__)
#define LOG(name, indent, ...)     Logger::log(name, indent, __LINE__, __VA_ARGS__)

#define LOGGER_SNI                 LOGGER("sni  ", LoggerIndentSniffer::indent)
#define LOG_SNI(...)               LOG("sni  ", LoggerIndentSniffer::indent, __VA_ARGS__)

#define LOGGER_TEST                LOGGER("test ", LoggerIndentTest::indent)
#define LOG_TEST(...)              LOG("test ", LoggerIndentTest::indent, __VA_ARGS__)



template <typename T>
struct LoggerIndent {
  static inline int indent;
};

struct LoggerIndentSniffer : LoggerIndent<LoggerIndentSniffer> { };
struct LoggerIndentTest    : LoggerIndent<LoggerIndentTest> { };



class Logger {
 public:
  Logger(int& indent, const char* name, const char* file, const char* function, const int line)
  : indent(indent), name(name), file(file), function(function), line(line) {
    fprintf(stderr, "%s %d    %*s--> %s #%d\n", name, indent / 2, indent, "", function, line);
    fflush(stderr);
    indent += 2;
    time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  }

  ~Logger( ) {
    indent -= 2;

    time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - time;
    fprintf(stderr, "%s %d %c  %*s<-- %s %ldms\n", name, indent / 2, std::uncaught_exceptions() ? '*' : ' ', indent, "", function, time);
    fflush(stderr);
  }

  static void log(const char* name, int indent, int line, const char* format, ...) {
    fprintf(stderr, "%s %d    %*s#%d    ", name, indent / 2, indent, "", line);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
  }

 private:
  int&         indent;
  const  char* name;
  const  char* file; // TODO
  const  char* function;
  const  int   line;
  uint64_t     time;
};

