#include "gui_manager.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <algorithm>

GUIManager::GUIManager(SystemData& sys_data) : sysdata(sys_data),
    window_(nullptr), notebook_(nullptr),
    temp_grid_(nullptr), cpu_mem_grid_(nullptr), disk_grid_(nullptr), settings_grid_(nullptr),
    cpu_usage_label_(nullptr), cpu_chart_area_(nullptr),
    mem_total_label_(nullptr), mem_used_label_(nullptr), mem_free_label_(nullptr), mem_usage_label_(nullptr),
    disk_total_label_(nullptr), disk_used_label_(nullptr), disk_free_label_(nullptr), disk_usage_label_(nullptr),
    update_interval_spin_button_(nullptr), timeout_source_id_(0)
{}

void GUIManager::run() {
    GtkApplication* app = gtk_application_new("org.example.systemmonitor", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), this);
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);
}

void GUIManager::activate(GtkApplication* app, gpointer user_data) {
    GUIManager* self = static_cast<GUIManager*>(user_data);
    self->onActivate(app);
}

void GUIManager::onActivate(GtkApplication* app) {
    window_ = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window_), "System Monitor");
    gtk_window_set_default_size(GTK_WINDOW(window_), 550, 600);

    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "label { padding: 3px; }"
        "label.temp-normal { color: green; }"
        "label.temp-warning { color: orange; font-weight: bold; }"
        "label.temp-critical { color: red; font-weight: bold; background-color: #FFDDDD; }"
        "label.title-section { font-weight: bold; font-size: large; color: #333; }"
        , -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                          GTK_STYLE_PROVIDER(provider),
                                          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    notebook_ = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(window_), notebook_);

    temp_grid_ = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(temp_grid_), 5);
    gtk_grid_set_column_spacing(GTK_GRID(temp_grid_), 10);
    gtk_container_set_border_width(GTK_CONTAINER(temp_grid_), 10);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_), temp_grid_, gtk_label_new("Temperatures"));

    int row = 0;
    GtkWidget* temp_section_label = gtk_label_new("<span>Temperature (CPU/GPU/Other)</span>");
    gtk_label_set_use_markup(GTK_LABEL(temp_section_label), TRUE);
    gtk_widget_set_halign(temp_section_label, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(temp_grid_), temp_section_label, 0, row++, 2, 1);

    auto all_temps = sysdata.getAllTemperatures();
    for (const auto& pair : all_temps) {
        GtkWidget* name_label = gtk_label_new(pair.first.c_str());
        gtk_widget_set_halign(name_label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(temp_grid_), name_label, 0, row, 1, 1);

        GtkWidget* temp_label = gtk_label_new("N/A");
        gtk_widget_set_halign(temp_label, GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(temp_grid_), temp_label, 1, row, 1, 1);
        templabels[pair.first] = temp_label;
        row++;
    }

    cpu_mem_grid_ = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(cpu_mem_grid_), 5);
    gtk_grid_set_column_spacing(GTK_GRID(cpu_mem_grid_), 10);
    gtk_container_set_border_width(GTK_CONTAINER(cpu_mem_grid_), 10);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_), cpu_mem_grid_, gtk_label_new("CPU & Memory"));

    row = 0;

    GtkWidget* cpu_section_label = gtk_label_new("<span>CPU Usage</span>");
    gtk_label_set_use_markup(GTK_LABEL(cpu_section_label), TRUE);
    gtk_widget_set_halign(cpu_section_label, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(cpu_mem_grid_), cpu_section_label, 0, row++, 2, 1);

    GtkWidget* cpu_label_static = gtk_label_new("Current Usage:");
    gtk_widget_set_halign(cpu_label_static, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(cpu_mem_grid_), cpu_label_static, 0, row, 1, 1);
    cpu_usage_label_ = gtk_label_new("N/A");
    gtk_widget_set_halign(cpu_usage_label_, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(cpu_mem_grid_), cpu_usage_label_, 1, row++, 1, 1);

    GtkWidget* cpu_chart_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(cpu_chart_frame), GTK_SHADOW_IN);
    gtk_grid_attach(GTK_GRID(cpu_mem_grid_), cpu_chart_frame, 0, row, 2, 5);

    cpu_chart_area_ = gtk_drawing_area_new();
    gtk_widget_set_size_request(cpu_chart_area_, 300, 150);
    gtk_container_add(GTK_CONTAINER(cpu_chart_frame), cpu_chart_area_);
    g_signal_connect(G_OBJECT(cpu_chart_area_), "draw", G_CALLBACK(on_draw_cpu_chart), this);
    row += 5;

    row++;
    GtkWidget* mem_section_label = gtk_label_new("<span>Memory (RAM)</span>");
    gtk_label_set_use_markup(GTK_LABEL(mem_section_label), TRUE);
    gtk_widget_set_halign(mem_section_label, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(cpu_mem_grid_), mem_section_label, 0, row++, 2, 1);

    GtkWidget* mem_total_static = gtk_label_new("Total RAM:");
    gtk_widget_set_halign(mem_total_static, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(cpu_mem_grid_), mem_total_static, 0, row, 1, 1);
    mem_total_label_ = gtk_label_new("N/A");
    gtk_widget_set_halign(mem_total_label_, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(cpu_mem_grid_), mem_total_label_, 1, row++, 1, 1);

    GtkWidget* mem_used_static = gtk_label_new("Used RAM:");
    gtk_widget_set_halign(mem_used_static, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(cpu_mem_grid_), mem_used_static, 0, row, 1, 1);
    mem_used_label_ = gtk_label_new("N/A");
    gtk_widget_set_halign(mem_used_label_, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(cpu_mem_grid_), mem_used_label_, 1, row++, 1, 1);

    GtkWidget* mem_free_static = gtk_label_new("Free RAM:");
    gtk_widget_set_halign(mem_free_static, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(cpu_mem_grid_), mem_free_static, 0, row, 1, 1);
    mem_free_label_ = gtk_label_new("N/A");
    gtk_widget_set_halign(mem_free_label_, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(cpu_mem_grid_), mem_free_label_, 1, row++, 1, 1);

    GtkWidget* mem_usage_static = gtk_label_new("RAM Usage %:");
    gtk_widget_set_halign(mem_usage_static, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(cpu_mem_grid_), mem_usage_static, 0, row, 1, 1);
    mem_usage_label_ = gtk_label_new("N/A");
    gtk_widget_set_halign(mem_usage_label_, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(cpu_mem_grid_), mem_usage_label_, 1, row++, 1, 1);

    disk_grid_ = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(disk_grid_), 5);
    gtk_grid_set_column_spacing(GTK_GRID(disk_grid_), 10);
    gtk_container_set_border_width(GTK_CONTAINER(disk_grid_), 10);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_), disk_grid_, gtk_label_new("Disk Usage"));

    row = 0;
    GtkWidget* disk_section_label = gtk_label_new("<span>Disk Usage (Root '/')</span>");
    gtk_label_set_use_markup(GTK_LABEL(disk_section_label), TRUE);
    gtk_widget_set_halign(disk_section_label, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(disk_grid_), disk_section_label, 0, row++, 2, 1);

    GtkWidget* disk_total_static = gtk_label_new("Total Space:");
    gtk_widget_set_halign(disk_total_static, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(disk_grid_), disk_total_static, 0, row, 1, 1);
    disk_total_label_ = gtk_label_new("N/A");
    gtk_widget_set_halign(disk_total_label_, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(disk_grid_), disk_total_label_, 1, row++, 1, 1);

    GtkWidget* disk_used_static = gtk_label_new("Used Space:");
    gtk_widget_set_halign(disk_used_static, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(disk_grid_), disk_used_static, 0, row, 1, 1);
    disk_used_label_ = gtk_label_new("N/A");
    gtk_widget_set_halign(disk_used_label_, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(disk_grid_), disk_used_label_, 1, row++, 1, 1);

    GtkWidget* disk_free_static = gtk_label_new("Free Space:");
    gtk_widget_set_halign(disk_free_static, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(disk_grid_), disk_free_static, 0, row, 1, 1);
    disk_free_label_ = gtk_label_new("N/A");
    gtk_widget_set_halign(disk_free_label_, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(disk_grid_), disk_free_label_, 1, row++, 1, 1);

    GtkWidget* disk_usage_static = gtk_label_new("Disk Usage %:");
    gtk_widget_set_halign(disk_usage_static, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(disk_grid_), disk_usage_static, 0, row, 1, 1);
    disk_usage_label_ = gtk_label_new("N/A");
    gtk_widget_set_halign(disk_usage_label_, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(disk_grid_), disk_usage_label_, 1, row++, 1, 1);


    settings_grid_ = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(settings_grid_), 5);
    gtk_grid_set_column_spacing(GTK_GRID(settings_grid_), 10);
    gtk_container_set_border_width(GTK_CONTAINER(settings_grid_), 10);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook_), settings_grid_, gtk_label_new("Settings"));

    row = 0;
    GtkWidget* settings_section_label = gtk_label_new("<span>Application Settings</span>");
    gtk_label_set_use_markup(GTK_LABEL(settings_section_label), TRUE);
    gtk_widget_set_halign(settings_section_label, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(settings_grid_), settings_section_label, 0, row++, 2, 1);

    GtkWidget* interval_static_label = gtk_label_new("Update Interval (seconds):");
    gtk_widget_set_halign(interval_static_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(settings_grid_), interval_static_label, 0, row, 1, 1);

    GtkAdjustment* adj = gtk_adjustment_new(2.0, 1.0, 10.0, 1.0, 5.0, 0.0);
    update_interval_spin_button_ = gtk_spin_button_new(adj, 1.0, 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(update_interval_spin_button_), 2.0);
    gtk_grid_attach(GTK_GRID(settings_grid_), update_interval_spin_button_, 1, row++, 1, 1);

    g_signal_connect(G_OBJECT(update_interval_spin_button_), "value-changed", G_CALLBACK(on_update_interval_changed), this);

    onUpdateData();

    timeout_source_id_ = g_timeout_add_seconds(2, update_data_cb, this);

    gtk_widget_show_all(window_);
}

gboolean GUIManager::update_data_cb(gpointer user_data) {
    GUIManager* self = static_cast<GUIManager*>(user_data);
    return self->onUpdateData();
}

void GUIManager::on_update_interval_changed(GtkSpinButton* spinner, gpointer user_data) {
    GUIManager* self = static_cast<GUIManager*>(user_data);
    int new_interval = gtk_spin_button_get_value_as_int(spinner);

    if (self->timeout_source_id_ > 0) {
        g_source_remove(self->timeout_source_id_);
    }

    self->timeout_source_id_ = g_timeout_add_seconds(new_interval, update_data_cb, self);
    std::cout << "Update interval changed to " << new_interval << " seconds." << std::endl;
}

gboolean GUIManager::onUpdateData() {
    updateTemperatureLabels();
    updateCpuUsageLabel();
    updateMemoryLabels();
    updateDiskLabels();

    if (cpu_chart_area_) {
        gtk_widget_queue_draw(cpu_chart_area_);
    }

    return G_SOURCE_CONTINUE;
}

void GUIManager::updateTemperatureLabels() {
    auto current_temps = sysdata.getAllTemperatures();
    for (const auto& pair : current_temps) {
        if (templabels.count(pair.first)) {
            std::string temp_str;
            double temp_value = pair.second;
            GtkWidget* temp_label = templabels[pair.first];

            GtkStyleContext *context = gtk_widget_get_style_context(temp_label);
            gtk_style_context_remove_class(context, "temp-normal");
            gtk_style_context_remove_class(context, "temp-warning");
            gtk_style_context_remove_class(context, "temp-critical");


            if (temp_value != -1.0) {
                temp_str = std::to_string(static_cast<int>(std::round(temp_value))) + " °C";

                if (temp_value >= 85.0) {
                    gtk_style_context_add_class(context, "temp-critical");
                } else if (temp_value >= 75.0) {
                    gtk_style_context_add_class(context, "temp-warning");
                } else {
                    gtk_style_context_add_class(context, "temp-normal");
                }

            } else {
                temp_str = "Error";
            }
            gtk_label_set_text(GTK_LABEL(temp_label), temp_str.c_str());
        }
    }
}

void GUIManager::updateCpuUsageLabel() {
    double cpu_usage = sysdata.getCpuUsage();
    std::stringstream ss;
    if (cpu_usage >= 0) {
        ss << std::fixed << std::setprecision(1) << cpu_usage << " %";
    } else {
        ss << "Error";
    }
    gtk_label_set_text(GTK_LABEL(cpu_usage_label_), ss.str().c_str());
}

void GUIManager::updateMemoryLabels() {
    MemoryInfo mem_info = sysdata.getMemoryInfo();
    std::stringstream ss;

    if (mem_info.total_kb > 0) {
        double total_gb = static_cast<double>(mem_info.total_kb) / (1024.0 * 1024.0);
        double used_gb = static_cast<double>(mem_info.used_kb) / (1024.0 * 1024.0);
        double free_gb = static_cast<double>(mem_info.available_kb) / (1024.0 * 1024.0);

        ss.str(""); ss << std::fixed << std::setprecision(2) << total_gb << " GB";
        gtk_label_set_text(GTK_LABEL(mem_total_label_), ss.str().c_str());

        ss.str(""); ss << std::fixed << std::setprecision(2) << used_gb << " GB";
        gtk_label_set_text(GTK_LABEL(mem_used_label_), ss.str().c_str());

        ss.str(""); ss << std::fixed << std::setprecision(2) << free_gb << " GB";
        gtk_label_set_text(GTK_LABEL(mem_free_label_), ss.str().c_str());

        ss.str(""); ss << std::fixed << std::setprecision(1) << mem_info.usage_percent << " %";
        gtk_label_set_text(GTK_LABEL(mem_usage_label_), ss.str().c_str());
    } else {
        gtk_label_set_text(GTK_LABEL(mem_total_label_), "Error");
        gtk_label_set_text(GTK_LABEL(mem_used_label_), "Error");
        gtk_label_set_text(GTK_LABEL(mem_free_label_), "Error");
        gtk_label_set_text(GTK_LABEL(mem_usage_label_), "Error");
    }
}

void GUIManager::updateDiskLabels() {
    DiskInfo disk_info = sysdata.getDiskUsage("/");
    std::stringstream ss;

    if (disk_info.total_space_gb >= 0) {
        ss.str(""); ss << disk_info.total_space_gb << " GB";
        gtk_label_set_text(GTK_LABEL(disk_total_label_), ss.str().c_str());

        ss.str(""); ss << disk_info.used_space_gb << " GB";
        gtk_label_set_text(GTK_LABEL(disk_used_label_), ss.str().c_str());

        ss.str(""); ss << disk_info.free_space_gb << " GB";
        gtk_label_set_text(GTK_LABEL(disk_free_label_), ss.str().c_str());

        ss.str(""); ss << std::fixed << std::setprecision(1) << disk_info.usage_percent << " %";
        gtk_label_set_text(GTK_LABEL(disk_usage_label_), ss.str().c_str());
    } else {
        gtk_label_set_text(GTK_LABEL(disk_total_label_), "Error");
        gtk_label_set_text(GTK_LABEL(disk_used_label_), "Error");
        gtk_label_set_text(GTK_LABEL(disk_free_label_), "Error");
        gtk_label_set_text(GTK_LABEL(disk_usage_label_), "Error");
    }
}

gboolean GUIManager::on_draw_cpu_chart(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    GUIManager* self = static_cast<GUIManager*>(user_data);
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    double width = static_cast<double>(allocation.width);
    double height = static_cast<double>(allocation.height);

    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_paint(cr);

    const auto& history = self->sysdata.getCpuUsageHistory();

    if (history.empty()) {
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14);
        cairo_move_to(cr, width / 2 - 40, height / 2);
        cairo_show_text(cr, "No data");
        return FALSE;
    }

    double padding_x = 30;
    double padding_y = 15;
    double plot_width = width - 2 * padding_x;
    double plot_height = height - 2 * padding_y;

    double max_val = 100.0;

    cairo_set_line_width(cr, 0.5);
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);

    for (int i = 0; i <= 4; ++i) {
        double y_grid = padding_y + plot_height * (1.0 - (i * 0.25));
        cairo_move_to(cr, padding_x, y_grid);
        cairo_line_to(cr, padding_x + plot_width, y_grid);
        cairo_stroke(cr);

        std::string percent_label = std::to_string(i * 25) + "%";
        cairo_move_to(cr, padding_x - 25, y_grid + 5);
        cairo_show_text(cr, percent_label.c_str());
    }

    cairo_set_source_rgb(cr, 0.0, 0.4, 0.8);
    cairo_set_line_width(cr, 2.0);

    bool first_point = true;
    for (size_t i = 0; i < history.size(); ++i) {
        double x_pos = padding_x + (static_cast<double>(i) / (self->sysdata.getMaxHistoryPoints() - 1)) * plot_width;
        double clamped_value = std::max(0.0, std::min(100.0, history[i]));
        double y_pos = padding_y + plot_height - (clamped_value / max_val) * plot_height;

        if (first_point) {
            cairo_move_to(cr, x_pos, y_pos);
            first_point = false;
        } else {
            cairo_line_to(cr, x_pos, y_pos);
        }
    }
    cairo_stroke(cr);

    if (!history.empty()) {
        std::string current_val_str = "Current: " + std::to_string(static_cast<int>(std::round(history.back()))) + "%";
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 16);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, current_val_str.c_str(), &extents);
        cairo_move_to(cr, width - extents.width - 5, padding_y + extents.height);
        cairo_show_text(cr, current_val_str.c_str());
    }

    return FALSE;
}