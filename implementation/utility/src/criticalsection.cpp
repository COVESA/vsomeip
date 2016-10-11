#include "../include/criticalsection.hpp"


#ifdef WIN32

#include <Windows.h>

namespace vsomeip {

    struct CriticalSection::Impl final {
        CRITICAL_SECTION m_criticalSection;
    };


    CriticalSection::CriticalSection()
    : m_impl(new CriticalSection::Impl()) {
        InitializeCriticalSection(&m_impl->m_criticalSection);
    }

    CriticalSection::~CriticalSection() {
        DeleteCriticalSection(&m_impl->m_criticalSection);
    }

    void CriticalSection::lock() {
        EnterCriticalSection(&m_impl->m_criticalSection);
    }

    bool CriticalSection::try_lock() {
        return (TryEnterCriticalSection(&m_impl->m_criticalSection) != 0);
    }

    void CriticalSection::unlock(){
        LeaveCriticalSection(&m_impl->m_criticalSection);
    }

} // namespace vsomeip

#endif
