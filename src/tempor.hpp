#pragma once

#include "core.hpp"
#include "window_manager.hpp"
#include "io.hpp"
#include "logger.hpp"

#include <memory>
#include <chrono>
#include <deque>
#include <thread>




class WaitClock {

    public:
        double fps;
        double epsilon;
        size_t fpsSamplesCount;

        WaitClock(double fps = 60, double epsilon = -0.000054, size_t fpsSamplesCount = 10)
            : fps(fps), epsilon(epsilon), fpsSamplesCount(fpsSamplesCount) {}

    private:
        double prevTime;
        double time;
        
        std::deque<double> fpsHistory;
        double fpsSampleSum = 0.0;

        double deltaTime = 0.0;

        bool firstTick = true;

    
    public:

        double getDeltaTime() {
            return deltaTime;
        };

        double estimateFps() {
            if (fpsHistory.empty()) return fps;
            return fpsSampleSum / (double)fpsHistory.size();
        }

        void tick() {

            prevTime = time;

            time = std::chrono::duration_cast<std::chrono::duration<double>>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count();

            double inversedFps = (fps > 0) ? 1.0 / fps : 0.0;
            double waitTime = inversedFps - time + prevTime + epsilon;

            auto start = std::chrono::high_resolution_clock::now();

            while (waitTime > 0.0) {
                std::this_thread::sleep_for(std::chrono::duration<double>(waitTime));
                auto now = std::chrono::high_resolution_clock::now();
                double elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - start).count();
                waitTime -= elapsed;
                start = now;
            }

            time = std::chrono::duration_cast<std::chrono::duration<double>>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count();

            if (firstTick) {
                deltaTime = inversedFps;
                firstTick = false;
            } else {
                deltaTime = time - prevTime;
            }

            double inversedDeltaTime = (deltaTime != 0.0) ? 1.0 / deltaTime : 0.0;

            fpsSampleSum += inversedDeltaTime;
            fpsHistory.push_back(inversedDeltaTime);

            while (fpsHistory.size() > fpsSamplesCount) {
                fpsSampleSum -= fpsHistory.front();
                fpsHistory.erase(fpsHistory.begin());
            }
        }
};



class GlobalServiceLocator {
    public:
        template<typename T>
        static void provide(T* service) {
            getSlot<T>() = service;
        }

        template<typename T>
        static T& get() {
            auto* s = getSlot<T>();
            if (!s) throw Exception(ErrCode::InternalError, "Internal service is not provided");
            return *s;
        }

    private:
        template<typename T>
        static T*& getSlot() {
            static T* instance = nullptr;
            return instance;
        }
};



class Backend {

    public:
        virtual void init(const WindowManager* windowManager, const GlobalServiceLocator* serviceLocator) = 0;
        virtual void destroy() noexcept = 0;
        virtual void render() = 0;
        virtual void update() = 0;
        virtual ~Backend() = default;

};




class TemporEngine {

    public:
        int run(int argc, char* argv[]) noexcept;

    private:
        std::unique_ptr<WindowManager> mWindowManager;
        std::unique_ptr<Backend> mBackend;
        WaitClock mClock{6000000};
        IOManager mIO;
        GlobalServiceLocator mServiceLocator;
        Logger mLogger;

        inline void initialize();
        inline void mainloop();
        inline void shutdown() noexcept;

};



