#include "EDBCLIOptions.h"

#include "EDBUtils.h"

#include <cstdio>
#include <string>
#include <utility>

void addFileOption(CLI::App& app, std::vector<flashImg>& images,
                   std::deque<std::string>& imageNames) {
    app.add_option_function<std::vector<std::string>>(
           "-f,--file",
           [&images, &imageNames](const std::vector<std::string>& values) {
               if (values.size() < 2 || values.size() > 3) {
                   throw CLI::ValidationError("--file requires <path> <page> [b]");
               }
               flashImg item;
               if (!parsePage(values[1].c_str(), &item.toPage)) {
                   throw CLI::ValidationError("Invalid flash page: " + values[1]);
               }
               item.f.reset(fopen(values[0].c_str(), "rb"));
               if (!item.f) {
                   throw CLI::ValidationError("Unable to open firmware file: " + values[0]);
               }
               imageNames.push_back(values[0]);
               item.filename = const_cast<char*>(imageNames.back().c_str());
               if (values.size() == 3) {
                   if (values[2] != "b") {
                       throw CLI::ValidationError("The optional --file argument must be 'b'");
                   }
                   item.bootImg = true;
               }
               images.push_back(std::move(item));
           },
           "Flash a binary image: <path> <page> [b].")
        ->expected(2, 3)
        ->multi_option_policy(CLI::MultiOptionPolicy::TakeAll)
        ->trigger_on_parse();
}
