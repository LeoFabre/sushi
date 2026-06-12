#include "gtest/gtest.h"

#include "test_utils/test_utils.h"
#include "test_utils/host_control_mockup.h"

#include "elk-warning-suppressor/warning_suppressor.hpp"

#include "plugins/send_return_factory.cpp"
#include "plugins/send_plugin.cpp"
#include "plugins/multi_send_plugin.cpp"
#include "plugins/return_plugin.cpp"

namespace sushi::internal::send_plugin
{

class Accessor
{
public:
    explicit Accessor(SendPlugin& plugin) : _plugin(plugin) {}

    [[nodiscard]] return_plugin::ReturnPlugin* destination()
    {
        return _plugin._destination;
    }

    void set_destination(return_plugin::ReturnPlugin* destination)
    {
        _plugin._set_destination(destination);
    }

private:
    SendPlugin& _plugin;
};

}

namespace sushi::internal::multi_send_plugin
{

class Accessor
{
public:
    explicit Accessor(MultiSendPlugin& plugin) : _plugin(plugin) {}

    [[nodiscard]] return_plugin::ReturnPlugin* destination(int slot)
    {
        return _plugin._slots[slot].destination;
    }

    [[nodiscard]] ObjectId destination_property_id(int slot)
    {
        return _plugin._slots[slot].destination_property_id;
    }

private:
    MultiSendPlugin& _plugin;
};

}

namespace sushi::internal::return_plugin
{

class Accessor
{
public:
    explicit Accessor(ReturnPlugin& plugin) : _plugin(plugin) {}

    void swap_buffers()
    {
        _plugin._swap_buffers();
    }

private:
    ReturnPlugin& _plugin;
};

}

using namespace sushi;
using namespace sushi::internal;
using namespace sushi::internal::send_plugin;
using namespace sushi::internal::return_plugin;

constexpr float TEST_SAMPLERATE = 44100;

TEST(TestSendReturnFactory, TestFactoryCreation)
{
    SendReturnFactory factory;
    HostControlMockup host_control_mockup;
    auto host_ctrl = host_control_mockup.make_host_control_mockup(TEST_SAMPLERATE);
    PluginInfo info{.uid = "sushi.testing.send", .path = "", .type = PluginType::INTERNAL};

    auto [send_status, send_plugin] = factory.new_instance(info, host_ctrl, TEST_SAMPLERATE);
    ASSERT_EQ(ProcessorReturnCode::OK, send_status);
    ASSERT_EQ("Send", send_plugin->label());
    ASSERT_GT(send_plugin.use_count(), 0);
    ASSERT_GT(send_plugin->id(), 0u);

    info.uid = "sushi.testing.return";
    auto [ret_status, return_plugin] = factory.new_instance(info, host_ctrl, TEST_SAMPLERATE);
    ASSERT_EQ(ProcessorReturnCode::OK, ret_status);
    ASSERT_EQ("Return", return_plugin->label());
    ASSERT_GT(return_plugin.use_count(), 0);
    ASSERT_GT(return_plugin->id(), 0u);

    // Negative test
    info.uid = "sushi.testing.aux_";
    auto [error_status, error_plugin] = factory.new_instance(info, host_ctrl, TEST_SAMPLERATE);
    ASSERT_NE(ProcessorReturnCode::OK, error_status);
    ASSERT_FALSE(error_plugin);
}


class TestSendReturnPlugins : public ::testing::Test
{
protected:
    TestSendReturnPlugins() = default;

    void SetUp() override
    {
        ASSERT_EQ(ProcessorReturnCode::OK, _send_instance.init(TEST_SAMPLERATE));
        _send_instance.set_channels(2, 2);
        _send_instance.set_active_rt_processing(true, 0);

        ASSERT_EQ(ProcessorReturnCode::OK, _return_instance.init(TEST_SAMPLERATE));
        _return_instance.set_channels(2, 2);
        _return_instance.set_active_rt_processing(true, 1);
    }

    SendReturnFactory   _factory;
    HostControlMockup   _host_control_mockup;
    HostControl         _host_ctrl {_host_control_mockup.make_host_control_mockup(TEST_SAMPLERATE)};

    SendPlugin          _send_instance {_host_ctrl, &_factory};
    ReturnPlugin        _return_instance {_host_ctrl, &_factory};

    sushi::internal::send_plugin::Accessor _send_accessor {_send_instance};
    sushi::internal::return_plugin::Accessor _return_accessor {_return_instance};
};

TEST_F(TestSendReturnPlugins, TestDestinationSetting)
{
    PluginInfo info;
    info.uid = "sushi.testing.return";
    auto [status, return_instance_2] = _factory.new_instance(info, _host_ctrl, TEST_SAMPLERATE);
    ASSERT_EQ(ProcessorReturnCode::OK, status);

    _return_instance.set_name("return_1");
    return_instance_2->set_name("return_2");

    EXPECT_EQ(DEFAULT_DEST, _send_instance.property_value(DEST_PROPERTY_ID).second);
    status = _send_instance.set_property_value(DEST_PROPERTY_ID, "return_2");
    EXPECT_EQ(ProcessorReturnCode::OK, status);
    EXPECT_EQ(_send_accessor.destination(), return_instance_2.get());
    EXPECT_EQ("return_2", _send_instance.property_value(DEST_PROPERTY_ID).second);

    // Destroy the second return and it should be automatically unlinked.
    return_instance_2.reset();
    EXPECT_EQ(_send_accessor.destination(), nullptr);
    EXPECT_EQ(DEFAULT_DEST, _send_instance.property_value(DEST_PROPERTY_ID).second);
}

TEST_F(TestSendReturnPlugins, TestProcessing)
{
    ChunkSampleBuffer buffer_1(2);
    ChunkSampleBuffer buffer_2(2);
    test_utils::fill_sample_buffer(buffer_1, 1.0f);

    // Test that processing without destination doesn't break and passes though
    _send_instance.process_audio(buffer_1, buffer_2);
    test_utils::assert_buffer_value(1.0f, buffer_2);

    _send_accessor.set_destination(&_return_instance);
    _send_instance.process_audio(buffer_1, buffer_2);
    buffer_2.clear();

    // Swap manually and verify that signal is returned
    _return_accessor.swap_buffers();
    _return_instance.process_audio(buffer_1, buffer_2);
    test_utils::assert_buffer_value(1.0f, buffer_2);
}

TEST_F(TestSendReturnPlugins, TestZeroDelayProcessing)
{
    ChunkSampleBuffer buffer_1(2);
    ChunkSampleBuffer buffer_2(2);
    test_utils::fill_sample_buffer(buffer_1, 1.0f);

    // Set the send plugin to use the same thread as the return plugin
    _send_instance.set_active_rt_processing(true, 1);

    // Test that processing without destination doesn't break and passes though
    _send_instance.process_audio(buffer_1, buffer_2);
    test_utils::assert_buffer_value(1.0f, buffer_2);

    _send_accessor.set_destination(&_return_instance);
    _send_instance.process_audio(buffer_1, buffer_2);
    buffer_2.clear();

    // Don't swap, the send plugin should have immediately copied to the output without delay
    _return_instance.process_audio(buffer_1, buffer_2);
    test_utils::assert_buffer_value(1.0f, buffer_2);
}

TEST_F(TestSendReturnPlugins, TestMultipleSends)
{
    ChunkSampleBuffer buffer_1(2);
    ChunkSampleBuffer buffer_2(2);
    test_utils::fill_sample_buffer(buffer_1, 1.0f);

    _host_control_mockup._transport.set_time(Time(0), 0);

    _send_accessor.set_destination(&_return_instance);
    _send_instance.process_audio(buffer_1, buffer_2);

    SendPlugin send_instance_2(_host_ctrl, &_factory);
    sushi::internal::send_plugin::Accessor _send_accessor_2 {send_instance_2};

    _send_accessor_2.set_destination(&_return_instance);
    send_instance_2.process_audio(buffer_1, buffer_2);
    buffer_2.clear();

    // Call process on the return, the buffers should not be swapped so output should be 0
    _return_instance.process_audio(buffer_1, buffer_2);
    test_utils::assert_buffer_value(0.0f, buffer_2);

    // Fast forward time and call process again, buffers should now be swapped and we should
    // read both sends on the output
    _host_control_mockup._transport.set_time(Time(10), AUDIO_CHUNK_SIZE);
    _return_instance.process_audio(buffer_1, buffer_2);
    test_utils::assert_buffer_value(2.0f, buffer_2);
}

TEST_F(TestSendReturnPlugins, TestSelectiveChannelSending)
{
    auto channel_count_param_id = _send_instance.parameter_from_name("channel_count")->id();
    auto start_channel_param_id = _send_instance.parameter_from_name("start_channel")->id();
    auto dest_channel_param_id = _send_instance.parameter_from_name("dest_channel")->id();


    ChunkSampleBuffer buffer_1(2);
    ChunkSampleBuffer buffer_2(2);
    test_utils::fill_sample_buffer(buffer_1, 1.0f);

    _send_instance.set_channels(2, 2);
    _send_accessor.set_destination(&_return_instance);

    // Send only 1 channel
    auto event = RtEvent::make_parameter_change_event(_send_instance.id(), 0, channel_count_param_id,
                                                      1.0f / (MAX_TRACK_CHANNELS - 1));
    _send_instance.process_event(event);
    _send_instance.process_audio(buffer_1, buffer_1);

    // Swap manually and verify that signal only the first channel was sent
    _return_accessor.swap_buffers();
    _return_instance.process_audio(buffer_1, buffer_2);
    EXPECT_FLOAT_EQ(1.0f, buffer_2.channel(0)[0]);
    EXPECT_FLOAT_EQ(0.0f, buffer_2.channel(1)[0]);

    // Set the destination channel to channel 1
    event = RtEvent::make_parameter_change_event(_send_instance.id(), 0, dest_channel_param_id,
                                                 1.0f / (MAX_TRACK_CHANNELS - 1));
    _send_instance.process_event(event);
    _send_instance.process_audio(buffer_1, buffer_1);

    // Swap manually and verify that signal only the first channel was sent to channel 2
    _return_accessor.swap_buffers();
    _return_instance.process_audio(buffer_1, buffer_2);
    EXPECT_FLOAT_EQ(0.0f, buffer_2.channel(0)[0]);
    EXPECT_FLOAT_EQ(1.0f, buffer_2.channel(1)[0]);

    // Set a destination channel outside the range of the return plugin's channel range
    event = RtEvent::make_parameter_change_event(_send_instance.id(), 0, dest_channel_param_id, 1.0);
    _send_instance.process_event(event);
    _send_instance.process_audio(buffer_1, buffer_1);

    // Both return channels should be 0
    _return_accessor.swap_buffers();
    _return_instance.process_audio(buffer_1, buffer_2);
    EXPECT_FLOAT_EQ(0.0f, buffer_2.channel(0)[0]);
    EXPECT_FLOAT_EQ(0.0f, buffer_2.channel(1)[0]);

    // Send both channels the send plugin to channels 3 & 4 of the return plugin
    _return_instance.set_channels(4, 4);

    buffer_1.channel(0)[0] = 2.0;
    buffer_1.channel(1)[0] = 3.0;
    event = RtEvent::make_parameter_change_event(_send_instance.id(), 0, start_channel_param_id, 0);
    _send_instance.process_event(event);
    event = RtEvent::make_parameter_change_event(_send_instance.id(), 0, dest_channel_param_id,
                                                 2.0f / (MAX_TRACK_CHANNELS - 1));
    _send_instance.process_event(event);
    event = RtEvent::make_parameter_change_event(_send_instance.id(), 0, channel_count_param_id,
                                                 2.0f / (MAX_TRACK_CHANNELS - 1));
    _send_instance.process_event(event);

    _send_instance.process_audio(buffer_1, buffer_1);

    buffer_1 = ChunkSampleBuffer(4);
    buffer_2 = ChunkSampleBuffer(4);

    _return_accessor.swap_buffers();
    _return_instance.process_audio(buffer_1, buffer_2);
    EXPECT_FLOAT_EQ(0.0f, buffer_2.channel(0)[0]);
    EXPECT_FLOAT_EQ(0.0f, buffer_2.channel(1)[0]);
    EXPECT_FLOAT_EQ(2.0f, buffer_2.channel(2)[0]);
    EXPECT_FLOAT_EQ(3.0f, buffer_2.channel(3)[0]);
}

TEST_F(TestSendReturnPlugins, TestRampedProcessing)
{
    ChunkSampleBuffer buffer_1(2);
    ChunkSampleBuffer buffer_2(2);
    test_utils::fill_sample_buffer(buffer_1, 1.0f);

    // Test only ramping
    _return_instance.send_audio_with_ramp(buffer_1, 0, 2.0f, 0.0f, THREAD_ID_UNKNOWN);
    _return_accessor.swap_buffers();
    _return_instance.process_audio(buffer_1, buffer_2);
    EXPECT_NEAR(2.0f, buffer_2.channel(0)[0], 0.01);
    EXPECT_NEAR(1.0f, buffer_2.channel(0)[AUDIO_CHUNK_SIZE / 2], 0.1);
    EXPECT_NEAR(0.0f, buffer_2.channel(0)[AUDIO_CHUNK_SIZE - 1], 0.01);
    _return_accessor.swap_buffers();

    // Test parameter smoothing
    _send_accessor.set_destination(&_return_instance);
    auto event = RtEvent::make_parameter_change_event(0, 0, 0, 0.0f);
    _send_instance.process_event(event);
    _send_instance.process_audio(buffer_1, buffer_2);
    _return_accessor.swap_buffers();
    _return_instance.process_audio(buffer_1, buffer_2);

    // Audio should now begin to ramp down
    EXPECT_FLOAT_EQ(1.0f, buffer_2.channel(0)[0]);
    EXPECT_LT(buffer_2.channel(0)[AUDIO_CHUNK_SIZE -1], 1.0f);
    EXPECT_GT(buffer_2.channel(0)[AUDIO_CHUNK_SIZE / 2], buffer_2.channel(0)[AUDIO_CHUNK_SIZE - 1]);
 }
class TestMultiSendPlugin : public ::testing::Test
{
protected:
    TestMultiSendPlugin() = default;

    void SetUp() override
    {
        ASSERT_EQ(ProcessorReturnCode::OK, _multi_send.init(TEST_SAMPLERATE));
        _multi_send.set_channels(2, 2);
        _multi_send.set_active_rt_processing(true, 0);

        PluginInfo info{.uid = "sushi.testing.return", .path = "", .type = PluginType::INTERNAL};

        auto [status_1, return_1] = _factory.new_instance(info, _host_ctrl, TEST_SAMPLERATE);
        ASSERT_EQ(ProcessorReturnCode::OK, status_1);
        auto [status_2, return_2] = _factory.new_instance(info, _host_ctrl, TEST_SAMPLERATE);
        ASSERT_EQ(ProcessorReturnCode::OK, status_2);

        _return_1 = std::static_pointer_cast<ReturnPlugin>(return_1);
        _return_2 = std::static_pointer_cast<ReturnPlugin>(return_2);

        _return_1->set_name("return_1");
        _return_2->set_name("return_2");

        for (auto& r : {_return_1, _return_2})
        {
            r->set_channels(2, 2);
            r->set_active_rt_processing(true, 1);
        }
    }

    ObjectId _dest_property_id(int slot)
    {
        return _multi_send.parameter_from_name("destination_" + std::to_string(slot + 1))->id();
    }

    ObjectId _gain_parameter_id(int slot)
    {
        return _multi_send.parameter_from_name("gain_" + std::to_string(slot + 1))->id();
    }

    SendReturnFactory   _factory;
    HostControlMockup   _host_control_mockup;
    HostControl         _host_ctrl {_host_control_mockup.make_host_control_mockup(TEST_SAMPLERATE)};

    multi_send_plugin::MultiSendPlugin _multi_send {_host_ctrl, &_factory};
    std::shared_ptr<ReturnPlugin> _return_1;
    std::shared_ptr<ReturnPlugin> _return_2;

    sushi::internal::multi_send_plugin::Accessor _accessor {_multi_send};
};

TEST_F(TestMultiSendPlugin, TestDestinationSetting)
{
    // All slots start inactive
    for (int slot = 0; slot < multi_send_plugin::MAX_SEND_DESTINATIONS; ++slot)
    {
        EXPECT_EQ(nullptr, _accessor.destination(slot));
        EXPECT_EQ(_dest_property_id(slot), _accessor.destination_property_id(slot));
    }

    auto status = _multi_send.set_property_value(_dest_property_id(0), "return_1");
    EXPECT_EQ(ProcessorReturnCode::OK, status);
    status = _multi_send.set_property_value(_dest_property_id(1), "return_2");
    EXPECT_EQ(ProcessorReturnCode::OK, status);

    EXPECT_EQ(_return_1.get(), _accessor.destination(0));
    EXPECT_EQ(_return_2.get(), _accessor.destination(1));
    EXPECT_EQ("return_1", _multi_send.property_value(_dest_property_id(0)).second);

    // Setting an empty name deactivates the slot
    status = _multi_send.set_property_value(_dest_property_id(0), "");
    EXPECT_EQ(ProcessorReturnCode::OK, status);
    EXPECT_EQ(nullptr, _accessor.destination(0));

    // Destroying a return automatically unlinks all slots pointing to it
    status = _multi_send.set_property_value(_dest_property_id(2), "return_2");
    EXPECT_EQ(ProcessorReturnCode::OK, status);
    EXPECT_EQ(_return_2.get(), _accessor.destination(2));

    _return_2.reset();
    EXPECT_EQ(nullptr, _accessor.destination(1));
    EXPECT_EQ(nullptr, _accessor.destination(2));
    EXPECT_EQ("", _multi_send.property_value(_dest_property_id(1)).second);
}

TEST_F(TestMultiSendPlugin, TestProcessingToMultipleDestinations)
{
    ChunkSampleBuffer in_buffer(2);
    ChunkSampleBuffer out_buffer(2);
    test_utils::fill_sample_buffer(in_buffer, 1.0f);

    // Test that processing without any destination doesn't break and passes through
    _multi_send.process_audio(in_buffer, out_buffer);
    test_utils::assert_buffer_value(1.0f, out_buffer);

    ASSERT_EQ(ProcessorReturnCode::OK, _multi_send.set_property_value(_dest_property_id(0), "return_1"));
    ASSERT_EQ(ProcessorReturnCode::OK, _multi_send.set_property_value(_dest_property_id(1), "return_2"));

    // Slot 1 at 0 dB (unity), slot 2 at -48 dB
    constexpr float GAIN_1_NORM = 120.0f / 144.0f;
    constexpr float GAIN_2_NORM = 0.5f;
    constexpr float GAIN_2_LIN = 0.00398107f; // -48 dB

    auto event = RtEvent::make_parameter_change_event(_multi_send.id(), 0, _gain_parameter_id(0), GAIN_1_NORM);
    _multi_send.process_event(event);
    event = RtEvent::make_parameter_change_event(_multi_send.id(), 0, _gain_parameter_id(1), GAIN_2_NORM);
    _multi_send.process_event(event);

    // Process enough chunks for the gain smoothers to settle, advancing the
    // transport so the return plugins swap buffers every chunk
    for (int i = 0; i < 300; ++i)
    {
        _host_control_mockup._transport.set_time(Time(i + 1), static_cast<int64_t>(i + 1) * AUDIO_CHUNK_SIZE);
        _multi_send.process_audio(in_buffer, out_buffer);
    }

    // The audio is passed through unchanged on the multi_send's own output
    test_utils::assert_buffer_value(1.0f, out_buffer);

    // Read the last chunk from both returns and verify the gains
    _return_1->process_audio(in_buffer, out_buffer);
    EXPECT_NEAR(1.0f, out_buffer.channel(0)[0], 1.0e-3);
    EXPECT_NEAR(1.0f, out_buffer.channel(1)[AUDIO_CHUNK_SIZE - 1], 1.0e-3);

    _return_2->process_audio(in_buffer, out_buffer);
    EXPECT_NEAR(GAIN_2_LIN, out_buffer.channel(0)[0], 1.0e-5);
    EXPECT_NEAR(GAIN_2_LIN, out_buffer.channel(1)[AUDIO_CHUNK_SIZE - 1], 1.0e-5);
}

TEST_F(TestMultiSendPlugin, TestInactiveSlotsAreSkipped)
{
    ChunkSampleBuffer in_buffer(2);
    ChunkSampleBuffer out_buffer(2);
    test_utils::fill_sample_buffer(in_buffer, 1.0f);

    // Only slot 8 is active, all others inactive
    ASSERT_EQ(ProcessorReturnCode::OK,
              _multi_send.set_property_value(_dest_property_id(multi_send_plugin::MAX_SEND_DESTINATIONS - 1), "return_1"));

    auto event = RtEvent::make_parameter_change_event(_multi_send.id(), 0,
                                                      _gain_parameter_id(multi_send_plugin::MAX_SEND_DESTINATIONS - 1),
                                                      120.0f / 144.0f);
    _multi_send.process_event(event);

    for (int i = 0; i < 300; ++i)
    {
        _host_control_mockup._transport.set_time(Time(i + 1), static_cast<int64_t>(i + 1) * AUDIO_CHUNK_SIZE);
        _multi_send.process_audio(in_buffer, out_buffer);
    }

    _return_1->process_audio(in_buffer, out_buffer);
    EXPECT_NEAR(1.0f, out_buffer.channel(0)[0], 1.0e-3);

    // return_2 was never sent to
    _return_2->process_audio(in_buffer, out_buffer);
    EXPECT_FLOAT_EQ(0.0f, out_buffer.channel(0)[0]);
}
