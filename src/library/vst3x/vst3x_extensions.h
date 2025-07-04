/*
* Copyright 2017-2025 Elk Audio AB
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
* @brief Optional extension apis for Vst3 plugins
* @copyright 2017-2025 Elk Audio AB, Stockholm
*/

#ifndef SUSHI_LIBRARY_VST3X_EXTENSIONS_H
#define SUSHI_LIBRARY_VST3X_EXTENSIONS_H

#include "pluginterfaces/base/funknown.h"

#include "pluginterfaces/vst/vsttypes.h"

namespace elk {

constexpr auto STRING_PROPERTY_DEFAULT_LENGTH = 65535;

struct PropertyValue
{
    const Steinberg::char8* value{nullptr}; ///< Pointer to a string buffer.
    Steinberg::int32 length{0};             ///< Length in chars, not including null terminator.
};

struct PropertyInfo
{
    Steinberg::Vst::ParamID id;         ///< unique identifier of this property, must not be the same as any parameter
    Steinberg::Vst::String128 name;     ///< Unique parameter name (e.g. "sample_1_path")
    Steinberg::Vst::String128 label;    ///< Parameter display name (e.g. "Sample Path")
    Steinberg::int32 flags;			    ///< Propertyflags (see below)
    enum PropertyFlags
    {
        kNoFlags		    = 0,		///< no flags wanted
        kIsReadOnly		    = 1 << 0,	///< parameter cannot be changed from outside the plug-in (implies that kCanAutomate is NOT set)
        kAudioThreadNotify	= 1 << 1,	///< if the host set the property IElkProcessorExtension::propertyValueChanged() will be called in the audio thread
    };
};

typedef Steinberg::int32 (*AsyncWorkCallback)(void* data, Steinberg::uint16 requestId);

/**
 * @brief Host-side extension for requesting background work from the audio processor.
 * Extends Steinberg::Vst::IHostApplication.
 */
class IElkHostExtension : public Steinberg::FUnknown
{
public:
    /**
     * @brief Call from an audio thread to request work in a non-rt thread. The host will call the callback in a background
     *        worker thread and deliver the return value back to the audio thread via a call to asyncWorkCompleted.
     * @param callback This function pointer will be called in a worker thread.
     * @param data Opaque pointer that will be passed to the callback, can be null.
     * @param requestId [out] Populated by the host with a unique id that can be used to keep track of requests. The same
     *                  id will be passed to the callback.
     * @return kResultOk if successful, error code otherwise.
     */
    virtual Steinberg::tresult PLUGIN_API requestAsyncWork(AsyncWorkCallback callback, void* data, Steinberg::int32& requestId /*out*/) = 0;

    static const Steinberg::FUID iid;
};

DECLARE_CLASS_IID (IElkHostExtension, 0xE0BDB69E, 0xE6944226, 0xB3E097AE, 0x17DB39D3)

/**
 * @brief Host-side extension interface for notifying property changes and asynchronous work requests.
 * Extends Steinberg::Vst::IComponentHandler.
 */
class IElkComponentHandlerExtension : public Steinberg::FUnknown
{
public:
    /**
     * @brief Call from a non-realtime thread to notify the host that a string property has changed.
     * @param propertyId The unique ID of the property
     * @param value The new value. The plugin should set the PropertyValue value and length members to point to string data
     *              owned by the plugin. The host will copy this data before returning.
     * @return kResultOk if successful, error code otherwise.
     */
    virtual Steinberg::tresult PLUGIN_API notifyPropertyValueChange(Steinberg::int32 propertyId, const PropertyValue& value) = 0;

    static const Steinberg::FUID iid;
};

DECLARE_CLASS_IID (IElkComponentHandlerExtension, 0x83952AFF, 0x52844C67, 0xAC127387, 0x196C5DD4)

/**
 * @brief Plugin-side controller extension interface for managing properties.
 * Extends Steinberg::Vst::IEditController.
 */
class IElkControllerExtension : public Steinberg::FUnknown
{
public:
    /**
     * @brief The number of string properties implemented by the plugin.
     * @return An integer containing the number of string properties.
     */
    virtual Steinberg::int32 PLUGIN_API getPropertyCount() = 0;

    /**
     * @brief Query information on string properties.
     * @param propertyIndex an index ranging from 0 to the number returned by getPropertyCount - 1.
     * @param info A PropertyInfo struct provided by the caller that should be populated by the plugin.
     * @return kResultOk if PropertyIndex corresponds to a valid property, error code otherwise.
     */
    virtual Steinberg::tresult PLUGIN_API getPropertyInfo (Steinberg::int32 propertyIndex, elk::PropertyInfo& info /*out*/) = 0;

    /**
     * @brief Called by the host from a non-realtime thread to query the value of a string property. May be called concurrently
     *        with setPropertyValue.
     * @param propertyId the unique id of the property.
     * @param value A PropertyValue struct provided by the host. The plugin should populate the data and length members to point to
     *              memory owned by the plugin. The host will copy the data if the call returns with kResultOk.
     * @return kResultOk if propertyId corresponds to a valid property, error code otherwise.
     */
    virtual Steinberg::tresult PLUGIN_API getPropertyValue(Steinberg::int32 propertyId, PropertyValue& value) = 0;

    /**
     * @brief Called by the host from a non-realtime thread to set the value of a string property. May be called concurrently
     *        with getPropertyValue.
     * @param propertyId The unique id of the property.
     * @param value A reference to a PropertyValue struct with string data owned by the host and valid for the duration of
     *              the call. The plugin must copy the data if it is to be retained after the call has returned.
     * @return kResultOk if propertyId corresponds to a valid property, error code otherwise.
     */
    virtual Steinberg::tresult PLUGIN_API setPropertyValue(Steinberg::int32 propertyId, const PropertyValue& value) = 0;

    static const Steinberg::FUID iid;
};

DECLARE_CLASS_IID (IElkControllerExtension, 0x1016CCA4, 0x930B4F58, 0x83918C4F, 0x8C4F99AB)

/**
 * @brief Plugin-side controller extension interface for receiving notifications.
 * Extends Steinberg::Vst::IAudioProcessor
 */
class IElkProcessorExtension : public Steinberg::FUnknown
{
public:
    /**
     * @brief If kAudioThreadNotify is set to true for this property, id addition to calling setPropertyValue() from a non
     *        realtime thread, the host will call this function with the new value on the audio thread before a call to process().
     * @param propertyId The unique id of the property.
     * @param value A reference to a PropertyValue struct with string data owned by the host and valid for the duration of the call.
     */
    virtual void PLUGIN_API propertyValueChanged(Steinberg::int32 propertyId, const PropertyValue& value) = 0;

    /**
     * @brief Called by the host on the audio thread after an async work request requested by ICompponentHandlerExtension::requestAsyncWork()
     *        has completed.
     * @param requestId The unique request id returned by requestAsyncWork().
     * @param requestReturnValue The return value from the non realtime callback.
     */
    virtual void PLUGIN_API asyncWorkCompleted(Steinberg::int32 requestId, Steinberg::int32 requestReturnValue) = 0;

    static const Steinberg::FUID iid;
};

DECLARE_CLASS_IID (IElkProcessorExtension, 0x9A9D53E7, 0xF68A4270, 0x8A95E3C2, 0x7D0BD6E6)

} // namespace elk

#endif //SUSHI_LIBRARY_VST3X_EXTENSIONS_H
