#include <Nodos/PluginAPI.h>
#include <Nodos/PluginHelpers.hpp>
#include <Nodos/Helpers.hpp>

#include "NDI_generated.h"
#include "Processing.NDI.Lib.h"

namespace nos {


static NOS_REGISTER_NAME(SourceName);
static NOS_REGISTER_NAME(Framesync);
static NOS_REGISTER_NAME(ColorFormat);
static NOS_REGISTER_NAME(Receiver);

static NDIlib_recv_color_format_e FbsColorFormat2NdiLibColorFormat(ndi::ColorFormat format)
{
	switch (format) {
		case ndi::ColorFormat::BGRX_BGRA:
			return NDIlib_recv_color_format_BGRX_BGRA;
		case ndi::ColorFormat::UYVY_BGRA:
			return NDIlib_recv_color_format_UYVY_BGRA;
		case ndi::ColorFormat::RGBX_RGBA:
			return NDIlib_recv_color_format_RGBX_RGBA;
		case ndi::ColorFormat::UYVY_RGBA:
			return NDIlib_recv_color_format_UYVY_RGBA;
		case ndi::ColorFormat::fastest:
			return NDIlib_recv_color_format_fastest;
		case ndi::ColorFormat::best:
			return NDIlib_recv_color_format_best;
		default:
			return NDIlib_recv_color_format_RGBX_RGBA;
	}
}

struct NdiGetReceiverContext : public NodeContext {
	std::string SourceName;
	bool UseFramesync = false;
	NDIlib_recv_color_format_e ColorFormat;
	bool shouldUpdateReceiver = true;
	NDIlib_recv_instance_t ndiReceiver = nullptr;
	NDIlib_framesync_instance_t ndiFramesync = nullptr;

	std::string NodeStatus;

	NdiGetReceiverContext(fb::Node const* node)
		: NodeContext()
	{
		auto name2pin = Load(*node);
		SourceName = (char*)name2pin[NSN_SourceName]->data()->Data();
		UseFramesync = *(bool*)name2pin[NSN_Framesync]->data()->Data();
		ndi::ColorFormat fbsColorFormat = *(ndi::ColorFormat*)name2pin[NSN_ColorFormat]->data()->Data();
		ColorFormat = FbsColorFormat2NdiLibColorFormat(fbsColorFormat);
	}


	virtual ~NdiGetReceiverContext() {
		if (ndiFramesync) {
			NDIlib_framesync_destroy(ndiFramesync);
		}
		if (ndiReceiver) {
			NDIlib_recv_destroy(ndiReceiver);
		}
	}


	virtual void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value)
	{
		NodeContext::OnPinValueChanged(pinName, pinId, value);

		if (pinName == NSN_SourceName) {
			const char* newValue = static_cast<const char*>(value.Data);
			if (SourceName != newValue) {
				SourceName = static_cast<const char*>(value.Data);
				shouldUpdateReceiver = true;
			}
		}
		else if (pinName == NSN_Framesync) {
			UseFramesync = *static_cast<bool*>(value.Data);
			shouldUpdateReceiver = true;
		}
		else if (pinName == NSN_ColorFormat) {
			auto fbsColorFormat = *static_cast<ndi::ColorFormat*>(value.Data);
			ColorFormat = FbsColorFormat2NdiLibColorFormat(fbsColorFormat);
			shouldUpdateReceiver = true;
		}
	}


	void UpdateNodeStatus()
	{
		std::string status;
		fb::NodeStatusMessageType messageType;
		if (ndiReceiver && NDIlib_recv_get_no_connections(ndiReceiver) > 0) {
			status = std::format("Receiving from: {}", SourceName);
			messageType = fb::NodeStatusMessageType::INFO;
		}
		else if (SourceName.size() > 0) {
			status = std::format("Trying to connect to: {}", SourceName);
			messageType = fb::NodeStatusMessageType::WARNING;
		}
		else {
			status = "No source name set";
			messageType = fb::NodeStatusMessageType::WARNING;

		}

		if (NodeStatus != status)
		{
			NodeStatus = status;

			std::vector<flatbuffers::Offset<nos::fb::NodeStatusMessage>> msg;
			flatbuffers::FlatBufferBuilder fbb;
			msg.push_back(fb::CreateNodeStatusMessageDirect(fbb, NodeStatus.c_str(), messageType));

			HandleEvent(CreateAppEvent(
				fbb, nos::CreatePartialNodeUpdateDirect(fbb, &NodeId, nos::ClearFlags::NONE, 0, 0, 0, 0, 0, 0, &msg)));
		}
	}


	void UpdateNdiReceiver() {
		if (shouldUpdateReceiver) {
			if (ndiFramesync) {
				NDIlib_framesync_destroy(ndiFramesync);
				ndiFramesync = nullptr;
			}
			if (ndiReceiver) {
				NDIlib_recv_destroy(ndiReceiver);
				ndiReceiver = nullptr;
			}

			NDIlib_recv_create_v3_t receiverSettings;
			receiverSettings.color_format = ColorFormat;
			receiverSettings.source_to_connect_to = {
				SourceName.c_str(),
				nullptr
			};

			ndiReceiver = NDIlib_recv_create_v3(&receiverSettings);

			if (UseFramesync && ndiReceiver) {
				ndiFramesync = NDIlib_framesync_create(ndiReceiver);
			}

			shouldUpdateReceiver = false;
		}
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params)
	{
		auto pins = NodeExecuteParams(params);

		UpdateNdiReceiver();
		UpdateNodeStatus();

		ndi::TReceiver receiver;
		receiver.name = SourceName;
		receiver.recv_instance = (uint64_t)ndiReceiver;
		receiver.framesync_instance = (uint64_t)ndiFramesync;

		nosEngine.SetPinValueByName(NodeId, NSN_Receiver, nos::Buffer::From(receiver));

		return NOS_RESULT_SUCCESS;
	}
};

void RegisterNdiGetReceiverNode(nosNodeFunctions* nodeFunctions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("nos.ndi.NdiGetReceiver"), NdiGetReceiverContext, nodeFunctions);
}

}