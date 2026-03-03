static u64 PAGE_SIZE;

static void init_arena_globals() {
#if OS_WINDOWS
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	PAGE_SIZE = info.dwPageSize;
#elif OS_UNIX
	PAGE_SIZE = sysconf(_SC_PAGESIZE);
#endif
}

struct Arena {
	u8* base = NULL;
	u8* cur = NULL;
	u8* end = NULL;
	u64 commited_memory = 0;

	Arena(u64 cap);
	~Arena();
	
	template <typename T>
	inline T* alloc(u64 amount = 1) {
		return (T*)_alloc(sizeof(T) * amount, alignof(T));
	}

	inline u8* mark() const { return cur; }
	inline void reset(u8* mark = NULL) { cur = mark ? mark : base; }

	void* _alloc(u64 size, u64 alignment);
	void _commit_memory(u64 pages);
};

Arena::Arena(u64 cap) {
#if OS_WINDOWS
	base = (u8*)VirtualAlloc(NULL, cap, MEM_RESERVE, PAGE_NOACCESS);
	if (base == NULL) {
		Panic("Failed to allocate arena buffer!");
	}
#elif OS_UNIX
	base = (u8*)mmap(NULL, cap, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (base == MAP_FAILED) {
		Panic("Failed to allocate arena buffer!");
	}
#endif
	cur = base;
	end = base + cap;
}

Arena::~Arena() {
#if OS_WINDOWS
	VirtualFree(base, 0, MEM_RELEASE);
#elif OS_UNIX
	munmap(base, end - base);
#endif
	base = end = cur = NULL;
	commited_memory = 0;
}

void* Arena::_alloc(u64 size, u64 alignment) {
	u8* aligned_ptr = (u8*)((u64(cur) + alignment - 1) & ~(alignment - 1));
	u8* target = aligned_ptr + size;
	if (target > end) return NULL;

	if (target > base + commited_memory) {
		u64 pages = ceil(f32(size) / f32(PAGE_SIZE));
		_commit_memory(pages);
	}

	cur = target;

	return aligned_ptr;
}

void Arena::_commit_memory(u64 pages) {
	u8* pos = base + commited_memory;
	u64 amount = pages * PAGE_SIZE;

#if OS_WINDOWS
	void* res = VirtualAlloc(pos, amount, MEM_COMMIT, PAGE_READWRITE);
	if (res == NULL) {
		Panic("Failed to commit memory!");
	}
#elif OS_UNIX
	s32 res = mprotect(pos, amount, PROT_READ | PROT_WRITE);
	if (res == -1) {
		Panic("Failed to commit memory!");
	}
#endif

	commited_memory += amount;
}
