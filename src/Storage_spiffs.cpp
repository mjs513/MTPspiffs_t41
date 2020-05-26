// Storage.cpp - Teensy MTP Responder library
// Copyright (C) 2017 Fredrik Hubinette <hubbe@hubbe.net>
//
// With updates from MichaelMC and Yoong Hor Meng <yoonghm@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// modified for SDFS by WMXZ

#include "core_pins.h"
#include "usb_dev.h"
#include "usb_serial.h"
#include "spiffs_t4.h"
#include "spiffs.h"

#include "Storage_spiffs.h"

extern "C" {
  extern uint8_t external_psram_size;
}

  uint32_t flash_free, flash_available, flash_used;
  
  spiffs_t4 eFLASH;
 bool Storage_init(void)
  { 
    #if DO_DEBUG>0
      Serial.println("Using SPIFFS");
    #endif
    if (eFLASH.begin() < 0) {
		return false;
	} else {
		eFLASH.fs_mount();
		return true;
	}
	return 1;
  }
  
  void initializeStorage(void)
  {
	eFLASH.fs_unmount();
	if (external_psram_size == 16) {
		eFLASH.eraseDevice();
	} else {
		eFLASH.eraseFlashChip();
	}
	eFLASH.fs_mount();
  }

// TODO:
//   support multiple storages
//   support serialflash
//   partial object fetch/receive
//   events (notify usb host when local storage changes)

// These should probably be weak.
void mtp_yield() {}
void mtp_lock_storage(bool lock) {}

  bool MTPStorage_SPIFFS::readonly() { return false; }
  bool MTPStorage_SPIFFS::has_directories() { return false; }
  
//????????
  void MTPStorage_SPIFFS::capacity(){
	uint32_t total, used;
	eFLASH.fs_space(&total, &used);
	flash_available = total;
	flash_used = used;
	flash_free = flash_available - flash_used;
  }
	  
  
  uint32_t MTPStorage_SPIFFS::clusterCount() { capacity(); return flash_available; }
  uint32_t MTPStorage_SPIFFS::freeClusters() { capacity(); return flash_free; }
  uint32_t MTPStorage_SPIFFS::clusterSize() { return 0; }



  void MTPStorage_SPIFFS::ResetIndex() {
	  
    if(!index_){
		return;
	}
    int res;
    mtp_lock_storage(true);
    //if(index_.isOpen()) index_.close();
	res = eFLASH.f_open(index_, "mtpindex.dat", SPIFFS_RDWR);
	if(res < 0) CloseIndex();
    eFLASH.f_remove("mtpindex.dat");
	res = eFLASH.f_open(index_, "mtpindex.dat", SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_RDWR | SPIFFS_APPEND);
    mtp_lock_storage(false);

    all_scanned_ = false;
    index_generated=false;
    open_file_ = 0xFFFFFFFEUL;
  }

  void MTPStorage_SPIFFS::CloseIndex()
  {
    mtp_lock_storage(true);
	eFLASH.f_close(index_);
    mtp_lock_storage(false);
    index_generated = false;
    index_entries_ = 0;
  }

  void MTPStorage_SPIFFS::OpenIndex() 
  { 
    if(index_ > 0) return;// only once
    mtp_lock_storage(true);
	eFLASH.f_open(index_, "mtpindex.dat", SPIFFS_CREAT | SPIFFS_RDWR );
    mtp_lock_storage(false);
  }

  void MTPStorage_SPIFFS::WriteIndexRecord(uint32_t i, const Record& r) 
  {
    OpenIndex();
    mtp_lock_storage(true);
	eFLASH.f_seek(index_, sizeof(r) * i, SPIFFS_SEEK_SET);
	eFLASH.f_write(index_, (char*)&r, sizeof(r));
    mtp_lock_storage(false);
  }

  uint32_t MTPStorage_SPIFFS::AppendIndexRecord(const Record& r) 
  {
    uint32_t new_record = index_entries_++;
    WriteIndexRecord(new_record, r);
    return new_record;
  }

  // TODO(hubbe): Cache a few records for speed.
  Record MTPStorage_SPIFFS::ReadIndexRecord(uint32_t i) 
  {
    Record ret;

    if (i > index_entries_) 
    { memset(&ret, 0, sizeof(ret));
      return ret;
    }
    OpenIndex();
    mtp_lock_storage(true);
	eFLASH.f_seek(index_, sizeof(ret) * i, SPIFFS_SEEK_SET);
	eFLASH.f_read(index_, (char *)&ret, sizeof(ret));
    mtp_lock_storage(false);
    return ret;
  }

  void MTPStorage_SPIFFS::ConstructFilename(int i, char* out, int len) // construct filename rexursively
  {
    if (i == 0) 
    { strcpy(out, "/");
    }
    else 
    { Record tmp = ReadIndexRecord(i);
      ConstructFilename(tmp.parent, out, len);
      //if (out[strlen(out)-1] != '/') strcat(out, "/");
      //if(((strlen(out)+strlen(tmp.name)+1) < (unsigned) len)) strcat(out, tmp.name);
	  strcpy(out, tmp.name);
    }
  }

  /// this needs work but have to see first =========================================
  void MTPStorage_SPIFFS::OpenFileByIndex(uint32_t i, uint32_t mode) 
  {
    if (open_file_ == i) return;
    char filename[256];
    ConstructFilename(i, filename, 256);
    mtp_lock_storage(true);
	if(file_) eFLASH.f_close(file_);
	eFLASH.f_open(file_, filename, mode);
    open_file_ = i;
    mode_ = mode;
    mtp_lock_storage(false);
  }
  //===========================================================================
  
  
  // MTP object handles should not change or be re-used during a session.
  // This would be easy if we could just have a list of all files in memory.
  // Since our RAM is limited, we'll keep the index in a file instead.
  void MTPStorage_SPIFFS::GenerateIndex()
  {
    if (index_generated) return;
    index_generated = true;

    // first remove old index file
    mtp_lock_storage(true);
	eFLASH.f_remove("mtpindex.dat");
    //sd.remove((char*)"mtpindex.dat");
    mtp_lock_storage(false);
    index_entries_ = 0;

    Record r;
    r.parent = 0;
    r.sibling = 0;
    r.child = 0;
    r.isdir = true;
    r.scanned = false;
    strcpy(r.name, "/");
    AppendIndexRecord(r);
  }

  void MTPStorage_SPIFFS::ScanDir(uint32_t i) 
  {
    Record record = ReadIndexRecord(i);
	
    if (record.isdir && !record.scanned) {
      //OpenFileByIndex(i);
	  uint16_t numrecs;
	   mtp_lock_storage(true);
		dir test;
		test = eFLASH.fs_getDir(&numrecs);
        mtp_lock_storage(false);
      //if (!file_) return;
      int sibling = 0;
	  
      for(uint16_t rec_count=0; rec_count<numrecs; rec_count++) 
      {
        Record r;
        r.parent = i;
        r.sibling = sibling;
        r.isdir = false;
        r.child = r.isdir ? 0 : test.fsize[rec_count];
        r.scanned = false;
		for(uint8_t j=0;j<test.fnamelen[rec_count];j++)
				r.name[j] = test.filename[rec_count][j];
        sibling = AppendIndexRecord(r);

//Serial.printf("ScanDir1\n\tIndex: %d\n", i);
//Serial.printf("\tname: %s, parent: %d, child: %d, sibling: %d\n", r.name, r.parent, r.child, r.sibling);
//Serial.printf("\tIsdir: %d, IsScanned: %d\n", r.isdir, r.scanned);
      }
      record.scanned = true;
      record.child = sibling;
      WriteIndexRecord(i, record);
    }
  }

  void MTPStorage_SPIFFS::ScanAll() 
  {
    if (all_scanned_) return;
    all_scanned_ = true;

    GenerateIndex();
    for (uint32_t i = 0; i < index_entries_; i++)  ScanDir(i);
  }

  void MTPStorage_SPIFFS::StartGetObjectHandles(uint32_t parent) 
  {
    GenerateIndex();
    if (parent) 
    { if (parent == 0xFFFFFFFF) parent = 0;
      ScanDir(parent);
      follow_sibling_ = true;
      // Root folder?
      next_ = ReadIndexRecord(parent).child;
    } 
    else 
    { ScanAll();
      follow_sibling_ = false;
      next_ = 1;
    }
  }

  uint32_t MTPStorage_SPIFFS::GetNextObjectHandle()
  {
    while (true) {
      if (next_ == 0) return 0;

      int ret = next_;
      Record r = ReadIndexRecord(ret);
      if (follow_sibling_) 
      { next_ = r.sibling;
      } 
      else 
      {
        next_++;
        if (next_ >= index_entries_) next_ = 0;
      }
      if (r.name[0]) return ret;
    }
  }

  void MTPStorage_SPIFFS::GetObjectInfo(uint32_t handle, char* name, uint32_t* size, uint32_t* parent)
  {
    Record r = ReadIndexRecord(handle);
    strcpy(name, r.name);
    *parent = r.parent;
    *size = r.isdir ? 0xFFFFFFFFUL : r.child;
  }

  uint32_t MTPStorage_SPIFFS::GetSize(uint32_t handle) 
  {
    return ReadIndexRecord(handle).child;
  }

  void MTPStorage_SPIFFS::read(uint32_t handle, uint32_t pos, char* out, uint32_t bytes)
  {
    OpenFileByIndex(handle);
    char filename[256];
    ConstructFilename(handle, filename, 256);
    mtp_lock_storage(true);
	eFLASH.f_seek(file_, pos, SPIFFS_SEEK_SET);
	eFLASH.f_read(file_, out, bytes);
    //file_.seek(pos);
    //file_.read(out,bytes);
    mtp_lock_storage(false);
  }

  bool MTPStorage_SPIFFS::DeleteObject(uint32_t object)
  {
    char filename[256];

    Record r;
    while (true) {
      r = ReadIndexRecord(object == 0xFFFFFFFFUL ? 0 : object);
      if (!r.isdir) break;
      if (!r.child) break;
      if (!DeleteObject(r.child))  return false;
    }

    // We can't actually delete the root folder,
    // but if we deleted everything else, return true.
    if (object == 0xFFFFFFFFUL) return true;
	
    ConstructFilename(object, filename, 256);

    int success;
    mtp_lock_storage(true);
    success = eFLASH.f_remove(filename);
    mtp_lock_storage(false);
    if (success < 0) return false;

    r.name[0] = 0;
    int p = r.parent;
    WriteIndexRecord(object, r);
    Record tmp = ReadIndexRecord(p);
    if (tmp.child == object) 
    { tmp.child = r.sibling;
      WriteIndexRecord(p, tmp);
    } 
    else 
    { int c = tmp.child;
      while (c) 
      { tmp = ReadIndexRecord(c);
        if (tmp.sibling == object) 
        { tmp.sibling = r.sibling;
          WriteIndexRecord(c, tmp);
          break;
        } 
        else 
        { c = tmp.sibling;
        }
      }
    }
    return true;
  }

  uint32_t MTPStorage_SPIFFS::Create(uint32_t parent,  bool folder, const char* filename)
  {
    uint32_t ret;
    if (parent == 0xFFFFFFFFUL) parent = 0;
    Record p = ReadIndexRecord(parent);
    Record r;
    if (strlen(filename) > 62) return 0;
	strcpy(r.name, filename);
    r.parent = parent;
    r.child = 0;
    r.sibling = p.child;
    r.isdir = folder;
    // New folder is empty, scanned = true.
    r.scanned = 1;
    ret = p.child = AppendIndexRecord(r);
    WriteIndexRecord(parent, p);
    if (folder) 
    {
      char filename[256];
      ConstructFilename(ret, filename, 256);
      mtp_lock_storage(true);
      //sd.mkdir(filename);
      mtp_lock_storage(false);
    } 
    else 
    {
      OpenFileByIndex(ret, SPIFFS_CREAT | SPIFFS_RDWR);
    }
    return ret;
  }

  void MTPStorage_SPIFFS::write(const char* data, uint32_t bytes)
  {
      mtp_lock_storage(true);
	  eFLASH.f_write(file_, data, bytes);
      mtp_lock_storage(false);
	  fileSize = bytes;
  }

  void MTPStorage_SPIFFS::close() 
  {
    mtp_lock_storage(true);
    uint64_t size = fileSize;
	eFLASH.f_close(file_);
    mtp_lock_storage(false);
    Record r = ReadIndexRecord(open_file_);
	r.child = size;
    WriteIndexRecord(open_file_, r);
    open_file_ = 0xFFFFFFFEUL;
  }

  void MTPStorage_SPIFFS::rename(uint32_t handle, const char* name) 
  { char oldName[256];
    char newName[256];

    ConstructFilename(handle, oldName, 256);
    Record p1 = ReadIndexRecord(handle);
    strcpy(p1.name,name);
    WriteIndexRecord(handle, p1);
    ConstructFilename(handle, newName, 256);

    eFLASH.f_rename(oldName,newName);
  }

  void MTPStorage_SPIFFS::move(uint32_t handle, uint32_t newParent ) 
  { char oldName[256];
    char newName[256];

    ConstructFilename(handle, oldName, 256);
    Record p1 = ReadIndexRecord(handle);

    if (newParent == 0xFFFFFFFFUL) newParent = 0;
    Record p2 = ReadIndexRecord(newParent); // is pointing to last object in directory

    p1.sibling = p2.child;
    p1.parent = newParent;

    p2.child = handle; 
    WriteIndexRecord(handle, p1);
    WriteIndexRecord(newParent, p2);

    ConstructFilename(handle, newName, 256);
    eFLASH.f_rename(oldName,newName);
  }
