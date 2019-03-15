/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * This is a helper command line tool for setting the capabilities of the AVB test executables.
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/capability.h>

int main(int argc, char *argv[])
{
  const char *cmd = "setcap cap_net_admin,cap_net_raw,cap_net_bind_service,cap_sys_nice=pe ";
  char *full_cmd = NULL;
  size_t size = 0, sprintf_size = 0;
  int result = 0;
  cap_t cap;
  unsigned num_caps = 4;
  cap_value_t capList[4] = { CAP_NET_ADMIN, CAP_NET_RAW, CAP_NET_BIND_SERVICE, CAP_SYS_NICE };
  cap_t mycap;
  cap_flag_value_t myvalue = CAP_CLEAR;

  mycap = cap_get_proc();
  cap_get_flag(mycap, CAP_SETFCAP, CAP_PERMITTED, &myvalue);
  if (CAP_SET == myvalue)
  {
    cap_get_flag(mycap, CAP_SETFCAP, CAP_EFFECTIVE, &myvalue);
  }
  cap_free(mycap);

  if (CAP_SET != myvalue)
  {
    printf("setcap_tool requires the CAP_SETFCAP+PE capability to work.\n"
        "Please do: sudo setcap cap_setfcap=pe ./setcap_tool\n");
    return 0;
  }

  if (argc != 2)
  {
    printf("Syntax: setcap_tool </absolute/path/to/test_executable>\n");
    return 0;
  }

  size = sizeof (char) * (strlen(cmd) + strlen(argv[1]) + 1);
  full_cmd = malloc(size);
  if (NULL != full_cmd)
  {
    memset(full_cmd, 0, size);

    sprintf_size = (size_t)sprintf(full_cmd, "%s%s", cmd, argv[1]);

    assert ( (size -1) == sprintf_size );

    cap = cap_init();
    cap_set_flag(cap, CAP_PERMITTED, (int)num_caps, capList, CAP_SET);
    cap_set_flag(cap, CAP_EFFECTIVE, (int)num_caps, capList, CAP_SET);
    result = cap_set_file(argv[1], cap);

    printf("[%s:%d] cap_set_file(%s, %s)\n",
      (result == 0 ? "SUCCESS" : "FAILED"),
      result,
      argv[1],
      cap_to_text(cap, NULL));

    cap_free(cap);

    free(full_cmd);
  }
  else
  {
    result = -1;
  }

  return result;
}

