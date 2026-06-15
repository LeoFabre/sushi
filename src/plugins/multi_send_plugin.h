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

#ifndef SUSHI_MULTI_SEND_PLUGIN_H
#define SUSHI_MULTI_SEND_PLUGIN_H

#include <array>

#include "sushi/constants.h"

#include "send_return_factory.h"
#include "sender_interface.h"
#include "library/internal_plugin.h"

ELK_PUSH_WARNING
ELK_DISABLE_DOMINANCE_INHERITANCE

namespace sushi::internal {

class SendReturnFactory;

namespace return_plugin { class ReturnPlugin; }

namespace multi_send_plugin {

constexpr int MAX_SEND_DESTINATIONS = 8;

class Accessor;

class MultiSendPlugin : public InternalPlugin, public UidHelper<MultiSendPlugin>, public SenderInterface
{
public:
    MultiSendPlugin(HostControl host_control, SendReturnFactory* manager);

    ~MultiSendPlugin() override;

    // From SenderInterface
    void return_deleted(return_plugin::ReturnPlugin* destination) override;

    // From Processor
    ProcessorReturnCode init(float sample_rate) override;

    void configure(float sample_rate) override;

    void process_event(const RtEvent& event) override;

    void process_audio(const ChunkSampleBuffer& in_buffer, ChunkSampleBuffer& out_buffer) override;

    bool bypassed() const override;

    void set_bypassed(bool bypassed) override;

    ProcessorReturnCode set_property_value(ObjectId property_id, const std::string& value) override;

    static std::string_view static_uid();

private:
    friend Accessor;

    struct SendSlot
    {
        return_plugin::ReturnPlugin* destination{nullptr};
        FloatParameterValue*         gain_parameter{nullptr};
        ValueSmootherFilter<float>   gain_smoother;
        ObjectId                     destination_property_id{0};
    };

    void _set_slot_destination(SendSlot& slot, return_plugin::ReturnPlugin* destination);

    void _change_slot_destination(SendSlot& slot, const std::string& dest_name);

    int  _destination_use_count(const return_plugin::ReturnPlugin* destination) const;

    float                                     _sample_rate{0.0f};
    std::array<SendSlot, MAX_SEND_DESTINATIONS> _slots;

    SendReturnFactory* _manager;
    BypassManager      _bypass_manager;
};

} // end namespace multi_send_plugin
} // end namespace sushi::internal

ELK_POP_WARNING

#endif // SUSHI_MULTI_SEND_PLUGIN_H
