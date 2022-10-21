#include <vulkan/vk_layer.h>
#include <trace/trace.h>
#include <stdint.h>

#define ii(...) printf("(ii) " __VA_ARGS__)
#define ee(...) {\
	fprintf(stderr, "(ee) " __VA_ARGS__);\
	fprintf(stderr, "\033[2m(ee) %s:%d\033[0m\n", __func__, __LINE__);\
}

struct dispatch_table {
#define decl_fn(name) PFN_##name name

	/* instance */
	decl_fn(vkGetInstanceProcAddr);
	decl_fn(vkCreateInstance);
	decl_fn(vkDestroyInstance);
	decl_fn(vkCreateDevice);
	/* device */
	decl_fn(vkGetDeviceProcAddr);
	decl_fn(vkQueueSubmit);
	decl_fn(vkQueueWaitIdle);
	decl_fn(vkDeviceWaitIdle);
	decl_fn(vkWaitForFences);
	decl_fn(vkWaitSemaphores);
	decl_fn(vkWaitSemaphoresKHR);
	decl_fn(vkWaitForPresentKHR);
	decl_fn(vkQueuePresentKHR);

#undef decl_fn
};

static struct dispatch_table dispatch_;
static size_t trace_id_;
static const uint32_t MIN_LAYER_IFACE_VERSION = 2;
static VkInstance instance_ = VK_NULL_HANDLE;

VkResult vkmon_vkCreateInstance(const VkInstanceCreateInfo *inst_info,
 const VkAllocationCallbacks *allocator, VkInstance *inst)
{
	if (instance_ != VK_NULL_HANDLE) {
		ee("Vulkan instance already exists, only one is supported\n");
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	VkLayerInstanceCreateInfo *layer_info =
	 (VkLayerInstanceCreateInfo *) inst_info;

	while ((layer_info != NULL) &&
	 ((layer_info->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO) ||
	 (layer_info->function != VK_LAYER_LINK_INFO))) {
		layer_info = (VkLayerInstanceCreateInfo *) layer_info->pNext;
	}

	if (layer_info == NULL) {
		ee("Layer instance create info not found\n");
		return VK_ERROR_INITIALIZATION_FAILED;
	} else if (layer_info->u.pLayerInfo == NULL) {
		ee("Layer instance link info not found\n");
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	VkLayerInstanceLink *link = layer_info->u.pLayerInfo;

	const PFN_vkGetInstanceProcAddr get_instance_proc =
	 link->pfnNextGetInstanceProcAddr;
	if (get_instance_proc == NULL) {
		ee("Missing next layer vkGetInstanceProcAddr\n");
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	const PFN_vkCreateInstance create_instance =
	 (PFN_vkCreateInstance) get_instance_proc(NULL, "vkCreateInstance");
	if (create_instance == NULL) {
		ee("Next layer does not provide a vkCreateInstance\n");
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	layer_info->u.pLayerInfo = link->pNext;

	const VkResult res = create_instance(inst_info, allocator, inst);
	if (res != VK_SUCCESS) {
		ee("Failed to create instance\n");
		return res;
	}

	/* now we can actually use our instance link */
	layer_info->u.pLayerInfo = link;

#define getproc(name) (PFN_##name) get_instance_proc(*inst, #name)
#define setproc(name)\
	dispatch_.name = getproc(name);\
	if (!dispatch_.name) {\
		ee(#name " not found\n");\
		return VK_ERROR_INITIALIZATION_FAILED;\
	}

	setproc(vkGetInstanceProcAddr);
	setproc(vkCreateInstance);
	setproc(vkDestroyInstance);
	setproc(vkCreateDevice);

#undef setproc
#undef getproc
	return VK_SUCCESS;
}

VkResult vkmon_vkQueueSubmit(const VkQueue queue, uint32_t submit_count,
 const VkSubmitInfo *submits, const VkFence fence)
{
	trace_marker("%s id %zu", __func__, trace_id_++);
	return dispatch_.vkQueueSubmit(queue, submit_count, submits, fence);
}

VkResult vkmon_vkQueueWaitIdle(const VkQueue queue)
{
	trace_marker("%s id %zu", __func__, trace_id_++);
	return dispatch_.vkQueueWaitIdle(queue);
}

VkResult vkmon_vkDeviceWaitIdle(const VkDevice dev)
{
	trace_marker("%s id %zu", __func__, trace_id_++);
	return dispatch_.vkDeviceWaitIdle(dev);
}

VkResult vkmon_vkWaitForFences(const VkDevice dev, uint32_t fence_count,
 const VkFence *fences, VkBool32 wait_all, uint64_t timeout)
{
	trace_marker("%s id %zu", __func__, trace_id_++);
	return dispatch_.vkWaitForFences(dev, fence_count, fences, wait_all,
	 timeout);
}

VkResult vkmon_vkWaitSemaphores(const VkDevice dev,
 const VkSemaphoreWaitInfo *wait_info, uint64_t timeout)
{
	trace_marker("%s id %zu", __func__, trace_id_++);
	return dispatch_.vkWaitSemaphores(dev, wait_info, timeout);
}

VkResult vkmon_vkWaitSemaphoresKHR(const VkDevice dev,
 const VkSemaphoreWaitInfoKHR *wait_info, uint64_t timeout)
{
	trace_marker("%s id %zu", __func__, trace_id_++);
	return dispatch_.vkWaitSemaphoresKHR(dev, wait_info, timeout);
}

VkResult vkmon_vkWaitForPresentKHR(const VkDevice dev,
 const VkSwapchainKHR swapchain, uint64_t id, uint64_t timeout)
{
	trace_marker("%s id %zu present id %zu", __func__, trace_id_++, id);
	return dispatch_.vkWaitForPresentKHR(dev, swapchain, id, timeout);
}

VkResult vkmon_vkQueuePresentKHR(const VkQueue queue,
 const VkPresentInfoKHR *present_info)
{
	static size_t frame_count_;
	trace_marker("%s id %zu frame %zu", __func__, trace_id_,
	 frame_count_++);
	return dispatch_.vkQueuePresentKHR(queue, present_info);
}

VkResult vkmon_vkCreateDevice(VkPhysicalDevice gpu,
 const VkDeviceCreateInfo *dev_info, const VkAllocationCallbacks *allocator,
 VkDevice *dev)
{
	VkLayerDeviceCreateInfo *layer_info =
	 (VkLayerDeviceCreateInfo *) dev_info;

	while ((layer_info != NULL) &&
	 ((layer_info->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO) ||
	 (layer_info->function != VK_LAYER_LINK_INFO))) {
		layer_info = (VkLayerDeviceCreateInfo *) layer_info->pNext;
	}

	if (layer_info == NULL) {
		ee("Layer device create info not found\n");
		return VK_ERROR_INITIALIZATION_FAILED;
	} else if (layer_info->u.pLayerInfo == NULL) {
		ee("Layer device link info not found\n");
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	VkLayerDeviceLink *link = layer_info->u.pLayerInfo;

	const PFN_vkGetInstanceProcAddr get_instance_proc =
	 link->pfnNextGetInstanceProcAddr;
	if (get_instance_proc == NULL) {
		ee("Missing next layer vkGetInstanceProcAddr\n");
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	const PFN_vkGetDeviceProcAddr get_dev_proc =
	 link->pfnNextGetDeviceProcAddr;
	if (get_dev_proc == NULL) {
		ee("Missing next layer vkGetDeviceProcAddr\n");
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	const PFN_vkCreateDevice create_dev =
	 (PFN_vkCreateDevice) get_instance_proc(NULL, "vkCreateDevice");
	if (create_dev == NULL) {
		ee("Next layer does not provide a vkCreateDevice\n");
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	layer_info->u.pLayerInfo = link->pNext;

	const VkResult res = create_dev(gpu, dev_info, allocator, dev);
	if (res != VK_SUCCESS) {
		ee("Failed to create device\n");
		return res;
	}

#define getproc(name) (PFN_##name) get_dev_proc(*dev, #name)
#define setproc(name)\
	dispatch_.name = getproc(name);\
	if (!dispatch_.name) {\
		ee(#name " not found\n");\
		return VK_ERROR_INITIALIZATION_FAILED;\
	}

	setproc(vkGetDeviceProcAddr);
	setproc(vkQueueSubmit);
	setproc(vkQueueWaitIdle);
	setproc(vkDeviceWaitIdle);
	setproc(vkWaitForFences);
	setproc(vkWaitSemaphores);
#if 0
	setproc(vkWaitSemaphoresKHR);
	setproc(vkWaitForPresentKHR);
	setproc(vkQueuePresentKHR);
#endif

#undef setproc
#undef getproc
	return VK_SUCCESS;
}

PFN_vkVoidFunction vkmon_vkGetInstanceProcAddr(const VkInstance inst,
 const char *name)
{
	if (strcmp(name, "vkCreateInstance") == 0) {
		return (PFN_vkVoidFunction) vkmon_vkCreateInstance;
	} else if (strcmp(name, "vkCreateDevice") == 0) {
		return (PFN_vkVoidFunction) vkmon_vkCreateDevice;
	} else {
		return dispatch_.vkGetInstanceProcAddr(inst, name);
	}
}

PFN_vkVoidFunction vkmon_vkGetDeviceProcAddr(const VkDevice dev,
 const char *name)
{
#define else_if_call_fn(fn)\
	} else if (strcmp(name, #fn) == 0) {\
		return (PFN_vkVoidFunction) vkmon_##fn;\

	if (strcmp(name, "vkQueueSubmit") == 0) {
		return (PFN_vkVoidFunction) vkmon_vkQueueSubmit;
	else_if_call_fn(vkQueueSubmit)
	else_if_call_fn(vkQueueWaitIdle)
	else_if_call_fn(vkDeviceWaitIdle)
	else_if_call_fn(vkWaitForFences)
	else_if_call_fn(vkWaitSemaphores)
#if 0
	else_if_call_fn(vkWaitSemaphoresKHR)
	else_if_call_fn(vkWaitForPresentKHR)
	else_if_call_fn(vkQueuePresentKHR)
#endif
	} else {
		return dispatch_.vkGetDeviceProcAddr(dev, name);
	}
}

VkResult
vkmon_vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *ver)
{
	if ((ver == NULL) || (ver->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT)) {
		ee("Bad negotiate layer interface structure\n");
	} else if (ver->loaderLayerInterfaceVersion < MIN_LAYER_IFACE_VERSION) {
		ee("Unsupported loader layer interface version %u < %u+\n",
		 ver->loaderLayerInterfaceVersion, MIN_LAYER_IFACE_VERSION);
	} else {
		ver->loaderLayerInterfaceVersion = MIN_LAYER_IFACE_VERSION;
		ver->pfnGetInstanceProcAddr = vkmon_vkGetInstanceProcAddr;
		ver->pfnGetDeviceProcAddr = vkmon_vkGetDeviceProcAddr;
		ver->pfnGetPhysicalDeviceProcAddr = NULL;
		return VK_SUCCESS;
	}

	return VK_ERROR_INITIALIZATION_FAILED;
}
