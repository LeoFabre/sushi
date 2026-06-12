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
 * @brief Aux send plugin that can send audio to multiple return plugins
 *        with an individual gain per destination, using a single processor
 *        instance instead of a chain of single-destination sends.
 * @Copyright 2017-2023 Elk Audio AB, Stockholm
 */

#include <cassert>
#include <string>

#include "elklog/static_logger.h"

#include "multi_send_plugin.h"

#include "sushi/constants.h"

#include "return_plugin.h"

ELKLOG_GET_LOGGER_WITH_MODULE_NAME("multi_send_plugin");

namespace sushi::internal::multi_send_plugin {

constexpr auto PLUGIN_UID = "sushi.testing.multi_send";
constexpr auto DEFAULT_LABEL = "Multi Send";
constexpr auto DEFAULT_DEST = "";

MultiSendPlugin::MultiSendPlugin(HostControl host_control, SendReturnFactory* manager) : InternalPlugin(host_control),
                                                                                         _manager(manager)
{
    Processor::set_name(PLUGIN_UID);
    Processor::set_label(DEFAULT_LABEL);

    for (int i = 0; i < MAX_SEND_DESTINATIONS; ++i)
    {
        auto index = std::to_string(i + 1);
        _slots[i].gain_parameter = register_float_parameter("gain_" + index, "Gain " + index, "dB",
                                                            -120.0f, -120.0f, 24.0f,
                                                            Direction::AUTOMATABLE,
                                                            new dBToLinPreProcessor(-120.0f, 24.0f));
        assert(_slots[i].gain_parameter);
        _slots[i].gain_smoother.set_direct(_slots[i].gain_parameter->processed_value());
    }

    for (int i = 0; i < MAX_SEND_DESTINATIONS; ++i)
    {
        auto index = std::to_string(i + 1);
        auto name = "destination_" + index;
        [[maybe_unused]] bool str_pr_ok = register_property(name, "Destination " + index, DEFAULT_DEST);
        assert(str_pr_ok);
        auto descriptor = parameter_from_name(name);
        assert(descriptor);
        _slots[i].destination_property_id = descriptor->id();
    }
}

MultiSendPlugin::~MultiSendPlugin()
{
    for (auto& slot : _slots)
    {
        if (slot.destination)
        {
            /* remove_sender() removes all entries for this plugin, and is a no-op
             * if called again for a destination shared by several slots */
            slot.destination->remove_sender(this);
            slot.destination = nullptr;
        }
    }
}

void MultiSendPlugin::return_deleted(return_plugin::ReturnPlugin* destination)
{
    for (auto& slot : _slots)
    {
        if (slot.destination == destination)
        {
            slot.destination = nullptr;
            InternalPlugin::set_property_value(slot.destination_property_id, DEFAULT_DEST);
        }
    }
}

ProcessorReturnCode MultiSendPlugin::init(float sample_rate)
{
    configure(sample_rate);
    return ProcessorReturnCode::OK;
}

void MultiSendPlugin::configure(float sample_rate)
{
    _sample_rate = sample_rate;
    for (auto& slot : _slots)
    {
        slot.gain_smoother.set_lag_time(GAIN_SMOOTHING_TIME, sample_rate / AUDIO_CHUNK_SIZE);
    }
}

void MultiSendPlugin::process_event(const RtEvent& event)
{
    switch (event.type())
    {
        case RtEventType::SET_BYPASS:
        {
            bool bypassed = static_cast<bool>(event.processor_command_event()->value());
            Processor::set_bypassed(bypassed);
            _bypass_manager.set_bypass(bypassed, _sample_rate);
            break;
        }

        default:
            InternalPlugin::process_event(event);
            break;
    }
}

void MultiSendPlugin::process_audio(const ChunkSampleBuffer& in_buffer, ChunkSampleBuffer& out_buffer)
{
    bypass_process(in_buffer, out_buffer);

    if (_bypass_manager.should_process() == false)
    {
        return;
    }

    bool bypass_ramp = _bypass_manager.should_ramp();
    float bypass_start = 1.0f;
    float bypass_end = 1.0f;
    if (bypass_ramp)
    {
        std::tie(bypass_start, bypass_end) = _bypass_manager.get_ramp();
    }

    for (auto& slot : _slots)
    {
        auto destination = slot.destination;
        if (destination == nullptr)
        {
            continue;
        }

        float gain = slot.gain_parameter->processed_value();
        slot.gain_smoother.set(gain);

        // Ramp if bypass was recently toggled
        if (bypass_ramp)
        {
            float start = bypass_start * slot.gain_smoother.value();
            float end = bypass_end * slot.gain_smoother.next_value();
            destination->send_audio_with_ramp(in_buffer, 0, start, end, _current_processing_thread);
        }

        // Don't ramp, nominal case
        else if (slot.gain_smoother.stationary())
        {
            destination->send_audio(in_buffer, 0, gain, _current_processing_thread);
        }

        // Ramp because the slot gain was recently changed
        else
        {
            float start = slot.gain_smoother.value();
            float end = slot.gain_smoother.next_value();
            destination->send_audio_with_ramp(in_buffer, 0, start, end, _current_processing_thread);
        }
    }
}

bool MultiSendPlugin::bypassed() const
{
    return _bypass_manager.bypassed();
}

void MultiSendPlugin::set_bypassed(bool bypassed)
{
    _host_control.post_event(std::make_unique<SetProcessorBypassEvent>(this->id(), bypassed, IMMEDIATE_PROCESS));
}

ProcessorReturnCode MultiSendPlugin::set_property_value(ObjectId property_id, const std::string& value)
{
    for (auto& slot : _slots)
    {
        if (slot.destination_property_id == property_id)
        {
            _change_slot_destination(slot, value);
            break;
        }
    }
    return InternalPlugin::set_property_value(property_id, value);
}

std::string_view MultiSendPlugin::static_uid()
{
    return PLUGIN_UID;
}

void MultiSendPlugin::_set_slot_destination(SendSlot& slot, return_plugin::ReturnPlugin* destination)
{
    auto previous = slot.destination;
    if (previous == destination)
    {
        return;
    }

    slot.destination = destination;

    /* Only register/unregister this plugin as a sender on the first/last slot
     * using a given destination, as ReturnPlugin tracks senders, not slots */
    if (previous && _destination_use_count(previous) == 0)
    {
        previous->remove_sender(this);
    }

    if (destination && _destination_use_count(destination) == 1)
    {
        destination->add_sender(this);
    }
}

void MultiSendPlugin::_change_slot_destination(SendSlot& slot, const std::string& dest_name)
{
    if (dest_name.empty())
    {
        _set_slot_destination(slot, nullptr);
        return;
    }

    return_plugin::ReturnPlugin* return_plugin = _manager->lookup_return_plugin(dest_name);
    if (return_plugin)
    {
        _set_slot_destination(slot, return_plugin);
    }
    else
    {
        ELKLOG_LOG_WARNING("Return plugin {} not found", dest_name);
    }
}

int MultiSendPlugin::_destination_use_count(const return_plugin::ReturnPlugin* destination) const
{
    int count = 0;
    for (const auto& slot : _slots)
    {
        if (slot.destination == destination)
        {
            count++;
        }
    }
    return count;
}

} // end namespace sushi::internal::multi_send_plugin
