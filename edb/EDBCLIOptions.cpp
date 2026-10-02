#include "EDBCLIOptions.h"

#include "EDBUtils.h"

#include <cstdio>
#include <string>
#include <utility>

void addFileOption(CLI::App& app, std::vector<flashImg>& images) {
    app.add_option_function<std::vector<std::string>>(
           "-f,--file",
           [&images](const std::vector<std::string>& values) {
               if (values.size() < 2 || values.size() > 3) {
                   throw CLI::ValidationError("--file requires <path> <page> [b]");
               }
               flashImg item;
               const std::optional<uint32_t> page = parsePage(values[1]);
               if (!page) {
                   throw CLI::ValidationError("Invalid flash page: " + values[1]);
               }
               item.toPage = *page;
               if (values.size() == 3 && values[2] != "b") {
                   throw CLI::ValidationError("The optional --file argument must be 'b'");
               }
               item.f.reset(fopen(values[0].c_str(), "rb"));
               if (!item.f) {
                   throw CLI::ValidationError("Unable to open firmware file: " + values[0]);
               }
               item.filename = values[0];
               item.bootImg = values.size() == 3;
               images.push_back(std::move(item));
           },
           "Flash a binary image: <path> <page> [b].")
        ->expected(2, 3)
        ->multi_option_policy(CLI::MultiOptionPolicy::TakeAll)
        ->trigger_on_parse();
}
