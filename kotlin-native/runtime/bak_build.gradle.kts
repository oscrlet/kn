        module("tests") {
            sourceSets{
                main {
                    inputFiles.from(srcRoot.dir("./"));
                    inputFiles.include("**/*.cpp")
                    //inputFiles.exclude("**/tests/")
                    headersDirs.setFrom(srcRoot.dir("../"), 
                    srcRoot.dir("./"), 
                    srcRoot.dir("../common_interfaces"), 
                    srcRoot.dir("../libpandabase"),
                    srcRoot.dir("../third_party_bounds_checking_function/include")) 
                }
            }
            compilerArgs.set(listOfNotNull(
                "-Wall",
                "-Wshadow",
               // "-Werror",
                "-Wextra",
                "-pedantic",
                "-Wno-invalid-offsetof",
                "-Wno-gnu-statement-expression",
                "-pipe",
                "-Wdate-time",
                "-funwind-tables",
                "-fno-rtti",
                "-fasynchronous-unwind-tables",
                "-Wformat=2",
                "-std=c++17",
                "-Wno-unused-command-line-argument",
                "-Wno-variadic-macros",
                "-Wno-gnu-anonymous-struct",
                "-Wno-zero-length-array",
                "-Wno-nested-anon-types",
                "-Wno-c99-extensions",
                "-Wno-unused-parameter",
                "-Wno-shadow",
                "-Wno-pedantic",
                "-Wno-gnu-zero-variadic-macro-arguments",
                "-Wno-unused-lambda-capture",
                "-Wno-unused-function",
                "-Wno-unused-variable",
                //"-Wno-unused-but-set-variable",
            ))
        }

        module("libpandabase") {
            sourceSets{
                main {
                    inputFiles.from(srcRoot.dir("./"));
                    inputFiles.include("utils/debug.cpp")
                    inputFiles.exclude("**/tests/", "os/**", "mem/**", "arch/**", "utils/json_parser.cpp",
                    "utils/logger.cpp", "utils/time.cpp", "utils/timers.cpp",
                    "utils/type_converter.cpp", "utils/utf.cpp", "utils/workerQueue.cpp")
                    headersDirs.setFrom(srcRoot.dir("../"), 
                    srcRoot.dir("./"), 
                    srcRoot.dir("../common_interfaces"), 
                    srcRoot.dir("../libpandabase"),
                    srcRoot.dir("../third_party_bounds_checking_function/include")) 
                }
            }
            compilerArgs.set(listOfNotNull(
                "-Wall",
                "-Wshadow",
                "-Werror",
                "-Wextra",
                "-pedantic",
                "-Wno-invalid-offsetof",
                "-Wno-gnu-statement-expression",
                "-pipe",
                "-Wdate-time",
                "-funwind-tables",
                "-fno-rtti",
                "-fasynchronous-unwind-tables",
                "-Wformat=2",
                "-std=c++17",
                "-Wno-unused-command-line-argument",
                "-Wno-variadic-macros",
                "-Wno-gnu-anonymous-struct",
                "-Wno-zero-length-array",
                "-Wno-nested-anon-types",
                "-Wno-c99-extensions",
                "-Wno-unused-parameter",
                "-Wno-shadow",
                "-Wno-pedantic",
                "-Wno-gnu-zero-variadic-macro-arguments",
                "-Wno-unused-lambda-capture",
                "-Wno-unused-function",
                "-Wno-unused-variable",
                //"-Wno-unused-but-set-variable",
            ))
        }

        module("third_party_bounds_checking_function") {
            sourceSets{
                main {
                    inputFiles.from(srcRoot.dir("./"));
                    inputFiles.include("src/sprintf_s.c", "src/memmove_s.c", "src/memcpy_s.c", "src/vsprintf_s.c")
                    inputFiles.exclude("**/tests/", "src/securecutil.c",
                                       "src/secureprintoutput_a.c")
                    headersDirs.setFrom(srcRoot.dir("../"),
                    srcRoot.dir("./"), 
                    srcRoot.dir("../common_interfaces"), 
                    srcRoot.dir("../libpandabase"),
                    srcRoot.dir("../third_party_bounds_checking_function/include")) 
                }
            }

            compiler.set("clang")
            compilerArgs.set(listOfNotNull(
                "-std=gnu11",
                "-funwind-tables",
                "-W",
                "-Wall",
                "-Wwrite-strings",
                "-Wstrict-prototypes",
                "-Wmissing-prototypes",
                "-Wold-style-definition",
                "-Wmissing-format-attribute",
                "-Wcast-qual",
                "-O2",
                "-Wno-atomic-alignment"
            ))
       }