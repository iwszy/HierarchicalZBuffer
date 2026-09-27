cmake_minimum_required(VERSION 3.16)

project(HierarchicalZBuffer
        VERSION 1.0.0
        DESCRIPTION "层次 Z-Buffer 消隐算法实现"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 单配置生成器（Ninja / Makefile）下默认用 Release —— 性能数据必须用 Release 采集，
# Debug 下 glm 的模板数学无法内联，光栅化部分会慢 10~30 倍，各算法的相对关系会被扭曲。
# 多配置生成器（Visual Studio）用 --config 指定，下面的属性按配置生效。
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(HierarchicalZBuffer
    src/main.cpp
    src/model.cpp
    src/scene.cpp
    src/basicZBuffer.cpp
    src/scanlineZBuffer.cpp
    src/basicHierarchicalZBuffer.cpp
    src/hierarchicalZBuffer.cpp
)

# 其余源码都是头文件形式：
#   algorithm 类（*.hpp）  zPyramid / triangleRasterizer / occlusionScene 等
target_include_directories(HierarchicalZBuffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include    # 第三方 glm
    ${CMAKE_CURRENT_SOURCE_DIR}/src        # 本工程的头文件
)

# 程序要读 models/、往 results/ 写 PNG，工作目录必须是工程根目录。
# 直接从 Visual Studio 里 F5 运行时也按这个目录启动。
set_target_properties(HierarchicalZBuffer PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(MSVC)
    # 源码是 UTF-8（含中文注释与中文字符串），必须显式告诉 MSVC。
    # 否则在中文 Windows 的 GBK 代码页下，控制台里打印的中文会变成乱码。
    target_compile_options(HierarchicalZBuffer PRIVATE /source-charset:utf-8)
    # 与 HierarchicalZBuffer.vcxproj 的 Release 配置保持一致：/O2 /Oi + 全程序优化(/GL /LTCG)。
    # 换编译器或改优化选项后，README 里的性能数据不再可比。
    target_compile_options(HierarchicalZBuffer PRIVATE $<$<CONFIG:Release>:/Oi>)
    set_target_properties(HierarchicalZBuffer PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else()
    # 非 MSVC 工具链：至少保证源码按 UTF-8 读、且按 UTF-8 生成可执行文件里的字符串
    target_compile_options(HierarchicalZBuffer PRIVATE
        -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

install(TARGETS HierarchicalZBuffer RUNTIME DESTINATION bin)
