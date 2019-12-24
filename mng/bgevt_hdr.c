/*************************************************************************
    > File Name: bgevt_hdr.c
    > Author: Kevin
    > Created Time: 2019-12-20
    > Description:
 ************************************************************************/

/* Includes *********************************************************** */
#include <stdlib.h>
#include <unistd.h>

#include "bg_uart_cbs.h"
#include "uart.h"
#include "bgevt_hdr.h"
#include "socket_handler.h"
#include "gecko_bglib.h"
#include "logging.h"
/* Defines  *********************************************************** */
BGLIB_DEFINE();

/* Global Variables *************************************************** */

/* Static Variables *************************************************** */
static volatile int ncp_sync = false;

static bgevt_hdr hdrs[] = {
  NULL
};

/* Static Functions Declaractions ************************************* */
void conn_ncptarget(void)
{
  /**
   * Initialize BGLIB with our output function for sending messages.
   */
  const bguart_t *u = get_bguart_impl();
  BGLIB_INITIALIZE_NONBLOCK(u->bglib_output, u->bglib_input, u->bglib_peek);
  if (u->enc) {
    /* if (interface_args_ptr->encrypted) { */
    if (connect_domain_socket_server(u->ser_sockpath, u->client_sockpath, 1)) {
      LOGE("Connection to encrypted domain socket unsuccessful. Exiting..\n");
      exit(EXIT_FAILURE);
    }
    LOGM("Turning on Encryption. "
         "All subsequent BGAPI commands and events will be encrypted..\n");
    turnEncryptionOn();
    /* } else { */
    /* if (connect_domain_socket_server(interface_args_ptr->pSerSockPath, CLIENT_UNENCRYPTED_PATH, 0)) { */
    /* printf("Connection to unencrypted domain socket unsuccessful. Exiting..\n"); */
    /* exit(EXIT_FAILURE); */
    /* } */
    /* } */
  } else {
    if (0 != uartOpen((int8_t *)u->ser_sockpath, 115200, 1, 100)) {
      LOGE("Open %s failed. Exiting..\n", u->ser_sockpath);
      exit(EXIT_FAILURE);
    }
  }
}

void sync_host_and_ncp_target(void)
{
  struct gecko_cmd_packet *p;

  ncp_sync = false;
  LOGM("Syncing up NCP Host and Target\n");
  do {
    if (get_bguart_impl()->enc) {
      poll_update(50);
    }
    p = gecko_peek_event();
    if (p) {
      switch (BGLIB_MSG_ID(p->header)) {
        case gecko_evt_system_boot_id:
        {
          LOGM("System Booted. NCP Target and Host Sync-ed Up\n");
          ncp_sync = true;
        }
        break;
        default:
          sleep(1);
          break;
      }
    } else {
      gecko_cmd_system_reset(0);
      LOGD("Sent reset signal to NCP target\n");
      sleep(1);
    }

    if (ncp_sync) {
      break;
    }
  } while (1);
}

void bgevt_dispenser(void)
{
  int ret;
  struct gecko_cmd_packet *evt = NULL;
  if (!ncp_sync) {
    sync_host_and_ncp_target();
    return;
  }

  do {
    ret = 0;
    if (get_bguart_impl()->enc) {
      poll_update(50);
    }
    evt = gecko_peek_event();
    if (evt) {
      bgevt_hdr *h = hdrs;
      while (h && !ret) {
        ret = (*h)(evt);
      }
    }
  } while (evt);
}