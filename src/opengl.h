static void render_gui(void)
{
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

	if (gui_demo_) {
		ImGui::ShowDemoWindow(&gui_demo_);
	} else if (plot_demo_) {
		ImPlot::ShowDemoWindow(&plot_demo_);
	} else {
#ifdef FTRACE_PLOTTER_H_
		plot(width_, height_);
#endif
	}

        ImGui::Render();
	glfwGetFramebufferSize(win_, &width_, &height_);
	glViewport(0, 0, width_, height_);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	glfwSwapBuffers(win_);
}

static void cleanup_gui(void)
{
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
	ImPlot::DestroyContext();
        ImGui::DestroyContext();
}

static bool init_gui_backend(void)
{
	ImGui_ImplGlfw_InitForOpenGL(win_, true);
	ImGui_ImplOpenGL3_Init("#version 150");
	return true;
}

static bool init_engine_surface(void)
{
	return true;
}
