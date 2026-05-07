#include "RideData.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(font_cn_18);

using namespace Page;

/*
 * ========= RideData 页面最终布局参数 =========
 *
 * 这版不再让每个格子自己画边框，
 * 而是：一个整体外框 + 分隔线。
 *
 * 如果后面还要微调，只改这里：
 *
 * UI_OFFSET_X:
 *   整体左右微调。正数往右，负数往左。
 *
 * UI_OFFSET_Y:
 *   整体上下微调。正数往下，负数往上。
 *
 * UI_MARGIN_X:
 *   左右统一边距。越小越接近全屏，但太小会贴边。
 *
 * UI_MARGIN_TOP:
 *   顶部边距。
 *
 * UI_MARGIN_BOTTOM:
 *   底部安全区。你的屏幕底部外壳会挡住内容，所以这里必须留大一点。
 */
#define UI_OFFSET_X          -10
#define UI_OFFSET_Y          0

#define UI_MARGIN_X         0
#define UI_MARGIN_TOP        -1
#define UI_MARGIN_BOTTOM     20

#define SPEED_PANEL_H        112
#define TIME_CELL_H          50

#define GRID_COLOR           0x999999
#define UNIT_RIGHT_INSET     20

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

    // 只绑定 ui.cont，避免 _root 和 ui.cont 同时触发导致重复退出
    lv_obj_add_event_cb(ui.cont, onEvent, LV_EVENT_ALL, this);
}

void RideData::onViewDidLoad()
{
}

void RideData::onViewWillAppear()
{
    lv_indev_wait_release(lv_indev_get_act());

    InitModel();
    SetStatusBarStyle(DataProc::STATUS_BAR_STYLE_TRANSP);

    Update();

    lv_group_t* group = lv_group_get_default();
    if (group)
    {
        lv_group_remove_all_objs(group);
        lv_group_add_obj(group, ui.cont);
        lv_group_focus_obj(ui.cont);
    }
}

void RideData::onViewDidAppear()
{
    timer = lv_timer_create(onTimerUpdate, 1000, this);
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
    const lv_coord_t maxAreaW = 48;

    // 顶部速度区和时间区之间的横线
    CreateSolidLine(parent, 0, SPEED_PANEL_H, fullW, 1);

    // MAX 区域竖线，只在速度区内显示
    CreateSolidLine(parent, fullW - maxAreaW, 0, 1, SPEED_PANEL_H);

    ui.arcSpeed = lv_arc_create(parent);
    lv_obj_set_size(ui.arcSpeed, 102, 102);
    lv_obj_set_pos(ui.arcSpeed, (fullW - 102) / 2 - 6, 6);
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

    ui.labelMax = lv_label_create(parent);
    lv_obj_set_style_text_font(ui.labelMax, ResourcePool::GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(ui.labelMax, lv_color_black(), 0);
    lv_obj_set_width(ui.labelMax, maxAreaW - 6);
    lv_obj_set_style_text_align(ui.labelMax, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ui.labelMax, "最大\n0");
    lv_obj_set_pos(ui.labelMax, fullW - maxAreaW + 3, 8);

    ui.labelAvg = lv_label_create(parent);
    lv_obj_set_style_text_font(ui.labelAvg, &font_cn_18, 0);
    lv_obj_set_style_text_color(ui.labelAvg, lv_color_hex(0x444444), 0);
    lv_label_set_text(ui.labelAvg, "均速");
    lv_obj_set_pos(ui.labelAvg, 16, SPEED_PANEL_H / 2 - 12);
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

    if (rowH1 < 48) rowH1 = 48;
    if (rowH2 < 48) rowH2 = 48;
    if (rowH3 < 48) rowH3 = 48;

    const lv_coord_t yRow2 = yRow1 + rowH1;
    const lv_coord_t yRow3 = yRow2 + rowH2;

    // 统一画分隔线，避免每个 cell 自己画边框导致重叠、歪斜
    CreateSolidLine(parent, 0, yRow1, fullW, 1);
    CreateSolidLine(parent, 0, yRow2, fullW, 1);
    CreateSolidLine(parent, 0, yRow3, fullW, 1);

    // 中间竖线，只从双列区域开始
    CreateSolidLine(parent, colW, yRow1, 1, fullH - yRow1);

    CreateCell(parent, &ui.cells[0], "时间", "",      0, yTime, fullW, TIME_CELL_H);

    CreateCell(parent, &ui.cells[1], "距离", "km",    0, yRow1, colW, rowH1);
    CreateCell(parent, &ui.cells[2], "总时", "",      colW, yRow1, fullW - colW, rowH1);

    CreateCell(parent, &ui.cells[3], "坡度", "%",     0, yRow2, colW, rowH2);
    CreateCell(parent, &ui.cells[4], "踏频", "rpm",   colW, yRow2, fullW - colW, rowH2);

    CreateCell(parent, &ui.cells[5], "温度", "C",     0, yRow3, colW, rowH3);
    CreateCell(parent, &ui.cells[6], "心率", "bpm",   colW, yRow3, fullW - colW, rowH3);
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
        return Account::RES_OK;
    }

    if (strcmp(param->tran->ID, "PHT") == 0 &&
        param->size == sizeof(HAL::PHT_Info_t))
    {
        memcpy(&instance->phtInfo, param->data_p, param->size);
        return Account::RES_OK;
    }

    return Account::RES_PARAM_ERROR;
}

void RideData::Update()
{
    if (!ui.labelSpeed || !ui.arcSpeed || !ui.labelMax)
    {
        return;
    }

    char buf[32];

    int speed = (int)(sportStatusInfo.speedKph + 0.5f);
    int maxSpeed = (int)(sportStatusInfo.speedMaxKph + 0.5f);

    if (speed < 0)
    {
        speed = 0;
    }

    if (maxSpeed < 0)
    {
        maxSpeed = 0;
    }

    lv_label_set_text_fmt(ui.labelSpeed, "%d", speed);
    lv_arc_set_value(ui.arcSpeed, speed > 60 ? 60 : speed);
    lv_label_set_text_fmt(ui.labelMax, "MAX\n%d", maxSpeed);

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
        lv_label_set_text(ui.cells[3].value, "---");
    }

    if (ui.cells[4].value)
    {
        lv_label_set_text(ui.cells[4].value, "---");
    }

    if (ui.cells[5].value)
    {
        snprintf(buf, sizeof(buf), "%.0f", phtInfo.temperature);
        lv_label_set_text(ui.cells[5].value, buf);
    }

    if (ui.cells[6].value)
    {
        lv_label_set_text(ui.cells[6].value, "---");
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

    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_LEAVE)
    {
        instance->_Manager->Pop();
        return;
    }

    if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_event_get_key(event);

        if (key == LV_KEY_ENTER)
        {
            instance->_Manager->Pop();
            return;
        }

        if (key == LV_KEY_ESC)
        {
            instance->_Manager->Pop();
            return;
        }
    }
}