#include "RideData.h"
#include "HAL_Bluetooth.h"

#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(font_cn_18);

using namespace Page;

static uint32_t s_rideEnterTick = 0;

#define UI_OFFSET_X        -10
#define UI_OFFSET_Y        0

#define UI_MARGIN_X        0
#define UI_MARGIN_TOP      -1
#define UI_MARGIN_BOTTOM   20

#define SPEED_PANEL_H      112
#define TIME_CELL_H        50

#define GRID_COLOR         0x999999
#define UNIT_RIGHT_INSET   20
#define SPEED_SIDE_AREA_W  56

static lv_obj_t* CreateSolidLine(
    lv_obj_t* parent,
    lv_coord_t x,
    lv_coord_t y,
    lv_coord_t w,
    lv_coord_t h
)
{
    lv_obj_t* line = lv_obj_create(parent);

    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, w, h);
    lv_obj_set_style_bg_color(line, lv_color_hex(GRID_COLOR), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);

    return line;
}

RideData::RideData()
    : account(nullptr)
    , timer(nullptr)
{
    memset(&ui, 0, sizeof(ui));

    DATA_PROC_INIT_STRUCT(sportStatusInfo);
    DATA_PROC_INIT_STRUCT(phtInfo);
    DATA_PROC_INIT_STRUCT(gpsInfo);
}

RideData::~RideData()
{
}

void RideData::onCustomAttrConfig()
{
    SetCustomLoadAnimType(PageManager::LOAD_ANIM_NONE);
}

void RideData::onViewLoad()
{
    CreateUI();

    lv_obj_add_event_cb(ui.cont, onEvent, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(_root, onEvent, LV_EVENT_ALL, this);
}

void RideData::onViewDidLoad()
{
}

void RideData::onViewWillAppear()
{
    lv_indev_t* indev = lv_indev_get_act();

    if (indev)
    {
        lv_indev_wait_release(indev);
    }

    s_rideEnterTick = lv_tick_get();

    InitModel();
    SetStatusBarStyle(DataProc::STATUS_BAR_STYLE_TRANSP);
    Update();

    lv_group_t* group = lv_group_get_default();

    if (group)
    {
        lv_group_remove_all_objs(group);

        lv_obj_add_flag(ui.cont, LV_OBJ_FLAG_CLICKABLE);
        lv_group_add_obj(group, ui.cont);
        lv_group_focus_obj(ui.cont);
    }
}

void RideData::onViewDidAppear()
{
    // 500ms 刷新一次，让速度、时间、心率等数据实时更新
    timer = lv_timer_create(onTimerUpdate, 500, this);
}

void RideData::onViewWillDisappear()
{
    if (timer)
    {
        lv_timer_del(timer);
        timer = nullptr;
    }

    lv_group_t* group = lv_group_get_default();

    if (group)
    {
        lv_group_remove_all_objs(group);
    }
}

void RideData::onViewDidDisappear()
{
    DeinitModel();
}

void RideData::onViewUnload()
{
    DeleteUI();
}

void RideData::onViewDidUnload()
{
}

void RideData::CreateUI()
{
    lv_obj_set_style_bg_color(_root, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_root, LV_OBJ_FLAG_SCROLLABLE);

    ui.cont = lv_obj_create(_root);
    lv_obj_remove_style_all(ui.cont);
    lv_obj_set_pos(ui.cont, 0, 0);
    lv_obj_set_size(ui.cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(ui.cont, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ui.cont, LV_OPA_COVER, 0);
    lv_obj_add_flag(ui.cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui.cont, LV_OBJ_FLAG_SCROLLABLE);

    const lv_coord_t bodyW = LV_HOR_RES - UI_MARGIN_X * 2;
    const lv_coord_t bodyH = LV_VER_RES - UI_MARGIN_TOP - UI_MARGIN_BOTTOM;
    const lv_coord_t bodyX = UI_MARGIN_X + UI_OFFSET_X;
    const lv_coord_t bodyY = UI_MARGIN_TOP + UI_OFFSET_Y;

    lv_obj_t* body = lv_obj_create(ui.cont);
    lv_obj_remove_style_all(body);
    lv_obj_set_pos(body, bodyX, bodyY);
    lv_obj_set_size(body, bodyW, bodyH);
    lv_obj_set_style_bg_color(body, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(body, 1, 0);
    lv_obj_set_style_border_color(body, lv_color_hex(GRID_COLOR), 0);
    lv_obj_set_style_border_side(body, LV_BORDER_SIDE_FULL, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    CreateSpeedPanel(body);
    CreateCells(body);
}

void RideData::DeleteUI()
{
    if (ui.cont)
    {
        lv_obj_del(ui.cont);
        memset(&ui, 0, sizeof(ui));
    }
}

void RideData::CreateSpeedPanel(lv_obj_t* parent)
{
    const lv_coord_t fullW = LV_HOR_RES - UI_MARGIN_X * 2;
    const lv_coord_t sideAreaW = SPEED_SIDE_AREA_W;
    const lv_coord_t centerAreaW = fullW - sideAreaW * 2;

    // 顶部速度区和时间区之间的横线
    CreateSolidLine(parent, 0, SPEED_PANEL_H, fullW, 1);

    // 左右对称分隔线：左侧均速区 / 中间表盘区 / 右侧最大区
    CreateSolidLine(parent, sideAreaW, 0, 1, SPEED_PANEL_H);
    CreateSolidLine(parent, fullW - sideAreaW, 0, 1, SPEED_PANEL_H);

    // ===== 中间时速表盘 =====
    ui.arcSpeed = lv_arc_create(parent);
    lv_obj_set_size(ui.arcSpeed, 102, 102);
    lv_obj_set_pos(ui.arcSpeed, sideAreaW + (centerAreaW - 102) / 2, 6);
    lv_arc_set_range(ui.arcSpeed, 0, 60);
    lv_arc_set_value(ui.arcSpeed, 0);

    lv_obj_remove_style(ui.arcSpeed, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(ui.arcSpeed, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_arc_width(ui.arcSpeed, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui.arcSpeed, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui.arcSpeed, lv_color_hex(0xd8d8d8), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui.arcSpeed, lv_color_hex(0x57c7d9), LV_PART_INDICATOR);

    ui.labelSpeed = lv_label_create(parent);
    lv_obj_set_style_text_font(ui.labelSpeed, ResourcePool::GetFont("bahnschrift_65"), 0);
    lv_obj_set_style_text_color(ui.labelSpeed, lv_color_black(), 0);
    lv_label_set_text(ui.labelSpeed, "0");
    lv_obj_align_to(ui.labelSpeed, ui.arcSpeed, LV_ALIGN_CENTER, 0, -8);

    lv_obj_t* unit = lv_label_create(parent);
    lv_obj_set_style_text_font(unit, ResourcePool::GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(unit, lv_color_hex(0x555555), 0);
    lv_label_set_text(unit, "km/h");
    lv_obj_align_to(unit, ui.arcSpeed, LV_ALIGN_CENTER, 0, 28);

    // ===== 左侧：均速 =====
    ui.labelAvg = lv_label_create(parent);
    lv_obj_set_style_text_font(ui.labelAvg, &font_cn_18, 0);
    lv_obj_set_style_text_color(ui.labelAvg, lv_color_black(), 0);
    lv_obj_set_width(ui.labelAvg, sideAreaW - 6);
    lv_obj_set_style_text_align(ui.labelAvg, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ui.labelAvg, LV_LABEL_LONG_CLIP);
    lv_label_set_text(ui.labelAvg, "均速\n0");
    lv_obj_set_pos(ui.labelAvg, 3, SPEED_PANEL_H / 2 - 24);

    // ===== 右侧：最大 =====
    ui.labelMax = lv_label_create(parent);
    lv_obj_set_style_text_font(ui.labelMax, &font_cn_18, 0);
    lv_obj_set_style_text_color(ui.labelMax, lv_color_black(), 0);
    lv_obj_set_width(ui.labelMax, sideAreaW - 6);
    lv_obj_set_style_text_align(ui.labelMax, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ui.labelMax, LV_LABEL_LONG_CLIP);
    lv_label_set_text(ui.labelMax, "最大\n0");
    lv_obj_set_pos(ui.labelMax, fullW - sideAreaW + 3, SPEED_PANEL_H / 2 - 24);
}

void RideData::CreateCells(lv_obj_t* parent)
{
    const lv_coord_t fullW = LV_HOR_RES - UI_MARGIN_X * 2;
    const lv_coord_t fullH = LV_VER_RES - UI_MARGIN_TOP - UI_MARGIN_BOTTOM;

    const lv_coord_t colW = fullW / 2;

    const lv_coord_t yTime = SPEED_PANEL_H;
    const lv_coord_t yRow1 = yTime + TIME_CELL_H;

    const lv_coord_t remainH = fullH - SPEED_PANEL_H - TIME_CELL_H;

    lv_coord_t rowH1 = remainH / 3;
    lv_coord_t rowH2 = remainH / 3;
    lv_coord_t rowH3 = remainH - rowH1 - rowH2;

    if (rowH1 < 48)
    {
        rowH1 = 48;
    }

    if (rowH2 < 48)
    {
        rowH2 = 48;
    }

    if (rowH3 < 48)
    {
        rowH3 = 48;
    }

    const lv_coord_t yRow2 = yRow1 + rowH1;
    const lv_coord_t yRow3 = yRow2 + rowH2;

    // 统一画分隔线，避免每个 cell 自己画边框导致重叠、歪斜
    CreateSolidLine(parent, 0, yRow1, fullW, 1);
    CreateSolidLine(parent, 0, yRow2, fullW, 1);
    CreateSolidLine(parent, 0, yRow3, fullW, 1);

    // 中间竖线，只从双列区域开始
    CreateSolidLine(parent, colW, yRow1, 1, fullH - yRow1);

    CreateCell(parent, &ui.cells[0], "时间", "", 0, yTime, fullW, TIME_CELL_H);

    CreateCell(parent, &ui.cells[1], "距离", "km", 0, yRow1, colW, rowH1);
    CreateCell(parent, &ui.cells[2], "总时", "", colW, yRow1, fullW - colW, rowH1);

    CreateCell(parent, &ui.cells[3], "海拔", "m", 0, yRow2, colW, rowH2);
    CreateCell(parent, &ui.cells[4], "踏频", "rpm", colW, yRow2, fullW - colW, rowH2);

    CreateCell(parent, &ui.cells[5], "温度", "C", 0, yRow3, colW, rowH3);
    CreateCell(parent, &ui.cells[6], "心率", "bpm", colW, yRow3, fullW - colW, rowH3);
}

void RideData::CreateCell(
    lv_obj_t* parent,
    DataCell_t* cell,
    const char* title,
    const char* unit,
    lv_coord_t x,
    lv_coord_t y,
    lv_coord_t w,
    lv_coord_t h
)
{
    cell->cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cell->cont);
    lv_obj_set_pos(cell->cont, x, y);
    lv_obj_set_size(cell->cont, w, h);
    lv_obj_set_style_bg_opa(cell->cont, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(cell->cont, LV_OBJ_FLAG_SCROLLABLE);

    cell->title = lv_label_create(cell->cont);
    lv_obj_set_style_text_font(cell->title, &font_cn_18, 0);
    lv_obj_set_style_text_color(cell->title, lv_color_hex(0x333333), 0);
    lv_obj_set_width(cell->title, w - 8);
    lv_obj_set_style_text_align(cell->title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(cell->title, LV_LABEL_LONG_CLIP);
    lv_label_set_text(cell->title, title);
    lv_obj_align(cell->title, LV_ALIGN_TOP_MID, 0, 3);

    cell->value = lv_label_create(cell->cont);
    lv_obj_set_style_text_font(cell->value, ResourcePool::GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(cell->value, lv_color_black(), 0);
    lv_label_set_long_mode(cell->value, LV_LABEL_LONG_CLIP);
    lv_label_set_text(cell->value, "---");

    if (unit && strlen(unit) > 0)
    {
        lv_obj_set_width(cell->value, w - 52);
        lv_obj_set_style_text_align(cell->value, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(cell->value, LV_ALIGN_CENTER, -14, 6);
    }
    else
    {
        lv_obj_set_width(cell->value, w - 8);
        lv_obj_set_style_text_align(cell->value, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(cell->value, LV_ALIGN_CENTER, 0, 6);
    }

    cell->unit = lv_label_create(cell->cont);
    lv_obj_set_style_text_font(cell->unit, ResourcePool::GetFont("bahnschrift_13"), 0);
    lv_obj_set_style_text_color(cell->unit, lv_color_hex(0x333333), 0);
    lv_obj_set_width(cell->unit, 34);
    lv_obj_set_style_text_align(cell->unit, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(cell->unit, LV_LABEL_LONG_CLIP);
    lv_label_set_text(cell->unit, unit ? unit : "");

    // 单位单独往左收，不再影响整体表格位置
    lv_obj_align(cell->unit, LV_ALIGN_BOTTOM_RIGHT, -UNIT_RIGHT_INSET, -6);
}

void RideData::InitModel()
{
    if (account)
    {
        return;
    }

    account = new Account("RideData", DataProc::Center(), 0, this);

    account->Subscribe("SportStatus");
    account->Subscribe("PHT");
    account->Subscribe("GPS");
    account->Subscribe("StatusBar");

    account->SetEventCallback(onDataEvent);
}

void RideData::DeinitModel()
{
    if (account)
    {
        delete account;
        account = nullptr;
    }
}

void RideData::SetStatusBarStyle(DataProc::StatusBar_Style_t style)
{
    if (!account)
    {
        return;
    }

    DataProc::StatusBar_Info_t info;
    DATA_PROC_INIT_STRUCT(info);

    info.cmd = DataProc::STATUS_BAR_CMD_SET_STYLE;
    info.param.style = style;

    account->Notify("StatusBar", &info, sizeof(info));
}

int RideData::onDataEvent(Account* account, Account::EventParam_t* param)
{
    if (param->event != Account::EVENT_PUB_PUBLISH)
    {
        return Account::RES_UNSUPPORTED_REQUEST;
    }

    RideData* instance = (RideData*)account->UserData;

    if (!instance)
    {
        return Account::RES_PARAM_ERROR;
    }

    if (strcmp(param->tran->ID, "SportStatus") == 0 &&
        param->size == sizeof(HAL::SportStatus_Info_t))
    {
        memcpy(&instance->sportStatusInfo, param->data_p, param->size);

        // 收到运动数据发布后立即刷新页面
        instance->Update();

        return Account::RES_OK;
    }

    if (strcmp(param->tran->ID, "PHT") == 0 &&
        param->size == sizeof(HAL::PHT_Info_t))
    {
        memcpy(&instance->phtInfo, param->data_p, param->size);

        // 收到温度数据发布后立即刷新页面
        instance->Update();

        return Account::RES_OK;
    }

    if (strcmp(param->tran->ID, "GPS") == 0 &&
        param->size == sizeof(HAL::GPS_Info_t))
    {
        memcpy(&instance->gpsInfo, param->data_p, param->size);

        return Account::RES_OK;
    }

    return Account::RES_PARAM_ERROR;
}

void RideData::Update()
{
    if (!ui.labelSpeed || !ui.arcSpeed || !ui.labelMax || !ui.labelAvg)
    {
        return;
    }

    char buf[32];

    if (account)
    {
        account->Pull("PHT", &phtInfo, sizeof(phtInfo));
    }

    int speed = (int)(sportStatusInfo.speedKph + 0.5f);
    int maxSpeed = (int)(sportStatusInfo.speedMaxKph + 0.5f);
    int avgSpeed = (int)(sportStatusInfo.speedAvgKph + 0.5f);

    if (speed < 0)
    {
        speed = 0;
    }

    if (maxSpeed < 0)
    {
        maxSpeed = 0;
    }

    if (avgSpeed < 0)
    {
        avgSpeed = 0;
    }

    /*
     * 如果 speedAvgKph 还没有被数据中心计算出来，
     * 就用 singleDistance / singleTime 手动算一个均速。
     *
     * 这里按项目里的时间单位 ms 计算：
     * km/h = meters * 3600 / ms
     */
    if (avgSpeed == 0 &&
        sportStatusInfo.singleTime > 0 &&
        sportStatusInfo.singleDistance > 0)
    {
        float avgKph = (sportStatusInfo.singleDistance * 3600.0f) /
                       sportStatusInfo.singleTime;

        avgSpeed = (int)(avgKph + 0.5f);

        if (avgSpeed < 0)
        {
            avgSpeed = 0;
        }
    }

    lv_label_set_text_fmt(ui.labelSpeed, "%d", speed);
    lv_arc_set_value(ui.arcSpeed, speed > 60 ? 60 : speed);

    // 顶部左右对称显示：左均速，右最大
    lv_label_set_text_fmt(ui.labelAvg, "均速\n%d", avgSpeed);
    lv_label_set_text_fmt(ui.labelMax, "最大\n%d", maxSpeed);

    if (ui.cells[0].value)
    {
        lv_label_set_text(
            ui.cells[0].value,
            DataProc::MakeTimeString(sportStatusInfo.singleTime, buf, sizeof(buf))
        );
    }

    if (ui.cells[1].value)
    {
        snprintf(buf, sizeof(buf), "%.1f", sportStatusInfo.singleDistance / 1000.0f);
        lv_label_set_text(ui.cells[1].value, buf);
    }

    if (ui.cells[2].value)
    {
        lv_label_set_text(
            ui.cells[2].value,
            DataProc::MakeTimeString(sportStatusInfo.totalTime, buf, sizeof(buf))
        );
    }

    if (ui.cells[3].value)
    {
        if (gpsInfo.isVaild && gpsInfo.satellites >= 3)
        {
            snprintf(buf, sizeof(buf), "%.0f", gpsInfo.altitude);
            lv_label_set_text(ui.cells[3].value, buf);
        }
        else
        {
            lv_label_set_text(ui.cells[3].value, "---");
        }
    }

    /*
     * 踏频暂时保留原来的固定值。
     * 后面如果踏频 BLE 模块接好了，再把这里改成 Bluetooth_GetCadence()。
     */
    if (ui.cells[4].value)
    {
        lv_label_set_text(ui.cells[4].value, "20");
    }

    if (ui.cells[5].value)
    {
        snprintf(buf, sizeof(buf), "%.0f", phtInfo.temperature);
        lv_label_set_text(ui.cells[5].value, buf);
    }

    /*
     * 蓝牙心率显示。
     * 蓝牙没连接、没有收到心率、心率无效时显示 ---。
     * 收到有效心率时显示实际 bpm 数值。
     */
    if (ui.cells[6].value)
    {
        if (HAL::Bluetooth_IsHeartRateValid())
        {
            uint8_t heartRate = HAL::Bluetooth_GetHeartRate();

            if (heartRate > 0)
            {
                snprintf(buf, sizeof(buf), "%u", (unsigned int)heartRate);
                lv_label_set_text(ui.cells[6].value, buf);
            }
            else
            {
                lv_label_set_text(ui.cells[6].value, "---");
            }
        }
        else
        {
            lv_label_set_text(ui.cells[6].value, "---");
        }
    }
}

void RideData::onTimerUpdate(lv_timer_t* timer)
{
    RideData* instance = (RideData*)timer->user_data;

    if (instance)
    {
        instance->Update();
    }
}

void RideData::onEvent(lv_event_t* event)
{
    RideData* instance = (RideData*)lv_event_get_user_data(event);
    LV_ASSERT_NULL(instance);

    if (lv_tick_elaps(s_rideEnterTick) < 300)
    {
        return;
    }

    lv_event_code_t code = lv_event_get_code(event);

    static uint32_t lastExitTick = 0;

    if (lv_tick_elaps(lastExitTick) < 300)
    {
        return;
    }

    bool needExit = false;

    /*
     * 情况 1：
     * 键盘/按键输入，一般走 LV_EVENT_KEY。
     */
    if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_event_get_key(event);

        if (key == LV_KEY_ENTER || key == LV_KEY_ESC)
        {
            needExit = true;
        }
    }

    if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_CLICKED)
    {
        needExit = true;
    }

    if (code == LV_EVENT_LEAVE)
    {
        needExit = true;
    }

    if (needExit)
    {
        lastExitTick = lv_tick_get();
        instance->_Manager->Pop();
        return;
    }
}