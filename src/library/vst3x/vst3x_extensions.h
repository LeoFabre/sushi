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
#include "pluginterfaces/base/falignpush.h"

#include "pluginterfaces/vst/vsttypes.h"

namespace elk {

constexpr auto STRING_PROPERTY_DEFAULT_LENGTH = 65535;

struct PropertyValue
{
    const Steinberg::char8* value{nullptr};
    Steinberg::int32 length{0};
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
        kCanAutomate	    = 1 << 0,	///< property can be automated, i.e. set by the host
        kIsReadOnly		    = 1 << 1,	///< parameter cannot be changed from outside the plug-in (implies that kCanAutomate is NOT set)
        kAudioThreadNotify	= 1 << 3,	///< if the host set the property IElkProcessorExtension::propertyValueChanged() will be called in the audio thread
        kIdChangeAllowed    = 1 << 4,	///< if the id is already taken by another parameter, the host may change it
    };
};

typedef Steinberg::int32 (*AsyncWorkCallback)(void* data, Steinberg::uint16 requestId);

// Extends Steinberg::Vst::IComponentHandler
// Passed via the setComponentHandler() call
class IElkComponentHandlerExtension : public Steinberg::FUnknown
{
public:
    virtual Steinberg::tresult PLUGIN_API notifyPropertyValueChange(Steinberg::int32 propertyId, const PropertyValue& value) = 0;

    virtual Steinberg::tresult PLUGIN_API requestAsyncWork(AsyncWorkCallback callback, void* data, Steinberg::int32& requestId) = 0;

    static const Steinberg::FUID iid;
};

DECLARE_CLASS_IID (IElkComponentHandlerExtension, 0x83952AFF, 0x52844C67, 0xAC127387, 0x196C5DD4)

// Extends Steinberg::Vst::IEditController
// Implemented by the plugin
class IElkControllerExtension : public Steinberg::FUnknown
{
public:
    virtual Steinberg::int32 PLUGIN_API getPropertyCount() = 0;

    virtual Steinberg::tresult PLUGIN_API getPropertyInfo (Steinberg::int32 propertyIndex, elk::PropertyInfo& info /*out*/) = 0;

    virtual Steinberg::tresult PLUGIN_API getPropertyValue(Steinberg::int32 propertyId, PropertyValue& value) = 0;

    virtual Steinberg::tresult PLUGIN_API setPropertyValue(Steinberg::int32 propertyId, const PropertyValue& value) = 0;

    static const Steinberg::FUID iid;
};

DECLARE_CLASS_IID (IElkControllerExtension, 0x1016CCA4, 0x930B4F58, 0x83918C4F, 0x8C4F99AB)

// Extends Steinberg::Vst::IAudioProcessor
// Implemented by the plugin
class IElkProcessorExtension : public Steinberg::FUnknown
{
public:
    virtual Steinberg::tresult PLUGIN_API propertyValueChanged(Steinberg::int32 propertyId, const PropertyValue& value) = 0;

    virtual void PLUGIN_API asyncWorkCompleted(Steinberg::int32 requestId, Steinberg::int32 requestStatus) = 0;

    static const Steinberg::FUID iid;
};

DECLARE_CLASS_IID (IElkProcessorExtension, 0x9A9D53E7, 0xF68A4270, 0x8A95E3C2, 0x7D0BD6E6)



/* Generated UIDS
DECLARE_CLASS_IID (Interface, 0x1016CCA4, 0x930B4F58, 0x83918C4F, 0x8C4F99AB)
DECLARE_CLASS_IID (Interface, 0x05897D07, 0x978743BC, 0xBC627DCB, 0x7397083F)
DECLARE_CLASS_IID (Interface, 0x9A9D53E7, 0xF68A4270, 0x8A95E3C2, 0x7D0BD6E6)
DECLARE_CLASS_IID (Interface, 0x83952AFF, 0x52844C67, 0xAC127387, 0x196C5DD4)
DECLARE_CLASS_IID (Interface, 0xA4212333, 0xC1904138, 0xA4CB183A, 0x8942A620)
DECLARE_CLASS_IID (Interface, 0xDC72B397, 0x678F43FB, 0x813E3D50, 0x1AA5776A)
DECLARE_CLASS_IID (Interface, 0x45E10F5E, 0x7D344831, 0xACDB2DFC, 0xBB947448)
DECLARE_CLASS_IID (Interface, 0x63547A9B, 0x44EE402C, 0xB28A3503, 0xB5ACDF88)
DECLARE_CLASS_IID (Interface, 0xE0BDB69E, 0xE6944226, 0xB3E097AE, 0x17DB39D3)
DECLARE_CLASS_IID (Interface, 0x731A38C8, 0x23184AEE, 0xB2F3EAD6, 0x87DFEFCA)
DECLARE_CLASS_IID (Interface, 0x926BED23, 0xDC104EED, 0xBE468BAD, 0xE5EB6E8C)
DECLARE_CLASS_IID (Interface, 0xD929F626, 0x2E864F48, 0xB3745F15, 0x167AD252)
*/



} // namespace elk

#endif //SUSHI_LIBRARY_VST3X_EXTENSIONS_H
