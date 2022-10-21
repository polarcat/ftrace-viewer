#ifndef FTRACE_PLOTTER_H_
#define FTRACE_PLOTTER_H_

#include <sys/mman.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vector>
#include <string>

constexpr uint16_t max_buf_ = 4096;
constexpr int mmap_proto_ = PROT_READ | PROT_WRITE;
constexpr double y_scale_ = .001;
constexpr float fill_alpha_ = .2;
constexpr float y_low_ = .05;
constexpr float y_high_ = .4;
constexpr float y_tag_ = .9;

static ImPlotAxisFlags x_flags_ = ImPlotAxisFlags_NoMenus |
 ImPlotAxisFlags_NoInitialFit | ImPlotAxisFlags_NoGridLines |
 ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_AutoFit;

constexpr ImPlotAxisFlags y_flags_ = ImPlotAxisFlags_NoGridLines |
 ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoMenus | ImPlotAxisFlags_Lock;
constexpr ImPlotFlags plot_flags_ = ImPlotFlags_NoMenus | ImPlotFlags_NoFrame |
 ImPlotFlags_NoTitle;
constexpr ImGuiTableFlags table_flags1_ = ImGuiTableFlags_SizingFixedFit;
constexpr ImGuiTableFlags table_flags2_ = ImGuiTableFlags_SizingStretchProp |
 ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH |
 ImGuiTableFlags_Resizable;
constexpr ImGuiWindowFlags win_flags_ = ImGuiWindowFlags_NoBackground |
 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
 ImGuiWindowFlags_NoResize;

constexpr uint32_t dot_color_ = IM_COL32(255, 255, 255, 200);
constexpr uint32_t text_color_ = IM_COL32(200, 200, 200, 255);
constexpr uint32_t cursor_color_ = IM_COL32(40, 40, 40, 255);

struct gpu_job {
	char *name = nullptr;
	int32_t id = -1;
	uint32_t pid = 0;
	double offset_sec = -1; /* GPU clock offset */
	double start_ts = -1;
	double stop_ts = -1;
	double runtime_ms = -1;
};

struct point {
	double x = -1;
	double y = 0;
	uint16_t cpu;
	int32_t pid;
	char state;
	bool arrived;
	bool visible = false;
	double xx = -1; /* GPU job stop timestamp */
	ImU32 color; /* for GPU jobs */
};

struct y_axis {
	uint32_t pid = 0;
	double max_y = 0;
	std::string name;
	char list_name[32] = {0};
	std::vector<struct point> points;
	std::vector<double> markers;
	std::vector<struct marker_label> marker_labels;
	bool selected = false;
	bool added = false;
	bool monitor = false;
	bool gpu = false;
	bool measure = false;
	float prev_ex = -1;
	ImVec4 color;
	float x_offset = 0;
	float y_offset = 0;
};

struct marker_label {
	const char *name;
	double ts;
	bool visible = false;
};

struct plot_data {
	const char *comm = nullptr;
	char *marker = nullptr;
	bool monitor = false;
	bool gpu = false;
	uint32_t pid;
	double ts;
	double raw_ts;
	bool arrived;
	uint8_t cpu;
	uint64_t pcount;
	uint8_t id;
};

struct plot {
	int fd = -1;
	const char *filename;
	char *data;
	size_t file_size;
	double max_x;
	std::vector<struct plot_data> plot_data;
	std::vector<struct y_axis> y_axes;
	uint16_t id = 0;
	uint16_t gpu_plot_id = 0;
	double min_ts = 0;
	bool show_all_labels = false;
	bool enable_marker_info = true;
	bool enable_procinfo = true;
	bool link_axes = true;
	float ex;
	float ey;
	bool event = false;
	bool reset_measure = false;
	bool reset_labels = false;
};

static struct plot plot_;

uint32_t base_colors_[] = {
	/* colors */
	0x0000ee,
	0x00ee00,
	0xee0000,
	0x00eeee,
	0xeeee00,
	0xee00ee,
	/* multipliers */
	0xbbccee,
	0xbbeecc,
	0xccbbee,
	0xcceebb,
	0xeebbcc,
	0xeeccbb,
};

constexpr uint8_t color_inc_ = 10;
constexpr uint8_t color_dec_ = 20;

/* produce ordered colors */
static ImVec4 generate_color(struct y_axis *axis, uint16_t id)
{
	uint32_t color = base_colors_[id % 11];
	uint8_t r = color >> 16 & 0xff;
	uint8_t g = color >> 8 & 0xff;
	uint8_t b = color & 0xff;
	uint8_t inc = id * color_inc_;
	uint8_t dec = id * color_dec_;

	id %= 5;
	if (id == 0) {
		r += inc;
		g += inc;
		b -= dec;
	} else if (id == 1) {
		r += inc;
		g -= dec;
		b += inc;
	} else if (id == 2) {
		r -= dec;
		g += inc;
		b += inc;
	} else if (id == 3) {
		r += inc;
		g -= dec;
		b -= dec;
	} else if (id == 4) {
		r -= dec;
		g -= dec;
		b += inc;
	} else if (id == 5) {
		r -= dec;
		g += inc;
		b -= dec;
	} else if (id == 6) {
		r -= dec;
	} else if (id == 7) {
		g -= dec;
	} else {
		r -= dec;
		g += inc;
		b -= dec;
	}

	color = 0xff000000 | (r << 16) | (g << 8) | b;
	return ImGui::ColorConvertU32ToFloat4(color);
}

static char *skip_blanks(char *ptr, char *end)
{
	while (ptr < end) {
		if (isblank(*ptr))
			ptr++;
		else
			break;
	}

	return ptr;
}

static inline bool is_eol(char *ptr)
{
	return (*ptr == '\n' || *ptr == '\r');
}

static inline bool is_monitor(char *ptr)
{
	return (*ptr == 'm' && *(ptr + 1) == 'o' && *(ptr + 2) == 'n' &&
	 *(ptr + 3) == ',');
}

static inline bool is_gpu(char *ptr)
{
	return (*ptr == 'g' && *(ptr + 1) == 'p' && *(ptr + 2) == 'u' &&
	 *(ptr + 3) == ',');
}

static inline char *get_field_end(char *ptr, char *end)
{
	while (ptr < end) {
		if (!isspace(*ptr))
			ptr++;
		else
			break;
	}

	return ptr;
}

static bool parse_task_pid(char *ptr, char *end, char **task, uint32_t *pid)
{
	char *tmp = end - 1;
	*task = ptr;

	while (tmp > ptr) {
		if (isdigit(*tmp)) {
			if (*(tmp - 1) == '-') {
				*(tmp - 1) = '\0';
				*pid = atoi(tmp);
				return true;
			}
		}
		tmp--;
	}

	return false;
}

static bool open_data(const char *path)
{
	if (!path) {
		return false;
	} else if ((plot_.fd = open(path, O_RDONLY)) < 0) {
		ee("failed to open '%s'\n", path);
		return false;
	} else if ((plot_.file_size = lseek(plot_.fd, 0, SEEK_END)) < 1) {
		ee("failed to get size of '%s'\n", path);
		return false;
	}

	lseek(plot_.fd, 0, SEEK_SET); // restore cursor, ignore errors
	errno = 0;
	plot_.data = (char *) mmap(nullptr, plot_.file_size, mmap_proto_,
	 MAP_PRIVATE, plot_.fd, 0);

	if (plot_.data == MAP_FAILED) {
		ee("failed to map '%s' fd=%d\n", path, plot_.fd);
		close(plot_.fd);
		plot_.fd = -1;
		return false;
	}

	return true;
}

static char *get_marker_field(char *ptr, char *end, char **next)
{
	if (!(ptr = skip_blanks(ptr, end))) {
		ee("unexpected end of data\n");
		return nullptr;
	}

	*next = ptr;
	while (*next < end) {
		if (is_eol(*next))
			break;
		else
			(*next)++;
	}

	**next = '\0';
	return ptr;
}

static char *get_data_field(char *ptr, char *end, char **next)
{
	if (!(ptr = skip_blanks(ptr, end))) {
		ee("unexpected end of data\n");
		return nullptr;
	}

	*next = get_field_end(ptr, end);
	**next = '\0';
	return ptr;
}

static ImU32 make_job_color(struct y_axis *axis, struct gpu_job *job)
{
	for (size_t i = 0; i < plot_.y_axes.size(); ++i) {
		struct y_axis *axis = &plot_.y_axes[i];
		if (axis->pid == job->pid) {
			return ImGui::ColorConvertFloat4ToU32(axis->color);
		}
	}

	/* fallback if PID is not provided */
	std::string str = job->name;
	str += std::to_string(job->id);
	return std::hash<std::string>{}(str);
}

static void parse_gpu_job(struct plot_data *data, struct gpu_job *job)
{
	/* data format:
	 * <tag>,<offset_sec>,<job_id>,<start_ns>,<stop_ns>,<job_runtime_ms>,<pid>
	 */
	char *ptr = data->marker;
	double offset_sec = -1;

	while (*ptr != '\0') {
		char *tmp;
		if (!(tmp = strchr(ptr, ','))) { /* last item */
			job->pid = atoi(ptr);
			break;
		}

		if (!job->name) {
			job->name = ptr;
			*tmp = '\0';
		} else if (offset_sec < 0) {
			offset_sec = atof(ptr);
		} else if (job->id < 0) {
			job->id = atoi(ptr);
		} else if (job->start_ts < 0) {
			job->start_ts = atof(ptr) / 1e9; /* ns to seconds */
		} else if (job->stop_ts < 0) {
			job->stop_ts = atof(ptr) / 1e9; /* ns to seconds */
		} else if (job->runtime_ms < 0) {
			job->runtime_ms = atof(ptr);
		}

		ptr = tmp + 1;
	}

	job->start_ts -= (plot_.min_ts + offset_sec);
	job->stop_ts -= (plot_.min_ts + offset_sec);

#if 0
	printf("gpu: offset %f, id %d start %f stop %f run %f pid %d\n",
	 offset_sec, job->id, job->start_ts, job->stop_ts,
	 job->runtime_ms, job->pid);
#endif
}

static inline void set_axis_name(struct y_axis *axis, const char *comm)
{
	axis->name = " ";
	axis->name += comm;
	axis->name += " ";
	axis->name += std::to_string(axis->pid);
}

static void update_y_axis(const char *comm, struct plot_data *data)
{
	for (size_t i = 0; i < plot_.y_axes.size(); ++i) {
		if (data->gpu && plot_.y_axes[i].gpu) {
			data->id = i;
			return;
		} else if (plot_.y_axes[i].pid == data->pid && !data->gpu) {
			data->id = i;
			/* also update name so it matches actual process;
			 * otherwise, if process is invoked by shell script
			 * the script name will be displayed
			 */
			set_axis_name(&plot_.y_axes[i], comm);
			return;
		}
	}

	data->id = plot_.id;
	plot_.id++;

	struct y_axis axis;

	if (!data->gpu) {
		axis.pid = data->pid;
		set_axis_name(&axis, comm);
		axis.color = generate_color(&axis, data->id);
	} else {
		plot_.gpu_plot_id = data->id;
		axis.name = " GPU jobs";
		axis.color.x = .25;
		axis.color.y = .25;
		axis.color.z = .25;
		axis.color.w = 1;
	}

	axis.gpu = data->gpu;
	plot_.y_axes.push_back(std::move(axis));
}

static void add_data_point(struct plot_data *data)
{
	struct y_axis *axis = &plot_.y_axes[data->id];
	struct point point;
	struct gpu_job job;

	point.cpu = data->cpu;
	point.x = data->ts;

	if (data->monitor && !axis->monitor)
		axis->monitor = data->monitor;

	if (data->gpu && !axis->gpu)
		axis->gpu = data->gpu;

	if (axis->gpu) {
		parse_gpu_job(data, &job);
		point.color = make_job_color(axis, &job);
		point.x = job.start_ts;
		point.xx = job.stop_ts;
		point.cpu = job.id; /* NB: use cpu field */
		point.pid = job.pid;
		point.arrived = true;
	} else if (!axis->monitor) {
		(data->arrived) ? (point.arrived = true) : (point.arrived = false);
	} else if (data->marker) {
		point.arrived = true;
		point.y = double(atol(data->marker + 4));
		if (axis->max_y < point.y)
			axis->max_y = point.y;
	} else {
		return;
	}

	axis->points.push_back(std::move(point));
}

static void update_y_markers(struct plot_data *data, uint32_t pid)
{
	for (auto &axis : plot_.y_axes) {
		/* ignore markers' leftovers from incomplete log */
		if (axis.pid == pid && axis.points.size()) {
			axis.markers.push_back(data->ts);

			struct marker_label l;
			l.name = data->marker;
			l.ts = data->ts;
			axis.marker_labels.push_back(std::move(l));
			break;
		}
	}
}

static void add_items(struct y_axis *axis, std::vector<struct y_axis> *axes)
{
	for (size_t i = 0; i < plot_.y_axes.size(); ++i) {
		if (plot_.y_axes[i].name == axis->name) {
			char prefix;
			if (plot_.y_axes[i].markers.size())
				prefix = '*';
			else if (plot_.y_axes[i].monitor)
				prefix = '=';
			else if (plot_.y_axes[i].gpu)
				prefix = '#';
			else
				prefix = ' ';

			snprintf(plot_.y_axes[i].list_name,
			 sizeof(plot_.y_axes[i].list_name), "%c%s", prefix,
			 plot_.y_axes[i].name.c_str());

#if 0
			printf("axis: %s | color %#x\n", axis->name.c_str(),
			 ImGui::ColorConvertFloat4ToU32(axis->color));
#endif

			plot_.y_axes[i].added = true;
			axes->push_back(std::move(plot_.y_axes[i]));
		}
	}

}

static inline void sort_y_axes(void)
{
	std::vector<struct y_axis> axes;

	for (size_t i = 0; i < plot_.y_axes.size(); ++i) {
		if (!plot_.y_axes[i].added)
			add_items(&plot_.y_axes[i], &axes);
	}

	plot_.y_axes.clear();
	plot_.y_axes = std::move(axes);
}

static char *init_marker_data(struct plot_data *data, char *ptr, char *end)
{
	char *next;

	/* get marker string */
	if (!(ptr = get_marker_field(ptr, end, &next)))
		return nullptr;

	data->marker = ptr;
	ptr = next + 1;
	return ptr;
}

static char *init_info_data(struct plot_data *data, char *ptr, char *end)
{
	char *next;

	/* get timestamp */
	if (!(ptr = get_data_field(ptr, end, &next)))
		return nullptr;

	data->raw_ts = atoll(ptr);
	data->ts = data->raw_ts / 1e9;
	ptr = next + 1;

	/* get pid */
	if (!(ptr = get_data_field(ptr, end, &next)))
		return nullptr;

	data->pid = atoi(ptr);
	ptr = next + 1;

	/* get task on cpu arrival flag */
	if (!(ptr = get_data_field(ptr, end, &next)))
		return nullptr;

	data->arrived = atoi(ptr);
	ptr = next + 1;

	/* get cpu */
	if (!(ptr = get_data_field(ptr, end, &next)))
		return nullptr;

	data->cpu = atoi(ptr);
	ptr = next + 1;

	/* get pcount */
	if (!(ptr = get_data_field(ptr, end, &next)))
		return nullptr;

	data->pcount = atol(ptr);
	ptr = next + 1;

	/* get task name */
	if (!(ptr = get_data_field(ptr, end, &next)))
		return nullptr;

	data->comm = ptr;
	ptr = next + 1;
	return ptr;
}

static bool init_data(void)
{
	char *ptr = plot_.data;
	char *end = plot_.data + plot_.file_size;
	char *next;
	bool marker;
	double trace_ts;
	uint32_t trace_pid;
	uint32_t trace_cpu;
	char *trace_comm = nullptr;

	while (ptr < end) {
		if (*ptr == '#') {
			ptr = strchr(ptr, '\n');
			ptr++;
			continue;
		}

		/* skip some unused fields set by ftrace itself */

		/* skip task-pid */
		if (!(ptr = get_data_field(ptr, end, &next)))
			return false;

		if (!parse_task_pid(ptr, next, &trace_comm, &trace_pid)) {
			ee("malformed 'task-pid' field: '%s'\n", ptr);
			return false;
		}

		ptr = next + 1;

		if (!(ptr = get_data_field(ptr, end, &next)))
			return false;

		trace_cpu = atoi(ptr + 1); /* skip leading '[' */
		ptr = next + 1;

		/* skip flags */
		if (!(ptr = get_data_field(ptr, end, &next)))
			return false;

		ptr = next + 1;

		/* handle timestamp */
		if (!(ptr = get_data_field(ptr, end, &next)))
			return false;

		trace_ts = atof(ptr);
		ptr = next + 1;

		/* handle function */
		if (!(ptr = get_data_field(ptr, end, &next)))
			return false;

		if (strcmp("tracing_mark_write:", ptr) == 0)
			marker = true;
		else
			marker = false;

		ptr = next + 1;

		if (!plot_.min_ts)
			plot_.min_ts = trace_ts;

		/* start task info fields */

		struct plot_data data;

		if (marker) {
			data.comm = trace_comm;
			data.pid = trace_pid;

			if (!(ptr = init_marker_data(&data, ptr, end))) {
				return false;
			} else if (data.marker) {
				data.monitor = is_monitor(data.marker);
				data.gpu = is_gpu(data.marker);

				if (data.gpu)
					data.comm = "gpu";
			}
		} else {
			if (!(ptr = init_info_data(&data, ptr, end)))
				return false;
		}
		data.ts = trace_ts - plot_.min_ts;
		update_y_axis(data.comm, &data);

		if (marker && !data.monitor && !data.gpu) {
			update_y_markers(&data, trace_pid);
#if 0
		printf("[%u] %f %u %u %u '%s' | '%s' | %f\n", data.id, data.ts,
		 data.pid, data.arrived, data.cpu, data.comm, data.marker,
		 data.raw_ts);
#endif
		} else {
#if 0
		printf("[%u] %f %u %u %u '%s' | '%s' | %f\n", data.id, data.ts,
		 data.pid, data.arrived, data.cpu, data.comm, data.marker,
		 data.raw_ts);
#endif
			add_data_point(&data);
			plot_.plot_data.push_back(std::move(data));
		}

		ptr++;
	}

	plot_.max_x = plot_.plot_data.back().ts - plot_.plot_data.front().ts;
	printf("max seconds: %f max id: %u\n", plot_.max_x, plot_.id);
	ii("total data points: %zu\n", plot_.plot_data.size());
	sort_y_axes();
	return true;
}

static bool init_plot(const char *path)
{
	if (!open_data(path))
		return false;
	else if (!init_data())
		return false;

	plot_.filename = path;
	return true;
}

static ImVec2 get_marker_offset(struct y_axis *axis, size_t i)
{
	uint8_t idx = i % 4;
	if (idx == 0) {
		axis->x_offset = -10;
		axis->y_offset = -20;
	} else if (idx == 1) {
		axis->x_offset = -10;
		axis->y_offset = 20;
	} else if (idx == 2) {
		axis->x_offset = 10;
		axis->y_offset = -20;
	} else { // idx == 3
		axis->x_offset = 10;
		axis->y_offset = 20;
	}

	return ImVec2(axis->x_offset, axis->y_offset);
}

static void show_marker_label(struct y_axis *axis, double y, size_t i)
{
	struct marker_label *l = &axis->marker_labels[i];
	ImVec2 offset = get_marker_offset(axis, i);

	double ts_diff;
	if (i == 0)
		ts_diff = 0;
	else
		ts_diff = l->ts - axis->marker_labels[i - 1].ts;

	ImVec4 bg = ImVec4(axis->color.x * .4, axis->color.y * .4,
	 axis->color.z * .4, 0);
	ImPlot::Annotation(l->ts, y , bg, offset, false,
	 " %s \n time %f \n diff %f\n", l->name, l->ts, ts_diff);
}

static inline bool is_clicked(double x, double y)
{
	ImVec2 pt = ImPlot::PlotToPixels(ImPlotPoint(x, y));

	return (plot_.event &&
	 abs(plot_.ex - pt.x) <= 4 && abs(plot_.ey - pt.y) <= 4);
}

static void handle_events(void)
{
	ImVec2 pt = ImGui::GetMousePos();
	plot_.ex = pt.x;
	plot_.ey = pt.y;

	if (ImPlot::IsPlotHovered()) {
		ImGui::SetMouseCursor(7);
		if (ImGui::IsMouseClicked(0)) {
			plot_.event = true;
			plot_.reset_measure = false;
		} else if (ImGui::IsMouseClicked(1)) {
			plot_.reset_measure = true;
		} else if (ImGui::IsMouseDown(0)) {
			plot_.reset_measure = true;
		}

		ImGuiIO &io = ImGui::GetIO();
		if (io.MouseWheel != 0 || io.MouseWheelH != 0)
			plot_.reset_measure = true;
	}
}

static inline void plot_dot(double x, double y)
{
	ImVec2 c = ImPlot::PlotToPixels(ImPlotPoint(x, y));
	ImPlot::GetPlotDrawList()->AddCircleFilled(c, 5, dot_color_, 8);
}

static inline void show_markers(struct y_axis *axis)
{
        ImPlot::PushPlotClipRect();
	for (size_t i = 0; i < axis->markers.size(); ++i) {
		double x = axis->markers[i];
		double y = y_high_;

		plot_dot(x, y);

		if (plot_.show_all_labels && plot_.enable_marker_info) {
			show_marker_label(axis, y, i);
		} else if (plot_.enable_marker_info) {
			if (is_clicked(x, y)) {
				if (axis->marker_labels[i].visible)
					axis->marker_labels[i].visible = false;
				else
					axis->marker_labels[i].visible = true;
			}

			if (plot_.reset_labels)
				axis->marker_labels[i].visible = false;
			else if (axis->marker_labels[i].visible)
				show_marker_label(axis, y, i);
		}
	}
        ImPlot::PopPlotClipRect();
}

static inline void show_process_label(struct y_axis *axis, size_t i)
{
	ImVec4 col = ImPlot::GetLastItemColor();
	ImVec2 offset = ImVec2(15, -15);
	double diff;
	double x = -1;

	/* find previous rise */
	diff = -1;
	for (ssize_t n = i - 1; n > 0; --n) {
		if (axis->points[n].x >= 0) {
			diff = axis->points[i].x - axis->points[n].x;
			x = axis->points[n].x;
			break;
		}
	}

	ImVec4 bg = ImVec4(0, 0, 0, 0);
	ImPlot::Annotation(axis->points[i].x, y_low_, bg, offset,
	 false, " ts %f \n rt %f \n cpu %u state '%c' ", axis->points[i].x,
	 diff, axis->points[i].cpu, axis->points[i].state);
}

static void plot_cursor(struct y_axis *axis, double x, bool locked)
{
	double y;

	(axis->gpu) ? (y = 0) : (y = y_low_);

	ImPlotPoint pt = ImPlot::PixelsToPlot(x, 0);
	double vx[] = { pt.x, pt.x };
	double vy[] = { y, 1 };
	const char *name = axis->name.c_str();

	ImPlot::PushStyleColor(ImPlotCol_Line, cursor_color_);
	ImPlot::PlotLine(name, vx, vy, ARRAY_SIZE(vx));
	ImPlot::PopStyleColor(ImPlotCol_Line);

	ImVec2 offset = ImVec2(-15, -15);
	ImVec4 bg = ImVec4(axis->color.x * .4, axis->color.y * .4,
	 axis->color.z * .4, 1);

	if (plot_.event && !axis->measure) {
		axis->measure = true;
		axis->prev_ex = x;
	} else if (plot_.event && axis->measure) {
		axis->prev_ex = plot_.ex;
	} else if (plot_.reset_measure) {
		axis->measure = false;
	}

	double x_val;
	if (axis->measure && !locked) {
		ImPlotPoint prev = ImPlot::PixelsToPlot(axis->prev_ex, 0);
		ImPlot::Annotation(pt.x, y, bg, offset, false,
		 " %f \n %f ", pt.x, pt.x - prev.x);
		return;
	} else if (locked) {
		ImPlotPoint prev = ImPlot::PixelsToPlot(axis->prev_ex, 0);
		x_val = prev.x;
	} else {
		x_val = pt.x;
	}

	ImPlot::Annotation(pt.x, y_low_, bg, offset, false, " %f ", x_val);
}

static void plot_gpu(struct y_axis *axis, size_t i)
{
	double x[] = {
		axis->points[i].x,
		axis->points[i].x,
		axis->points[i].xx,
		axis->points[i].xx,
	};

	double y[] = {
		0,
		y_high_,
		y_high_,
		0,
	};

	ImPlotPoint pt = ImPlot::PixelsToPlot(plot_.ex, 0);
	double vx[] = { pt.x, pt.x };
	double vy[] = { 0, 1 };
	const char *name = axis->name.c_str();
	ImPlot::PushStyleColor(ImPlotCol_Line, cursor_color_);
	ImPlot::PlotLine(name, vx, vy, ARRAY_SIZE(vx));
	ImPlot::PopStyleColor(ImPlotCol_Line);

	ImPlot::PushStyleColor(ImPlotCol_Line, axis->points[i].color);
	ImPlot::PlotLine(name, x, y, ARRAY_SIZE(x));
	ImPlot::PopStyleColor(ImPlotCol_Line);
	ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, fill_alpha_);
	ImPlot::PlotShaded(name, x, y, ARRAY_SIZE(x), 0, 0);
	ImPlot::PopStyleVar();

	plot_cursor(axis, plot_.ex, false);

	if (axis->measure)
		plot_cursor(axis, axis->prev_ex, true);

	/* process info marker */
	ImPlot::PushPlotClipRect();
	plot_dot(axis->points[i].xx, y_high_);
	ImPlot::PopPlotClipRect();

	if (plot_.enable_procinfo) {
		if (is_clicked(axis->points[i].xx, y_high_)) {
			if (axis->points[i].visible)
				axis->points[i].visible = false;
			else
				axis->points[i].visible = true;
		}

		if (plot_.reset_labels) {
			axis->points[i].visible = false;
		} else if (axis->points[i].visible) {
			ImVec2 offset = get_marker_offset(axis, i);
			ImVec4 bg =
			 ImGui::ColorConvertU32ToFloat4(axis->points[i].color);
			bg.x *= .4;
			bg.y *= .4;
			bg.z *= .4;
			bg.w = 0;
			ImPlot::Annotation(axis->points[i].xx, y_high_, bg,
			 offset, false, " job %u pid %u \n ts %f \n rt %f ",
			 axis->points[i].cpu,
			 axis->points[i].pid,
			 axis->points[i].xx,
			 axis->points[i].xx - axis->points[i].x);
		}
	}
}

static void plot_monitor(struct y_axis *axis, size_t i)
{
	if (i == 0)
		return; /* skip first point */

	double x[] = {
		axis->points[i - 1].x,
		axis->points[i].x,
	};

	double y[] = {
		axis->points[i - 1].y,
		axis->points[i].y,
	};

	const char *name = axis->name.c_str();
	ImPlot::PushStyleColor(ImPlotCol_Line, axis->color);
	ImPlot::PlotLine(name, x, y, ARRAY_SIZE(x));
	ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, fill_alpha_);
	ImPlot::PlotShaded(name, x, y, ARRAY_SIZE(x), -INFINITY, 0);
	ImPlot::PopStyleVar();

	plot_dot(axis->points[i].x, axis->points[i].y);

	if (is_clicked(axis->points[i].x, axis->points[i].y)) {
		if (axis->points[i].visible)
			axis->points[i].visible = false;
		else
			axis->points[i].visible = true;
	}

	if (plot_.reset_labels) {
		axis->points[i].visible = false;
	} else if (axis->points[i].visible) {
		ImVec2 offset = ImVec2(15, -15);
		ImPlot::Annotation(axis->points[i].x, axis->points[i].y,
		 axis->color, offset, false, " %.f ", axis->points[i].y);
	}

	ImPlot::PopStyleColor(ImPlotCol_Line);
}

static size_t plot_axis(struct y_axis *axis, size_t i, double *prev_x)
{
	size_t ii = i;
	/* search for next departure point */
	for (size_t n = i; n < axis->points.size(); ++n) {
		if (axis->points[n].x > axis->points[i].x &&
		 !axis->points[n].arrived) {
			ii = n;
			break;
		}
	}

	if (i == ii)
		return i;

	double x[] = {
		*prev_x,
		axis->points[i].x,
		axis->points[i].x,
		axis->points[ii].x,
		axis->points[ii].x,
	};

	double y[] = {
		y_low_,
		y_low_,
		y_high_,
		y_high_,
		y_low_,
	};

	const char *name = axis->name.c_str();
	ImPlot::PushStyleColor(ImPlotCol_Line, axis->color);
	ImPlot::PlotLine(name, x, y, ARRAY_SIZE(x));
	ImPlot::PopStyleColor(ImPlotCol_Line);
	ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, fill_alpha_);
	ImPlot::PlotShaded(name, x, y, ARRAY_SIZE(x), y_low_, 0);
	ImPlot::PopStyleVar();

	plot_cursor(axis, plot_.ex, false);

	if (axis->measure)
		plot_cursor(axis, axis->prev_ex, true);

	/* process info marker */
	ImPlot::PushPlotClipRect();
	plot_dot(*prev_x, y_low_);
	ImPlot::PopPlotClipRect();

	if (plot_.show_all_labels && plot_.enable_procinfo) {
		show_process_label(axis, ii);
	} else if (plot_.enable_procinfo) {
		if (is_clicked(axis->points[ii].x, y_low_)) {
			if (axis->points[ii].visible)
				axis->points[ii].visible = false;
			else
				axis->points[ii].visible = true;
		}

		if (plot_.reset_labels)
			axis->points[ii].visible = false;
		else if (axis->points[ii].visible)
			show_process_label(axis, ii);
	}

	*prev_x = axis->points[ii].x;
	return ii;
}

static inline void show_plot(struct y_axis *axis)
{
	if (!axis->selected)
		return;
	else if (!ImPlot::BeginPlot("", ImVec2(-1, -1), plot_flags_))
		return;

	ImPlot::SetupAxes("", nullptr, x_flags_, y_flags_);

	if (axis->monitor) {
		ImPlot::SetupAxesLimits2(0, plot_.max_x, 0, axis->max_y * 1.5,
		 ImPlotCond_Once, ImPlotCond_Always);
	} else {
		ImPlot::SetupAxesLimits2(0, plot_.max_x, 0, 1,
		 ImPlotCond_Once, ImPlotCond_Always);
		ImPlot::SetupAxisFormat(ImAxis_Y1, "");
	}

	handle_events(); /* get event's xy */

	double prev_x = 0;
	for (size_t i = 0; i < axis->points.size() - 1; ++i) {
		if (axis->points[i].x < 0)
			continue;
		else if (!axis->points[i].arrived)
			continue;

		if (axis->gpu)
			plot_gpu(axis, i);
		else if (axis->monitor)
			plot_monitor(axis, i);
		else
			i = plot_axis(axis, i, &prev_x);
	}
	show_markers(axis);

	if (axis->gpu) {
		ImPlot::TagY(y_high_, axis->color, " Run  ");
		axis->color.w = .5;
		ImPlot::TagY(y_low_, axis->color, " Idle ");
		axis->color.w = 1;
	} else if (!axis->monitor) {
		ImPlot::TagY(y_high_, axis->color, " Run  ");
		axis->color.w = .5;
		ImPlot::TagY(y_low_, axis->color, " Idle ");
		axis->color.w = 1;
	}

	ImPlot::EndPlot();
}

static inline void show_view(void)
{
	if (!ImGui::BeginTable("controls", 4, table_flags1_, ImVec2( -1, 0)))
		return;

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::Checkbox("Link axes ", &plot_.link_axes);
	ImGui::TableSetColumnIndex(1);
	ImGui::Checkbox("Show all labels ", &plot_.show_all_labels);

	ImGui::TableSetColumnIndex(2);
        if (ImGui::Button(" Reset labels "))
            plot_.reset_labels = true;

	ImGui::TableSetColumnIndex(3);
        ImGui::Text(" File: %s ", plot_.filename);

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::Checkbox("Enable marker info ", &plot_.enable_marker_info);
	ImGui::TableSetColumnIndex(1);
	ImGui::Checkbox("Enable process info ", &plot_.enable_procinfo);

        ImGui::EndTable();

	if (!ImGui::BeginTable("plot", 2, table_flags2_, ImVec2( -1, 0)))
		return;

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);

	uint16_t rows = 0;

	if (ImGui::BeginListBox("", ImVec2(-1, -50))) {
		for (auto &axis : plot_.y_axes) {
			ImGui::Selectable(axis.list_name, &axis.selected);
			rows += axis.selected;
		}

		ImGui::EndListBox();
	}

	ImGui::TableSetColumnIndex(1);

	ImPlotSubplotFlags flags = ImPlotSubplotFlags_LinkRows |
	 ImPlotSubplotFlags_NoMenus;

	if (!rows)
		goto out;

	if (plot_.link_axes)
		flags |= ImPlotSubplotFlags_LinkAllX;

	if (!ImPlot::BeginSubplots("", rows, 1, ImVec2(-1 , -50), flags))
		goto out;

	for (auto &axis : plot_.y_axes) {
		if (!axis.gpu)
			show_plot(&axis);
	}

	show_plot(&plot_.y_axes[plot_.gpu_plot_id]);

	ImPlot::EndSubplots();
out:
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::BulletText("%u processes", plot_.id);
	ImGui::TableSetColumnIndex(1);
	ImGui::Text(" state\t\t\ttimeline in seconds");
        ImGui::EndTable();
}

static void plot(double w, double h)
{
	static bool p_open;
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(w, h), 0);

	if (!ImGui::Begin("Ftrace viewer", &p_open, win_flags_))
		return;

	show_view();
	plot_.event = false;
	plot_.reset_labels = false;
	x_flags_ &= ~ImPlotAxisFlags_AutoFit; /* only need it once */

	ImGui::End();
}

#endif /* FTRACE_PLOTTER_H_ */
