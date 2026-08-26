#include <Obol/render/SoNanoRTContextManager.h>

#include <Inventor/SoDB.h>

int main()
{
  SoNanoRTContextManager manager;
  SoDB::init(&manager);
  return SoDB::isInitialized() ? 0 : 1;
}
