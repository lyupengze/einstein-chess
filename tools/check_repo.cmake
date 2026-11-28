# 仓库一致性检查：文件头、脚本头、注释语法、禁词、缺陷锚点、相对链接与测试数量。
#
# 用法（在仓库根或任意目录执行均可，不需要先配置构建目录）：
#
#     cmake -P tools/check_repo.cmake
#
# 只读，不产生任何构建产物；发现问题时列出全部条目，并以非 0 退出。

cmake_minimum_required(VERSION 3.20)

# 脚本可能被 target 或 CI 从任意目录调用，仓库根只按脚本位置推算。
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# 问题先累积在全局属性里，跑完全部检查再统一打印，避免漏报。
function(add_problem text)
    set_property(GLOBAL APPEND PROPERTY check_repo_problems "${text}")
endfunction()

function(problem_count out_var)
    get_property(items GLOBAL PROPERTY check_repo_problems)
    if(DEFINED items)
        list(LENGTH items count)
    else()
        set(count 0)
    endif()
    set(${out_var} "${count}" PARENT_SCOPE)
endfunction()

# 按行读取，供后续逐行定位；分号先转义，避免 C++ 语句被当成列表分隔符。
function(read_lines path out_var)
    file(READ "${path}" content)
    string(REPLACE "\r\n" "\n" content "${content}")
    string(REPLACE "\r" "\n" content "${content}")
    string(REPLACE ";" "\\;" content "${content}")
    string(REPLACE "\n" ";" lines "${content}")
    set(${out_var} "${lines}" PARENT_SCOPE)
endfunction()

# 在一组行里找某个子串，返回命中的行号列表（从 1 开始）。
function(find_lines lines_var needle out_var)
    set(hits "")
    set(number 0)
    foreach(line IN LISTS ${lines_var})
        math(EXPR number "${number} + 1")
        string(FIND "${line}" "${needle}" position)
        if(NOT position EQUAL -1)
            list(APPEND hits "${number}")
        endif()
    endforeach()
    set(${out_var} "${hits}" PARENT_SCOPE)
endfunction()

# 取某个标记之后的第一个标识符，例如 `einstein_add_test(cli_input ...)` 里 `(` 之后的
# `cli_input`，或 `add_test(NAME cli_eof ...)` 里 `NAME` 之后的 `cli_eof`。
# 遇到 `${name}` 这类展开会返回空串，调用方按“没取到名字”处理。
function(identifier_after text marker out_var)
    string(FIND "${text}" "${marker}" position)
    if(position EQUAL -1)
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    string(LENGTH "${marker}" marker_length)
    math(EXPR begin "${position} + ${marker_length}")
    string(SUBSTRING "${text}" ${begin} -1 rest)
    string(STRIP "${rest}" rest)
    string(REGEX MATCH "^[A-Za-z0-9_]+" token "${rest}")
    set(${out_var} "${token}" PARENT_SCOPE)
endfunction()

# 只认受版本控制的文件：build/ 等生成目录天然被排除。
execute_process(
    COMMAND git -C "${REPO_ROOT}" ls-files -- src tests benchmarks docs README.md
    OUTPUT_VARIABLE tracked
    RESULT_VARIABLE git_result
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT git_result EQUAL 0)
    message(FATAL_ERROR "无法列出受跟踪文件：请在 git 仓库内运行本脚本。")
endif()
string(REPLACE "\n" ";" tracked "${tracked}")

function(report_section name before)
    problem_count(after)
    math(EXPR section "${after} - ${before}")
    if(section EQUAL 0)
        message("[${name}] 通过")
    else()
        message("[${name}] ${section} 处问题")
    endif()
endfunction()

# 1. 每个 .h/.cpp 的第 1 行必须是 /// @file <相对路径>，路径与实际文件一致。
problem_count(before_headers)
foreach(rel IN LISTS tracked)
    get_filename_component(extension "${rel}" EXT)
    if(NOT extension STREQUAL ".h" AND NOT extension STREQUAL ".cpp")
        continue()
    endif()
    read_lines("${REPO_ROOT}/${rel}" lines)
    list(LENGTH lines line_count)
    if(line_count EQUAL 0)
        add_problem("${rel}:1: 文件为空，缺少文件头")
        continue()
    endif()
    list(GET lines 0 first_line)
    if(NOT first_line STREQUAL "/// @file ${rel}")
        add_problem("${rel}:1: 第 1 行必须是「/// @file ${rel}」")
    endif()
endforeach()
report_section("文件头" "${before_headers}")

# 1b. 每个 CMakeLists.txt 与 .cmake 的首行必须是以 # 开头的非空注释。
# 这类文件没有 Doxygen，脚本头就是唯一的“这是什么文件”说明。
execute_process(
    COMMAND git -C "${REPO_ROOT}" ls-files
    OUTPUT_VARIABLE all_tracked
    RESULT_VARIABLE git_all_result
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT git_all_result EQUAL 0)
    message(FATAL_ERROR "无法列出受跟踪文件：请在 git 仓库内运行本脚本。")
endif()
string(REPLACE "\n" ";" all_tracked "${all_tracked}")
problem_count(before_scripts)
foreach(rel IN LISTS all_tracked)
    if(NOT rel MATCHES "(^|/)CMakeLists\\.txt$" AND NOT rel MATCHES "\\.cmake$")
        continue()
    endif()
    read_lines("${REPO_ROOT}/${rel}" lines)
    list(LENGTH lines line_count)
    if(line_count EQUAL 0)
        add_problem("${rel}: 文件为空，缺少 # 脚本头")
        continue()
    endif()
    list(GET lines 0 first_line)
    if(NOT first_line MATCHES "^[ \t]*#[ \t]*[^ \t]")
        add_problem("${rel}:1: 首行必须是以 # 开头的注释")
    endif()
endforeach()
report_section("脚本头" "${before_scripts}")

# 1c. 注释语法：C++ 文件禁块注释 /**，文件头禁作者、日期、版本、版权字段。
problem_count(before_syntax)
foreach(rel IN LISTS tracked)
    get_filename_component(extension "${rel}" EXT)
    if(NOT extension STREQUAL ".h" AND NOT extension STREQUAL ".cpp")
        continue()
    endif()
    read_lines("${REPO_ROOT}/${rel}" lines)
    find_lines(lines "/**" block_comments)
    foreach(number IN LISTS block_comments)
        add_problem("${rel}:${number}: 禁用块注释 /**，改用 /// 或 //")
    endforeach()
    foreach(field IN ITEMS "@author" "@date" "@version" "@copyright")
        find_lines(lines "${field}" field_lines)
        foreach(number IN LISTS field_lines)
            add_problem("${rel}:${number}: 文件头不写「${field}」，作者与时间由 git 记录")
        endforeach()
    endforeach()
endforeach()
report_section("注释语法" "${before_syntax}")

# 2. 禁词扫描：源码、脚本与文档；规范本身与缺陷台账按定义要写这些词，跳过。
set(BANNED_WORDS
    旧实现
    旧版本
    原实现
    曾经的
    历史遗留
    迁移
    废弃
    改为
    改成
    不再
    换成
    替换为
    升级为
    已经不支持
    目前
    暂时
    暂时保留
    以后可以
    先这样
    BUG-
    P0-
    TODO
    FIXME
)
problem_count(before_words)
foreach(rel IN LISTS tracked)
    if(rel STREQUAL "docs/COMMENTING.md" OR rel STREQUAL "docs/HISTORY.md")
        continue()
    endif()
    get_filename_component(extension "${rel}" EXT)
    if(extension MATCHES "^\\.(h|cpp|cmake|txt)$"
       OR rel MATCHES "^docs/.*\\.md$"
       OR rel STREQUAL "README.md")
        read_lines("${REPO_ROOT}/${rel}" lines)
        foreach(word IN LISTS BANNED_WORDS)
            find_lines(lines "${word}" hit_lines)
            foreach(number IN LISTS hit_lines)
                add_problem("${rel}:${number}: 出现禁词「${word}」")
            endforeach()
        endforeach()
    endif()
endforeach()
report_section("禁词" "${before_words}")

# 3. 源码注释里的 `#锚点` 必须在缺陷台账里有对应条目。
read_lines("${REPO_ROOT}/docs/HISTORY.md" history_lines)
set(history_text "")
foreach(line IN LISTS history_lines)
    string(APPEND history_text "${line}\n")
endforeach()
problem_count(before_anchors)
foreach(rel IN LISTS tracked)
    get_filename_component(extension "${rel}" EXT)
    if(NOT extension STREQUAL ".h" AND NOT extension STREQUAL ".cpp")
        continue()
    endif()
    read_lines("${REPO_ROOT}/${rel}" lines)
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        string(REGEX MATCHALL "`#[a-z0-9][a-z0-9-]*`" anchors "${line}")
        foreach(anchor IN LISTS anchors)
            string(LENGTH "${anchor}" length)
            math(EXPR anchor_length "${length} - 3")
            string(SUBSTRING "${anchor}" 2 ${anchor_length} name)
            string(FIND "${history_text}" "### `#${name}`" position)
            if(position EQUAL -1)
                add_problem("${rel}:${number}: 锚点「#${name}」在 docs/HISTORY.md 中没有对应条目")
            endif()
        endforeach()
    endforeach()
endforeach()
report_section("锚点" "${before_anchors}")

# 4. Markdown 相对链接必须存在；链接文字不能只是文件名（§4.4）；外链与同页锚点跳过。
problem_count(before_links)
foreach(rel IN LISTS tracked)
    if(NOT rel MATCHES "\\.md$")
        continue()
    endif()
    get_filename_component(directory "${REPO_ROOT}/${rel}" DIRECTORY)
    read_lines("${REPO_ROOT}/${rel}" lines)
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        string(REGEX MATCHALL "\\[[^]]*\\]\\([^)]+\\)" links "${line}")
        foreach(link IN LISTS links)
            string(FIND "${link}" "](" open)
            math(EXPR begin "${open} + 2")
            string(LENGTH "${link}" total)
            math(EXPR target_length "${total} - ${begin} - 1")
            string(SUBSTRING "${link}" ${begin} ${target_length} target)
            # 规范文件里贴着反例，跳过它的链接文字检查。
            if(NOT rel STREQUAL "docs/COMMENTING.md")
                string(FIND "${link}" "[" bracket)
                math(EXPR text_begin "${bracket} + 1")
                math(EXPR text_length "${open} - ${text_begin}")
                string(SUBSTRING "${link}" ${text_begin} ${text_length} link_text)
                if(link_text MATCHES "\\.(md|h|cpp|txt)$")
                    add_problem("${rel}:${number}: 链接文字只写了文件名「${link_text}」，应说明目标")
                endif()
            endif()
            if(target MATCHES "^(https?://|mailto:|#)")
                continue()
            endif()
            string(FIND "${target}" "#" fragment)
            if(NOT fragment EQUAL -1)
                string(SUBSTRING "${target}" 0 ${fragment} target)
            endif()
            if(target STREQUAL "")
                continue()
            endif()
            if(NOT EXISTS "${directory}/${target}")
                add_problem("${rel}:${number}: 链接目标不存在 -> ${target}")
            endif()
        endforeach()
    endforeach()
endforeach()
report_section("Markdown 链接" "${before_links}")

# 5. 测试数量：两条注册路径都要算，且与文档口径一致。
set(EXPECTED_TEST_COUNT 24)
read_lines("${REPO_ROOT}/tests/CMakeLists.txt" registry_lines)
set(registered "")
foreach(line IN LISTS registry_lines)
    if(line MATCHES "^[ \t]*einstein_add_test\\(")
        identifier_after("${line}" "(" name)
    elseif(line MATCHES "add_test\\(NAME[ \t]+")
        identifier_after("${line}" "NAME" name)
    else()
        continue()
    endif()
    if(name STREQUAL "")
        continue()
    endif()
    list(FIND registered "${name}" existing)
    if(existing EQUAL -1)
        list(APPEND registered "${name}")
    endif()
endforeach()
problem_count(before_tests)
list(LENGTH registered test_count)
if(NOT test_count EQUAL EXPECTED_TEST_COUNT)
    add_problem("tests/CMakeLists.txt: 注册了 ${test_count} 个用例，文档口径是 ${EXPECTED_TEST_COUNT} 个")
else()
    foreach(doc IN ITEMS "docs/BUILD.md" "README.md")
        file(READ "${REPO_ROOT}/${doc}" text)
        string(FIND "${text}" "${EXPECTED_TEST_COUNT}" position)
        if(position EQUAL -1)
            add_problem("${doc}: 没有出现当前用例数 ${EXPECTED_TEST_COUNT}")
        endif()
    endforeach()
endif()
report_section("测试数量" "${before_tests}")

# 汇总：先逐条打印，再以失败退出，保证列出全部问题。
problem_count(total)
if(total GREATER 0)
    get_property(items GLOBAL PROPERTY check_repo_problems)
    message("")
    foreach(item IN LISTS items)
        message("  ${item}")
    endforeach()
    message(FATAL_ERROR "共 ${total} 处问题；复现：cmake -P tools/check_repo.cmake")
endif()
message("全部检查通过")
