using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

template <typename F>
struct Defer {
    F f;
    Defer(F f) : f(f) {}
    ~Defer() { f(); }
};

#define DEFER3(x, y) x##y
#define DEFER2(x, y) DEFER3(x, y)
#define DEFER(x)     DEFER2(x, __COUNTER__)
#define defer(code)  const Defer DEFER(_defer_){[&]{code;}}

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define CLAMP(min, max, x) MIN(min, MAX(max, x))

#define KB(x) (u64(x) << 10)
#define MB(x) (u64(x) << 20)
#define GB(x) (u64(x) << 30)
#define TB(x) (u64(x) << 40)

#if defined(_MSC_VER)
#define DEBUG_TRAP() __debugbreak()
#else
#define DEBUG_TRAP() __builtin_trap()
#endif

#define Panic(fmt, ...) do { \
    fprintf(stderr, "\033[41;30m PANIC \033[0m\t\033[31m" fmt "\033[0m\n",## __VA_ARGS__); \
    DEBUG_TRAP(); \
    exit(EXIT_FAILURE); \
} while (0)

#define Log(fmt, ...) printf("\033[30;44m LOG \033[0m\t\033[34m" fmt "\033[0m\n",## __VA_ARGS__)
#define Warn(fmt, ...) printf("\033[30;43m WARN \033[0m\t\033[33m" fmt "\033[0m\n",## __VA_ARGS__)
#define Error(fmt, ...) fprintf(stderr, "\033[30;41m ERROR \033[0m\t\033[31m" fmt "\033[0m\n",## __VA_ARGS__)

#if !defined(NDEBUG)
#define DEBUG true
#else
#define DEBUG false
#endif

static constexpr u32 MAX_U32 = 0xFFFFFFFF;
