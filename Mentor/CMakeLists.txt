macro(add_example name)
  configure_file(${name}.cpp.in ${name}.cpp @ONLY)
  add_executable(${name} ${CMAKE_CURRENT_BINARY_DIR}/${name}.cpp)
  target_link_libraries(${name} ${PROJECT_NAME} ${COIN_TARGET_LINK_LIBRARIES})
  
  # Add include directories for OSMesa support (needed for headless examples)
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
  )
endmacro(add_example)

add_example(02.1.HelloCone.headless)
add_example(03.1.Molecule.headless)
add_example(05.1.FaceSet.headless)
add_example(07.1.BasicTexture.headless)
add_example(11.2.ReadString.headless)
add_example(13.6.Calculator.headless)

# Disable GUI examples - missing GUI libraries in CI environment
#add_example(02.1.HelloCone)
#add_example(02.2.EngineSpin)
#add_example(02.3.Trackball)
#add_example(02.4.Examiner)
# NOTE: Examples 02.2-02.4 require GUI toolkits (viewers, render areas) - not suitable for headless

#add_example(03.1.Molecule)
#add_example(03.2.Robot)
#add_example(03.3.Naming)
# NOTE: 03.2.Robot and 03.3.Naming require GUI viewers - not suitable for headless

#add_example(04.1.Cameras)
#add_example(04.2.Lights)
# NOTE: 04.1.Cameras depends on external file "parkbench.iv" not available in CI
# NOTE: 04.2.Lights may be suitable for headless but requires data files

#add_example(05.1.FaceSet)
#add_example(05.2.IndexedFaceSet)
#add_example(05.3.TriangleStripSet)
#add_example(05.4.QuadMesh)
#add_example(05.5.Binding)
#add_example(05.6.TransformOrdering)
# NOTE: 05.2-05.6 are good candidates for headless conversion but not implemented yet

#add_example(06.1.Text)
#add_example(06.2.Simple3DText)
#add_example(06.3.Complex3DText)
# NOTE: Text examples are suitable for headless but not implemented yet

#add_example(07.1.BasicTexture)
#add_example(07.2.TextureCoordinates)
#add_example(07.3.TextureFunction)
# NOTE: 07.2-07.3 depend on texture files and are not implemented yet

#add_example(09.2.Texture)
#add_example(09.3.Search)
#add_example(09.4.PickAction)
#add_example(09.5.GenSph)
# NOTE: 09.2 depends on texture files, 09.3-09.4 involve search/picking

#add_example(10.1.addEventCB)
#add_example(10.5.SelectionCB)
#add_example(10.6.PickFilterTopLevel)
#add_example(10.7.PickFilterManip)
# NOTE: Chapter 10 examples all require event handling and user interaction - not suitable for headless

#add_example(11.1.ReadFile)
#add_example(11.2.ReadString)
# NOTE: 11.1.ReadFile requires external data files

#add_example(12.1.FieldSensor)
#add_example(12.2.NodeSensor)
#add_example(12.3.AlarmSensor)
#add_example(12.4.TimerSensor)
# NOTE: Sensor examples require GUI interaction or real-time updates - not suitable for static headless renders

#add_example(13.1.GlobalFlds)
#add_example(13.2.ElapsedTime)
#add_example(13.3.TimeCounter)
#add_example(13.4.Gate)
#add_example(13.5.Boolean)
#add_example(13.6.Calculator)
#add_example(13.7.Rotor)
#add_example(13.8.Blinker)
# NOTE: Most engine examples involve animation/time - 13.6.Calculator was implemented

#add_example(14.1.FrolickingWords)
#add_example(14.3.Balance)
# NOTE: Chapter 14 examples involve complex animations not suitable for static headless renders

#add_example(15.1.ConeRadius)
#add_example(15.2.SliderBox)
#add_example(15.3.AttachManip)
#add_example(15.4.Customize)
# NOTE: Chapter 15 examples use draggers/manipulators which may have been removed from this fork
#
