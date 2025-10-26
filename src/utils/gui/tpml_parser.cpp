
#include "gui_processor.hpp"
#include "io.hpp"
#include "logger.hpp"

#include <cstring>
#include <string_view>



GUISystem TPMLParser::parse(std::filesystem::path filepath) {

    auto& log = mpServiceLocator->get<Logger>();

    std::string file;
    {
        auto binFile = mpServiceLocator->get<IOManager>().readAll(filepath);
        file.insert(file.begin(), binFile.begin(), binFile.end());
        std::memcpy(file.data(), binFile.data(), binFile.size());
    }

    GUISystem system{};

    std::vector<GUIBlock*> stack;

    // bool isTag = false;

    // size_t cursor = 0;

    // for (size_t pos = 0; pos < file.size(); pos++) {

    //     if (!isTag && file[pos] == '<') {
    //         isTag = true;
    //         cursor = file.find_first_not_of(std::string_view(" /\n"), pos + 1);
    //         pos = file.find_first_of(std::string_view(" />\n"), cursor);
    //         std::string_view tagName(file.data() + cursor, pos - cursor);
    //         log << "t: " << tagName << "\n";
    //     }

    //     else if (isTag) {
    //         cursor = file.find_first_not_of(std::string_view(" \n"), pos);

    //         if (file[cursor] == '/') {
    //             isTag = false;
    //             pos = cursor + 2;
    //             continue;
    //         }
    //         if (file[cursor] == '>') {
    //             isTag = false;
    //             pos = cursor + 1;
    //             continue;
    //         }
    //         if (file[cursor] == '<') {
    //             isTag = false;
    //             pos--;
    //             continue;
    //         }

    //         pos = file.find_first_of(std::string_view(" =\n"), cursor);
    //         std::string_view argName(file.data() + cursor, pos - cursor);
    //         log << "n: " << argName << "\n";

    //         pos = file.find_first_of('=', pos);
    //         cursor = file.find_first_not_of(std::string_view(" \n"), pos + 1);
    //         pos = cursor + 1;

    //         if (file[cursor] == '"') {
    //             for ( ; pos < file.size(); pos++) {
    //                 if (file[pos] == '"') {
    //                     size_t backslashes = 0;
    //                     for (size_t j = pos; j-- > 0 && file[j] == '\\'; ) backslashes++;
    //                     if (backslashes % 2 == 0) break;
    //                 }
    //             }

    //         } else if (file[cursor] == '\'') {
    //             for ( ; pos < file.size(); pos++) {
    //                 if (file[pos] == '\'') {
    //                     size_t backslashes = 0;
    //                     for (size_t j = pos; j-- > 0 && file[j] == '\\'; ) backslashes++;
    //                     if (backslashes % 2 == 0) break;
    //                 }
    //             }

    //         } else {
    //             pos = file.find_first_of(std::string_view(" />\n"), pos) - 1;
    //         }

    //         std::string_view argValue(file.data() + cursor, pos - cursor + 1);
    //         log << "a: " << argValue << "\n";
    //     }

    // }

    

    return {};
}

