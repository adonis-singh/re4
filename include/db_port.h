#ifndef DB_PORT_H
#define DB_PORT_H

#include "types.h"
#include "vec.h"
#include "esp.h"

// t_esp REL, D:/Bio4/Prog/db_port.cpp: the bridge between the effect tool (t_esp.cpp) and the game
// (model viewer, event debug data, light tool, drawing helpers, the .cfg model set loader). The entry
// points have C linkage in the .sym. Only the exports other units call are declared here; t_esp.cpp
// keeps a few local prototypes whose vendor-side parameter lists differ from the definitions.

class DB_KEYBORD;

extern int db_modelNo;   // model viewer slot the tool edits (the BasePos "WorKNo" numeric)

extern "C" {
u8 DB_GetStageNo();
u8 DB_GetRoomNo();
int LoadData(const char* path, void* buf);
void SaveData(const char* path, void* buf, int size);
int DB_ConfigLoad(const char* file);
void DB_SetFog(int on);
void DB_SetCinesco(int on);
void DB_SetMotionCam(int on);
void DB_SetBgColor(u8 r, u8 g, u8 b, u8 a);
void DB_DrawGrid(int on);
void DB_DrawMod_sk(int on);
void DB_WorkPush(int flags, int emArray);
void DB_WorkPop(int flags, int emArray);
void DB_GetCamFrontPos(f32 dist, f32* x, f32* y, f32* z);
void DB_DrawCursor2D(Vec* pos);
int DB_isGetComeEventTool();
void DB_EventCamStart();
void DB_RoomCamStart(int cut);
void DB_EffDelete();
void DB_DispProc();
void DB_Sleep(int n);
int DB_IsEmLoad();
int DB_IsWorkPush();
void DB_GetKeybordData(DB_KEYBORD* k);
int LoadModel();
void EspToolExitEstSet(EspSeqData* head, int on, int mode);
void EspToolCameraMode();
void CoreEstSet(u8 id);
void SeqSet(EspSeqData* head, int mode);
void sp_tex_trans(int no);
}

void EspToolInit(bool& out, u8& stage, u8& cut);

#endif
