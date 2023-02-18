#include <wchar.h>
#include <string.h>

#include "erl_nif.h"
#include "hidapi.h"

typedef struct {
  ERL_NIF_TERM nil;
  ERL_NIF_TERM ok;
  ERL_NIF_TERM error;
  ERL_NIF_TERM atom_device_info;
  ERL_NIF_TERM error_open;
  ERL_NIF_TERM error_enum;
  ERL_NIF_TERM error_write;
  ERL_NIF_TERM error_read;
  ErlNifResourceType *device;
} context;

typedef struct {
  hid_device *handle;
} device;

void hid_device_destructor(ErlNifEnv *env, void *res) {
  device *dev = (device *)res;
  if (!dev) return;
  if (dev->handle) hid_close(dev->handle);
}

ERL_NIF_TERM make_wchar(ErlNifEnv *env, const wchar_t *str) {
  size_t len = wcslen(str);
  ERL_NIF_TERM *arr = (ERL_NIF_TERM *)enif_alloc(sizeof(ERL_NIF_TERM) * len);
  for (int i = 0; i < len; i++) arr[i] = enif_make_uint(env, str[i]);
  ERL_NIF_TERM list = enif_make_list_from_array(env, arr, len);
  enif_free(arr);
  return list;
}

int get_wchar(ErlNifEnv *env, ERL_NIF_TERM in, wchar_t **out) {
  if (!enif_is_list(env, in)) return 0;
  unsigned int len = 0;
  int ch;
  if (!enif_get_list_length(env, in, &len)) return 0;
  *out = (wchar_t *)enif_alloc(sizeof(wchar_t) * (len + 1));
  ERL_NIF_TERM head;
  for (int i = 0; i < len; i++) {
    if (!enif_get_list_cell(env, in, &head, &in)) {
      enif_free(*out);
      return 0;
    }
    if (!enif_get_int(env, head, &ch)) {
      enif_free(*out);
      return 0;
    }
    (*out)[i] = ch;
  }
  (*out)[len] = 0;
  return 1;
}

ERL_NIF_TERM make_info(ErlNifEnv *env, struct hid_device_info *in) {
  ERL_NIF_TERM path, serial, manufacturer, product, info[11];

  context *ctx = (context *)enif_priv_data(env);
  if (!ctx) return NULL;

  path = enif_make_string(env, in->path, ERL_NIF_LATIN1);
  if (!path) return NULL;
  serial = make_wchar(env, in->serial_number);
  if (!serial) return NULL;
  manufacturer = make_wchar(env, in->manufacturer_string);
  if (!manufacturer) return NULL;
  product = make_wchar(env, in->product_string);
  if (!product) return NULL;

  info[0] = ctx->atom_device_info;
  info[1] = path;
  info[2] = enif_make_uint(env, in->vendor_id);
  info[3] = enif_make_uint(env, in->product_id);
  info[4] = serial;
  info[5] = enif_make_uint(env, in->release_number);
  info[6] = manufacturer;
  info[7] = product;
  info[8] = enif_make_uint(env, in->usage_page);
  info[9] = enif_make_uint(env, in->usage);
  info[10] = enif_make_int(env, in->interface_number);

  return enif_make_tuple_from_array(env, info, 11);
}

ERL_NIF_TERM make_error(ErlNifEnv *env, ERL_NIF_TERM error, hid_device *dev) {
  printf("making error\n");
  context *ctx = (context *)enif_priv_data(env);
  if (!ctx) return NULL;

  if (dev) {
    const wchar_t* msg = hid_error(dev);
    if (msg) {
      printf("msg: %ls\n", msg);
      ERL_NIF_TERM msg_term = make_wchar(env, msg);
      if (msg_term) {
        return enif_make_tuple3(env, ctx->error, error, msg_term);
      }
    }
  }
  return enif_make_tuple2(env, ctx->error, error);
}

static ERL_NIF_TERM enumerate(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  context *ctx = (context *)enif_priv_data(env);
  if (!ctx) return enif_make_badarg(env);

  unsigned int vendor = 0x00;
  unsigned int product = 0x00;
  enif_get_uint(env, argv[0], &vendor);
  enif_get_uint(env, argv[1], &product);
  struct hid_device_info *devs, *cur;
  devs = cur = hid_enumerate(0x0, 0x0);
  ERL_NIF_TERM result = enif_make_list(env, 0);
  while (cur) {
    ERL_NIF_TERM info = make_info(env, cur);
    if (!info) return make_error(env, ctx->error_enum, NULL);
    result = enif_make_list_cell(env, info, result);
    if (!result) return make_error(env, ctx->error_enum, NULL);
    cur = cur->next;
  }
  hid_free_enumeration(devs);
  return enif_make_tuple2(env, ctx->ok, result);
}

static ERL_NIF_TERM open(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  context *ctx = (context *)enif_priv_data(env);
  if (!ctx) return enif_make_badarg(env);
  hid_device *handle = NULL;

  if (argc == 1) {
    char *path;
    unsigned int len = 0;
    if (enif_is_binary(env, argv[0])) {
      ErlNifBinary bin;
      if (!enif_inspect_binary(env, argv[0], &bin)) return enif_make_badarg(env);
      len = bin.size;
      path = (char *)enif_alloc(len + 1);
      memcpy(path, bin.data, len);
      path[len] = 0;
    } else if (enif_is_list(env, argv[0])) {
      if (!enif_get_list_length(env, argv[0], &len)) return enif_make_badarg(env);
      path = (char *)enif_alloc(len + 1);
      if (!enif_get_string(env, argv[0], path, len + 1, ERL_NIF_LATIN1)) return enif_make_badarg(env);
    } else {
      return enif_make_badarg(env);
    }
    handle = hid_open_path(path);
    enif_free(path);
  } else {
    unsigned int vendor = 0x00;
    unsigned int product = 0x00;
    wchar_t *serial = NULL;
    enif_get_uint(env, argv[0], &vendor);
    enif_get_uint(env, argv[1], &product);
    if (argc > 2 && !get_wchar(env, argv[2], &serial)) return enif_make_badarg(env);
    handle = hid_open(vendor, product, serial);
  }

  if (!handle) return make_error(env, ctx->error_open, NULL);
  if (hid_set_nonblocking(handle, 1) != 0) return make_error(env, ctx->error_open, handle);

  device *dev = (device *)enif_alloc_resource(ctx->device, sizeof(device));
  if (!dev) return enif_raise_exception(env, ctx->error_open);
  dev->handle = handle;
  return enif_make_tuple2(env, ctx->ok, enif_make_resource(env, (void *)dev));
}

static ERL_NIF_TERM close(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  context *ctx = (context *)enif_priv_data(env);
  if (!ctx) return enif_make_badarg(env);
  device *dev = NULL;
  if (!enif_get_resource(env, argv[0], ctx->device, (void *)&dev)) return enif_make_badarg(env);
  if (dev->handle) hid_close(dev->handle);
  dev->handle = NULL;
  enif_release_resource(dev);
  return ctx->ok;
}

static ERL_NIF_TERM write(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  unsigned char *buf = NULL;
  unsigned int len = 0;

  context *ctx = (context *)enif_priv_data(env);
  if (!ctx) return enif_make_badarg(env);
  device *dev = NULL;
  if (!enif_get_resource(env, argv[0], ctx->device, (void *)&dev)) return enif_make_badarg(env);

  if (enif_is_binary(env, argv[1])) {
    ErlNifBinary bin;
    if (!enif_inspect_binary(env, argv[1], &bin)) return enif_make_badarg(env);
    len = bin.size;
    buf = (unsigned char *)enif_alloc(len);
    memcpy(buf, bin.data, len);
  } else if (enif_is_list(env, argv[1])) {
    if (!enif_get_list_length(env, argv[1], &len)) return enif_make_badarg(env);
    buf = (unsigned char *)enif_alloc(len + 1);
    if (!enif_get_string(env, argv[1], (char *)buf, len, ERL_NIF_LATIN1)) return enif_make_badarg(env);
  } else {
    return enif_make_badarg(env);
  }
  //printf("length: %u\n");

  //int bytes = hid_write(dev->handle, buf, len + 1);
  int bytes = hid_write(dev->handle, buf, len);
  enif_free(buf);
  if (bytes < 0) return make_error(env, ctx->error_write, dev->handle);
  return enif_make_tuple2(env, ctx->ok, enif_make_int(env, bytes));
}

static ERL_NIF_TERM write_report(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  unsigned char *buf = NULL;
  unsigned int len = 0;

  context *ctx = (context *)enif_priv_data(env);
  if (!ctx) return enif_make_badarg(env);
  device *dev = NULL;
  if (!enif_get_resource(env, argv[0], ctx->device, (void *)&dev)) return enif_make_badarg(env);

  if (enif_is_binary(env, argv[1])) {
    ErlNifBinary bin;
    if (!enif_inspect_binary(env, argv[1], &bin)) return enif_make_badarg(env);
    len = bin.size;
    buf = (unsigned char *)enif_alloc(len);
    memcpy(buf, bin.data, len);
  } else if (enif_is_list(env, argv[1])) {
    if (!enif_get_list_length(env, argv[1], &len)) return enif_make_badarg(env);
    buf = (unsigned char *)enif_alloc(len + 1);
    if (!enif_get_string(env, argv[1], (char *)buf, len, ERL_NIF_LATIN1)) return enif_make_badarg(env);
  } else {
    return enif_make_badarg(env);
  }

  //int bytes = hid_send_feature_report(dev->handle, buf, len + 1);
  int bytes = hid_send_feature_report(dev->handle, buf, len);
  enif_free(buf);
  if (bytes < 0) return make_error(env, ctx->error_write, dev->handle);
  return enif_make_tuple2(env, ctx->ok, enif_make_int(env, bytes));
}

static ERL_NIF_TERM read(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  unsigned char *buf = NULL;
  ERL_NIF_TERM *arr = NULL;
  unsigned int len = 0;

  context *ctx = (context *)enif_priv_data(env);
  if (!ctx) return enif_make_badarg(env);
  device *dev = NULL;
  if (!enif_get_resource(env, argv[0], ctx->device, (void *)&dev)) return enif_make_badarg(env);

  if (argc > 1) {
    if (!enif_is_number(env, argv[1])) return enif_make_badarg(env);
    if (!enif_get_uint(env, argv[1], &len)) return enif_make_badarg(env);
  }

  if (len <= 0) return enif_make_tuple2(env, ctx->ok, enif_make_list(env, 0));

  buf = enif_alloc(len);
  int bytes = hid_read(dev->handle, buf, len);
  if (bytes <= 1) {
    enif_free(buf);
    if (bytes >= 0) {
      return enif_make_tuple2(env, ctx->ok, enif_make_list(env, 0));
    } else {
      return make_error(env, ctx->error_read, dev->handle);
    }
  }
  arr = enif_alloc(sizeof(ERL_NIF_TERM) * bytes);
  for (int i = 0; i < bytes; i++) arr[i] = enif_make_uint(env, buf[i]);
  enif_free(buf);
  ERL_NIF_TERM list = enif_make_list_from_array(env, arr, bytes);
  enif_free(arr);
  return enif_make_tuple2(env, ctx->ok, list);
}

static ERL_NIF_TERM read_report(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  unsigned char *buf = NULL;
  ERL_NIF_TERM *arr = NULL;
  unsigned int len = 0;
  unsigned int report_id = 0;

  context *ctx = (context *)enif_priv_data(env);
  if (!ctx) return enif_make_badarg(env);
  device *dev = NULL;
  if (!enif_get_resource(env, argv[0], ctx->device, (void *)&dev)) return enif_make_badarg(env);

  if (!enif_is_number(env, argv[1])) return enif_make_badarg(env);
  if (!enif_get_uint(env, argv[1], &report_id)) return enif_make_badarg(env);
  if (!enif_is_number(env, argv[2])) return enif_make_badarg(env);
  if (!enif_get_uint(env, argv[2], &len)) return enif_make_badarg(env);

  if (len <= 0) return enif_make_tuple2(env, ctx->ok, enif_make_list(env, 0));

  buf = enif_alloc(len + 1);
  buf[0] = report_id;
  int bytes = hid_get_feature_report(dev->handle, buf, len + 1);
  if (bytes <= 1) {
    enif_free(buf);
    if (bytes >= 0) {
      return enif_make_tuple2(env, ctx->ok, enif_make_list(env, 0));
    } else {
      return make_error(env, ctx->error_read, dev->handle);
    }
  }
  bytes -= 1;
  arr = enif_alloc(sizeof(ERL_NIF_TERM) * bytes);
  for (int i = 0; i < bytes; i++) arr[i] = enif_make_uint(env, buf[i + 1]);
  enif_free(buf);
  ERL_NIF_TERM list = enif_make_list_from_array(env, arr, len);
  enif_free(arr);
  return enif_make_tuple2(env, ctx->ok, list);
}

static ErlNifFunc funcs[] = {
  {"nif_enumerate", 2, enumerate},
  {"nif_open", 1, open},
  {"nif_open", 2, open},
  {"nif_open", 3, open},
  {"nif_close", 1, close},
  {"nif_write", 2, write},
  {"nif_write_report", 2, write_report},
  {"nif_read", 2, read},
  {"nif_read_report", 3, read_report}
};

static int load(ErlNifEnv* env, void** priv, ERL_NIF_TERM info) {
  context* ctx = enif_alloc(sizeof(context));
  if (ctx == NULL) return 1;
  ctx->nil              = enif_make_atom(env, "nil");
  ctx->ok               = enif_make_atom(env, "ok");
  ctx->error            = enif_make_atom(env, "error");
  ctx->atom_device_info = enif_make_atom(env, "hid_device_info");
  ctx->error_open       = enif_make_atom(env, "ehidopen");
  ctx->error_write      = enif_make_atom(env, "ehidwrite");
  ctx->error_read       = enif_make_atom(env, "ehidread");
  ctx->error_enum       = enif_make_atom(env, "ehidenum");
  ctx->device = enif_open_resource_type(env, "hid", "device", hid_device_destructor,
                                        ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER, NULL);
  *priv = (void *)ctx;
  hid_init();
  return 0;
}

static int reload(ErlNifEnv* env, void** priv, ERL_NIF_TERM info) {
  return 0;
}

static int upgrade(ErlNifEnv* env, void** priv, void** old_priv, ERL_NIF_TERM info) {
  return load(env, priv, info);
}

static void unload(ErlNifEnv* env, void* priv) {
  hid_exit();
  enif_free(priv);
}

ERL_NIF_INIT(Elixir.HID, funcs, &load, &reload, &upgrade, &unload)
