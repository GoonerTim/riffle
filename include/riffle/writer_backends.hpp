#pragma once

#include <memory>

#include "riffle/writer.hpp"

namespace arrow {
class Schema;
}

namespace riffle {

std::expected<std::unique_ptr<Writer>, std::string> open_parquet_writer(
    const Config& config, const InferredSchema& schema);
std::expected<std::unique_ptr<Writer>, std::string> open_parquet_writer_arrow(
    const Config& config, const std::shared_ptr<arrow::Schema>& schema);
std::expected<std::unique_ptr<Writer>, std::string> open_columnar_raw_writer(
    const Config& config, const InferredSchema& schema);

}
