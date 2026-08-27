#include <Inventor/SbName.h>
#include <Inventor/SoType.h>

class WrongProbe {
public:
  static void initClass();
};

static void * createDifferentProbeValue(void *)
{
  return new int(7);
}

void
WrongProbe::initClass()
{
  (void)SoType::createType(SoType::badType(), SbName("DifferentProbe"),
                           createDifferentProbeValue);
}
