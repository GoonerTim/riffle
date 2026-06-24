#pragma once

#include <expected>
#include <memory>
#include <string>

#include "riffle/batch.hpp"
#include "riffle/types.hpp"

namespace riffle {

// Port: a sink that consumes record batches and finalizes an output file.
class Writer {
public:
    virtual ~Writer() = default;
    virtual std::expected<void, std::string> write(const RecordBatch& batch) = 0;
    virtual std::expected<void, std::string> finish() = 0;
};

// Open a writer for the configured output format and schema.
std::expected<std::unique_ptr<Writer>, std::string> open_writer(const Config& config,
                                                                const InferredSchema& schema);

}  // namespace riffle
