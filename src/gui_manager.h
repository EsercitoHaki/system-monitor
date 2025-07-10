#ifndef GUI_MANAGER_H
#define GUI_MANAGER_H

#include <gtk/gtk.h>
#include "system_data.h"
#include <map>

class GUIManager {
public:
    GUIManager(SystemData& sys_data);
    void run();

private:
    SystemData& sysdata;
    GtkWidget* window_;
    GtkWidget* notebook_;

    GtkWidget* temp_grid_;
    GtkWidget* cpu_mem_grid_;
    GtkWidget* disk_grid_;
    GtkWidget* settings_grid_;

    std::map<std::string, GtkWidget*> templabels;

    GtkWidget* cpu_usage_label_;
    GtkWidget* cpu_chart_area_;

    GtkWidget* mem_total_label_;
    GtkWidget* mem_used_label_;
    GtkWidget* mem_free_label_;
    GtkWidget* mem_usage_label_;

    GtkWidget* disk_total_label_;
    GtkWidget* disk_used_label_;
    GtkWidget* disk_free_label_;
    GtkWidget* disk_usage_label_;

    GtkWidget* update_interval_spin_button_;
    guint timeout_source_id_;

    static void activate(GtkApplication* app, gpointer user_data);
    static gboolean update_data_cb(gpointer user_data);
    static void on_update_interval_changed(GtkSpinButton* spinner, gpointer user_data);
    static gboolean on_draw_cpu_chart(GtkWidget *widget, cairo_t *cr, gpointer user_data);

    void onActivate(GtkApplication* app);
    gboolean onUpdateData();

    void updateTemperatureLabels();
    void updateCpuUsageLabel();
    void updateMemoryLabels();
    void updateDiskLabels();
};

#endif