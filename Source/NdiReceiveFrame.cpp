#include <Nodos/PluginAPI.h>
#include <Nodos/PluginHelpers.hpp>
#include <Nodos/Helpers.hpp>
#include <nosVulkanSubsystem/nosVulkanSubsystem.h>
#include <nosVulkanSubsystem/Helpers.hpp>

#include "NDI_generated.h"
#include "Processing.NDI.Lib.h"

namespace nos {


static NOS_REGISTER_NAME(Receiver);
static NOS_REGISTER_NAME(FrameBuffer);
static NOS_REGISTER_NAME(Size);
static NOS_REGISTER_NAME(TextureFormat);

struct NdiReceiveFrameContext : public NodeContext {
	nosResourceShareInfo FrameBuffer = {};
	uint8_t* FrameBufferPtr = nullptr;

	NdiReceiveFrameContext(fb::Node const* node)
		: NodeContext(node)
	{
	}


	virtual ~NdiReceiveFrameContext() {
	}


	void ProcessNdiFrame(NDIlib_video_frame_v2_t& frame, nos::NodeExecuteParams& pins)
	{
		nosTextureFieldType fieldType;
		switch (frame.frame_format_type) {
		case NDIlib_frame_format_type_progressive:
			fieldType = nosTextureFieldType::NOS_TEXTURE_FIELD_TYPE_PROGRESSIVE;
			break;
		case NDIlib_frame_format_type_field_0:
			fieldType = nosTextureFieldType::NOS_TEXTURE_FIELD_TYPE_EVEN;
			break;
		case NDIlib_frame_format_type_field_1:
			fieldType = nosTextureFieldType::NOS_TEXTURE_FIELD_TYPE_ODD;
			break;
		default:
			return;
		}

		sys::vulkan::Format textureFormat = sys::vulkan::Format::NONE;
		uint32_t textureSize = 0;
		switch (frame.FourCC) {
		case NDIlib_FourCC_type_UYVY:
			// TODO
			break;
		case NDIlib_FourCC_type_UYVA:
			// TODO
			break;
		case NDIlib_FourCC_type_P216:
			// TODO
			break;
		case NDIlib_FourCC_type_PA16:
			// TODO
			break;
		case NDIlib_FourCC_type_YV12:
			// TODO
			break;
		case NDIlib_FourCC_type_I420:
			// TODO
			break;
		case NDIlib_FourCC_type_NV12:
			// TODO
			break;
		case NDIlib_FourCC_type_BGRA:
		case NDIlib_FourCC_type_BGRX:
			textureFormat = sys::vulkan::Format::B8G8R8A8_SRGB;
			textureSize = 4 * frame.xres * frame.yres;
			break;
		case NDIlib_FourCC_type_RGBA:
		case NDIlib_FourCC_type_RGBX:
			textureFormat = sys::vulkan::Format::R8G8B8A8_SRGB;
			textureSize = 4 * frame.xres * frame.yres;
			break;
		}

		if (!FrameBuffer.Memory.Handle || FrameBuffer.Info.Buffer.Size != textureSize) {
			if (FrameBuffer.Memory.Handle) {
				nosVulkan->DestroyResource(&FrameBuffer);
				FrameBuffer.Memory.Handle = 0;
			}

			FrameBuffer.Info.Type = NOS_RESOURCE_TYPE_BUFFER;
			FrameBuffer.Info.Buffer.Usage = nosBufferUsage(NOS_BUFFER_USAGE_TRANSFER_SRC);
			FrameBuffer.Info.Buffer.MemoryFlags = NOS_MEMORY_FLAGS_HOST_VISIBLE;
			FrameBuffer.Info.Buffer.Size = textureSize;

			nosVulkan->CreateResource(&FrameBuffer);
			FrameBufferPtr = nosVulkan->Map(&FrameBuffer);
		}

		if (FrameBuffer.Memory.Handle) {
			memcpy(FrameBufferPtr, frame.p_data, 4 * frame.xres * frame.yres);
			nos::fb::vec2u size(frame.xres, frame.yres);
			SetPinValue(NSN_FrameBuffer, nos::Buffer::From(vkss::ConvertBufferInfo(FrameBuffer)));
			SetPinValue(NSN_Size, nos::Buffer::From(size));
			SetPinValue(NSN_TextureFormat, nos::Buffer::From(textureFormat));
		}
	}

	nosResult ExecuteNode(nosNodeExecuteParams* execParams)
	{
		auto pins = NodeExecuteParams(execParams);

		NodeExecuteParams params = execParams;
		ndi::Receiver* receiver = InterpretPinValue<ndi::Receiver>(params[NSN_Receiver].Data->Data);

		if (!receiver)
			return NOS_RESULT_SUCCESS;

		NDIlib_recv_instance_t ndiReceiver = (NDIlib_recv_instance_t)receiver->recv_instance();
		NDIlib_framesync_instance_t ndiFramesync = (NDIlib_framesync_instance_t)receiver->framesync_instance();

		if (ndiReceiver) {
			NDIlib_video_frame_v2_t videoData;
			if (ndiFramesync) {
				NDIlib_framesync_capture_video(ndiFramesync, &videoData);
				if (videoData.data_size_in_bytes > 0)
					ProcessNdiFrame(videoData, pins);
				NDIlib_framesync_free_video(ndiFramesync, &videoData);
			}
			else {
				auto recvResult = NDIlib_recv_capture_v2(ndiReceiver, &videoData, nullptr, nullptr, 0);
				if (recvResult == NDIlib_frame_type_video) {
					ProcessNdiFrame(videoData, pins);
					NDIlib_recv_free_video_v2(ndiReceiver, &videoData);
				}
			}
		}

		return NOS_RESULT_SUCCESS;
	}
};

void RegisterNdiReceiveFrameNode(nosNodeFunctions* nodeFunctions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("nos.ndi.NdiReceiveFrame"), NdiReceiveFrameContext, nodeFunctions);
}

}