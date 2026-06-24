#include "riffle/writer.hpp"

#include "riffle/writer_backends.hpp"

namespace riffle {

std::expected<std::unique_ptr<Writer>, std::string> open_writer(const Config& config,
                                                                const InferredSchema& schema) {
    if (config.output_format == OutputFormat::COLUMNAR_RAW) {
        return open_columnar_raw_writer(config, schema);
    }
    return open_parquet_writer(config, schema);
}

}
