#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "implot.h"
#include "implot_internal.h"

#define LOG_TAG "ftrace"
#include "utils.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef USE_VULKAN
#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan.h>
#else
#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <imgui_impl_opengl3.h>
#endif

#include <GLFW/glfw3.h>

#ifndef APP_NAME
#define APP_NAME "Ftrace viewer"
#endif

constexpr uint8_t font_size_ = 28;
static int width_ = 640;
static int height_ = 640;
static bool gui_demo_;
static bool plot_demo_;
static GLFWwindow *win_;

#include "ftrace-plotter.h"

#ifdef GLFW_INCLUDE_VULKAN
#include "vulkan.h"
#else
#include "opengl.h"
#endif

static bool init_window(const char *filename)
{
	const char *geom = getenv("WIN_SIZE");

	if (geom) {
		const char *h_str;
		width_ = atoi(geom);
		if (!(h_str = strchr(geom, 'x'))) {
			ww("malformed WIN_SIZE, use default height\n");
		} else {
			height_ = atoi(h_str + 1);
		}
	}

	size_t len = strlen(filename) + sizeof(APP_NAME) + 2; /* 2 --> ': ' */
	char *title = (char *) calloc(1, len);
	if (!title) {
		ee("failed to allocate %zu bytes\n", len);
		return false;
	}
	snprintf(title, len, APP_NAME ": %s", filename);

#ifdef GLFW_INCLUDE_VULKAN
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#else
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

	win_ = glfwCreateWindow(width_, height_, title, NULL, NULL);
	free(title);

	if (!win_) {
		ee("failed to create window\n");
		return false;
	}

#ifdef GLFW_INCLUDE_VULKAN
	if (!glfwVulkanSupported()) {
		ee("GLFW does not support vulkan\n");
		return false;
	}
#else
	glfwMakeContextCurrent(win_);
	gladLoadGL(glfwGetProcAddress);
	glfwSwapInterval(1);
	glClearColor(1, 0, 0, 1);
#endif

	return true;
}

static void glfw_error_cb(int err, const char *str)
{
    fprintf(stderr, "(ee) GLFW error %d: %s\n", err, str);
}

static void glfw_key_cb(GLFWwindow *win, int key, int code, int act, int mods)
{
        if (key == GLFW_KEY_ESCAPE && act == GLFW_PRESS)
                glfwSetWindowShouldClose(win, GLFW_TRUE);
}

static void init_gui_style(void)
{
#ifdef CLASSIC_UI
	ImGui::StyleColorsClassic();
#else
	ImGui::StyleColorsDark();
#endif

	ImGui::GetStyle().WindowTitleAlign = ImVec2(.5, .5);
	ImGui::GetStyle().WindowPadding = ImVec2(15, 15);
	ImGui::GetStyle().WindowBorderSize = 8.;
	ImGui::GetStyle().WindowRounding = 20.;

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.6, .6, .6, 1));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.08, .08, .08, 1));
	ImPlot::PushStyleColor(ImPlotCol_PlotBorder, ImVec4(.0, .0, .0, 1));
	ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImVec4(0, 0, 0, 1));
}

static bool init_surface(void)
{
	if (!init_engine_surface())
		return false;

	glfwGetFramebufferSize(win_, &width_, &height_);
	ii("framebuffer size (%d %d)\n", width_, height_);
	return true;
}

static bool init_gui(void)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();

	ImGuiIO &io = ImGui::GetIO();

#ifdef ENABLE_KEYBOARD
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#endif
#ifdef ENABLE_GAMEPAD
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
#endif

	ImFontConfig font_cfg;
	font_cfg.SizePixels = font_size_;
	io.Fonts->AddFontDefault(&font_cfg);

	return init_gui_backend();
}

static bool init(const char *path)
{
	glfwSetErrorCallback(glfw_error_cb);

	if (!glfwInit())
		return false;
	else if (!init_window(path))
		return false;
	else if (!init_surface())
		return false;
	else if (!init_gui())
		return false;

	glfwSetKeyCallback(win_, glfw_key_cb);

	if (getenv("GUI_DEMO"))
		gui_demo_ = true;
	else if (getenv("PLOT_DEMO"))
		plot_demo_ = true;
	else if (!init_plot(path))
		return false;

	init_gui_style();
	return true;
}

static void clean(void)
{
	cleanup_gui();
	glfwDestroyWindow(win_);
	glfwTerminate();
}

int main(int argc, const char *argv[])
{
	if (!argv[1]) {
		printf("Usage: %s <tracelog>\n", argv[0]);
		return 1;
	} else if (!init(argv[1])) {
		return 1;
	}

	while (!glfwWindowShouldClose(win_)) {
		render_gui();
#if 0
		glfwPollEvents();
#else
		glfwWaitEvents();
#endif
	}

	clean();
	return 0;
}
