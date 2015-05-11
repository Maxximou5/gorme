#include "extension.h"
#include "tmpbrush.h"
#include <toolframework/itoolentity.h>
#include <IGameHelpers.h>
#include <vscript/ivscript.h>
#include "vscript_fun.h"
#include "brush.h"
#include <KeyValues.h>
#include <filesystem.h>
#include "mdlcompile.h"
#include "downloader.h"
#include "callqueue.h"
#include "entfilter.h"

CGorme g_Gorme;
SMEXT_LINK(&g_Gorme);

IServerTools * g_pServerTools = NULL;
IScriptManager * g_pScriptManager = NULL;
IServerGameEnts * g_pServerGameEnts = NULL;

CVsfun * g_pVsfun = NULL;
KeyValues * g_pGormeConfig = NULL;
CMdlCompile * g_pMdlCompile = NULL;
CDownloader * g_pDownloader = NULL;
CCallQueue * g_pCallQueue = NULL;
CEntFilter *g_pEntFilter = NULL;

CThreadMutex g_tickMutex;

bool CGorme::SDK_OnLoad(char *error, size_t maxlength, bool late) {
	g_tickMutex.Lock();
	g_pGormeConfig = new KeyValues("Gorme Config");
	g_pGormeConfig->LoadFromFile(g_pFullFileSystem, "cfg/sourcemod/gorme.cfg");
	g_pMdlCompile = new CMdlCompile();
	g_pCallQueue = new CCallQueue();
	return true;
}

CUtlVector<int> hookIds;
SH_DECL_HOOK1_void(IScriptVM, RegisterFunction, SH_NOATTRIB, 0, ScriptFunctionBinding_t*);
SH_DECL_HOOK1(IScriptVM, RegisterClass, SH_NOATTRIB, 0, bool, ScriptClassDesc_t*);
SH_DECL_HOOK1_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool);
SH_DECL_HOOK3_void(IServerGameEnts, CheckTransmit, SH_NOATTRIB, 0, CCheckTransmitInfo *, const unsigned short *, int);
SH_DECL_HOOK2(IBaseFileSystem, FileExists, SH_NOATTRIB, 0, bool, const char *, const char *);

bool CGorme::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late) {
	//todo error msgs?
	GET_V_IFACE_ANY(GetPhysicsFactory, g_pPhysicsCollision, IPhysicsCollision, VPHYSICS_COLLISION_INTERFACE_VERSION);
	if(!g_pPhysicsCollision)
		return false;
	GET_V_IFACE_ANY(GetServerFactory, g_pServerTools, IServerTools, VSERVERTOOLS_INTERFACE_VERSION);
	if(!g_pServerTools)
		return false;
	GET_V_IFACE_ANY(GetEngineFactory, g_pScriptManager, IScriptManager, VSCRIPT_INTERFACE_VERSION);
	if(!g_pScriptManager)
		return false;
	GET_V_IFACE_ANY(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	if(!g_pFullFileSystem)
		return false;
	GET_V_IFACE_ANY(GetServerFactory, g_pServerGameEnts, IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS);
	if(!g_pServerGameEnts)
		return false;

	g_pVsfun = new CVsfun();
	g_pEntFilter = new CEntFilter();
	g_pDownloader = new CDownloader();

	IScriptVM * svm = g_pScriptManager->CreateVM();
	hookIds.AddToTail(SH_ADD_VPHOOK(IScriptVM, RegisterFunction, svm, SH_MEMBER(g_pVsfun, &CVsfun::OnRegisterFunction), true));
	hookIds.AddToTail(SH_ADD_VPHOOK(IScriptVM, RegisterClass, svm, SH_MEMBER(g_pVsfun, &CVsfun::OnRegisterClass), true));
	g_pScriptManager->DestroyVM(svm);
	hookIds.AddToTail(SH_ADD_VPHOOK(IServerGameDLL, GameFrame, gamedll, SH_MEMBER(this, &CGorme::OnGameFrame), false));
	hookIds.AddToTail(SH_ADD_VPHOOK(IServerGameEnts, CheckTransmit, g_pServerGameEnts, SH_MEMBER(g_pEntFilter, &CEntFilter::OnCheckTransmit), true));
	hookIds.AddToTail(SH_ADD_VPHOOK(IBaseFileSystem, FileExists, g_pFullFileSystem, SH_MEMBER(g_pDownloader, &CDownloader::OnFileExists), false));
	return true;
}

void CGorme::SDK_OnUnload() {
	FOR_EACH_VEC(hookIds, it)
		SH_REMOVE_HOOK_ID(hookIds[it]);
	delete g_pVsfun;
	g_pGormeConfig->deleteThis();
	delete g_pMdlCompile;
	delete g_pDownloader;
	delete g_pCallQueue;
	delete g_pEntFilter;
}

void CGorme::OnGameFrame(bool simulating) {
	g_pDownloader->Tick();
	(*g_pCallQueue)();
	g_tickMutex.Unlock();
	ThreadSleep(1);
	g_tickMutex.Lock();
	RETURN_META(MRES_HANDLED);
}

cell_t GormeTest(IPluginContext *pContext, const cell_t *params) {
	CTmpBrush tmpbrush;
	Vector points[] = {
		Vector(-8, -8, -8),
		Vector(8, -8, -8),
		Vector(-8, 8, -8),
		Vector(8, 8, -8),
		Vector(-8, -8, 8),
		Vector(8, -8, 8),
		Vector(-8, 8, 8),
		Vector(8, 8, 8),
	};
	for(int i = 0; i < 1; i++) {
		tmpbrush.SetPoints(points, 8);
		auto brush = new CBrush();
		brush->ApplyTmpBrush(&tmpbrush);
		for(int j = 0; j < 8; j++) {
			Vector random;
			random.Random(-8, 8);
			points[j] += random;
		}
	}
	return 0;
}

cell_t GormeTest2(IPluginContext *pContext, const cell_t *params) {
	g_pEntFilter->ClearAll();
	return 0;
}

const sp_nativeinfo_t MyNatives[] =
{
	{"GormeTest", GormeTest},
	{"GormeTest2", GormeTest2},
	{NULL, NULL},
};

void CGorme::SDK_OnAllLoaded() {
	sharesys->AddNatives(myself, MyNatives);
}