#include "SystemInfos.h"
#include "../App/Version.h"

using namespace Page;

SystemInfos::SystemInfos()
{
}

SystemInfos::~SystemInfos()
{
}

void SystemInfos::onCustomAttrConfig()
{
}

void SystemInfos::onViewLoad()
{
    Serial.println("SystemInfos::onViewLoad enter");
    Model.Init();

    Serial.println("before View.Create");
    View.Create(_root);
    Serial.println("after View.Create");

    // 先不要绑任何事件
    // AttachEvent(_root);

    // SystemInfosView::item_t* item_group = ((SystemInfosView::item_t*)&View.ui);
    // for (int i = 0; i < sizeof(View.ui) / sizeof(SystemInfosView::item_t); i++)
    // {
    //     AttachEvent(item_group[i].icon);
    // }

    Serial.println("SystemInfos::onViewLoad exit");
}

void SystemInfos::onViewDidLoad()
{
    Serial.println("SystemInfos::onViewDidLoad");
}

void SystemInfos::onViewWillAppear()
{
    Serial.println("SystemInfos::onViewWillAppear enter");

    Model.SetStatusBarStyle(DataProc::STATUS_BAR_STYLE_BLACK);

    lv_indev_t* indev = lv_indev_get_act();
    if (indev) {
        lv_indev_wait_release(indev);
    }

    // 先不要定时器、不要动画、不要滚动
    // timer = lv_timer_create(onTimerUpdate, 1000, this);
    // lv_timer_ready(timer);

    // View.SetScrollToY(_root, -LV_VER_RES, LV_ANIM_OFF);
    // lv_obj_set_style_opa(_root, LV_OPA_TRANSP, 0);
    // lv_obj_fade_in(_root, 300, 0);

    Serial.println("SystemInfos::onViewWillAppear exit");
}

void SystemInfos::onViewDidAppear()
{
    Serial.println("SystemInfos::onViewDidAppear");

    // 先不要焦点处理
    // lv_group_t* group = lv_group_get_default();
    // LV_ASSERT_NULL(group);
    // View.onFocus(group);
}

void SystemInfos::onViewWillDisappear()
{
    Serial.println("SystemInfos::onViewWillDisappear");
    // lv_obj_fade_out(_root, 300, 0);
}

void SystemInfos::onViewDidDisappear()
{
    Serial.println("SystemInfos::onViewDidDisappear");
    // if (timer) lv_timer_del(timer);
}

void SystemInfos::onViewUnload()
{
    Serial.println("SystemInfos::onViewUnload");
    View.Delete();
    Model.Deinit();
}

void SystemInfos::onViewDidUnload()
{
    Serial.println("SystemInfos::onViewDidUnload");
}

void SystemInfos::AttachEvent(lv_obj_t* obj)
{
    lv_obj_add_event_cb(obj, onEvent, LV_EVENT_ALL, this);
}

void SystemInfos::Update()
{
    char buf[64];

    float trip;
    float maxSpd;
    Model.GetSportInfo(&trip, buf, sizeof(buf), &maxSpd);
    View.SetSport(trip, buf, maxSpd);

    float lat;
    float lng;
    float alt;
    float course;
    float speed;
    Model.GetGPSInfo(&lat, &lng, &alt, buf, sizeof(buf), &course, &speed);
    View.SetGPS(lat, lng, alt, buf, course, speed);

    float dir;
    float x;
    float y;
    float z;
    Model.GetMAGInfo(&dir, &x, &y, &z);
    View.SetMAG(dir, x, y, z);

    int steps;
    Model.GetIMUInfo(&steps, buf, sizeof(buf));
    View.SetIMU(steps, buf);

    float pressure;
    float humidity;
    float temperature;
    Model.GetPHTInfo(&pressure, &humidity, &temperature);
    View.SetPHT(pressure, humidity, temperature);

    Model.GetRTCInfo(buf, sizeof(buf));
    View.SetRTC(buf);

    int usage;
    float voltage;
    Model.GetBatteryInfo(&usage, &voltage, buf, sizeof(buf));
    View.SetBattery(usage, voltage, buf);

    bool detect;
    const char* type = "-";
    Model.GetStorageInfo(&detect, &type, buf, sizeof(buf));
    View.SetStorage(
        detect ? "OK" : "ERROR",
        buf,
        type,
        VERSION_FILESYSTEM
    );

    DataProc::MakeTimeString(lv_tick_get(), buf, sizeof(buf));
    View.SetSystem(
        VERSION_FIRMWARE_NAME " " VERSION_SOFTWARE,
        VERSION_AUTHOR_NAME,
        VERSION_LVGL,
        buf,
        VERSION_COMPILER,
        VERSION_BUILD_TIME
    );
}

void SystemInfos::onTimerUpdate(lv_timer_t* timer)
{
    SystemInfos* instance = (SystemInfos*)timer->user_data;
    instance->Update();
}

void SystemInfos::onEvent(lv_event_t* event)
{
    Serial.println("SystemInfos::onEvent");
}