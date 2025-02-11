#include <Windows.h>
#include <Nodos/PluginAPI.h>
#include <Nodos/PluginHelpers.hpp>
#include <Nodos/Helpers.hpp>
#include <glm/glm.hpp>
#include <nosVulkanSubsystem/nosVulkanSubsystem.h>
#include <nosVulkanSubsystem/Helpers.hpp>

#include "NDI_generated.h"
#include "Processing.NDI.Lib.h"

namespace nos {


static NOS_REGISTER_NAME(Name);
static NOS_REGISTER_NAME(Framerate);
static NOS_REGISTER_NAME(Size);
static NOS_REGISTER_NAME(Input);


struct NdiSendFrameContext : public NodeContext {
	std::string Name;
	nosVec2u Framerate = { 60000, 1000 };
	std::string CurrentSendInstanceName;
	NDIlib_send_instance_t SendInstance = nullptr;
	std::optional<vkss::Resource> IntermediateBuffer;

	std::string NodeStatus;


	NdiSendFrameContext(fb::Node const* node)
		: NodeContext()
	{
		auto name2pin = Load(*node);

		Name = std::string((char*)name2pin[NSN_Name]->data()->Data());
		ndi::Framerate framerateEnum = *(nos::ndi::Framerate*)name2pin[NSN_Framerate]->data()->Data();
		Framerate = FramerateEnumToVec2(framerateEnum);
	}


	virtual ~NdiSendFrameContext() {
		if (SendInstance) {
			NDIlib_send_destroy(SendInstance);
		}
	}

	void UpdateNodeStatus()
	{
		std::string status;
		if (SendInstance) {
			char infoBuf[MAX_COMPUTERNAME_LENGTH + 1];
			DWORD bufCharCount = MAX_COMPUTERNAME_LENGTH + 1;
			GetComputerNameA(infoBuf, &bufCharCount);

			status = format("Source name: {} ({})", infoBuf, CurrentSendInstanceName);
		}
		else {
			status = "";
		}

		if (NodeStatus != status)
		{
			NodeStatus = status;

			std::vector<flatbuffers::Offset<nos::fb::NodeStatusMessage>> msg;
			flatbuffers::FlatBufferBuilder fbb;
			msg.push_back(fb::CreateNodeStatusMessageDirect(fbb, NodeStatus.c_str(), fb::NodeStatusMessageType::INFO));

			HandleEvent(CreateAppEvent(
				fbb, nos::CreatePartialNodeUpdateDirect(fbb, &NodeId, nos::ClearFlags::NONE, 0, 0, 0, 0, 0, 0, &msg)));
		}
	}

	virtual void OnPathStart() override {
		NodeContext::OnPathStart();
		nosScheduleNodeParams schedule{ .NodeId = NodeId, .AddScheduleCount = 1 };
		nosEngine.ScheduleNode(&schedule);
	}

	virtual void GetScheduleInfo(nosScheduleInfo* info) override
	{
		info->Importance = 0;
		info->DeltaSeconds = { Framerate.y, Framerate.x };
		info->Type = NOS_SCHEDULE_TYPE_ON_DEMAND;
	}


	nosVec2u FramerateEnumToVec2(nos::ndi::Framerate framerate) {
		switch (framerate) {
		case nos::ndi::Framerate::_60_00:
			return { 60000, 1000 };
		case nos::ndi::Framerate::_59_94:
			return { 60000, 1001 };
		case nos::ndi::Framerate::_50_00:
			return { 50000, 1000 };
		case nos::ndi::Framerate::_30_00:
			return { 30000, 1000 };
		case nos::ndi::Framerate::_29_97:
			return { 30000, 1001 };
		case nos::ndi::Framerate::_24_00:
			return { 24000, 1000 };
		default:
			return { 60000, 1000 };
		}
	}


	virtual void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value)
	{
		NodeContext::OnPinValueChanged(pinName, pinId, value);

		if (pinName == NSN_Name) {
			std::string name = static_cast<const char*>(value.Data);
			if (Name != name) {
				Name = name;
				nosEngine.RecompilePath(NodeId);
			}
		}
		else if (pinName == NSN_Framerate) {
			auto framerateEnum = *static_cast<nos::ndi::Framerate*>(value.Data);
			auto framerate = FramerateEnumToVec2(framerateEnum);
			if (framerate.x != Framerate.x || framerate.y != Framerate.y) {
				Framerate = framerate;
				nosEngine.RecompilePath(NodeId);
			}
		}
	}


	virtual void OnPinConnected(nos::Name pinName, uuid connectedPin) {
		nosEngine.RecompilePath(NodeId);
	}


	nosResult ExecuteNode(nosNodeExecuteParams* params)
	{
		auto values = GetPinValues(params);

		auto inputBuffer =
			nos::vkss::ConvertToResourceInfo(*InterpretPinValue<nos::sys::vulkan::Buffer>(values[NSN_Input]));
		if (!inputBuffer.Memory.Handle)
		{
			return NOS_RESULT_FAILED;
		}

		auto size = *InterpretPinValue<glm::uvec2>(values[NSN_Size]);

		if (Name != CurrentSendInstanceName)
		{
			if (SendInstance)
			{
				NDIlib_send_destroy(SendInstance);
				SendInstance = nullptr;
			}
			if (Name.size() > 0)
			{
				NDIlib_send_create_t sendSettings;
				sendSettings.p_ndi_name = Name.c_str();
				SendInstance = NDIlib_send_create(&sendSettings);
			}
			CurrentSendInstanceName = Name;
		}

		UpdateNodeStatus();

		if (!SendInstance)
		{
			return NOS_RESULT_FAILED;
		}
		if (!IntermediateBuffer || IntermediateBuffer->Info.Buffer.Size != inputBuffer.Info.Buffer.Size)
		{
			nosBufferInfo bufInfo = {};
			bufInfo.Usage = nosBufferUsage(NOS_BUFFER_USAGE_TRANSFER_DST);
			bufInfo.MemoryFlags = nosMemoryFlags(NOS_MEMORY_FLAGS_HOST_VISIBLE | NOS_MEMORY_FLAGS_DOWNLOAD);
			bufInfo.Size = inputBuffer.Info.Buffer.Size;
			IntermediateBuffer = vkss::Resource::Create(bufInfo, "NDI Intermediate Buffer");
		}

		nosCmd cmd = vkss::BeginCmd(NOS_NAME("NDI Copy"), NodeId);
		nosVulkan->Copy(cmd, &inputBuffer, &*IntermediateBuffer, nullptr);
		nosGPUEvent event = {};
		nosCmdEndParams endParams{.ForceSubmit = true, .OutGPUEventHandle = &event};
		nosVulkan->End(cmd, &endParams);
		nosVulkan->WaitGpuEvent(&event, UINT64_MAX);

		auto buffer = nosVulkan->Map(&*IntermediateBuffer);

		NDIlib_video_frame_v2_t video_frame_data;
		video_frame_data.xres = size.x;
		video_frame_data.yres = size.y;
		video_frame_data.FourCC = NDIlib_FourCC_type_BGRA;
		video_frame_data.p_data = buffer;
		video_frame_data.frame_rate_N = Framerate.x;
		video_frame_data.frame_rate_D = Framerate.y;

		NDIlib_send_send_video_async_v2(SendInstance, &video_frame_data);

		nosScheduleNodeParams schedule{.NodeId = NodeId, .AddScheduleCount = 1};
		nosEngine.ScheduleNode(&schedule);

		return NOS_RESULT_SUCCESS;
	}
};

void RegisterNdiSendFrameNode(nosNodeFunctions* nodeFunctions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("nos.ndi.NdiSendFrame"), NdiSendFrameContext, nodeFunctions);
}

}