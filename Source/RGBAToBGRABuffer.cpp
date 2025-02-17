#include <Nodos/PluginAPI.h>
#include <Nodos/PluginHelpers.hpp>
#include <Nodos/Helpers.hpp>

#include <nosVulkanSubsystem/Helpers.hpp>
#include <nosVulkanSubsystem/nosVulkanSubsystem.h>


namespace nos {

struct RGBA2BGRABufferNodeContext : NodeContext
{
	RGBA2BGRABufferNodeContext(nosFbNodePtr node) : NodeContext(node)
	{
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		NodeExecuteParams execParams(params);
		const nosBuffer* inputPinData = execParams[NOS_NAME_STATIC("Source")].Data;
		const nosBuffer* outputPinData = execParams[NOS_NAME_STATIC("Output")].Data;
		auto input = vkss::DeserializeTextureInfo(inputPinData->Data);
		auto& output = *InterpretPinValue<sys::vulkan::Buffer>(outputPinData->Data);
		output.mutate_field_type(sys::vulkan::FieldType::PROGRESSIVE);

		nosVec2u ext = { input.Info.Texture.Width, input.Info.Texture.Height };

		uint32_t bufSize = ext.x * ext.y * 4;
		constexpr auto outMemoryFlags = nosMemoryFlags(NOS_MEMORY_FLAGS_DEVICE_MEMORY);
		if (output.size_in_bytes() != bufSize || output.memory_flags() != (sys::vulkan::MemoryFlags)(outMemoryFlags))
		{
			nosResourceShareInfo bufInfo = {
				.Info = {
					.Type = NOS_RESOURCE_TYPE_BUFFER,
					.Buffer = nosBufferInfo{
						.Size = (uint32_t)bufSize,
						.Usage = nosBufferUsage(NOS_BUFFER_USAGE_TRANSFER_SRC | NOS_BUFFER_USAGE_STORAGE_BUFFER),
						.MemoryFlags = outMemoryFlags,
						.FieldType = nosTextureFieldType::NOS_TEXTURE_FIELD_TYPE_PROGRESSIVE,
					}} };
			auto bufferDesc = vkss::ConvertBufferInfo(bufInfo);
			nosEngine.SetPinValueByName(NodeId, NOS_NAME_STATIC("Output"), Buffer::From(bufferDesc));
		}
		auto* dispatchSize = execParams.GetPinData<nosVec2u>(NOS_NAME_STATIC("DispatchSize"));
		*dispatchSize = { ext.x / 4, ext.y };
		return nosVulkan->ExecuteGPUNode(this, params);
	}
};

void RegisterRGBAToBGRABuffer(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("nos.ndi.RGBAToBGRABuffer"), RGBA2BGRABufferNodeContext, funcs);
}

}