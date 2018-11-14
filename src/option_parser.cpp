/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "config.hpp"
#include "ef.hpp"
#include "errno.hpp"
#include "fs_glob.hpp"
#include "num.hpp"
#include "policy.hpp"
#include "str.hpp"
#include "version.hpp"

#include <fuse.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

using std::string;
using std::vector;
using namespace mergerfs;

enum
  {
    MERGERFS_OPT_HELP,
    MERGERFS_OPT_VERSION
  };

static
void
set_option(fuse_args         &args,
           const std::string &option_)
{
  string option;

  option = "-o" + option_;

  fuse_opt_insert_arg(&args,1,option.c_str());
}

static
void
set_kv_option(fuse_args         &args,
              const std::string &key,
              const std::string &value)
{
  std::string option;

  option = key + '=' + value;

  set_option(args,option);
}

static
void
set_fsname(fuse_args      &args,
           const Branches &branches_)
{
  vector<string> branches;

  branches_.to_paths(branches);

  if(branches.size() > 0)
    {
      std::string fsname;

      fsname = str::remove_common_prefix_and_join(branches,':');

      set_kv_option(args,"fsname",fsname);
    }
}

static
void
set_subtype(fuse_args &args)
{
  set_kv_option(args,"subtype","mergerfs");
}

static
int
parse_and_process(const std::string *value_,
                  uint64_t          *uint64_)
{
  int rv;

  rv = num::to_uint64_t(value_,uint64_);
  if(rv == -1)
    return 1;

  return 0;
}

static
int
parse_and_process(const std::string *value_,
                  double            *d_)
{
  int rv;

  rv = num::to_double(value_,d_);
  if(rv == -1)
    return 1;

  return 0;
}

static
int
parse_and_process(const std::string *value_,
                  time_t            *time_)
{
  int rv;

  rv = num::to_time_t(value_,time_);
  if(rv == -1)
    return 1;

  return 0;
}

static
int
parse_and_process(const std::string *value_,
                  bool              *boolean_)
{
  if(*value_ == "false")
    *boolean_ = false;
  ef(*value_ == "true")
    *boolean_ = true;
  else
    return 1;

  return 0;
}

static
int
parse_and_process_errno(const std::string *value_,
                        int               *errno_)
{
  if(*value_ == "passthrough")
    *errno_ = 0;
  ef(*value_ == "nosys")
    *errno_ = ENOSYS;
  ef(*value_ == "noattr")
    *errno_ = ENOATTR;
  else
    return 1;

  return 0;
}

static
int
parse_and_process_statfs(const std::string    *value_,
                         Config::StatFS::Enum *enum_)
{
  if(*value_ == "base")
    *enum_ = Config::StatFS::BASE;
  ef(*value_ == "full")
    *enum_ = Config::StatFS::FULL;
  else
    return 1;

  return 0;
}

static
int
parse_and_process_statfsignore(const std::string          *value_,
                               Config::StatFSIgnore::Enum *enum_)
{
  if(*value_ == "none")
    *enum_ = Config::StatFSIgnore::NONE;
  ef(*value_ == "ro")
    *enum_ = Config::StatFSIgnore::RO;
  ef(*value_ == "nc")
    *enum_ = Config::StatFSIgnore::NC;
  else
    return 1;

  return 0;
}

static
int
parse_and_process_arg(Config            *config_,
                      const std::string *arg_)
{
  if(*arg_ == "direct_io")
    config_->direct_io = true;
  ef(*arg_ == "hard_remove")
    config_->hard_remove = true;
  ef(*arg_ == "kernel_cache")
    config_->kernel_cache = true;
  ef(*arg_ == "auto_cache")
    config_->auto_cache = true;
  else
    return 1;

  return 0;
}

static
int
parse_and_process_kv_arg(Config            *config_,
                         const std::string *key_,
                         const std::string *value_)
{
  int rv;
  std::vector<std::string> keypart;

  rv = -1;
  str::split(keypart,*key_,'.');
  if(keypart.size() == 2)
    {
      if(keypart[0] == "func")
        rv = config_->set_func_policy(keypart[1],*value_);
      ef(keypart[0] == "category")
        rv = config_->set_category_policy(keypart[1],*value_);
    }
  else
    {
      if(*key_ == "minfreespace")
        rv = parse_and_process(value_,&config_->minfreespace);
      ef(*key_ == "moveonenospc")
        rv = parse_and_process(value_,&config_->moveonenospc);
      ef(*key_ == "dropcacheonclose")
        rv = parse_and_process(value_,&config_->dropcacheonclose);
      ef(*key_ == "symlinkify")
        rv = parse_and_process(value_,&config_->symlinkify);
      ef(*key_ == "symlinkify_timeout")
        rv = parse_and_process(value_,&config_->symlinkify_timeout);
      ef(*key_ == "nullrw")
        rv = parse_and_process(value_,&config_->nullrw);
      ef(*key_ == "ignorepponrename")
        rv = parse_and_process(value_,&config_->ignorepponrename);
      ef(*key_ == "security_capability")
        rv = parse_and_process(value_,&config_->security_capability);
      ef(*key_ == "link_cow")
        rv = parse_and_process(value_,&config_->link_cow);
      ef(*key_ == "xattr")
        rv = parse_and_process_errno(value_,&config_->xattr);
      ef(*key_ == "statfs")
        rv = parse_and_process_statfs(value_,&config_->statfs);
      ef(*key_ == "statfs_ignore")
        rv = parse_and_process_statfsignore(value_,&config_->statfs_ignore);
      ef(*key_ == "hard_remove")
        rv = parse_and_process(value_,&config_->hard_remove);
      ef(*key_ == "direct_io")
        rv = parse_and_process(value_,&config_->direct_io);
      ef(*key_ == "kernel_cache")
        rv = parse_and_process(value_,&config_->kernel_cache);
      ef(*key_ == "auto_cache")
        rv = parse_and_process(value_,&config_->auto_cache);
      ef(*key_ == "entry_timeout")
        rv = parse_and_process(value_,&config_->entry_timeout);
      ef(*key_ == "negative_timeout")
        rv = parse_and_process(value_,&config_->negative_timeout);
      ef(*key_ == "attr_timeout")
        rv = parse_and_process(value_,&config_->attr_timeout);
      ef(*key_ == "ac_attr_timeout")
        rv = parse_and_process(value_,&config_->ac_attr_timeout);
    }

  if(rv == -1)
    rv = 1;

  return rv;
}

static
int
process_opt(Config     *config_,
            const char *arg_)
{
  int rv;
  std::vector<std::string> argvalue;

  str::split(argvalue,arg_,'=');
  switch(argvalue.size())
    {
    case 1:
      rv = parse_and_process_arg(config_,&argvalue[0]);
      break;

    case 2:
      rv = parse_and_process_kv_arg(config_,&argvalue[0],&argvalue[1]);
      break;

    default:
      rv = 1;
      break;
    };

  return rv;
}

static
int
process_branches(const char *arg,
                 Config     &config)
{
  config.branches.set(arg);

  return 0;
}

static
int
process_destmounts(const char *arg,
                   Config     &config)
{
  config.destmount = arg;

  return 1;
}

static
void
usage(void)
{
  std::cout <<
    "Usage: mergerfs [options] <srcpaths> <destpath>\n"
    "\n"
    "    -o [opt,...]           mount options\n"
    "    -h --help              print help\n"
    "    -v --version           print version\n"
    "\n"
    "mergerfs options:\n"
    "    <srcpaths>             ':' delimited list of directories. Supports\n"
    "                           shell globbing (must be escaped in shell)\n"
    "    -o defaults            Default FUSE options which seem to provide the\n"
    "                           best performance: atomic_o_trunc, auto_cache,\n"
    "                           big_writes, default_permissions, splice_read,\n"
    "                           splice_write, splice_move\n"
    "    -o func.<f>=<p>        Set function <f> to policy <p>\n"
    "    -o category.<c>=<p>    Set functions in category <c> to <p>\n"
    "    -o direct_io           Bypass additional caching, increases write\n"
    "                           speeds at the cost of reads. Please read docs\n"
    "                           for more details as there are tradeoffs.\n"
    "    -o use_ino             Have mergerfs generate inode values rather than\n"
    "                           autogenerated by libfuse. Suggested.\n"
    "    -o minfreespace=<int>  minimum free space needed for certain policies.\n"
    "                           default = 4G\n"
    "    -o moveonenospc=<bool> Try to move file to another drive when ENOSPC\n"
    "                           on write. default = false\n"
    "    -o dropcacheonclose=<bool>\n"
    "                           When a file is closed suggest to OS it drop\n"
    "                           the file's cache. This is useful when direct_io\n"
    "                           is disabled. default = false\n"
    "    -o symlinkify=<bool>   Read-only files, after a timeout, will be turned\n"
    "                           into symlinks. Read docs for limitations and\n"
    "                           possible issues. default = false\n"
    "    -o symlinkify_timeout=<int>\n"
    "                           timeout in seconds before will turn to symlinks.\n"
    "                           default = 3600\n"
    "    -o nullrw=<bool>       Disables reads and writes. For benchmarking.\n"
    "                           default = false\n"
    "    -o ignorepponrename=<bool>\n"
    "                           Ignore path preserving when performing renames\n"
    "                           and links. default = false\n"
    "    -o link_cow=<bool>     delink/clone file on open to simulate CoW.\n"
    "                           default = false\n"
    "    -o security_capability=<bool>\n"
    "                           When disabled return ENOATTR when the xattr\n"
    "                           security.capability is queried. default = true\n"
    "    -o xattr=passthrough|noattr|nosys\n"
    "                           Runtime control of xattrs. By default xattr\n"
    "                           requests will pass through to the underlying\n"
    "                           filesystems. notattr will short circuit as if\n"
    "                           nothing exists. nosys will respond as if not\n"
    "                           supported or disabled. default = passthrough\n"
    "    -o statfs=base|full    When set to 'base' statfs will use all branches\n"
    "                           when performing statfs calculations. 'full' will\n"
    "                           only include branches on which that path is\n"
    "                           available. default = base\n"
    "    -o statfs_ignore=none|ro|nc\n"
    "                           'ro' will cause statfs calculations to ignore\n"
    "                           available space for branches mounted or tagged\n"
    "                           as 'read only' or 'no create'. 'nc' will ignore\n"
    "                           available space for branches tagged as\n"
    "                           'no create'. default = none\n"
            << std::endl;
}

static
int
option_processor(void       *data,
                 const char *arg,
                 int         key,
                 fuse_args  *outargs)
{
  int     rv     = 0;
  Config &config = *(Config*)data;

  switch(key)
    {
    case FUSE_OPT_KEY_OPT:
      rv = process_opt(&config,arg);
      break;

    case FUSE_OPT_KEY_NONOPT:
      rv = config.branches.empty() ?
        process_branches(arg,config) :
        process_destmounts(arg,config);
      break;

    case MERGERFS_OPT_HELP:
      usage();
      close(2);
      dup(1);
      fuse_opt_add_arg(outargs,"-ho");
      break;

    case MERGERFS_OPT_VERSION:
      std::cout << "mergerfs version: "
                << (MERGERFS_VERSION[0] ? MERGERFS_VERSION : "unknown")
                << std::endl;
      fuse_opt_add_arg(outargs,"--version");
      break;

    default:
      break;
    }

  return rv;
}

namespace mergerfs
{
  namespace options
  {
    void
    parse(fuse_args &args,
          Config    &config)
    {
      const struct fuse_opt opts[] =
        {
          FUSE_OPT_KEY("-h",MERGERFS_OPT_HELP),
          FUSE_OPT_KEY("--help",MERGERFS_OPT_HELP),
          FUSE_OPT_KEY("-v",MERGERFS_OPT_VERSION),
          FUSE_OPT_KEY("-V",MERGERFS_OPT_VERSION),
          FUSE_OPT_KEY("--version",MERGERFS_OPT_VERSION),
          {NULL,-1U,0}
        };


      fuse_opt_parse(&args,
                     &config,
                     opts,
                     ::option_processor);

      set_fsname(args,config.branches);
      set_subtype(args);
    }
  }
}
