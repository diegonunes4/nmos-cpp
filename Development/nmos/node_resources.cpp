#include "nmos/node_resources.h"

#include "cpprest/host_utils.h"
#include "cpprest/uri_builder.h"
#include "nmos/api_utils.h" // for nmos::http_scheme
#include "nmos/channels.h"
#include "nmos/colorspace.h"
#include "nmos/connection_resources.h" // for nmos::make_connection_api_transportfile
#include "nmos/components.h"
#include "nmos/device_type.h"
#include "nmos/event_type.h"
#include "nmos/format.h"
#include "nmos/interlace_mode.h"
#include "nmos/is04_versions.h"
#include "nmos/is05_versions.h"
#include "nmos/is07_versions.h"
#include "nmos/media_type.h"
#include "nmos/resource.h"
#include "nmos/transfer_characteristic.h"
#include "nmos/transport.h"
#include "nmos/version.h"

namespace nmos
{
    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/device.json
    nmos::resource make_device(const nmos::id& id, const nmos::id& node_id, const std::vector<nmos::id>& senders, const std::vector<nmos::id>& receivers, const nmos::settings& settings)
    {
        using web::json::value;
        using web::json::value_of;
        using web::json::value_from_elements;

        auto data = details::make_resource_core(id, settings);

        data[U("type")] = value::string(nmos::device_types::generic.name);
        data[U("node_id")] = value::string(node_id);
        data[U("senders")] = value_from_elements(senders);
        data[U("receivers")] = value_from_elements(receivers);

        const auto at_least_one_host_address = value_of({ value::string(nmos::fields::host_address(settings)) });
        const auto& host_addresses = settings.has_field(nmos::fields::host_addresses) ? nmos::fields::host_addresses(settings) : at_least_one_host_address.as_array();

        if (0 <= nmos::fields::connection_port(settings))
        {
            for (const auto& version : nmos::is05_versions::from_settings(settings))
            {
                auto connection_uri = web::uri_builder()
                    .set_scheme(nmos::http_scheme(settings))
                    .set_port(nmos::fields::connection_port(settings))
                    .set_path(U("/x-nmos/connection/") + make_api_version(version));
                auto type = U("urn:x-nmos:control:sr-ctrl/") + make_api_version(version);

                if (nmos::experimental::fields::client_secure(settings))
                {
                    web::json::push_back(data[U("controls")], value_of({
                        { U("href"), connection_uri.set_host(nmos::get_host(settings)).to_uri().to_string() },
                        { U("type"), type }
                    }));
                }
                else for (const auto& host_address : host_addresses)
                {
                    web::json::push_back(data[U("controls")], value_of({
                        { U("href"), connection_uri.set_host(host_address.as_string()).to_uri().to_string() },
                        { U("type"), type }
                    }));
                }
            }
        }

        if (0 <= nmos::fields::events_port(settings))
        {
            for (const auto& version : nmos::is07_versions::from_settings(settings))
            {
                auto events_uri = web::uri_builder()
                    .set_scheme(nmos::http_scheme(settings))
                    .set_port(nmos::fields::events_port(settings))
                    .set_path(U("/x-nmos/events/") + make_api_version(version));
                auto type = U("urn:x-nmos:control:events/") + make_api_version(version);

                if (nmos::experimental::fields::client_secure(settings))
                {
                    web::json::push_back(data[U("controls")], value_of({
                        { U("href"), events_uri.set_host(nmos::get_host(settings)).to_uri().to_string() },
                        { U("type"), type }
                    }));
                }
                else for (const auto& host_address : host_addresses)
                {
                    web::json::push_back(data[U("controls")], value_of({
                        { U("href"), events_uri.set_host(host_address.as_string()).to_uri().to_string() },
                        { U("type"), type }
                    }));
                }
            }
        }

        return{ is04_versions::v1_3, types::device, data, false };
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/source_core.json
    nmos::resource make_source(const nmos::id& id, const nmos::id& device_id, const nmos::rational& grain_rate, const nmos::settings& settings)
    {
        using web::json::value;

        auto data = details::make_resource_core(id, settings);

        if (0 != grain_rate) data[U("grain_rate")] = nmos::make_rational(grain_rate); // optional
        data[U("caps")] = value::object();
        data[U("device_id")] = value::string(device_id);
        data[U("parents")] = value::array();
        data[U("clock_name")] = value::null();

        return{ is04_versions::v1_3, types::source, data, false };
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/source_generic.json
    nmos::resource make_generic_source(const nmos::id& id, const nmos::id& device_id, const nmos::rational& grain_rate, const nmos::format& format, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_source(id, device_id, grain_rate, settings);
        auto& data = resource.data;

        data[U("format")] = value::string(format.name);

        return resource;
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/source_generic.json
    nmos::resource make_video_source(const nmos::id& id, const nmos::id& device_id, const nmos::rational& grain_rate, const nmos::settings& settings)
    {
        return make_generic_source(id, device_id, grain_rate, nmos::formats::video, settings);
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/source_generic.json
    nmos::resource make_data_source(const nmos::id& id, const nmos::id& device_id, const nmos::rational& grain_rate, const nmos::settings& settings)
    {
        return make_generic_source(id, device_id, grain_rate, nmos::formats::data, settings);
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.3-dev/APIs/schemas/source_data.json
    nmos::resource make_data_source(const nmos::id& id, const nmos::id& device_id, const nmos::rational& grain_rate, const nmos::event_type& event_type, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_data_source(id, device_id, grain_rate, settings);
        auto& data = resource.data;

        data[U("event_type")] = value::string(event_type.name);

        return resource;
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/source_audio.json
    nmos::resource make_audio_source(const nmos::id& id, const nmos::id& device_id, const nmos::rational& grain_rate, const std::vector<channel>& channels, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_source(id, device_id, grain_rate, settings);
        auto& data = resource.data;

        data[U("format")] = value::string(nmos::formats::audio.name);

        auto channels_data = value::array();
        for (auto channel : channels)
        {
            web::json::push_back(channels_data, make_channel(channel));
        }
        data[U("channels")] = std::move(channels_data);

        return resource;
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/flow_core.json
    nmos::resource make_flow(const nmos::id& id, const nmos::id& source_id, const nmos::id& device_id, const nmos::rational& grain_rate, const nmos::settings& settings)
    {
        using web::json::value;

        auto data = details::make_resource_core(id, settings);

        if (0 != grain_rate) data[U("grain_rate")] = nmos::make_rational(grain_rate); // optional

        data[U("source_id")] = value::string(source_id);
        data[U("device_id")] = value::string(device_id);
        data[U("parents")] = value::array();

        return{ is04_versions::v1_3, types::flow, data, false };
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/flow_video.json
    nmos::resource make_video_flow(const nmos::id& id, const nmos::id& source_id, const nmos::id& device_id, const nmos::rational& grain_rate, unsigned int frame_width, unsigned int frame_height, const nmos::interlace_mode& interlace_mode, const nmos::colorspace& colorspace, const nmos::transfer_characteristic& transfer_characteristic, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_flow(id, source_id, device_id, grain_rate, settings);
        auto& data = resource.data;

        data[U("format")] = value::string(nmos::formats::video.name);
        data[U("frame_width")] = frame_width;
        data[U("frame_height")] = frame_height;
        if (interlace_modes::none != interlace_mode) data[U("interlace_mode")] = value::string(interlace_mode.name); // optional
        data[U("colorspace")] = value::string(colorspace.name);
        if (transfer_characteristics::none != transfer_characteristic) data[U("transfer_characteristic")] = value::string(transfer_characteristic.name); // optional

        return resource;
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/flow_video_raw.json
    nmos::resource make_raw_video_flow(const nmos::id& id, const nmos::id& source_id, const nmos::id& device_id, const nmos::rational& grain_rate, unsigned int frame_width, unsigned int frame_height, const nmos::interlace_mode& interlace_mode, const nmos::colorspace& colorspace, const nmos::transfer_characteristic& transfer_characteristic, chroma_subsampling chroma_subsampling, unsigned int bit_depth, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_video_flow(id, source_id, device_id, grain_rate, frame_width, frame_height, interlace_mode, colorspace, transfer_characteristic, settings);
        auto& data = resource.data;

        data[U("media_type")] = value::string(nmos::media_types::video_raw.name);

        data[U("components")] = make_components(chroma_subsampling, frame_width, frame_height, bit_depth);

        return resource;
    }

    nmos::resource make_raw_video_flow(const nmos::id& id, const nmos::id& source_id, const nmos::id& device_id, const nmos::settings& settings)
    {
        return make_raw_video_flow(id, source_id, device_id, {}, 1920, 1080, nmos::interlace_modes::interlaced_bff, nmos::colorspaces::BT709, nmos::transfer_characteristics::SDR, YCbCr422, 10, settings);
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/flow_video_coded.json
    // (media_type must *not* be nmos::media_types::video_raw; cf. nmos::make_raw_video_flow)
    nmos::resource make_coded_video_flow(const nmos::id& id, const nmos::id& source_id, const nmos::id& device_id, const nmos::rational& grain_rate, unsigned int frame_width, unsigned int frame_height, const nmos::interlace_mode& interlace_mode, const nmos::colorspace& colorspace, const nmos::transfer_characteristic& transfer_characteristic, const nmos::media_type& media_type, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_video_flow(id, source_id, device_id, grain_rate, frame_width, frame_height, interlace_mode, colorspace, transfer_characteristic, settings);
        auto& data = resource.data;

        data[U("media_type")] = value::string(media_type.name);

        return resource;
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/flow_audio.json
    nmos::resource make_audio_flow(const nmos::id& id, const nmos::id& source_id, const nmos::id& device_id, const nmos::rational& sample_rate, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_flow(id, source_id, device_id, {}, settings);
        auto& data = resource.data;

        data[U("format")] = value::string(nmos::formats::audio.name);
        data[U("sample_rate")] = make_rational(sample_rate);

        return resource;
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/flow_audio_raw.json
    nmos::resource make_raw_audio_flow(const nmos::id& id, const nmos::id& source_id, const nmos::id& device_id, const nmos::rational& sample_rate, unsigned int bit_depth, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_audio_flow(id, source_id, device_id, sample_rate, settings);
        auto& data = resource.data;

        data[U("media_type")] = value::string(nmos::media_types::audio_L(bit_depth).name);
        data[U("bit_depth")] = bit_depth;

        return resource;
    }

    nmos::resource make_raw_audio_flow(const nmos::id& id, const nmos::id& source_id, const nmos::id& device_id, const nmos::settings& settings)
    {
        return make_raw_audio_flow(id, source_id, device_id, 48000, 24, settings);
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/flow_audio_coded.json
    // (media_type must *not* be nmos::media_types::audio_L(bit_depth); cf. nmos::make_raw_audio_flow)
    nmos::resource make_coded_audio_flow(const nmos::id& id, const nmos::id& source_id, const nmos::id& device_id, const nmos::rational& sample_rate, const nmos::media_type& media_type, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_audio_flow(id, source_id, device_id, sample_rate, settings);
        auto& data = resource.data;

        data[U("media_type")] = value::string(media_type.name);

        return resource;
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/flow_sdianc_data.json
    nmos::resource make_sdianc_data_flow(const nmos::id& id, const nmos::id& source_id, const nmos::id& device_id, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_flow(id, source_id, device_id, {}, settings);
        auto& data = resource.data;

        data[U("format")] = value::string(nmos::formats::data.name);
        data[U("media_type")] = value::string(nmos::media_types::video_smpte291.name);
        //data[U("DID_SDID")] = value::array(); // optional

        return resource;
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/flow_data.json
    // (media_type must *not* be nmos::media_types::video_smpte291; cf. nmos::make_sdianc_data_flow)
    nmos::resource make_data_flow(const nmos::id& id, const nmos::id& source_id, const nmos::id& device_id, const nmos::media_type& media_type, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_flow(id, source_id, device_id, {}, settings);
        auto& data = resource.data;

        data[U("format")] = value::string(nmos::formats::data.name);
        data[U("media_type")] = value::string(media_type.name);

        return resource;
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/flow_mux.json
    nmos::resource make_mux_flow(const nmos::id& id, const nmos::id& source_id, const nmos::id& device_id, const nmos::media_type& media_type, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_flow(id, source_id, device_id, {}, settings);
        auto& data = resource.data;

        data[U("format")] = value::string(nmos::formats::mux.name);
        data[U("media_type")] = value::string(media_type.name);

        return resource;
    }

    nmos::resource make_mux_flow(const nmos::id& id, const nmos::id& source_id, const nmos::id& device_id, const nmos::settings& settings)
    {
        return make_mux_flow(id, source_id, device_id, nmos::media_types::video_SMPTE2022_6, settings);
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/sender.json
    nmos::resource make_sender(const nmos::id& id, const nmos::id& flow_id, const nmos::transport& transport, const nmos::id& device_id, const utility::string_t& manifest_href, const std::vector<utility::string_t>& interfaces, const nmos::settings& settings)
    {
        using web::json::value;

        auto data = details::make_resource_core(id, settings);

        //data[U("caps")] = value::object(); // optional

        data[U("flow_id")] = !flow_id.empty() ? value::string(flow_id) : value::null();
        data[U("transport")] = value::string(transport.name);
        data[U("device_id")] = value::string(device_id);
        // "Permit a Sender's 'manifest_href' to be null when the transport type does not require a transport file" from IS-04 v1.3
        // See https://github.com/AMWA-TV/nmos-discovery-registration/pull/97
        data[U("manifest_href")] = !manifest_href.empty() ? value::string(manifest_href) : value::null();

        auto& interface_bindings = data[U("interface_bindings")] = value::array();
        for (const auto& interface : interfaces)
        {
            web::json::push_back(interface_bindings, interface);
        }

        value subscription;
        subscription[U("receiver_id")] = value::null();
        subscription[U("active")] = value::boolean(false);
        data[U("subscription")] = std::move(subscription);

        return{ is04_versions::v1_3, types::sender, data, false };
    }

    nmos::resource make_sender(const nmos::id& id, const nmos::id& flow_id, const nmos::id& device_id, const std::vector<utility::string_t>& interfaces, const nmos::settings& settings)
    {
        return make_sender(id, flow_id, nmos::transports::rtp_mcast, device_id, make_connection_api_transportfile(id, settings).to_string(), interfaces, settings);
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/receiver_core.json
    nmos::resource make_receiver(const nmos::id& id, const nmos::id& device_id, const nmos::transport& transport, const std::vector<utility::string_t>& interfaces, const nmos::settings& settings)
    {
        using web::json::value;

        auto data = details::make_resource_core(id, settings);

        data[U("device_id")] = value::string(device_id);
        data[U("transport")] = value::string(transport.name);

        auto& interface_bindings = data[U("interface_bindings")] = value::array();
        for (const auto& interface : interfaces)
        {
            web::json::push_back(interface_bindings, interface);
        }

        value subscription;
        subscription[U("sender_id")] = value::null();
        subscription[U("active")] = value::boolean(false);
        data[U("subscription")] = std::move(subscription);

        return{ is04_versions::v1_3, types::receiver, data, false };
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/receiver_video.json
    nmos::resource make_video_receiver(const nmos::id& id, const nmos::id& device_id, const nmos::transport& transport, const std::vector<utility::string_t>& interfaces, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_receiver(id, device_id, transport, interfaces, settings);
        auto& data = resource.data;

        data[U("format")] = value::string(nmos::formats::video.name);
        data[U("caps")][U("media_types")][0] = value::string(nmos::media_types::video_raw.name);

        return resource;
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/receiver_audio.json
    nmos::resource make_audio_receiver(const nmos::id& id, const nmos::id& device_id, const nmos::transport& transport, const std::vector<utility::string_t>& interfaces, const std::vector<unsigned int>& bit_depths, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_receiver(id, device_id, transport, interfaces, settings);
        auto& data = resource.data;

        data[U("format")] = value::string(nmos::formats::audio.name);
        for (const auto& bit_depth : bit_depths)
        {
            web::json::push_back(data[U("caps")][U("media_types")], value::string(nmos::media_types::audio_L(bit_depth).name));
        }

        return resource;
    }

    nmos::resource make_audio_receiver(const nmos::id& id, const nmos::id& device_id, const nmos::transport& transport, const std::vector<utility::string_t>& interfaces, unsigned int bit_depth, const nmos::settings& settings)
    {
        return make_audio_receiver(id, device_id, transport, interfaces, { 1, bit_depth }, settings);
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/receiver_data.json
    nmos::resource make_sdianc_data_receiver(const nmos::id& id, const nmos::id& device_id, const nmos::transport& transport, const std::vector<utility::string_t>& interfaces, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_receiver(id, device_id, transport, interfaces, settings);
        auto& data = resource.data;

        data[U("format")] = value::string(nmos::formats::data.name);
        data[U("caps")][U("media_types")][0] = value::string(nmos::media_types::video_smpte291.name);

        return resource;
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/receiver_data.json
    // (media_type must *not* be nmos::media_types::video_smpte291; cf. nmos::make_sdianc_data_receiver)
    nmos::resource make_data_receiver(const nmos::id& id, const nmos::id& device_id, const nmos::transport& transport, const std::vector<utility::string_t>& interfaces, const nmos::media_type& media_type, const std::vector<nmos::event_type>& event_types, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_receiver(id, device_id, transport, interfaces, settings);
        auto& data = resource.data;

        data[U("format")] = value::string(nmos::formats::data.name);
        data[U("caps")][U("media_types")][0] = value::string(media_type.name);
        for (const auto& event_type : event_types)
        {
            web::json::push_back(data[U("caps")][U("event_types")], value::string(event_type.name));
        }

        return resource;
    }

    nmos::resource make_data_receiver(const nmos::id& id, const nmos::id& device_id, const nmos::transport& transport, const std::vector<utility::string_t>& interfaces, const nmos::media_type& media_type, const nmos::settings& settings)
    {
        return make_data_receiver(id, device_id, transport, interfaces, media_type, {}, settings);
    }

    // See https://github.com/AMWA-TV/nmos-discovery-registration/blob/v1.2/APIs/schemas/receiver_mux.json
    nmos::resource make_mux_receiver(const nmos::id& id, const nmos::id& device_id, const nmos::transport& transport, const std::vector<utility::string_t>& interfaces, const nmos::media_type& media_type, const nmos::settings& settings)
    {
        using web::json::value;

        auto resource = make_receiver(id, device_id, transport, interfaces, settings);
        auto& data = resource.data;

        data[U("format")] = value::string(nmos::formats::data.name);
        data[U("caps")][U("media_types")][0] = value::string(media_type.name);

        return resource;
    }

    nmos::resource make_mux_receiver(const nmos::id& id, const nmos::id& device_id, const nmos::transport& transport, const std::vector<utility::string_t>& interfaces, const nmos::settings& settings)
    {
        return make_mux_receiver(id, device_id, transport, interfaces, nmos::media_types::video_SMPTE2022_6, settings);
    }
}
