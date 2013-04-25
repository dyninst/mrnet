/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: TLSKey.h,v 1.6 2008/10/09 19:54:08 mjbrim Exp $
#ifndef XPLAT_TLSKEY_H
#define XPLAT_TLSKEY_H

#include <cstdlib>
#include <cerrno>
#include "xplat/Thread.h"
#include "xplat/Monitor.h"
namespace XPlat
{

typedef struct {
    XPlat::Thread::Id tid;
    const char *tname;
    void *user_data;
} tls_t;

class TLSKey
{
public:
    class Data
    {
    public:
        Data( void ) { }
        virtual ~Data( void ) { }
        virtual void* Get( void ) const = 0;
        virtual int Set( void* val ) = 0;
    };

private:
    Data* data;
    mutable XPlat::Monitor data_sync;

public:
    TLSKey( void );
    virtual ~TLSKey( void ) {
        data_sync.Lock();
        delete data;
        data = NULL;
        data_sync.Unlock();
    }
    virtual XPlat::Thread::Id GetTid( void ) const {
        tls_t *tls_ret = NULL;
        XPlat::Thread::Id ret = 0;
        data_sync.Lock();
        if( data != NULL )
            tls_ret = (tls_t *)data->Get();
        if( tls_ret != NULL )
            ret = tls_ret->tid;
        data_sync.Unlock();
        return ret;
    }
    virtual int SetTid( XPlat::Thread::Id tid ) {
        tls_t *tls_ret = NULL;
        int ret = -1;
        data_sync.Lock();
        if( data != NULL )
            tls_ret = (tls_t *)data->Get();
        if( tls_ret != NULL ) {
            tls_ret->tid = tid;
            ret = 0;
            if( data->Set((void *)tls_ret) ) {
                ret = -1;
            }
        }
        data_sync.Unlock();
        return ret;
    }
    virtual const char* GetName( void ) const {
        tls_t *tls_ret = NULL;
        const char *ret = NULL;
        data_sync.Lock();
        if( data != NULL )
            tls_ret = (tls_t *)data->Get();
        if( tls_ret != NULL )
            ret = tls_ret->tname;
        data_sync.Unlock();
        return ret;
    }
    virtual int SetName( const char *name ) {
        tls_t *tls_ret = NULL;
        int ret = -1;
        data_sync.Lock();
        if( data != NULL )
            tls_ret = (tls_t *)data->Get();
        if( tls_ret != NULL ) {
            tls_ret->tname = name;
            ret = 0;
            if(data->Set((void *)tls_ret)) {
                ret = -1;
            }
        }
        data_sync.Unlock();
        return ret;
    }
    virtual void* GetUserData( void ) const {
        tls_t *tls_ret = NULL;
        void *ret = NULL;
        data_sync.Lock();
        if( data != NULL )
            tls_ret = (tls_t *)data->Get();
        if( tls_ret != NULL )
            ret = tls_ret->user_data;
        data_sync.Unlock();
        return ret;
    }
    virtual int SetUserData( void *val ) {
        tls_t *tls_ret = NULL;
        int ret = -1;
        data_sync.Lock();
        if( data != NULL )
            tls_ret = (tls_t *)data->Get();
        if( tls_ret != NULL ) {
            tls_ret->user_data = val;
            ret = 0;
            if(data->Set((void *)tls_ret)) {
                ret = -1;
            }
        }
        data_sync.Unlock();
        return ret;
    }
    virtual int InitTLS( const char* name, void* val ) {
        int ret = -1;
        void *get_ret = NULL;
        tls_t *tls_data;

        if( NULL == name )
            return ret;

        data_sync.Lock();
        if( NULL == data ) {
            data_sync.Unlock();
            return ret;
        }
        
        get_ret = data->Get();
        
        // Allocate our internal tls structure if nothing is found
        if( NULL == get_ret ) {
            tls_data = new tls_t;
            if( NULL == tls_data ) {
                data_sync.Unlock();
                return ENOMEM;
            }
        } else {
            tls_data = (tls_t*)get_ret;
        }
        
        tls_data->tid = XPlat::Thread::GetId();
        tls_data->tname = name;
        tls_data->user_data = val;
        ret = data->Set( tls_data );

        data_sync.Unlock();
        return ret;
    }
    // Assumes user (MRNet) has already freed user data
    virtual int DestroyData() {
        tls_t *tls_ret = NULL;
        int ret = 0;
        data_sync.Lock();
        if( data != NULL )
            tls_ret = (tls_t *)data->Get();
        if( tls_ret != NULL ) {
            if( tls_ret->user_data != NULL ) {
                data_sync.Unlock();
                return -1;
            }
            // Free given thread name
            // We assume the user used malloc and not new for thread name
            if( tls_ret->tname != NULL ) {
                free( const_cast<char*>(tls_ret->tname) );
                tls_ret->tname = NULL;
            }

            // Delete our internal tls structure
            delete tls_ret;
            tls_ret = NULL;
            
            // set TLS to NULL
            data->Set(NULL);
        } else {
            ret = -1;
        }
        data_sync.Unlock();

        return ret;
    }
};

extern TLSKey *XPlat_TLSKey;

} // namespace XPlat

#endif // XPLAT_TLSKEY_H
