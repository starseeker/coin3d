#include <Inventor/SbName.h>
#include <Inventor/SoType.h>

class Probe {
public:
  static void initClass();

private:
  static SoType classTypeId;
};

SoType Probe::classTypeId = SoType::badType();

void
Probe::initClass()
{
  if (Probe::classTypeId == SoType::badType()) {
    Probe::classTypeId =
      SoType::createType(SoType::badType(), SbName("Probe"));
  }
}
