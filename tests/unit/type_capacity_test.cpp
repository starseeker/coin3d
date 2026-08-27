#include <gtest/gtest.h>

#include <Inventor/SbName.h>
#include <Inventor/SbString.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoType.h>

#include <cstdint>
#include <limits>

TEST(TypeRegistryCapacity, RejectsExhaustionWithoutWrappingSignedKeys)
{
  ASSERT_FALSE(SoDB::isInitialized());
  SoDB::init(nullptr);

  int created = 0;
  int lastKey = -1;
  for (int i = 0; i <= std::numeric_limits<int16_t>::max(); ++i) {
    SbString name;
    name.sprintf("CapacityProbe_%d", i);
    const SoType type = SoType::createType(SoType::badType(), SbName(name));
    if (type == SoType::badType()) break;
    EXPECT_GE(type.getKey(), 0);
    lastKey = type.getKey();
    ++created;
  }

  EXPECT_GT(created, 0);
  EXPECT_EQ(lastKey, std::numeric_limits<int16_t>::max());
  EXPECT_EQ(SoType::createType(SoType::badType(),
                               SbName("CapacityProbe_Overflow")),
            SoType::badType());
}
