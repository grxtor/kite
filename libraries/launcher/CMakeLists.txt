cmake_minimum_required(VERSION 3.10)
project(launcher Java)
find_package(Java 1.7 REQUIRED COMPONENTS Development)

include(UseJava)
set(CMAKE_JAVA_JAR_ENTRY_POINT org.multimc.EntryPoint)
set(CMAKE_JAVA_COMPILE_FLAGS -target 7 -source 7 -Xlint:deprecation -Xlint:unchecked)

set(SRC
    io/github/grxtor/EntryPoint.java
    io/github/grxtor/Kite.java
    io/github/grxtor/LegacyFrame.java
    io/github/grxtor/NotFoundException.java
    io/github/grxtor/ParamBucket.java
    io/github/grxtor/ParseException.java
    io/github/grxtor/Utils.java
    io/github/grxtor/onesix/OneSixLauncher.java
    io/github/grxtor/modern/ModernLauncher.java
    net/minecraft/Kite.java
)
add_jar(NewLaunch ${SRC})
install_jar(NewLaunch "${JARS_DEST_DIR}")
