#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct LoaderCamera {
    static std::string generate(const LoaderContext& ctx);
    static void setupInitialOrientation(const LoaderContext& ctx, LoaderResult& result);

    static std::vector<std::string> getAvailableTypes();
};
} // namespace IG