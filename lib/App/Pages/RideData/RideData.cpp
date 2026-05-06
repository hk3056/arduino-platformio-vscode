#include "RideData.h"

LV_FONT_DECLARE(font_cn_18);

using namespace Page;

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

    lv_obj_add_event_cb(ui.cont, onEvent, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(_root, onEvent, LV_EVENT_ALL, this);
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
    lv_obj_set_size(ui.cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(ui.cont, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ui.cont, LV_OPA_COVER, 0);
    lv_obj_add_flag(ui.cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui.cont, LV_OBJ_FLAG_SCROLLABLE);

    CreateSpeedPanel(ui.cont);
    CreateCells(ui.cont);
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
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_set_size(panel, LV_HOR_RES, 116);
    lv_obj_set_style_bg_color(panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x999999), 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    ui.arcSpeed = lv_arc_create(panel);
    lv_obj_set_size(ui.arcSpeed, 112, 112);
    lv_obj_align(ui.arcSpeed, LV_ALIGN_TOP_MID, 0, 2);
    lv_arc_set_range(ui.arcSpeed, 0, 60);
    lv_arc_set_value(ui.arcSpeed, 0);
    lv_obj_remove_style(ui.arcSpeed, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(ui.arcSpeed, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(ui.arcSpeed, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui.arcSpeed, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui.arcSpeed, lv_color_hex(0xd8d8d8), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui.arcSpeed, lv_color_hex(0x57c7d9), LV_PART_INDICATOR);

    ui.labelSpeed = lv_label_create(panel);
    lv_obj_set_style_text_font(ui.labelSpeed, ResourcePool::GetFont("bahnschrift_65"), 0);
    lv_obj_set_style_text_color(ui.labelSpeed, lv_color_black(), 0);
    lv_label_set_text(ui.labelSpeed, "0");
    lv_obj_align(ui.labelSpeed, LV_ALIGN_CENTER, 0, -4);

    lv_obj_t* unit = lv_label_create(panel);
    lv_obj_set_style_text_font(unit, ResourcePool::GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(unit, lv_color_hex(0x555555), 0);
    lv_label_set_text(unit, "km/h");
    lv_obj_align(unit, LV_ALIGN_BOTTOM_MID, 0, -8);

    ui.labelMax = lv_label_create(panel);
    lv_obj_set_style_text_font(ui.labelMax, ResourcePool::GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(ui.labelMax, lv_color_black(), 0);
    lv_label_set_text(ui.labelMax, "最大\n--");
    lv_obj_align(ui.labelMax, LV_ALIGN_TOP_RIGHT, -8, 8);

    ui.labelAvg = lv_label_create(panel);
    lv_obj_set_style_text_font(ui.labelAvg, &font_cn_18, 0);
    lv_obj_set_style_text_color(ui.labelAvg, lv_color_hex(0x444444), 0);
    lv_label_set_text(ui.labelAvg, "当前\n平均");
    lv_obj_align(ui.labelAvg, LV_ALIGN_LEFT_MID, 10, 18);
}

void RideData::CreateCells(lv_obj_t* parent)
{
    CreateCell(parent, &ui.cells[0], "时间", "",     0, 116, LV_HOR_RES, 54);

    CreateCell(parent, &ui.cells[1], "距离", "km",   0, 170, LV_HOR_RES / 2, 50);
    CreateCell(parent, &ui.cells[2], "总用时", "",   LV_HOR_RES / 2, 170, LV_HOR_RES / 2, 50);

    CreateCell(parent, &ui.cells[3], "坡度", "%",    0, 220, LV_HOR_RES / 2, 50);
    CreateCell(parent, &ui.cells[4], "踏频", "rpm", LV_HOR_RES / 2, 220, LV_HOR_RES / 2, 50);

    CreateCell(parent, &ui.cells[5], "温度", "℃",    0, 270, LV_HOR_RES / 2, 50);
    CreateCell(parent, &ui.cells[6], "心率", "bpm", LV_HOR_RES / 2, 270, LV_HOR_RES / 2, 50);
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
    lv_obj_set_style_bg_color(cell->cont, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(cell->cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cell->cont, 1, 0);
    lv_obj_set_style_border_color(cell->cont, lv_color_hex(0x999999), 0);
    lv_obj_clear_flag(cell->cont, LV_OBJ_FLAG_SCROLLABLE);

    cell->title = lv_label_create(cell->cont);
    lv_obj_set_style_text_font(cell->title, &font_cn_18, 0);
    lv_obj_set_style_text_color(cell->title, lv_color_hex(0x333333), 0);
    lv_label_set_text(cell->title, title);
    lv_obj_align(cell->title, LV_ALIGN_TOP_MID, 0, -1);

    cell->value = lv_label_create(cell->cont);
    lv_obj_set_style_text_font(cell->value, ResourcePool::GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(cell->value, lv_color_black(), 0);
    lv_label_set_text(cell->value, "---");
    lv_obj_align(cell->value, LV_ALIGN_CENTER, 0, 8);

    cell->unit = lv_label_create(cell->cont);
    lv_obj_set_style_text_font(cell->unit, ResourcePool::GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(cell->unit, lv_color_hex(0x333333), 0);
    lv_label_set_text(cell->unit, unit);
    lv_obj_align(cell->unit, LV_ALIGN_BOTTOM_RIGHT, -4, -2);
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
    char buf[32];

    int speed = (int)(sportStatusInfo.speedKph + 0.5f);
    int maxSpeed = (int)(sportStatusInfo.speedMaxKph + 0.5f);

    lv_label_set_text_fmt(ui.labelSpeed, "%d", speed);
    lv_arc_set_value(ui.arcSpeed, speed > 60 ? 60 : speed);

    lv_label_set_text_fmt(ui.labelMax, "最大\n%d", maxSpeed);

    lv_label_set_text(
        ui.cells[0].value,
        DataProc::MakeTimeString(sportStatusInfo.singleTime, buf, sizeof(buf))
    );

    snprintf(buf, sizeof(buf), "%.1f", sportStatusInfo.singleDistance / 1000.0f);
    lv_label_set_text(ui.cells[1].value, buf);

    lv_label_set_text(
        ui.cells[2].value,
        DataProc::MakeTimeString(sportStatusInfo.totalTime, buf, sizeof(buf))
    );

    // 目前 SportStatus 里没有坡度、踏频、心率数据，先显示 ---
    lv_label_set_text(ui.cells[3].value, "---");
    lv_label_set_text(ui.cells[4].value, "---");

    snprintf(buf, sizeof(buf), "%.0f", phtInfo.temperature);
    lv_label_set_text(ui.cells[5].value, buf);

    lv_label_set_text(ui.cells[6].value, "---");
}

void RideData::onTimerUpdate(lv_timer_t* timer)
{
    RideData* instance = (RideData*)timer->user_data;
    instance->Update();
}

void RideData::onEvent(lv_event_t* event)
{
    RideData* instance = (RideData*)lv_event_get_user_data(event);
    LV_ASSERT_NULL(instance);

    lv_event_code_t code = lv_event_get_code(event);

    // 返回主页面
    if (code == LV_EVENT_LEAVE)
    {
        instance->_Manager->Pop();
        return;
    }

    if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_event_get_key(event);

        if (key == LV_KEY_ESC || key == LV_KEY_ENTER)
        {
            instance->_Manager->Pop();
            return;
        }
    }
}