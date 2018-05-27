#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <Task.h>
#include "reboot.h"

static const char* tag = "reboot";

class RebootTask : public Task {
private:
    int msec;
public:
    RebootTask(int msec) : msec(msec){};
private:
    void run(void *data) override {
	ESP_LOGI(tag, "wait for %d msec", msec);
	vTaskDelay(msec / portTICK_PERIOD_MS);
	ESP_LOGI(tag, "restart!");
	esp_restart();
    };
};

void rebootIn(int msec) {
    RebootTask* task = new RebootTask(msec);
    task->start();
}

