#ifndef OBOL_MISC_SOONCE_H
#define OBOL_MISC_SOONCE_H

#include <atomic>

// Small internal helper for diagnostics and compatibility fallbacks that
// should execute once across all rendering threads.  Unlike a mutable
// function-local bool, this remains race-free after SoDB::init().
class SoOnceFlag {
public:
  SoOnceFlag() noexcept = default;

  SoOnceFlag(const SoOnceFlag &) = delete;
  SoOnceFlag & operator=(const SoOnceFlag &) = delete;

  bool first() noexcept
  {
    return !this->flag.test_and_set(std::memory_order_relaxed);
  }

private:
  std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

#endif // OBOL_MISC_SOONCE_H
