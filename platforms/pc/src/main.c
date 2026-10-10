#include <psyz.h>
#include <libgpu.h>
#include <libetc.h>
#include <stdio.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define OTSIZE 1

typedef struct DB {
    DRAWENV draw;
    DISPENV disp;
    OT_TYPE ot[OTSIZE];
} DB;

static DB db[2];
static DB *cdb;
u_long _ramsize = 0x00200000;
u_long _stacksize = 0x00004000;

static void init_graphics(void) {
    SetDefDrawEnv(&db[0].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDrawEnv(&db[1].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[0].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[1].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetVideoMode(MODE_NTSC);
    ResetGraph(0);
    PutDrawEnv(&db[0].draw);
    PutDispEnv(&db[0].disp);
    ClearOTag(db[0].ot, OTSIZE);
    ClearOTag(db[1].ot, OTSIZE);
    SetDispMask(1);
    cdb = &db[0];
}

static void present(void) {
    cdb = (cdb == &db[0]) ? &db[1] : &db[0];
    DrawSync(0);
    VSync(0);
    ClearImage(&cdb->draw.clip, 16, 32, 64);
    DrawOTag(&cdb->ot[0]);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int frame = 0;
    init_graphics();
    FntLoad(960, 256);
    SetDumpFnt(FntOpen(8, 16, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 512));
    printf("dw3-pc Phase 1 skeleton\n");
    while (!Psyz_QuitRequested()) {
        FntPrint("dw3-pc Phase 1\n");
        FntPrint("frame %d\n", frame++);
        FntPrint("close window to quit\n");
        FntFlush(-1);
        present();
    }
    return 0;
}
