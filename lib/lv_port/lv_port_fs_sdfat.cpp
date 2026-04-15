#include "lv_port.h"
#include "lvgl.h"
#include "SdFat.h"
#include "HAL.h"

#include <stdio.h>
#include <string.h>

/*********************
 *      DEFINES
 *********************/
#define SD_LETTER '/'

/**********************
 *      TYPEDEFS
 **********************/

/* 用 FsFile，和 SdFs 配套 */
typedef FsFile file_t;
typedef FsFile dir_t;

#define SD_FILE(file_p) ((file_t*)file_p)
#define SD_DIR(dir_p)   ((dir_t*)dir_p)

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void fs_init(void);
static bool fs_ready(lv_fs_drv_t * drv);
static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode);
static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p);
static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br);
static lv_fs_res_t fs_write(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw);
static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence);
static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p);
static void * fs_dir_open(lv_fs_drv_t * drv, const char * path);
static lv_fs_res_t fs_dir_read(lv_fs_drv_t * drv, void * dir_p, char * fn);
static lv_fs_res_t fs_dir_close(lv_fs_drv_t * drv, void * dir_p);

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* 统一把 LVGL 传进来的 path 规整成绝对路径 */
static const char* fs_make_path(const char* path, char* out, size_t out_size)
{
    if(path == nullptr || path[0] == '\0')
    {
        snprintf(out, out_size, "/");
        return out;
    }

    if(path[0] == '/')
    {
        snprintf(out, out_size, "%s", path);
    }
    else
    {
        snprintf(out, out_size, "/%s", path);
    }

    return out;
}

void lv_port_fs_init(void)
{
    fs_init();

    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);

    fs_drv.letter = SD_LETTER;
    fs_drv.ready_cb = fs_ready;
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.write_cb = fs_write;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;

    fs_drv.dir_open_cb = fs_dir_open;
    fs_drv.dir_read_cb = fs_dir_read;
    fs_drv.dir_close_cb = fs_dir_close;

    lv_fs_drv_register(&fs_drv);
}

static bool fs_ready(lv_fs_drv_t * drv)
{
    (void)drv;
    return HAL::SD_GetReady();
}

static void fs_init(void)
{
    // SD 初始化已经在 HAL::SD_Init() 里做了
}

/**
 * Open a file
 */
static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
    (void)drv;

    oflag_t oflag = O_RDONLY;

    if((mode & LV_FS_MODE_WR) && (mode & LV_FS_MODE_RD))
    {
        oflag = O_RDWR | O_CREAT;
    }
    else if(mode & LV_FS_MODE_WR)
    {
        oflag = O_WRONLY | O_CREAT;
    }
    else
    {
        oflag = O_RDONLY;
    }

    char fixed_path[256];
    fs_make_path(path, fixed_path, sizeof(fixed_path));

    file_t* file_p = new file_t();
    if(file_p == nullptr)
    {
        return nullptr;
    }

    if(!file_p->open(fixed_path, oflag))
    {
        delete file_p;
        return nullptr;
    }

    return file_p;
}

/**
 * Close an opened file
 */
static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p)
{
    (void)drv;

    if(file_p == nullptr)
    {
        return LV_FS_RES_INV_PARAM;
    }

    bool ok = SD_FILE(file_p)->close();
    delete SD_FILE(file_p);

    return ok ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

/**
 * Read data from an opened file
 */
static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
{
    (void)drv;

    if(file_p == nullptr || buf == nullptr || br == nullptr)
    {
        return LV_FS_RES_INV_PARAM;
    }

    int ret = SD_FILE(file_p)->read(buf, btr);
    if(ret < 0)
    {
        *br = 0;
        return LV_FS_RES_FS_ERR;
    }

    *br = (uint32_t)ret;
    return LV_FS_RES_OK;
}

/**
 * Write into a file
 */
static lv_fs_res_t fs_write(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw)
{
    (void)drv;

    if(file_p == nullptr || buf == nullptr || bw == nullptr)
    {
        return LV_FS_RES_INV_PARAM;
    }

    int ret = SD_FILE(file_p)->write((const uint8_t*)buf, btw);
    if(ret < 0)
    {
        *bw = 0;
        return LV_FS_RES_FS_ERR;
    }

    *bw = (uint32_t)ret;
    return LV_FS_RES_OK;
}

/**
 * Set the read write pointer
 */
static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence)
{
    (void)drv;

    if(file_p == nullptr)
    {
        return LV_FS_RES_INV_PARAM;
    }

    bool ok = false;

    if(whence == LV_FS_SEEK_SET)
    {
        ok = SD_FILE(file_p)->seekSet(pos);
    }
    else if(whence == LV_FS_SEEK_CUR)
    {
        ok = SD_FILE(file_p)->seekCur((int32_t)pos);
    }
    else if(whence == LV_FS_SEEK_END)
    {
        /* LVGL 这里 pos 是无符号，通常传 0。
           这里按“从文件尾往回偏移 pos”处理。 */
        ok = SD_FILE(file_p)->seekEnd(-(int32_t)pos);
    }
    else
    {
        return LV_FS_RES_INV_PARAM;
    }

    return ok ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

/**
 * Tell the position of the read write pointer
 */
static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p)
{
    (void)drv;

    if(file_p == nullptr || pos_p == nullptr)
    {
        return LV_FS_RES_INV_PARAM;
    }

    *pos_p = (uint32_t)SD_FILE(file_p)->curPosition();
    return LV_FS_RES_OK;
}

/**
 * Open a directory
 */
static void * fs_dir_open(lv_fs_drv_t * drv, const char * path)
{
    (void)drv;

    char fixed_path[256];
    fs_make_path(path, fixed_path, sizeof(fixed_path));

    dir_t* dir_p = new dir_t();
    if(dir_p == nullptr)
    {
        return nullptr;
    }

    if(!dir_p->open(fixed_path, O_RDONLY) || !dir_p->isDir())
    {
        delete dir_p;
        return nullptr;
    }

    return dir_p;
}

/**
 * Read the next filename from a directory.
 * Directories begin with '/'
 */
static lv_fs_res_t fs_dir_read(lv_fs_drv_t * drv, void * dir_p, char * fn)
{
    (void)drv;

    if(dir_p == nullptr || fn == nullptr)
    {
        return LV_FS_RES_INV_PARAM;
    }

    file_t entry;
    char name[128];

    while(entry.openNext(SD_DIR(dir_p), O_RDONLY))
    {
        name[0] = '\0';
        entry.getName(name, sizeof(name));

        if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        {
            entry.close();
            continue;
        }

        if(entry.isDir())
        {
            fn[0] = '/';
            strcpy(&fn[1], name);
        }
        else
        {
            strcpy(fn, name);
        }

        entry.close();
        return LV_FS_RES_OK;
    }

    /* 读到末尾 */
    fn[0] = '\0';
    return LV_FS_RES_OK;
}

/**
 * Close the directory
 */
static lv_fs_res_t fs_dir_close(lv_fs_drv_t * drv, void * dir_p)
{
    (void)drv;

    if(dir_p == nullptr)
    {
        return LV_FS_RES_INV_PARAM;
    }

    bool ok = SD_DIR(dir_p)->close();
    delete SD_DIR(dir_p);

    return ok ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}