
-- https://premake.github.io/docs/Workspaces-and-Projects
workspace "VorbWorkspace"
   configurations { "DebugFast", "Debug", "Release" }
   platforms { "x64", "Win32" }
   startproject "vorb"

project "vorb"
   kind "ConsoleApp"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"

   files { "src/**.h", "src/**.cpp" }

   filter "configurations:DebugFast"
      defines { "DEBUG_FAST" }
      symbols "On"

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

   filter "platforms:x64"
      architecture "x64"

   -- You will need to add your specific settings such as include directories, library dependencies, etc.

project "GameClient"
   kind "ConsoleApp"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"

   files { "src/**.h", "src/**.cpp" }

   filter "configurations:DebugFast"
      defines { "DEBUG_FAST" }
      symbols "On"

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

   filter "platforms:x64"
      architecture "x64"

   filter "platforms:Win32"
      architecture "x86"

   -- You will need to add your specific settings such as include directories, library dependencies, etc.
