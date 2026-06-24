#pragma once

#include <expected>
#include <memory>
#include <string>

#include "riffle/batch.hpp"
#include "riffle/types.hpp"

namespace riffle {

class Writer {
public:
    virtual ~Writer() = default;
    virtual std::expected<void, std::string> write(const RecordBatch& batch) = 0;
    virtual std::expected<void, std::string> finish() = 0;
};

std::expected<std::unique_ptr<Writer>, std::string> open_writer(const Config& config,
                                                                const InferredSchema& schema);

}
