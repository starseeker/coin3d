#include <Inventor/SbName.h>
#include <Inventor/SoType.h>

#include <stdexcept>

class ThrowProbe {
public:
  static void initClass();
};

static void * createThrowProbeValue(void *)
{
  return new int(42);
}

void
ThrowProbe::initClass()
{
  (void)SoType::createType(SoType::badType(), SbName("ThrowProbe"),
                           createThrowProbeValue);
  throw std::runtime_error("intentional partial plugin initialization");
}
