#include <Nodos/PluginAPI.h>
#include <Nodos/PluginHelpers.hpp>
#include <Nodos/Helpers.hpp>

#include "Processing.NDI.Lib.h"



namespace nos {

static NOS_REGISTER_NAME(SourceName);
static NOS_REGISTER_NAME(Selected);

struct NdiSelectSourceContext : public NodeContext {
	NDIlib_find_instance_t sourceFinder = nullptr;
	std::atomic_bool shouldUpdateSources = true;
	std::atomic_bool killSourceFinderThread = false;
	std::thread SourceFinderThread;


	NdiSelectSourceContext(fb::Node const* node)
		: NodeContext()
	{
		auto name2pin = Load(*node);
		std::string sourceName = (char*)name2pin[NSN_SourceName]->data()->Data();
		SetPinValue(NSN_Selected, { .Data = (void*)sourceName.c_str(), .Size = sourceName.length() + 1 });

		sourceFinder = NDIlib_find_create_v2();
		SourceFinderThread = std::thread([&]() -> void {
			while (!killSourceFinderThread)
			{
				if (NDIlib_find_wait_for_sources(sourceFinder, 1000)) {
					UpdateAvailableSources();
				}
			}
		});
	}


	virtual ~NdiSelectSourceContext() {
		if (SourceFinderThread.joinable()) {
			killSourceFinderThread = true;
			SourceFinderThread.join();
		}
		if (sourceFinder) {
			NDIlib_find_destroy(sourceFinder);
		}
	}


	void UpdateAvailableSources() {
		uint32_t sourceCount = 0;
		auto sources = NDIlib_find_get_current_sources(sourceFinder, &sourceCount);
		std::vector<std::string> sourceNames;
		for (uint32_t i = 0; i < sourceCount; ++i) {
			sourceNames.push_back(sources[i].p_ndi_name);
		}
		nos::UpdateStringList("ndiSources", sourceNames);
	}


	virtual void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value)
	{
		NodeContext::OnPinValueChanged(pinName, pinId, value);

		if (pinName == NSN_SourceName) {
			SetPinValue(NSN_Selected, value);
		}
	}
};

void RegisterNdiSelectSourceNode(nosNodeFunctions* nodeFunctions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("nos.ndi.NdiSelectSource"), NdiSelectSourceContext, nodeFunctions);
}

}