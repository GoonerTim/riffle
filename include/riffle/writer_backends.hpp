#pragma once

#include "riffle/writer.hpp"

namespace riffle {

// Concrete Writer backends, selected by open_writer based on OutputFormat.
std::expected<std::unique_ptr<Writer>, std::string> open_parquet_writer(
    const Config& config, const InferredSchema& schema);
std::expected<std::unique_ptr<Writer>, std::string> open_columnar_raw_writer(
    const Config& config, const InferredSchema& schema);

}  // namespace riffle
