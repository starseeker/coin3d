#ifndef COIN_SBVECI32CONVERSION_H
#define COIN_SBVECI32CONVERSION_H

#include <cstdint>
#include <limits>

namespace CoinInternal {

inline int32_t
clampToInt32(const double value)
{
  // A NaN has no direction in which to saturate.  Zero is deterministic and
  // preserves the historical result produced by common target platforms.
  if (value != value) return 0;
  if (value > static_cast<double>(std::numeric_limits<int32_t>::max()))
    return std::numeric_limits<int32_t>::max();
  if (value < static_cast<double>(std::numeric_limits<int32_t>::min()))
    return std::numeric_limits<int32_t>::min();
  return static_cast<int32_t>(value);
}

inline bool
outsideInt32(const double value)
{
  return value != value ||
         value > static_cast<double>(std::numeric_limits<int32_t>::max()) ||
         value < static_cast<double>(std::numeric_limits<int32_t>::min());
}

} // namespace CoinInternal

#endif // COIN_SBVECI32CONVERSION_H
