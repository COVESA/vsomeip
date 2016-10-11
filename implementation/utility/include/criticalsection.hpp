#ifndef VSOMEIP_CRITICALSECTION_HPP
#define VSOMEIP_CRITICALSECTION_HPP

#include <memory>
#include <mutex>


namespace vsomeip {

#ifdef WIN32

    // Windows: CriticalSection uses win32 CRITICAL_SECTION.
    // Interface mimics std::mutex so we can use it in
    // conjunction with std::unique_lock.
    class CriticalSection final {
    public:
        CriticalSection();
        ~CriticalSection();

        // prevent copying
        CriticalSection(const CriticalSection&) = delete;
        CriticalSection& operator=(const CriticalSection&) = delete;

        void lock();
        void unlock();
        bool try_lock();

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

#else

    // Linux: CriticalSection is a type alias for std::mutex.
    using CriticalSection = std::mutex;

#endif

} // namespace vsomeip



#endif //VSOMEIP_CRITICALSECTION_HPP
