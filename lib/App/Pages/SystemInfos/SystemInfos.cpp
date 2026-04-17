#include <string.h>
#include "SystemInfos.h"
#include "../App/Version.h"

using namespace Page;

static uint32_t s_enterTick = 0;

SystemInfos::SystemInfos()
{
    timer = nullptr;
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
    View.Create(_root);

    SystemInfosView::item_t* item_group = ((SystemInfosView::item_t*)&View.ui);
    for (int i = 0; i < sizeof(View.ui) / sizeof(SystemInfosView::item_t); i++)
    {
        AttachEvent(item_group[i].icon);
    }

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

    // 等待打开菜单时那一下按键释放，避免刚进页面就误触发
    lv_indev_t* indev = lv_indev_get_act();
    if (indev)
    {
        lv_indev_wait_release(indev);
    }

    s_enterTick = lv_tick_get();

    // 先刷新一次数据
    Update();

    Serial.println("SystemInfos::onViewWillAppear exit");
}

void SystemInfos::onViewDidAppear()
{
    Serial.println("SystemInfos::onViewDidAppear");

    lv_group_t* group = lv_group_get_default();
    if (group)
    {
        View.onFocus(group);
    }
}

void SystemInfos::onViewWillDisappear()
{
    Serial.println("SystemInfos::onViewWillDisappear");
}

void SystemInfos::onViewDidDisappear()
{
    Serial.println("SystemInfos::onViewDidDisappear");

    if (timer)
    {
        lv_timer_del(timer);
        timer = nullptr;
    }
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
    char buf[96];

    memset(buf, 0, sizeof(buf));
    float trip = 0.0f;
    float maxSpd = 0.0f;
    Model.GetSportInfo(&trip, buf, sizeof(buf), &maxSpd);
    buf[sizeof(buf) - 1] = '\0';
    if (buf[0] == '\0')
    {
        strncpy(buf, "--:--:--", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
    }
    View.SetSport(trip, buf, maxSpd);

    memset(buf, 0, sizeof(buf));
    float lat = 0.0f;
    float lng = 0.0f;
    float alt = 0.0f;
    float course = 0.0f;
    float speed = 0.0f;
    Model.GetGPSInfo(&lat, &lng, &alt, buf, sizeof(buf), &course, &speed);
    buf[sizeof(buf) - 1] = '\0';
    if (buf[0] == '\0')
    {
        strncpy(buf, "0000-00-00\n00:00:00", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
    }
    View.SetGPS(lat, lng, alt, buf, course, speed);

    float dir = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    Model.GetMAGInfo(&dir, &x, &y, &z);
    View.SetMAG((int)dir, (int)x, (int)y, (int)z);

    memset(buf, 0, sizeof(buf));
    int steps = 0;
    Model.GetIMUInfo(&steps, buf, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    if (buf[0] == '\0')
    {
        strncpy(buf, "-\n-\n-\n-\n-\n-", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
    }
    View.SetIMU(steps, buf);

    float pressure = 0.0f;
    float humidity = 0.0f;
    float temperature = 0.0f;
    Model.GetPHTInfo(&pressure, &humidity, &temperature);
    View.SetPHT(pressure, humidity, temperature);

    memset(buf, 0, sizeof(buf));
    Model.GetRTCInfo(buf, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    if (buf[0] == '\0')
    {
        strncpy(buf, "0000-00-00\n00:00:00", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
    }
    View.SetRTC(buf);

    memset(buf, 0, sizeof(buf));
    int usage = 0;
    float voltage = 0.0f;
    Model.GetBatteryInfo(&usage, &voltage, buf, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    if (buf[0] == '\0')
    {
        strncpy(buf, "UNKNOWN", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
    }
    View.SetBattery(usage, voltage, buf);

    memset(buf, 0, sizeof(buf));
    bool detect = false;
    const char* type = "-";
    Model.GetStorageInfo(&detect, &type, buf, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    if (buf[0] == '\0')
    {
        strncpy(buf, "0.0 GB", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
    }
    if (type == nullptr)
    {
        type = "-";
    }
    View.SetStorage(
        detect ? "OK" : "ERROR",
        buf,
        type,
        VERSION_FILESYSTEM
    );

    memset(buf, 0, sizeof(buf));
    DataProc::MakeTimeString(lv_tick_get(), buf, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    if (buf[0] == '\0')
    {
        strncpy(buf, "--:--:--", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
    }
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
    if (instance)
    {
        instance->Update();
    }
}

void SystemInfos::onEvent(lv_event_t* event)
{
    SystemInfos* instance = (SystemInfos*)lv_event_get_user_data(event);
    LV_ASSERT_NULL(instance);

    if (lv_tick_elaps(s_enterTick) < 300)
    {
        return;
    }

    lv_obj_t* obj = lv_event_get_current_target(event);
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_event_get_key(event);

        if (key == LV_KEY_ESC)
        {
            instance->_Manager->Pop();
            return;
        }

        if (key == LV_KEY_ENTER)
        {
            if (obj == instance->View.ui.bluetooth.icon)
            {
                instance->_Manager->Push("Pages/Bluetooth");
                return;
            }

            instance->_Manager->Pop();
            return;
        }
    }

    if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_CLICKED)
    {
        if (obj == instance->View.ui.bluetooth.icon)
        {
            instance->_Manager->Push("Pages/Bluetooth");
            return;
        }

        SystemInfosView::item_t* item_group = (SystemInfosView::item_t*)&instance->View.ui;
        for (int i = 0; i < (int)(sizeof(instance->View.ui) / sizeof(SystemInfosView::item_t)); i++)
        {
            if (obj == item_group[i].icon)
            {
                instance->_Manager->Pop();
                return;
            }
        }
    }
}