
ADD_DEFINITIONS( ${RCF_DEFINES} )

INCLUDE_DIRECTORIES( ${RCF_INCLUDES} )

ADD_EXECUTABLE(
    DemoServer
    ${RCF_ROOT}/demo/DemoServer.cpp)

TARGET_LINK_LIBRARIES( DemoServer RcfLib ${RCF_LIBS} )
