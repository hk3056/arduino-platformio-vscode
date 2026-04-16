#include "LiveMap.h"
#include "Config/Config.h"

using namespace Page;

uint16_t LiveMap::mapLevelCurrent = CONFIG_LIVE_MAP_LEVEL_DEFAULT;

// 缩放优化相关状态，只在 LiveMap 页面内部使用
static bool s_zoomDirty = false;
static bool s_refreshFromZoom = false;
static bool s_needReloadTrack = false;
static uint32_t s_lastZoomChangeTime = 0;
static int32_t s_lastMapX = 0;
static int32_t s_lastMapY = 0;

LiveMap::LiveMap()
{
    memset(&priv, 0, sizeof(priv));
}

LiveMap::~LiveMap()
{
}

void LiveMap::onCustomAttrConfig()
{
    SetCustomCacheEnable(false);
}

void LiveMap::onViewLoad()
{
    const uint32_t tileSize = 256;

    Model.tileConv.SetTileSize(tileSize);
    Model.tileConv.SetViewSize(
        CONFIG_LIVE_MAP_VIEW_WIDTH,
        CONFIG_LIVE_MAP_VIEW_HEIGHT
    );
    Model.tileConv.SetFocusPos(0, 0);

    TileConv::Rect_t rect;
    uint32_t tileNum = Model.tileConv.GetTileContainer(&rect);

    View.Create(_root, tileNum);

    lv_slider_set_range(
        View.ui.zoom.slider,
        Model.mapConv.GetLevelMin(),
        Model.mapConv.GetLevelMax()
    );

    View.SetMapTile(tileSize, rect.width / tileSize);

#if CONFIG_LIVE_MAP_DEBUG_ENABLE
    lv_obj_t* contView = lv_obj_create(root);
    lv_obj_center(contView);
    lv_obj_set_size(contView, CONFIG_LIVE_MAP_VIEW_WIDTH, CONFIG_LIVE_MAP_VIEW_HEIGHT);
    lv_obj_set_style_border_color(contView, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_border_width(contView, 1, 0);
#endif

    AttachEvent(_root);
    AttachEvent(View.ui.zoom.slider);
    AttachEvent(View.ui.sportInfo.cont);

    lv_slider_set_value(View.ui.zoom.slider, mapLevelCurrent, LV_ANIM_OFF);
    Model.mapConv.SetLevel(mapLevelCurrent);

    lv_obj_add_flag(View.ui.map.cont, LV_OBJ_FLAG_HIDDEN);

    /* Point filter */
    Model.pointFilter.SetOffsetThreshold(CONFIG_TRACK_FILTER_OFFSET_THRESHOLD);
    Model.pointFilter.SetOutputPointCallback([](TrackPointFilter* filter, const TrackPointFilter::Point_t* point) {
        LiveMap* instance = (LiveMap*)filter->userData;
        instance->TrackLineAppendToEnd((int32_t)point->x, (int32_t)point->y);
    });
    Model.pointFilter.userData = this;

    /* Line filter */
    Model.lineFilter.SetOutputPointCallback(onTrackLineEvent);
    Model.lineFilter.userData = this;
}

void LiveMap::onViewDidLoad()
{
}

void LiveMap::onViewWillAppear()
{
    lv_obj_set_style_opa(_root, LV_OPA_COVER, LV_PART_MAIN);

    Model.Init();

    char theme[16];
    Model.GetArrowTheme(theme, sizeof(theme));
    View.SetArrowTheme(theme);

    priv.isTrackAvtive = Model.GetTrackFilterActive();
    Model.SetStatusBarStyle(DataProc::STATUS_BAR_STYLE_BLACK);

    SportInfoUpdate();

    lv_obj_clear_flag(View.ui.labelInfo, LV_OBJ_FLAG_HIDDEN);

    s_zoomDirty = false;
    s_refreshFromZoom = false;
    s_needReloadTrack = false;
    s_lastZoomChangeTime = 0;
    s_lastMapX = 0;
    s_lastMapY = 0;
}

void LiveMap::onViewDidAppear()
{
    // 从 100ms 改成 33ms，缩放和拖动反馈会明显更紧
    priv.timer = lv_timer_create([](lv_timer_t* timer) {
        LiveMap* instance = (LiveMap*)timer->user_data;
        instance->Update();
    }, 33, this);

    priv.lastMapUpdateTime = 0;

    lv_obj_clear_flag(View.ui.map.cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(View.ui.labelInfo, LV_OBJ_FLAG_HIDDEN);

    priv.lastTileContOriPoint.x = 0;
    priv.lastTileContOriPoint.y = 0;
    priv.isTrackAvtive = Model.GetTrackFilterActive();

    if (!priv.isTrackAvtive)
    {
        Model.pointFilter.SetOutputPointCallback(nullptr);
    }

    lv_group_t* group = lv_group_get_default();
    lv_group_add_obj(group, View.ui.zoom.slider);
    lv_group_set_editing(group, View.ui.zoom.slider);

    // 页面刚出现时先强制刷新一次
    s_zoomDirty = true;
}

void LiveMap::onViewWillDisappear()
{
    if (priv.timer)
    {
        lv_timer_del(priv.timer);
        priv.timer = nullptr;
    }

    lv_obj_add_flag(View.ui.map.cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_fade_out(_root, 250, 250);
}

void LiveMap::onViewDidDisappear()
{
    Model.Deinit();
}

void LiveMap::onViewUnload()
{
    View.Delete();
}

void LiveMap::onViewDidUnload()
{
}

void LiveMap::AttachEvent(lv_obj_t* obj)
{
    lv_obj_add_event_cb(obj, onEvent, LV_EVENT_ALL, this);
}

void LiveMap::Update()
{
    // 缩放时优先刷新地图，不等 GPS 周期
    if (s_zoomDirty)
    {
        CheckPosition();
        s_zoomDirty = false;
    }
    else if (lv_tick_elaps(priv.lastMapUpdateTime) >= CONFIG_GPS_REFR_PERIOD)
    {
        CheckPosition();
        SportInfoUpdate();
        priv.lastMapUpdateTime = lv_tick_get();
    }
    else if (lv_tick_elaps(priv.lastContShowTime) >= 3000)
    {
        lv_obj_add_state(View.ui.zoom.cont, LV_STATE_USER_1);
    }

    // 缩放过程中先不重载整条轨迹，等用户停手后再补
    if (s_needReloadTrack && priv.isTrackAvtive && lv_tick_elaps(s_lastZoomChangeTime) >= 250)
    {
        TileConv::Rect_t rect;
        Model.tileConv.GetTileContainer(&rect);

        Area_t area = {
            .x0 = rect.x,
            .y0 = rect.y,
            .x1 = rect.x + rect.width - 1,
            .y1 = rect.y + rect.height - 1
        };

        TrackLineReload(&area, s_lastMapX, s_lastMapY);
        s_needReloadTrack = false;
    }
}

void LiveMap::UpdateDelay(uint32_t ms)
{
    // 保留接口，避免头文件改动
    priv.lastMapUpdateTime = lv_tick_get() - 1000 + ms;
}

void LiveMap::SportInfoUpdate()
{
    lv_label_set_text_fmt(
        View.ui.sportInfo.labelSpeed,
        "%02d",
        (int)Model.sportStatusInfo.speedKph
    );

    lv_label_set_text_fmt(
        View.ui.sportInfo.labelTrip,
        "%0.1f km",
        Model.sportStatusInfo.singleDistance / 1000
    );

    char buf[16];
    lv_label_set_text(
        View.ui.sportInfo.labelTime,
        DataProc::MakeTimeString(Model.sportStatusInfo.singleTime, buf, sizeof(buf))
    );
}

void LiveMap::CheckPosition()
{
    bool refreshMap = false;
    bool zoomChanged = false;

    HAL::GPS_Info_t gpsInfo;
    Model.GetGPS_Info(&gpsInfo);

    mapLevelCurrent = lv_slider_get_value(View.ui.zoom.slider);

    if (mapLevelCurrent != Model.mapConv.GetLevel())
    {
        refreshMap = true;
        zoomChanged = true;
        Model.mapConv.SetLevel(mapLevelCurrent);
    }

    int32_t mapX, mapY;
    Model.mapConv.ConvertMapCoordinate(
        gpsInfo.longitude,
        gpsInfo.latitude,
        &mapX,
        &mapY
    );

    s_lastMapX = mapX;
    s_lastMapY = mapY;

    Model.tileConv.SetFocusPos(mapX, mapY);

    if (GetIsMapTileContChanged())
    {
        refreshMap = true;
    }

    if (refreshMap)
    {
        TileConv::Rect_t rect;
        Model.tileConv.GetTileContainer(&rect);

        Area_t area = {
            .x0 = rect.x,
            .y0 = rect.y,
            .x1 = rect.x + rect.width - 1,
            .y1 = rect.y + rect.height - 1
        };

        s_refreshFromZoom = zoomChanged;
        onMapTileContRefresh(&area, mapX, mapY);
    }

    HAL::MAG_Info_t magInfo = { 0 };
    bool hasMag = Model.GetMAG_Info(&magInfo);

    float arrowCourse = gpsInfo.course;

    // 时速低于 5 km/h 时优先用磁力计
    if (Model.sportStatusInfo.speedKph < 5.0f && hasMag)
    {
        arrowCourse = magInfo.dir;
    }

    MapTileContUpdate(mapX, mapY, arrowCourse);
    // 缩放那一瞬间不追加轨迹点，避免额外计算
    if (priv.isTrackAvtive && !zoomChanged)
    {
        Model.pointFilter.PushPoint(mapX, mapY);
    }
}

void LiveMap::onMapTileContRefresh(const Area_t* area, int32_t x, int32_t y)
{
    LV_LOG_INFO(
        "area: (%d, %d) [%dx%d]",
        area->x0,
        area->y0,
        area->x1 - area->x0 + 1,
        area->y1 - area->y0 + 1
    );

    MapTileContReload();

    if (priv.isTrackAvtive)
    {
        if (s_refreshFromZoom)
        {
            s_needReloadTrack = true;
            s_lastZoomChangeTime = lv_tick_get();
        }
        else
        {
            TrackLineReload(area, x, y);
        }
    }

    s_refreshFromZoom = false;
}

void LiveMap::MapTileContUpdate(int32_t mapX, int32_t mapY, float course)
{
    TileConv::Point_t offset;
    TileConv::Point_t curPoint = { mapX, mapY };

    Model.tileConv.GetOffset(&offset, &curPoint);

    /* arrow */
    lv_obj_t* img = View.ui.map.imgArrow;
    Model.tileConv.GetFocusOffset(&offset);

    lv_coord_t x = offset.x - lv_obj_get_width(img) / 2;
    lv_coord_t y = offset.y - lv_obj_get_height(img) / 2;
    View.SetImgArrowStatus(x, y, course);

    /* active line */
    if (priv.isTrackAvtive)
    {
        View.SetLineActivePoint((lv_coord_t)offset.x, (lv_coord_t)offset.y);
    }

    /* map cont */
    Model.tileConv.GetTileContainerOffset(&offset);

    lv_coord_t baseX = (LV_HOR_RES - CONFIG_LIVE_MAP_VIEW_WIDTH) / 2;
    lv_coord_t baseY = (LV_VER_RES - CONFIG_LIVE_MAP_VIEW_HEIGHT) / 2;
    lv_obj_set_pos(View.ui.map.cont, baseX - offset.x, baseY - offset.y);
}

void LiveMap::MapTileContReload()
{
    for (uint32_t i = 0; i < View.ui.map.tileNum; i++)
    {
        TileConv::Point_t pos;
        Model.tileConv.GetTilePos(i, &pos);

        char path[64];
        Model.mapConv.ConvertMapPath(pos.x, pos.y, path, sizeof(path));
        View.SetMapTileSrc(i, path);
    }
}

bool LiveMap::GetIsMapTileContChanged()
{
    TileConv::Point_t pos;
    Model.tileConv.GetTilePos(0, &pos);

    bool ret = (
        pos.x != priv.lastTileContOriPoint.x ||
        pos.y != priv.lastTileContOriPoint.y
    );

    priv.lastTileContOriPoint = pos;
    return ret;
}

void LiveMap::TrackLineReload(const Area_t* area, int32_t x, int32_t y)
{
    Model.lineFilter.SetClipArea(area);
    Model.lineFilter.Reset();

    Model.TrackReload([](TrackPointFilter* filter, const TrackPointFilter::Point_t* point) {
        LiveMap* instance = (LiveMap*)filter->userData;
        instance->Model.lineFilter.PushPoint((int32_t)point->x, (int32_t)point->y);
    }, this);

    Model.lineFilter.PushPoint(x, y);
    Model.lineFilter.PushEnd();
}

void LiveMap::TrackLineAppend(int32_t x, int32_t y)
{
    TileConv::Point_t offset;
    TileConv::Point_t curPoint = { x, y };
    Model.tileConv.GetOffset(&offset, &curPoint);
    View.ui.track.lineTrack->append((lv_coord_t)offset.x, (lv_coord_t)offset.y);
}

void LiveMap::TrackLineAppendToEnd(int32_t x, int32_t y)
{
    TileConv::Point_t offset;
    TileConv::Point_t curPoint = { x, y };
    Model.tileConv.GetOffset(&offset, &curPoint);
    View.ui.track.lineTrack->append_to_end((lv_coord_t)offset.x, (lv_coord_t)offset.y);
}

void LiveMap::onTrackLineEvent(TrackLineFilter* filter, TrackLineFilter::Event_t* event)
{
    LiveMap* instance = (LiveMap*)filter->userData;
    lv_poly_line* lineTrack = instance->View.ui.track.lineTrack;

    switch (event->code)
    {
    case TrackLineFilter::EVENT_START_LINE:
        lineTrack->start();
        instance->TrackLineAppend(event->point->x, event->point->y);
        break;

    case TrackLineFilter::EVENT_APPEND_POINT:
        instance->TrackLineAppend(event->point->x, event->point->y);
        break;

    case TrackLineFilter::EVENT_END_LINE:
        if (event->point != nullptr)
        {
            instance->TrackLineAppend(event->point->x, event->point->y);
        }
        lineTrack->stop();
        break;

    case TrackLineFilter::EVENT_RESET:
        lineTrack->reset();
        break;

    default:
        break;
    }
}

void LiveMap::onEvent(lv_event_t* event)
{
    LiveMap* instance = (LiveMap*)lv_event_get_user_data(event);
    LV_ASSERT_NULL(instance);

    lv_obj_t* obj = lv_event_get_current_target(event);
    lv_event_code_t code = lv_event_get_code(event);

    // 通用返回：右滑离开
    if (code == LV_EVENT_LEAVE)
    {
        instance->_Manager->Pop();
        return;
    }

    // 关键修复：处理按键/编码器确认键与返回键
    if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_event_get_key(event);

        if (key == LV_KEY_ESC || key == LV_KEY_ENTER)
        {
            instance->_Manager->Pop();
            return;
        }
    }

    if (obj == instance->View.ui.zoom.slider)
    {
        if (code == LV_EVENT_VALUE_CHANGED)
        {
            int32_t level = lv_slider_get_value(obj);
            int32_t levelMax = instance->Model.mapConv.GetLevelMax();

            lv_label_set_text_fmt(instance->View.ui.zoom.labelInfo, "%d/%d", level, levelMax);
            lv_obj_clear_state(instance->View.ui.zoom.cont, LV_STATE_USER_1);

            instance->priv.lastContShowTime = lv_tick_get();

            // 不再走原来的 200ms 延时，下一帧直接刷新地图
            s_lastZoomChangeTime = lv_tick_get();
            s_zoomDirty = true;
        }
        else if (code == LV_EVENT_PRESSED || code == LV_EVENT_SHORT_CLICKED)
        {
            instance->_Manager->Pop();
            return;
        }
    }

    if (obj == instance->View.ui.sportInfo.cont)
    {
        if (code == LV_EVENT_PRESSED || code == LV_EVENT_SHORT_CLICKED)
        {
            instance->_Manager->Pop();
            return;
        }
    }
}
