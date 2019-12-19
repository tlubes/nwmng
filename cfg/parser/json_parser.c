/*************************************************************************
    > File Name: json_parser.c
    > Author: Kevin
    > Created Time: 2019-12-18
    > Description:
 ************************************************************************/

/* Includes *********************************************************** */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <json.h>
#include <json_util.h>
#include <glib.h>

#include "cfgdb.h"
#include "generic_parser.h"
#include "json_object.h"
#include "json_parser.h"
#include "logging.h"
#include "utils.h"

/* Defines  *********************************************************** */
#define JSON_ECHO(msg, obj)              \
  do{                                    \
    LOGD("%s Item --- %s\n",             \
         (msg),                          \
         json_object_to_json_string_ext( \
           (obj),                        \
           JSON_C_TO_STRING_PRETTY));    \
  }while(0)

#ifndef JSON_ECHO_DBG
#define JSON_ECHO_DBG 2
#endif

#define json_array_foreach(i, n, obj)       \
  size_t n = json_object_array_length(obj); \
  for (int i = 0; i < n; i++)

#define WHOLE_WORD(x) "\"" x "\""

/*
 * For each json file, the root holds the pointer to the result of
 * json_object_from_file, whenever it's needed to release the memory allocated
 * from json_object_from_file, call json_object_put with root.
 */
typedef struct {
  uint16_t id;
  json_object *nodes;
}sbn_t;

typedef struct {
  struct {
    char *fp;
    json_object *root;
  }prov;
  struct {
    char *fp;
    json_object *root;
    int subnet_num;
    sbn_t *subnets;
    json_object *backlog;
  }nw;
  struct {
    char *fp;
    json_object *root;
  }tmpl;
}json_cfg_t;

/* Global Variables *************************************************** */
#define DECLLOADER1(name) \
  static err_t _load_##name(json_object * obj, int cfg_fd, void *out)
#define DECLLOADER2(name) \
  static err_t __load_##name(json_object * obj, int cfg_fd, void *out)
#define DECLLOADER3(name) \
  static err_t ___load_##name(json_object * obj, int cfg_fd, void *out)

/* Static Variables *************************************************** */
static json_cfg_t jcfg = { 0 };

/* Static Functions Declaractions ************************************* */
/*
 * out holds the pointer to value, it can be a real value or a pointer
 */
typedef err_t (*__load_func_t)(json_object *obj,
                               int cfg_fd,
                               void *out);
/* Used for both template and node */
DECLLOADER1(ttl);
DECLLOADER1(pub);
DECLLOADER1(snb);
DECLLOADER1(txp);
DECLLOADER1(bindings);
DECLLOADER1(sublist);

/* Used only for node */
DECLLOADER1(tmpl);

static const __load_func_t loaders[] = {
  /* Used for both template and node */
  _load_ttl,
  _load_pub,
  _load_snb,
  _load_txp,
  _load_bindings,
  _load_sublist,
  /* Used only for node */
  _load_tmpl,
};

static const int tmpl_loader_end = 6;
static const int node_loader_end = 7;

/**
 * @defgroup single_key_load
 *
 * Below functions are used to load a single key-value pair in the json file
 * @{ */
static inline err_t uint8_loader(const char *v, uint8_t *out)
{
  ASSERT(v && out);
  if (ec_success != str2uint(v, strlen(v), out, sizeof(uint8_t))) {
    LOGE("STR to UINT error\n");
    return err(ec_json_format);
  }
  return ec_success;
}

static inline err_t uint16_loader(const char *v, uint16_t *out)
{
  ASSERT(v && out);
  if (ec_success != str2uint(v, strlen(v), out, sizeof(uint16_t))) {
    LOGE("STR to UINT error\n");
    return err(ec_json_format);
  }
  return ec_success;
}
static inline err_t uint32_loader(const char *v, uint32_t *out)
{
  ASSERT(v && out);
  if (ec_success != str2uint(v, strlen(v), out, sizeof(uint32_t))) {
    LOGE("STR to UINT error\n");
    return err(ec_json_format);
  }
  return ec_success;
}

static inline err_t uint16list_loader(const char *v, uint16list_t *out)
{
  return ec_success;
}

static inline uint8_t **pttl_from_fd(int cfg_fd, void *out)
{
  if (cfg_fd == NW_NODES_CFG_FILE) {
    return (&((node_t *)out)->config.ttl);
  } else if (cfg_fd == TEMPLATE_FILE) {
    return (&((tmpl_t *)out)->ttl);
  }
  ASSERT(0);
}

static inline publication_t **ppub_from_fd(int cfg_fd, void *out)
{
  if (cfg_fd == NW_NODES_CFG_FILE) {
    return (&((node_t *)out)->config.pub);
  } else if (cfg_fd == TEMPLATE_FILE) {
    return (&((tmpl_t *)out)->pub);
  }
  ASSERT(0);
}

static inline txparam_t **ptxp_from_fd(int cfg_fd, void *out)
{
  if (cfg_fd == NW_NODES_CFG_FILE) {
    return (&((node_t *)out)->config.net_txp);
  } else if (cfg_fd == TEMPLATE_FILE) {
    return (&((tmpl_t *)out)->net_txp);
  }
  ASSERT(0);
}

static inline uint8_t **psnb_from_fd(int cfg_fd, void *out)
{
  if (cfg_fd == NW_NODES_CFG_FILE) {
    return (&((node_t *)out)->config.snb);
  } else if (cfg_fd == TEMPLATE_FILE) {
    return (&((tmpl_t *)out)->snb);
  }
  ASSERT(0);
}

static inline uint16list_t **pbindings_from_fd(int cfg_fd, void *out)
{
  if (cfg_fd == NW_NODES_CFG_FILE) {
    return (&((node_t *)out)->config.bindings);
  } else if (cfg_fd == TEMPLATE_FILE) {
    return (&((tmpl_t *)out)->bindings);
  }
  ASSERT(0);
}

static inline uint16list_t **psublist_from_fd(int cfg_fd, void *out)
{
  if (cfg_fd == NW_NODES_CFG_FILE) {
    return (&((node_t *)out)->config.sublist);
  } else if (cfg_fd == TEMPLATE_FILE) {
    return (&((tmpl_t *)out)->sublist);
  }
  ASSERT(0);
}

static err_t _load_pub(json_object *obj,
                       int cfg_fd,
                       void *out)
{
  err_t e = ec_success;
  publication_t **p = ppub_from_fd(cfg_fd, out);
  json_object *o;

  if (!json_object_object_get_ex(obj, STR_PUB, &o)) {
    goto free;
    e = err(e);
  }
  if (!*p) {
    *p = calloc(sizeof(publication_t), 1);
  }

#if (JSON_ECHO_DBG == 0)
  JSON_ECHO("Pub", o);
#endif
  const char *v;
  json_object_object_foreach(o, key, val){
    if (!strcmp(STR_ADDR, key)) {
      v = json_object_get_string(val);
      if (ec_success != (e = uint16_loader(v, &(*p)->addr))) {
        goto free;
      }
    } else if (!strcmp(STR_APPKEY, key)) {
      v = json_object_get_string(val);
      if (ec_success != (e = uint16_loader(v, &(*p)->aki))) {
        goto free;
      }
    } else if (!strcmp(STR_PERIOD, key)) {
      v = json_object_get_string(val);
      if (ec_success != (e = uint32_loader(v, &(*p)->period))) {
        goto free;
      }
    } else if (!strcmp(STR_TTL, key)) {
      v = json_object_get_string(val);
      if (ec_success != (e = uint8_loader(v, &(*p)->ttl))) {
        goto free;
      }
    } else if (!strcmp(STR_TXP, key)) {
      json_object *tmp;
      if (json_object_object_get_ex(val, STR_CNT, &tmp)) {
        v = json_object_get_string(tmp);
        if (ec_success != (e = uint8_loader(v, &(*p)->txp.cnt))) {
          goto free;
        }
      }
      if (json_object_object_get_ex(val, STR_INTV, &tmp)) {
        v = json_object_get_string(tmp);
        if (ec_success != (e = uint16_loader(v, &(*p)->txp.intv))) {
          goto free;
        }
      }
      /* TODO: sanity check, if invalid, memset to 0 */
    }
  }
  return ec_success;

  free:
  free(*p);
  *p = NULL;
  return e;
}

static err_t _load_txp(json_object *obj,
                       int cfg_fd,
                       void *out)
{
  err_t e = ec_success;
  txparam_t **p = ptxp_from_fd(cfg_fd, out);
  json_object *o;

  if (!json_object_object_get_ex(obj, STR_TXP, &o)) {
    goto free;
    e = err(e);
  }
  if (!*p) {
    *p = calloc(sizeof(txparam_t), 1);
  }

#if (JSON_ECHO_DBG == 1)
  JSON_ECHO("TxP", o);
#endif
  const char *v;
  json_object *tmp;
  if (json_object_object_get_ex(o, STR_CNT, &tmp)) {
    v = json_object_get_string(tmp);
    if (ec_success != (e = uint8_loader(v, &(*p)->cnt))) {
      goto free;
    }
  }
  if (json_object_object_get_ex(o, STR_INTV, &tmp)) {
    v = json_object_get_string(tmp);
    if (ec_success != (e = uint16_loader(v, &(*p)->intv))) {
      goto free;
    }
  }
  return ec_success;

  free:
  free(*p);
  *p = NULL;
  return e;
}

static err_t _load_ttl(json_object *obj,
                       int cfg_fd,
                       void *out)
{
  err_t e = ec_success;
  uint8_t **p = pttl_from_fd(cfg_fd, out);
  json_object *o;

  if (!json_object_object_get_ex(obj, STR_TTL, &o)) {
    goto free;
    e = err(e);
  }
#if (JSON_ECHO_DBG == 1)
  JSON_ECHO("TTL", o);
#endif
  const char *v = json_object_get_string(o);
  if (!*p) {
    *p = malloc(sizeof(uint8_t));
  }
  if (ec_success != (e = uint8_loader(v, *p))) {
    goto free;
  }
  return ec_success;

  free:
  free(*p);
  *p = NULL;
  return e;
}

static err_t _load_snb(json_object *obj,
                       int cfg_fd,
                       void *out)
{
  err_t e = ec_success;
  uint8_t **p = psnb_from_fd(cfg_fd, out);
  json_object *o;

  if (!json_object_object_get_ex(obj, STR_SNB, &o)) {
    goto free;
    e = err(e);
  }
#if (JSON_ECHO_DBG == 1)
  JSON_ECHO("SNB", o);
#endif
  const char *v = json_object_get_string(o);
  if (!*p) {
    *p = malloc(sizeof(uint8_t));
  }
  if (ec_success != (e = uint8_loader(v, *p))) {
    goto free;
  }
  return ec_success;

  free:
  free(*p);
  *p = NULL;
  return e;
}

static err_t __load_uint16list(json_object *o,
                               uint16list_t **p)
{
  err_t e;
  if (json_type_array != json_object_get_type(o)) {
    e =  err(ec_format);
    goto free;
  }
  if (!*p) {
    *p = calloc(sizeof(uint16list_t), 1);
  }

  const char *v;
  int len = json_object_array_length(o);
  if (!(*p)->data) {
    (*p)->len = len;
    (*p)->data = calloc(len, sizeof(uint16_t));
  } else if ((*p)->len != len) {
    (*p)->len = len;
    (*p)->data = realloc((*p)->data, len * sizeof(uint16_t));
    memset((*p)->data, 0, len * sizeof(uint16_t));
  }

  json_array_foreach(i, n, o)
  {
    json_object *tmp;
    tmp = json_object_array_get_idx(o, i);
    v = json_object_get_string(tmp);
    if (ec_success != (e = uint16_loader(v, &(*p)->data[i]))) {
      goto free;
    }
  }
  return ec_success;

  free:
  if (*p) {
    if ((*p)->data) {
      free((*p)->data);
    }
    free(*p);
    *p = NULL;
  }
  return e;
}

static err_t _load_bindings(json_object *obj,
                            int cfg_fd,
                            void *out)
{
  err_t e = ec_success;
  uint16list_t **p = pbindings_from_fd(cfg_fd, out);
  json_object *o;

  if (!json_object_object_get_ex(obj, STR_BIND, &o)) {
    goto free;
    e = err(e);
  }
#if (JSON_ECHO_DBG == 1)
  JSON_ECHO("Bindings", o);
#endif
  if (ec_success != (e = __load_uint16list(o, p))) {
    return e;
  }

  return ec_success;

  free:
  if (*p) {
    if ((*p)->data) {
      free((*p)->data);
    }
    free(*p);
    *p = NULL;
  }
  return e;
}

static err_t _load_sublist(json_object *obj,
                           int cfg_fd,
                           void *out)
{
  err_t e = ec_success;
  uint16list_t **p = psublist_from_fd(cfg_fd, out);
  json_object *o;

  if (!json_object_object_get_ex(obj, STR_SUB, &o)) {
    goto free;
    e = err(e);
  }
#if (JSON_ECHO_DBG == 1)
  JSON_ECHO("Sublist", o);
#endif
  if (ec_success != (e = __load_uint16list(o, p))) {
    return e;
  }
  return ec_success;

  free:
  if (*p) {
    if ((*p)->data) {
      free((*p)->data);
    }
    free(*p);
    *p = NULL;
  }
  return e;
}

/**
 * @brief copy_tmpl_to_node - copy the configuration in template to the node
 * only if the field in node is not set.
 *
 * @param t - tmplate
 * @param n - node
 */
static void copy_tmpl_to_node(const tmpl_t *t,
                              node_t *n)
{
  ASSERT(t && n);
  if (t->ttl && !n->config.ttl) {
    alloc_copy(&n->config.ttl, t->ttl, sizeof(uint8_t));
  }
  if (t->sublist && !n->config.sublist) {
    alloc_copy_u16list(&n->config.sublist, t->sublist);
  }
  if (t->snb && !n->config.snb) {
    alloc_copy(&n->config.snb, t->snb, sizeof(uint8_t));
  }
  if (t->relay_txp && !n->config.features.relay_txp) {
    alloc_copy((uint8_t **)&n->config.features.relay_txp, t->relay_txp, sizeof(txparam_t));
  }
  if (t->pub && !n->config.pub) {
    alloc_copy((uint8_t **)&n->config.pub, t->pub, sizeof(publication_t));
  }
  if (t->net_txp && !n->config.net_txp) {
    alloc_copy((uint8_t **)&n->config.net_txp, t->net_txp, sizeof(txparam_t));
  }
  if (t->bindings && !n->config.bindings) {
    alloc_copy_u16list(&n->config.bindings, t->bindings);
  }
}

static err_t _load_tmpl(json_object *obj,
                        int cfg_fd,
                        void *out)
{
  uint16_t tmplid = 0;
  json_object *tmp;
  const char *tmplid_str;
  tmpl_t *t;

  ASSERT(cfg_fd == NW_NODES_CFG_FILE);

  json_object_object_get_ex(obj, STR_TMPL, &tmp);
  tmplid_str = json_object_get_string(tmp);

  if (ec_success != str2uint(tmplid_str, strlen(tmplid_str), &tmplid, sizeof(uint8_t))) {
    LOGE("STR to UINT error\n");
    return err(ec_json_format);
  }

  t = cfgdb_tmpl_get(tmplid);
  if (!t) {
    return ec_success;
  }
  copy_tmpl_to_node(t, out);
  return ec_success;
}

static err_t load_to_tmpl_item(json_object *obj,
                               tmpl_t *tmpl)
{
  err_t e;
#if (JSON_ECHO_DBG == 1)
  JSON_ECHO("Template", obj);
#endif
  for (int i = 0; i < tmpl_loader_end; i++) {
    e = loaders[i](obj, TEMPLATE_FILE, tmpl);
    if (e != ec_success) {
      LOGE("Load %dth object failed.\n", i);
      elog(e);
    }
  }
  return ec_success;
}

static err_t load_to_node_item(json_object *obj,
                               node_t *node)
{
  err_t e;
#if (JSON_ECHO_DBG == 2)
  JSON_ECHO("Node", obj);
#endif
  for (int i = 0; i < node_loader_end; i++) {
    e = loaders[i](obj, NW_NODES_CFG_FILE, node);
    if (e != ec_success) {
      LOGE("Load %dth object failed.\n", i);
      elog(e);
    }
  }
  return ec_success;
}

/**  @} */

static inline char **fp_from_fd(int fd)
{
  return fd == PROV_CFG_FILE ? &jcfg.prov.fp
         : fd == NW_NODES_CFG_FILE ? &jcfg.nw.fp
         : fd == TEMPLATE_FILE ? &jcfg.tmpl.fp : NULL;
}

static inline json_object **root_from_fd(int fd)
{
  return fd == PROV_CFG_FILE ? &jcfg.prov.root
         : fd == NW_NODES_CFG_FILE ? &jcfg.nw.root
         : fd == TEMPLATE_FILE ? &jcfg.tmpl.root : NULL;
}

/**
 * @brief open_json_file - close the current root and reload it with the file
 * content, it also fill all the json_object in the struct
 *
 * @param cfg_fd -
 *
 * @return
 */
static err_t open_json_file(int cfg_fd)
{
  err_t e = ec_success;;
  json_object **root = root_from_fd(cfg_fd);
  char **fp;
  fp = fp_from_fd(cfg_fd);
  json_close(cfg_fd);
  if (!*fp) {
    return err(ec_param_invalid);
  }
  *root = json_object_from_file(*fp);
  if (!*root) {
    LOGE("json-c failed to open %s\n", *fp);
    return err(ec_json_open);
  }

  if (cfg_fd == NW_NODES_CFG_FILE) {
    /* Load the subnets, which fills the refid and the json_object nodes */
    json_object_object_foreach(*root, key, val){
      if (!strcmp(STR_SUBNETS, key)) {
        jcfg.nw.subnet_num = json_object_array_length(val);
        jcfg.nw.subnets = calloc(jcfg.nw.subnet_num, sizeof(sbn_t));
        for (int i = 0; i < jcfg.nw.subnet_num; i++) {
          json_object* tmp;
          json_object *n = json_object_array_get_idx(val, i);
          if (json_object_object_get_ex(n, STR_REFID, &tmp)) {
            const char *v = json_object_get_string(tmp);
            if (ec_success != str2uint(v, strlen(v), &jcfg.nw.subnets[i].id, sizeof(uint16_t))) {
              LOGE("STR to UINT error\n");
            }
          }
          if (!json_object_object_get_ex(n, STR_NODES, &jcfg.nw.subnets[i].nodes)) {
            LOGE("No Nodes node in the json file\n");
            e = err(ec_json_format);
            goto out;
          }
        }
      } else if (!strcmp(STR_BACKLOG, key)) {
        jcfg.nw.backlog = val;
      }
    }
  }

  out:
  if (ec_success != e) {
    json_close(cfg_fd);
  } else {
    LOGD("%s file opened\n", *fp);
  }
  return e;
}

static err_t new_json_file(int cfg_fd)
{
  /* TODO */
  return ec_success;
}

static err_t load_template(void)
{
  json_object *n, *ptmpl;
  err_t e;
  bool add = false;
  if (!jcfg.tmpl.root) {
    return err(ec_json_open);
  }
  if (!json_object_object_get_ex(jcfg.tmpl.root, STR_TEMPLATES, &ptmpl)) {
    return err(ec_json_format);
  }

  json_array_foreach(i, num, ptmpl)
  {
    json_object *tmp;
    n = json_object_array_get_idx(ptmpl, i);
    if (!json_object_object_get_ex(n, STR_REFID, &tmp)) {
      /* No reference ID, ignore it */
      continue;
    }
    const char *v = json_object_get_string(tmp);
    uint16_t refid;
    if (ec_success != str2uint(v, strlen(v), &refid, sizeof(uint16_t))) {
      LOGE("STR to UINT error\n");
      continue;
    }
    tmpl_t *t = cfgdb_tmpl_get(refid);
    if (!t) {
      t = (tmpl_t *)calloc(sizeof(tmpl_t), 1);
      ASSERT(t);
      add = true;
    }
    e = load_to_tmpl_item(n, t);
    elog(e);
    if (add) {
      if (e == ec_success) {
        t->refid = refid;
        EC(ec_success, cfgdb_tmpl_add(t));
      } else {
        free(t);
      }
    }
  }
  return ec_success;
}

static const char *mandatory_fields[] = {
  WHOLE_WORD(STR_UUID),
  WHOLE_WORD(STR_ADDR),
  WHOLE_WORD(STR_RMORBL),
  WHOLE_WORD(STR_DONE),
  WHOLE_WORD(STR_ERRBITS),
  NULL
};

static bool _node_valid_check(json_object *obj)
{
  ASSERT(obj);
  const char *v = json_object_get_string(obj);
  for (const char **c = mandatory_fields;
       *c != NULL;
       c++) {
    if (!strstr(v, *c)) {
      return false;
    }
  }
  return true;
}

static err_t load_nodes(void)
{
  /* NOTE: Only first subnet will be loaded */
  json_object *n, *pnode;
  err_t e;
  bool add = false;
  if (!jcfg.nw.subnets
      || !jcfg.nw.subnets[0].nodes
      || !jcfg.nw.root) {
    return err(ec_json_open);
  }
  pnode = jcfg.nw.subnets[0].nodes;

  json_array_foreach(i, num, pnode)
  {
    json_object *tmp;
    n = json_object_array_get_idx(pnode, i);
    if (!_node_valid_check(n)) {
      LOGE("Node[%d] invalid, pass.\n", i);
      continue;
    }

    if (!json_object_object_get_ex(n, STR_UUID, &tmp)) {
      /* No reference ID, ignore it */
      continue;
    }
    /*
     * Check if it's already provisioned to know which hash table to find the
     * node
     */
    const char *addr_str, *uuid_str, *rmbl_str, *done_str, *err_str;
    uint8_t uuid[16] = { 0 };
    uint32_t errbits;
    uint16_t addr;
    uint8_t rmbl, done;
    node_t *t;

    json_object_object_get_ex(n, STR_ADDR, &tmp);
    addr_str = json_object_get_string(tmp);

    json_object_object_get_ex(n, STR_UUID, &tmp);
    uuid_str = json_object_get_string(tmp);

    json_object_object_get_ex(n, STR_RMORBL, &tmp);
    rmbl_str = json_object_get_string(tmp);

    json_object_object_get_ex(n, STR_DONE, &tmp);
    done_str = json_object_get_string(tmp);

    json_object_object_get_ex(n, STR_ERRBITS, &tmp);
    err_str = json_object_get_string(tmp);

    if (ec_success != str2cbuf(uuid_str, 0, (char *)uuid, 16)) {
      LOGE("STR to CBUF error\n");
      continue;
    }
    if (ec_success != str2uint(err_str, strlen(err_str), &errbits, sizeof(uint32_t))) {
      LOGE("STR to UINT error\n");
      continue;
    }
    if (ec_success != str2uint(addr_str, strlen(addr_str), &addr, sizeof(uint16_t))) {
      LOGE("STR to UINT error\n");
      continue;
    }
    if (ec_success != str2uint(rmbl_str, strlen(rmbl_str), &rmbl, sizeof(uint8_t))) {
      LOGE("STR to UINT error\n");
      continue;
    }
    if (ec_success != str2uint(done_str, strlen(done_str), &done, sizeof(uint8_t))) {
      LOGE("STR to UINT error\n");
      continue;
    }

    if (addr) {
      t = cfgdb_node_get(addr);
    } else {
      t = cfgdb_unprov_dev_get((const uint8_t *)uuid);
    }
    if (!t) {
      t = (node_t *)calloc(sizeof(node_t), 1);
      ASSERT(t);
      add = true;
    }

    e = load_to_node_item(n, t);
    elog(e);

    if (add) {
      if (e == ec_success) {
        t->addr = addr;
        memcpy(t->uuid, uuid, 16);
        t->done = done;
        t->rmorbl = rmbl;
        t->err = errbits;
        EC(ec_success, cfgdb_add(t));
      } else {
        free(t);
      }
    }
  }
  return ec_success;
}

static err_t load_json_file(int cfg_fd,
                            bool clrctlfls,
                            void *out)
{
  /* TODO */
  if (cfg_fd == TEMPLATE_FILE) {
    return load_template();
  } else if (cfg_fd == NW_NODES_CFG_FILE) {
    return load_nodes();
  }
  return ec_success;
}

void json_close(int cfg_fd)
{
  json_object **root = root_from_fd(cfg_fd);
  if (!*root) {
    return;
  }
  json_object_put(*root);
  if (cfg_fd == NW_NODES_CFG_FILE) {
    SAFE_FREE(jcfg.nw.subnets);
  }
  *root = NULL;
}

err_t json_cfg_open(int cfg_fd,
                    const char *filepath,
                    unsigned int flags,
                    void *out)
{
  int tmp;
  err_t ret = ec_success;
  char **fp;

  if (cfg_fd > TEMPLATE_FILE || cfg_fd < PROV_CFG_FILE) {
    return err(ec_param_invalid);
  }
  fp = fp_from_fd(cfg_fd);

  /* Ensure the fp is not NULL */
  if (!(flags & FL_CLR_CTLFS)) {
    if (!filepath) {
      if (!(*fp)) {
        return err(ec_param_invalid);
      }
    } else {
      if (*fp) {
        free(*fp);
        *fp = NULL;
      }
      *fp = malloc(strlen(filepath) + 1);
      strcpy(*fp, filepath);
      (*fp)[strlen(filepath)] = '\0';
    }
  } else if (!(*fp)) {
    return err(ec_param_invalid);
  }
  ASSERT(*fp);

  /*
   * If need to turncate the file or the file doesn't exist, need to create,
   * do it
   */
  tmp = access(*fp, F_OK);
  if (cfg_fd == TEMPLATE_FILE) {
    if (tmp == -1) {
      return err(ec_not_exist);
    }
  } else {
    if (-1 == tmp) {
      if (!(flags & FL_CREATE)) {
        ret = err(ec_not_exist);
        goto fail;
      }
      if (ec_success != (ret = new_json_file(cfg_fd))) {
        goto fail;
      }
    } else {
      if (flags & FL_TRUNC) {
        if (ec_success != (ret = new_json_file(cfg_fd))) {
          goto fail;
        }
      }
    }
  }

  if (ec_success != (ret = open_json_file(cfg_fd))) {
    goto fail;
  }

  if (ec_success != (ret = load_json_file(cfg_fd,
                                          !!(flags & FL_CLR_CTLFS),
                                          out))) {
    goto fail;
  }

  fail:
  if (ec_success != ret) {
    /* TODO: Clean work need? */
    /* jsonConfigDeinit(); */
    LOGE("JSON[%s] Open failed\n", *fp);
    elog(ret);
  }

  return ret;
#if 0
  if (rootPtr) {
    jsonConfigClose();
  }
#endif
}

err_t json_flush(int cfg_fd)
{
  json_object **root;
  char **fp;
  if (cfg_fd > TEMPLATE_FILE || cfg_fd < PROV_CFG_FILE) {
    return err(ec_param_invalid);
  }
  fp = fp_from_fd(cfg_fd);
  root = root_from_fd(cfg_fd);
  if (!*root || !*fp) {
    return err(ec_json_null);
  }

  if (-1 == json_object_to_file_ext(*fp, *root, JSON_C_TO_STRING_PRETTY)) {
    LOGE("json file save error, reason[%s]\n",
         json_util_get_last_err());
    return err(ec_json_save);
  }
  return ec_success;
}
