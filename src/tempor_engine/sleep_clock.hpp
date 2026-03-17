

#ifndef TEMPOR_ENGINE_SLEEP_CLOCK_HPP_
#define TEMPOR_ENGINE_SLEEP_CLOCK_HPP_


#include <chrono>
#include <stdexcept>
#include <vector>
#include <thread>
#include <algorithm>


namespace {
    using namespace std::chrono;
}



class sleep_clock {

    public:

        sleep_clock(
            double fps = 60.0, size_t sample_count = 20, double error_gain = 0.08,
            double corr_min = 0.0002, double corr_max = 1.0, double epsilon = 0.0000026
        ) {
                if (sample_count == 0) throw std::runtime_error("sleep_clock: sample_count must be greater or equal to 1");
                set_fps(fps);
                set_sample_count(sample_count);
                set_epsilon(epsilon);
                set_error_gain(error_gain);
                set_correction_min(corr_min);
                set_correction_max(corr_max);
                m_prev_time = std::chrono::steady_clock::now();
            }



        void set_fps(double fps) {
            m_fps = fps;
            m_desired_delta_time = duration<double>((m_fps != 0) ? 1.0 / m_fps : 0.0);
            m_time_samples.clear();
        }

        void set_error_gain(double error_gain) {
            m_error_gain = error_gain;
        }

        void set_sample_count(size_t sample_count) {
            m_sample_count = sample_count;
            m_recorded_sample_count = 0;
            m_sample_walker = 0;
            m_sample_sum = duration<double>(0.0);
            m_time_samples.assign(m_sample_count, duration<double>(0.0));
        }

        void set_epsilon(double epsilon) {
            m_epsilon = duration<double>(epsilon);
        }

        void set_correction_min(double correctionMin) {
            m_correction_min = duration<double>(correctionMin);
        }

        void set_correction_max(double correctionMax) {
            m_correction_max = duration<double>(correctionMax);
        }

        void begin() {
            m_prev_time = std::chrono::steady_clock::now();
            m_ticked_time = std::chrono::steady_clock::now();
        }

        void tick() {

            m_curr_time = steady_clock::now();
            m_delta_time = m_curr_time - m_prev_time;
            duration<double> sleep_time = m_desired_delta_time - m_delta_time - m_epsilon;
            
            std::this_thread::sleep_for(sleep_time - m_sleep_correction);

            // correcting sleep_for
            auto wake = m_curr_time + sleep_time;
            while (steady_clock::now() < wake) {
                std::this_thread::yield();
            }

            duration<double> total_time = steady_clock::now() - m_prev_time;

            m_prev_time = steady_clock::now();

            m_sample_sum += total_time;
            m_sample_sum -= m_time_samples[m_sample_walker];
            m_time_samples[m_sample_walker] = total_time;

            m_sample_walker++;
            if (m_sample_walker == m_sample_count) {
                m_sample_walker = 0;
            }
            if (m_recorded_sample_count < m_sample_count) {
                m_recorded_sample_count++;
            }

            duration<double> error = duration<double>(estimate_delta_time()) - m_desired_delta_time;
            m_sleep_correction += error * m_error_gain;
            m_sleep_correction = std::clamp(m_sleep_correction, m_correction_min, m_correction_max);
        }

        double estimateFps() {
            if (m_recorded_sample_count == 0) return m_fps;
            return 1.0 / (m_sample_sum / m_recorded_sample_count).count();
        }

        double estimate_delta_time() {
            if (m_recorded_sample_count == 0) return m_desired_delta_time.count();
            return (m_sample_sum / m_recorded_sample_count).count();
        }

        size_t ticked() {
            using namespace std::chrono;

            auto curr_time = steady_clock::now();
            size_t ticks = static_cast<size_t>((curr_time - m_ticked_time) / m_desired_delta_time);
            if (ticks > 0) m_ticked_time = steady_clock::now();

            return ticks;
        }


    private:
        double m_fps;
        duration<double> m_desired_delta_time;

        std::chrono::time_point<std::chrono::steady_clock> m_prev_time;
        std::chrono::time_point<std::chrono::steady_clock> m_curr_time;
        duration<double> m_delta_time;
        std::chrono::time_point<std::chrono::steady_clock> m_ticked_time;

        size_t m_sample_count;
        size_t m_sample_walker;
        size_t m_recorded_sample_count;
        std::vector<duration<double>> m_time_samples;
        duration<double> m_sample_sum{0.0};

        double m_error_gain;
        duration<double> m_sleep_correction{0.0};
        duration<double> m_correction_min;
        duration<double> m_correction_max;
        duration<double> m_epsilon;

};



#endif  // TEMPOR_ENGINE_SLEEP_CLOCK_HPP_

