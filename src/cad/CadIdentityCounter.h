#ifndef OBOL_CAD_IDENTITY_COUNTER_H
#define OBOL_CAD_IDENTITY_COUNTER_H

#include <atomic>
#include <exception>
#include <limits>
#include <type_traits>

namespace Obol {
namespace internal {

template <typename UInt>
constexpr bool
cadIdentitySuccessor(UInt current, UInt& successor) noexcept
{
    static_assert(std::is_integral<UInt>::value &&
            std::is_unsigned<UInt>::value,
        "CAD identities require an unsigned integral representation");
    if (current == std::numeric_limits<UInt>::max())
        return false;
    successor = static_cast<UInt>(current + UInt{1});
    return true;
}

[[noreturn]] inline void
cadIdentityExhausted() noexcept
{
    /* Reusing an authorization identity can make stale retained state look
     * current.  Exhaustion is therefore a process-integrity failure, not a
     * reason to wrap or silently saturate. */
    std::terminate();
}

template <typename UInt>
inline void
cadAdvanceIdentity(UInt& identity) noexcept
{
    UInt successor = identity;
    if (!cadIdentitySuccessor(identity, successor))
        cadIdentityExhausted();
    identity = successor;
}

template <typename UInt>
inline UInt
cadTakeNonzeroIdentity(UInt& nextIdentity) noexcept
{
    if (nextIdentity == 0)
        cadIdentityExhausted();
    const UInt identity = nextIdentity;
    cadAdvanceIdentity(nextIdentity);
    return identity;
}

template <typename UInt>
inline UInt
cadAtomicTakeNonzeroIdentity(std::atomic<UInt>& nextIdentity,
    std::memory_order successOrder = std::memory_order_relaxed,
    std::memory_order failureOrder = std::memory_order_relaxed) noexcept
{
    UInt observed = nextIdentity.load(std::memory_order_relaxed);
    for (;;) {
        if (observed == 0)
            cadIdentityExhausted();
        UInt successor = observed;
        if (!cadIdentitySuccessor(observed, successor))
            cadIdentityExhausted();
        if (nextIdentity.compare_exchange_weak(observed, successor,
                successOrder, failureOrder))
            return observed;
    }
}

} // namespace internal
} // namespace Obol

#endif // OBOL_CAD_IDENTITY_COUNTER_H
