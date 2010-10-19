//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: evdata.h,v 1.1 2004/02/13 13:55:03 wschweer Exp $
//
//  (C) Copyright 1999-2003 Werner Schweer (ws@seh.de)
//=========================================================

#ifndef __EVDATA_H__
#define __EVDATA_H__

//#include <memory.h>
#include <string.h>    // p4.0.2  

//---------------------------------------------------------
//   EvData
//    variable len event data (sysex, meta etc.)
//---------------------------------------------------------

class EvData {
      int* refCount;

   public:
      unsigned char* data;
      int dataLen;

      EvData()  {
            data     = 0;
            dataLen  = 0;
            refCount = new int(1);
            }
      EvData(const EvData& ed) {
            data     = ed.data;
            dataLen  = ed.dataLen;
            refCount = ed.refCount;
            (*refCount)++;
            }

      EvData& operator=(const EvData& ed) {
            if (data == ed.data)
                  return *this;
            if (--(*refCount) == 0) {
                  delete refCount;
                  delete[] data;
                  }
            data     = ed.data;
            dataLen  = ed.dataLen;
            refCount = ed.refCount;
            (*refCount)++;
            return *this;
            }

      ~EvData() {
            if (--(*refCount) == 0) {
                  delete[] data;
                  delete refCount;
                  }
            }
      void setData(const unsigned char* p, int l) {
            data = new unsigned char[l];
            memcpy(data, p, l);
            dataLen = l;
            }
      };

#endif

