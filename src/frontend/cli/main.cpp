#include "Camera.h"
#include "IO.h"

#ifdef WITH_UI
#include "UI.h"
#else
#include "StatusObserver.h"
#endif

#include "Logger.h"
#include "ProgramOptions.h"
#include "Runtime.h"
#include "config/Build.h"

#include "Timer.h"

#include <optional>

using namespace IG;

struct SectionTimer {
    Timer timer;
    size_t duration_ms = 0;

    inline void start() { timer.start(); }
    inline void stop() { duration_ms += timer.stopMS(); }
};

int main(int argc, char** argv)
{
    ProgramOptions cmd(argc, argv, ApplicationType::CLI, "Command Line Interface");
    if (cmd.ShouldExit)
        return EXIT_SUCCESS;

    RuntimeOptions opts;
    cmd.populate(opts);

    if (!cmd.Quiet)
        std::cout << Build::getCopyrightString() << std::endl;

    if (cmd.InputScene.empty()) {
        IG_LOG(L_ERROR) << "No input file given" << std::endl;
        return EXIT_FAILURE;
    }

    if (cmd.SPP.value_or(0) <= 0) {
        IG_LOG(L_ERROR) << "No valid spp count given" << std::endl;
        return EXIT_FAILURE;
    }

    if (cmd.Output.empty()) {
        IG_LOG(L_ERROR) << "No output file given" << std::endl;
        return EXIT_FAILURE;
    }

    SectionTimer timer_all;
    SectionTimer timer_loading;
    timer_all.start();
    timer_loading.start();

    std::unique_ptr<Runtime> runtime;
    try {
        runtime = std::make_unique<Runtime>(cmd.InputScene, opts);
    } catch (const std::exception& e) {
        IG_LOG(L_ERROR) << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    timer_loading.stop();

    const auto def = runtime->initialCameraOrientation();
    runtime->setup();
    runtime->setParameter("__camera_eye", cmd.Eye.value_or(def.Eye));
    runtime->setParameter("__camera_dir", cmd.Dir.value_or(def.Dir));
    runtime->setParameter("__camera_up", cmd.Up.value_or(def.Up));

    const size_t SPI          = runtime->samplesPerIteration();
    const size_t desired_iter = static_cast<size_t>(std::ceil(cmd.SPP.value_or(0) / (float)SPI));

    if (cmd.SPP.has_value() && (cmd.SPP.value() % SPI) != 0)
        IG_LOG(L_WARNING) << "Given spp " << cmd.SPP.value() << " is not a multiple of the spi " << SPI << ". Using spp " << desired_iter * SPI << " instead" << std::endl;

    StatusObserver observer(!cmd.NoColor, 2, desired_iter * SPI /* Approx */);
    observer.begin();

    IG_LOG(L_INFO) << "Started rendering..." << std::endl;

    std::vector<double> samples_sec;

    SectionTimer timer_render;
    while (true) {
        if (!cmd.NoProgress)
            observer.update(runtime->currentSampleCount());

        auto ticks = std::chrono::high_resolution_clock::now();

        timer_render.start();
        runtime->step();
        timer_render.stop();

        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - ticks).count();

        samples_sec.emplace_back(1000.0 * double(SPI * runtime->framebufferWidth() * runtime->framebufferHeight()) / double(elapsed_ms));
        if (samples_sec.size() == desired_iter)
            break;
    }

    if (!cmd.NoProgress)
        observer.end();

    SectionTimer timer_saving;
    timer_saving.start();
    if (!saveImageOutput(cmd.Output, *runtime))
        IG_LOG(L_ERROR) << "Failed to save EXR file '" << cmd.Output << "'" << std::endl;
    else
        IG_LOG(L_INFO) << "Result saved to '" << cmd.Output << "'" << std::endl;
    timer_saving.stop();

    timer_all.stop();

    auto stats = runtime->getStatistics();
    if (stats) {
        IG_LOG(L_INFO)
            << stats->dump(runtime->currentIterationCount(), cmd.AcquireFullStats)
            << "  Iterations: " << runtime->currentIterationCount() << std::endl
            << "  SPP: " << runtime->currentSampleCount() << std::endl
            << "  SPI: " << SPI << std::endl
            << "  Time: " << timer_all.duration_ms << "ms" << std::endl
            << "    Loading> " << timer_loading.duration_ms << "ms" << std::endl
            << "    Render>  " << timer_render.duration_ms << "ms" << std::endl
            << "    Saving>  " << timer_saving.duration_ms << "ms" << std::endl;
    }

    runtime.reset();

    if (!samples_sec.empty()) {
        auto inv = 1.0e-6;
        std::sort(samples_sec.begin(), samples_sec.end());
        IG_LOG(L_INFO) << "# " << samples_sec.front() * inv
                       << "/" << samples_sec[samples_sec.size() / 2] * inv
                       << "/" << samples_sec.back() * inv
                       << " (min/med/max Msamples/s)" << std::endl;
    }

    return EXIT_SUCCESS;
}
