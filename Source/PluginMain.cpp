#include <Nodos/PluginAPI.h>
#include <Nodos/PluginHelpers.hpp>
#include <Nodos/Helpers.hpp>
#include <nosVulkanSubsystem/Helpers.hpp>

NOS_INIT()
NOS_VULKAN_INIT()
NOS_BEGIN_IMPORT_DEPS()
NOS_VULKAN_IMPORT()
NOS_END_IMPORT_DEPS()


namespace nos
{
void RegisterNdiSendFrameNode(nosNodeFunctions* nodeFunctions);
void RegisterNdiSelectSourceNode(nosNodeFunctions* nodeFunctions);
void RegisterNdiGetReceiverNode(nosNodeFunctions* nodeFunctions);
void RegisterNdiReceiveFrameNode(nosNodeFunctions* nodeFunctions);
void RegisterRGBAToBGRABuffer(nosNodeFunctions* nodeFunctions);

enum NodeType : int {
	NdiSendFrame,
	NdiSelectSource,
	NdiGetReceiver,
	NdiReceiveFrame,
	RGBAToBGRABuffer,
	Count
};

extern "C"
nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outCount, nosNodeFunctions** outFunctions)
{
	*outCount = (size_t)NodeType::Count;
	if (!outFunctions)
		return NOS_RESULT_SUCCESS;

	for (int i = 0; i < NodeType::Count; ++i)
	{
		auto node = outFunctions[i];
		switch ((NodeType)i)
		{
		case NodeType::NdiSendFrame:
			RegisterNdiSendFrameNode(node);
			break;
		case NodeType::NdiSelectSource:
			RegisterNdiSelectSourceNode(node);
			break;
		case NodeType::NdiGetReceiver:
			RegisterNdiGetReceiverNode(node);
			break;
		case NodeType::NdiReceiveFrame:
			RegisterNdiReceiveFrameNode(node);
			break;
		case NodeType::RGBAToBGRABuffer:
			RegisterRGBAToBGRABuffer(node);
			break;
		};
	}
	return NOS_RESULT_SUCCESS;
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* out)
{
	out->ExportNodeFunctions = ExportNodeFunctions;
	return NOS_RESULT_SUCCESS;
}
}
}