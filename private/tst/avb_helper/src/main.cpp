/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include "gtest/gtest.h"

extern "C"{
#include <pci/pci.h>
}

#include <cstdlib>

#include <new>
#include <malloc.h>

std::string cIasChannelNamePolicyTest = "IASBus";
size_t heapSpaceLeft = size_t(-1);
size_t heapSpaceInitSize = 10 * 1024 * 1024; // 10MB should be sufficient

void* mynewimpl(size_t, bool);
void mydeleteimpl(void*);

void* operator new(std::size_t size)
{
  void* x = NULL;
  x = mynewimpl(size, false);
  return x;
}

void* operator new[](std::size_t size)
{
  void* x = NULL;
  x = mynewimpl(size, false);
  return x;
}

void operator delete(void* ptr)
{
  mydeleteimpl(ptr);
}

void operator delete[](void* ptr)
{
  mydeleteimpl(ptr);
}

void* operator new(std::size_t size, const std::nothrow_t&)
{
  void* x = NULL;
#if VERBOSE_TEST_PRINTOUT
  printf("[NEW_NOTHROW] Attempting to allocate %zd from heap of size %zd\n", size, heapSpaceLeft);
#endif
  x = mynewimpl(size, true);
  return x;
}

void* operator new[](std::size_t size, const std::nothrow_t&)
{
  void* x = NULL;
#if VERBOSE_TEST_PRINTOUT
  printf("[NEW_NOTHROW[]] Attempting to allocate %zd from heap of size %zd\n", size, heapSpaceLeft);
#endif
  x = mynewimpl(size, true);
  return x;
}

void operator delete(void*, const std::nothrow_t&)
{
  assert(0);
}

void operator delete[](void*, const std::nothrow_t&)
{
  assert(0);
}

void* mynewimpl(size_t size, bool isNoThrow)
{
  void* ret = NULL;

  if(isNoThrow)
  {
    if (size <= heapSpaceLeft)
    {
      heapSpaceLeft -= size;
    }
    else
    {
      return ret;
    }
  }

  ret = malloc( size );

  return ret;
}


void mydeleteimpl(void* ptr)
{
  if (NULL != ptr)
  {
    free(ptr);
    ptr = NULL;
  }
}

int32_t verbosity = 1;

int main(int argc, char* argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  int32_t result = RUN_ALL_TESTS();
  return result;
}
