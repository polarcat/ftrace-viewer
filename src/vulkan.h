struct context {
	ImGui_ImplVulkanH_Window gui_win;
	VkInstance vk_instance = VK_NULL_HANDLE;
	VkPhysicalDevice vk_gpu = VK_NULL_HANDLE;
	VkDevice vk_dev = VK_NULL_HANDLE;
	VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;
	VkPipelineCache vk_pipeline_cache = VK_NULL_HANDLE;
	VkQueue vk_queue = VK_NULL_HANDLE;
	uint32_t vk_queue_family = (uint32_t) -1;
	VkAllocationCallbacks *vk_alloctor = NULL;
	int image_count = 2;
	bool rebuild_swapchain = false;
	VkDebugReportCallbackEXT vk_debug = VK_NULL_HANDLE;
};

static struct context ctx_;

static inline bool is_minimized(ImDrawData *data)
{
	return (data->DisplaySize.x <= 0. || data->DisplaySize.y <= 0.);
}

static VkResult create_instance(void)
{
	VkResult err;
	uint32_t cnt = 0;
	const char **ext = glfwGetRequiredInstanceExtensions(&cnt);

	VkInstanceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.enabledExtensionCount = cnt;
	info.ppEnabledExtensionNames = ext;

	vk_call(err, vkCreateInstance(&info, ctx_.vk_alloctor,
	 &ctx_.vk_instance));
	return err;
}

static VkResult select_gpu(void)
{
	VkResult err;
	uint32_t cnt;
	VkPhysicalDevice *gpus;
	uint32_t gpu;

	vk_call(err, vkEnumeratePhysicalDevices(ctx_.vk_instance, &cnt,
	 NULL));
	if (cnt == 0)
		return VK_NOT_READY;

	size_t bytes = sizeof(VkPhysicalDevice) * cnt;

	if (!(gpus = (VkPhysicalDevice *) malloc(bytes))) {
		ee("failed to allocate %zu bytes\n", bytes);
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}

	vk_call(err, vkEnumeratePhysicalDevices(ctx_.vk_instance, &cnt,
	 gpus));
	if (err != VK_SUCCESS)
		goto out;

	/* find discrete GPU; use first if not found */
	gpu = 0;
	for (uint32_t i = 0; i < cnt; ++i) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(gpus[i], &props);
		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			gpu = i;
			break;
		}
	}

out:
	ctx_.vk_gpu = gpus[gpu];
	free(gpus);
	return err;
}

static VkResult select_queue_family(void)
{
	VkResult err;
	uint32_t cnt;
	VkQueueFamilyProperties *queues;

	vkGetPhysicalDeviceQueueFamilyProperties(ctx_.vk_gpu, &cnt, NULL);
	if (cnt == 0)
		return VK_ERROR_INITIALIZATION_FAILED;

	size_t bytes = sizeof(VkQueueFamilyProperties) * cnt;

	if (!(queues = (VkQueueFamilyProperties *) malloc(bytes))) {
		ee("failed to allocate %zu bytes\n", bytes);
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}

	vkGetPhysicalDeviceQueueFamilyProperties(ctx_.vk_gpu, &cnt, queues);

	for (uint32_t i = 0; i < cnt; ++i) {
		if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			ctx_.vk_queue_family = i;
			break;
		}
	}

out:
	free(queues);

	if (ctx_.vk_queue_family != (uint32_t) -1) {
		err = VK_SUCCESS;
	} else {
		ee("failed to select vulkan queue family\n");
		err = VK_ERROR_INITIALIZATION_FAILED;
	}

	return err;
}

static VkResult create_device(void)
{
	VkResult err;
        const char *ext[] = { "VK_KHR_swapchain" };
        int ext_cnt = ARRAY_SIZE(ext);
        const float queue_priority[] = { 1. };

        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = ctx_.vk_queue_family;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;

        VkDeviceCreateInfo dev_info = {};
        dev_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dev_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
        dev_info.pQueueCreateInfos = queue_info;
        dev_info.enabledExtensionCount = ext_cnt;
        dev_info.ppEnabledExtensionNames = ext;

        vk_call(err, vkCreateDevice(ctx_.vk_gpu, &dev_info,
	 ctx_.vk_alloctor, &ctx_.vk_dev));
	if (err != VK_SUCCESS)
		return err;

        vkGetDeviceQueue(ctx_.vk_dev, ctx_.vk_queue_family, 0,
	 &ctx_.vk_queue);
	return err;
}

static VkResult create_descriptor_pool(void)
{
	VkResult err;
	VkDescriptorPoolSize sizes[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	info.maxSets = 1000 * ARRAY_SIZE(sizes);
	info.poolSizeCount = (uint32_t) ARRAY_SIZE(sizes);
	info.pPoolSizes = sizes;

	vk_call(err, vkCreateDescriptorPool(ctx_.vk_dev, &info,
	 ctx_.vk_alloctor, &ctx_.vk_descriptor_pool));
	return err;
}

static bool init_vulkan(void)
{
	if (create_instance() != VK_SUCCESS)
		return false;
	else if (select_gpu() != VK_SUCCESS)
		return false;
	else if (select_queue_family() != VK_SUCCESS)
		return false;
	else if (create_device() != VK_SUCCESS)
		return false;
	else if (create_descriptor_pool() != VK_SUCCESS)
		return false;
	else
		return true;
}

static bool init_engine_surface(void)
{
	if (!init_vulkan())
		return false;

	VkResult err;

	vk_call(err, glfwCreateWindowSurface(ctx_.vk_instance,
	 win_, ctx_.vk_alloctor, &ctx_.gui_win.Surface));
	if (err != VK_SUCCESS) {
		ee("failed to create window surface\n");
		return false;
	}

	VkBool32 res;
	vkGetPhysicalDeviceSurfaceSupportKHR(ctx_.vk_gpu,
	 ctx_.vk_queue_family, ctx_.gui_win.Surface, &res);
	if (res != VK_TRUE) {
		ee("WSI is not supported\n");
		return false;
	}

	return true;
}

static void cleanup_vulkan(void)
{
	vkDestroyDescriptorPool(ctx_.vk_dev, ctx_.vk_descriptor_pool,
	 ctx_.vk_alloctor);
	vkDestroyDevice(ctx_.vk_dev, ctx_.vk_alloctor);
	vkDestroyInstance(ctx_.vk_instance, ctx_.vk_alloctor);
}

static void render_frame(ImDrawData *draw_data)
{
	VkResult err;
	ImGui_ImplVulkanH_Window *win = &ctx_.gui_win;
	uint8_t sem_idx = uint8_t(win->SemaphoreIndex);

	VkSemaphore img_sem;
	img_sem = win->FrameSemaphores[sem_idx].ImageAcquiredSemaphore;
	VkSemaphore rend_sem;
	rend_sem = win->FrameSemaphores[sem_idx].RenderCompleteSemaphore;

	vk_call(err, vkAcquireNextImageKHR(ctx_.vk_dev, win->Swapchain,
	 UINT64_MAX, img_sem, VK_NULL_HANDLE, &win->FrameIndex));
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
		ctx_.rebuild_swapchain = true;
		return;
	} else if (err != VK_SUCCESS) {
		return;
	}

	ImGui_ImplVulkanH_Frame *fd = &win->Frames[win->FrameIndex];

	/* blocking call */
	vk_call(err, vkWaitForFences(ctx_.vk_dev, 1, &fd->Fence, VK_TRUE,
	 UINT64_MAX));
	if (err != VK_SUCCESS)
		return;

	vk_call(err, vkResetFences(ctx_.vk_dev, 1, &fd->Fence));
	if (err != VK_SUCCESS)
		return;

	vk_call(err, vkResetCommandPool(ctx_.vk_dev, fd->CommandPool, 0));
	if (err != VK_SUCCESS)
		return;

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vk_call(err, vkBeginCommandBuffer(fd->CommandBuffer, &begin_info));
	if (err != VK_SUCCESS)
		return;

	VkRenderPassBeginInfo pass_info = {};
	pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	pass_info.renderPass = win->RenderPass;
	pass_info.framebuffer = fd->Framebuffer;
	pass_info.renderArea.extent.width = win->Width;
	pass_info.renderArea.extent.height = win->Height;
	pass_info.clearValueCount = 1;
	pass_info.pClearValues = &win->ClearValue;

	vkCmdBeginRenderPass(fd->CommandBuffer, &pass_info,
	 VK_SUBPASS_CONTENTS_INLINE);

	ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);
	vkCmdEndRenderPass(fd->CommandBuffer);
	vk_call(err, vkEndCommandBuffer(fd->CommandBuffer));
	if (err != VK_SUCCESS)
		return;

	VkPipelineStageFlags wait_stage;
	wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo queue_info = {};
	queue_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	queue_info.waitSemaphoreCount = 1;
	queue_info.pWaitSemaphores = &img_sem;
	queue_info.pWaitDstStageMask = &wait_stage;
	queue_info.commandBufferCount = 1;
	queue_info.pCommandBuffers = &fd->CommandBuffer;
	queue_info.signalSemaphoreCount = 1;
	queue_info.pSignalSemaphores = &rend_sem;

	vk_call(err, vkQueueSubmit(ctx_.vk_queue, 1, &queue_info, fd->Fence));
	if (err != VK_SUCCESS)
		return;
}

static void present_gui(void)
{
	VkResult err;

	if (ctx_.rebuild_swapchain)
		return;

	ImGui_ImplVulkanH_Window *win = &ctx_.gui_win;
	uint8_t i = uint8_t(win->SemaphoreIndex);
	VkSemaphore sem = win->FrameSemaphores[i].RenderCompleteSemaphore;

	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &sem;
	info.swapchainCount = 1;
	info.pSwapchains = &win->Swapchain;
	info.pImageIndices = &win->FrameIndex;

	err = vkQueuePresentKHR(ctx_.vk_queue, &info);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
		ctx_.rebuild_swapchain = true;
		return;
	} else if (err != VK_SUCCESS) {
		ee("queue present failed, %s\n", vk_strerror(err));
		return;
	}

	/* use the next set of semaphores */
	win->SemaphoreIndex += 1;
	win->SemaphoreIndex %= win->ImageCount;
}

static void vk_result_cb(VkResult err)
{
	if (err != VK_SUCCESS) {
		fprintf(stderr, "(ee) %s\n", vk_strerror(err));
	}
}

static bool init_vulkan_font(void)
{
	VkResult err;
	ImGui_ImplVulkanH_Window *win = &ctx_.gui_win;

	VkCommandPool cmdpool = win->Frames[win->FrameIndex].CommandPool;
	VkCommandBuffer cmdbuf = win->Frames[win->FrameIndex].CommandBuffer;

	vk_call(err, vkResetCommandPool(ctx_.vk_dev, cmdpool, 0));
	if (err != VK_SUCCESS)
		return false;

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vk_call(err, vkBeginCommandBuffer(cmdbuf, &begin_info));
	if (err != VK_SUCCESS)
		return false;

	ImGui_ImplVulkan_CreateFontsTexture(cmdbuf);

	VkSubmitInfo end_info = {};
	end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	end_info.commandBufferCount = 1;
	end_info.pCommandBuffers = &cmdbuf;

	vk_call(err, vkEndCommandBuffer(cmdbuf));
	if (err != VK_SUCCESS)
		return false;

	vk_call(err, vkQueueSubmit(ctx_.vk_queue, 1, &end_info,
	 VK_NULL_HANDLE));
	if (err != VK_SUCCESS)
		return false;

	vk_call(err, vkDeviceWaitIdle(ctx_.vk_dev));
	if (err != VK_SUCCESS)
		return false;

	ImGui_ImplVulkan_DestroyFontUploadObjects();
	return true;
}

static bool init_gui_backend(void)
{
	ImGui_ImplVulkanH_Window *win = &ctx_.gui_win;
	VkSurfaceFormatKHR format;
	const VkFormat formats[] = {
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_R8G8B8_UNORM,
	};
	const VkColorSpaceKHR color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };

	win->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(ctx_.vk_gpu,
	 win->Surface, formats, ARRAY_SIZE(formats), color_space);
	win->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(ctx_.vk_gpu,
	 win->Surface, &present_modes[0], ARRAY_SIZE(present_modes));

	ImGui_ImplVulkanH_CreateOrResizeWindow(ctx_.vk_instance, ctx_.vk_gpu,
	 ctx_.vk_dev, win, ctx_.vk_queue_family, ctx_.vk_alloctor,
	 width_, height_, ctx_.image_count);

	ImGui_ImplGlfw_InitForVulkan(win_, true);

	ImGui_ImplVulkan_InitInfo info = {};
	info.Instance = ctx_.vk_instance;
	info.PhysicalDevice = ctx_.vk_gpu;
	info.Device = ctx_.vk_dev;
	info.QueueFamily = ctx_.vk_queue_family;
	info.Queue = ctx_.vk_queue;
	info.PipelineCache = ctx_.vk_pipeline_cache;
	info.DescriptorPool = ctx_.vk_descriptor_pool;
	info.Allocator = ctx_.vk_alloctor;
	info.MinImageCount = ctx_.image_count;
	info.ImageCount = win->ImageCount;
	info.CheckVkResultFn = vk_result_cb;

	ImGui_ImplVulkan_Init(&info, win->RenderPass);

	return init_vulkan_font();
}

static void resize_window(void)
{
	ImGui_ImplVulkan_SetMinImageCount(ctx_.image_count);
	ImGui_ImplVulkanH_CreateOrResizeWindow(ctx_.vk_instance, ctx_.vk_gpu,
	 ctx_.vk_dev, &ctx_.gui_win, ctx_.vk_queue_family, ctx_.vk_alloctor,
	 width_, height_, ctx_.image_count);

	ctx_.gui_win.FrameIndex = 0;
	ctx_.rebuild_swapchain = false;
}

static void clear_window(void)
{
	ImGui_ImplVulkanH_Window *win = &ctx_.gui_win;
	ImVec4 clear_color = ImVec4(.1, .1, .1, 1.);

	win->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
	win->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
	win->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
	win->ClearValue.color.float32[3] = clear_color.w;
}

static void render_gui(void)
{
	if (ctx_.rebuild_swapchain) {
		glfwGetFramebufferSize(win_, &ctx_.gui_win.Width,
		 &ctx_.gui_win.Height);

		if (ctx_.gui_win.Width > 0 && ctx_.gui_win.Height > 0)
			resize_window();
	}

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if (gui_demo_) {
		ImGui::ShowDemoWindow(&gui_demo_);
	} else if (plot_demo_) {
		ImPlot::ShowDemoWindow(&plot_demo_);
	} else {
#ifdef FTRACE_PLOTTER_H_
		plot(ctx_.gui_win.Width, ctx_.gui_win.Height);
#endif
	}

	ImGui::Render();
	ImDrawData* draw_data = ImGui::GetDrawData();
	if (!is_minimized(draw_data)) {
		clear_window();
		render_frame(draw_data);
		present_gui();
	}
}

static void cleanup_gui(void)
{
	VkResult err;
	vk_call(err, vkDeviceWaitIdle(ctx_.vk_dev));

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();
	ImGui_ImplVulkanH_DestroyWindow(ctx_.vk_instance, ctx_.vk_dev,
	 &ctx_.gui_win, ctx_.vk_alloctor);

	cleanup_vulkan();
}
