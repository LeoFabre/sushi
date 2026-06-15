/*
 * Copyright 2017-2023 Elk Audio AB
 *
 * SUSHI is free software: you can redistribute it and/or modify it under the terms of
 * the GNU Affero General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * SUSHI is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License along with
 * SUSHI. If not, see http://www.gnu.org/licenses/
 */

/**
 * @brief Wrapper around the list of tracks used for rt processing and its associated
 *        multicore management
 * @Copyright 2017-2023 Elk Audio AB, Stockholm
 */

#include "elklog/static_logger.h"

#include "audio_graph.h"

namespace sushi::internal::engine {

ELKLOG_GET_LOGGER_WITH_MODULE_NAME("audio graph");

constexpr bool DISABLE_DENORMALS = true;

/**
 * Real-time worker thread callback method.
 */
void external_render_callback(void* data)
{
    auto node = reinterpret_cast<sushi::internal::engine::AudioGraph::GraphNode*>(data);
    auto start = node->timer->start_timer();

    for (auto track : node->tracks)
    {
        track->render();
    }
    node->timer->stop_timer_rt_safe(start, (-2 - node->thread_id)); // Thread ids are counted backwards from -2
}

AudioGraph::AudioGraph(int threads,
                       int max_no_tracks,
                       performance::PerformanceTimer* timer,
                       [[maybe_unused]] float sample_rate,
                       [[maybe_unused]] std::optional<std::string> device_name,
                       bool debug_mode_switches) : _audio_graph(threads),
                                                   _event_outputs(threads),
                                                   _cores(threads),
                                                   _current_core(0)
{
    assert(threads > 0);

    if (_cores > 1)
    {
        twine::apple::AppleMultiThreadData apple_data{};
#ifdef SUSHI_APPLE_THREADING
        apple_data.chunk_size = AUDIO_CHUNK_SIZE;
        apple_data.current_sample_rate = sample_rate;
        if (device_name.has_value())
        {
            apple_data.device_name = device_name.value();
        }
#endif
        int thread_id = 0;
        _worker_pool = twine::WorkerPool::create_worker_pool(_cores,
                                                             apple_data,
                                                             DISABLE_DENORMALS,
                                                             debug_mode_switches);

        for (auto& node : _audio_graph)
        {

            auto status = _worker_pool->add_worker(external_render_callback,
                                                   &node);

            if (status.first != twine::WorkerPoolStatus::OK)
            {
#ifdef SUSHI_APPLE_THREADING
                ELKLOG_LOG_ERROR("Failed to start twine worker: {}", twine::apple::status_to_string(status.second));
#endif
            }

            node.tracks.reserve(max_no_tracks);
            node.timer = timer;
            node.thread_id = thread_id++;
        }
        std::string cpu_ids;
        for (const auto& info : _worker_pool->core_info())
        {
            _core_ids.push_back(info.id);
            ELKLOG_LOG_WARNING_IF(info.workers > 1, "Multiple workers assigned to core {}", info.id);
            cpu_ids.append(std::to_string(info.id)).append(", ");
        }
        ELKLOG_LOG_INFO("Worker pool created with {} cores on cpus: {}", _core_ids.size(), cpu_ids);
    }
    else
    {
        _audio_graph[0].tracks.reserve(max_no_tracks);
        _core_ids.push_back(0);
    }
}

bool AudioGraph::add(Track* track)
{
    auto& slot = _audio_graph[_current_core].tracks;
    if (slot.size() < slot.capacity())
    {
        track->set_event_output(&_event_outputs[_current_core]);
        track->set_active_rt_processing(true, _current_core);
        slot.push_back(track);
        _current_core = (_current_core + 1) % _cores;
        return true;
    }
    return false;
}

bool AudioGraph::add_to_thread(Track* track, int thread)
{
    assert(thread < _cores);
    // TODO - Clamp thread or return false to avoid segfault?
    auto& slot = _audio_graph[thread].tracks;
    if (slot.size() < slot.capacity())
    {
        track->set_event_output(&_event_outputs[thread]);
        track->set_active_rt_processing(true, thread);
        slot.push_back(track);
        return true;
    }
    return false;
}

bool AudioGraph::remove(Track* track)
{
    for (auto& node : _audio_graph)
    {
        for (auto i = node.tracks.begin(); i != node.tracks.end(); ++i)
        {
            if (*i == track)
            {
                node.tracks.erase(i);
                track->set_active_rt_processing(false, 0);
                return true;
            }
        }
    }
    return false;
}

void AudioGraph::render()
{
    if (_cores == 1)
    {
        for (auto& track : _audio_graph[0].tracks)
        {
            track->render();
        }
    }
    else
    {
        _worker_pool->wakeup_and_wait();
    }
}

} // end namespace sushi::internal::engine
