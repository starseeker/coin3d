#pragma once

// Legacy render cases include this adapter after their scene implementation.
// A system-GL build may therefore have pulled in Xlib first.  Its generic
// macros collide with identifiers in GoogleTest and are not needed by the
// registration code below.
#ifdef None
#undef None
#endif
#ifdef Bool
#undef Bool
#endif

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

namespace ObolTest {

// Run a source-level rendering case inside the shared render test process.
// The source keeps its historical argv[1] output-stem contract; generated
// files are redirected to the system temporary area for CTest isolation.
inline int runRenderingCase(int (*entry)(int, char **), const char *name)
{
    const std::filesystem::path output =
        std::filesystem::temp_directory_path() /
        (std::string("obol_render_") + name);
    std::string output_string = output.string();
    std::string program_string = std::string("obol_") + name;
    std::vector<char> program(program_string.begin(), program_string.end());
    std::vector<char> output_arg(output_string.begin(), output_string.end());
    program.push_back('\0');
    output_arg.push_back('\0');
    char *argv[] = {program.data(), output_arg.data(), nullptr};
    return entry(2, argv);
}

inline int runRenderingCase(int (*entry)(), const char * /*name*/)
{
    return entry();
}

} // namespace ObolTest
