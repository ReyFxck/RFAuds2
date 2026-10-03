#ifndef RFAUDS2_TEST_SIFRPC_H
#define RFAUDS2_TEST_SIFRPC_H
#include <tamtypes.h>
#define SIF_RPC_M_NOWAIT 1
typedef void *(*SifRpcFunc_t)(int, void *, int);
typedef void (*SifRpcEndFunc_t)(void *);
typedef struct { void *server; } SifRpcClientData_t;
typedef struct { int unused; } SifRpcDataQueue_t;
typedef struct { int unused; } SifRpcServerData_t;
void sceSifInitRpc(int);
int sceSifBindRpc(SifRpcClientData_t *, int, int);
int sceSifCallRpc(SifRpcClientData_t *, int, int, void *, int,
    void *, int, SifRpcEndFunc_t, void *);
int sceSifCheckStatRpc(SifRpcClientData_t *);
void sceSifSetRpcQueue(SifRpcDataQueue_t *, int);
void sceSifRegisterRpc(SifRpcServerData_t *, int, SifRpcFunc_t, void *,
    SifRpcFunc_t, void *, SifRpcDataQueue_t *);
void sceSifRpcLoop(SifRpcDataQueue_t *);
#endif
