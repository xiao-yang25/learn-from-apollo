/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "cyber/mainboard/module_argument.h"

#include <getopt.h>
#include <libgen.h>

using apollo::cyber::common::GlobalData;

namespace apollo {
namespace cyber {
namespace mainboard {

void ModuleArgument::DisplayUsage() {
  AINFO << "Usage: \n    " << binary_name_ << " [OPTION]...\n"
        << "Description: \n"
        << "    -h, --help : help information \n"
        << "    -d, --dag_conf=CONFIG_FILE : module dag config file\n"
        << "    -p, --process_group=process_group: the process "
           "namespace for running this module, default in manager process\n"
        << "    -s, --sched_name=sched_name: sched policy "
           "conf for hole process, sched_name should be conf in cyber.pb.conf\n"
        << "Example:\n"
        << "    " << binary_name_ << " -h\n"
        << "    " << binary_name_ << " -d dag_conf_file1 -d dag_conf_file2 "
        << "-p process_group -s sched_name\n";
}

void ModuleArgument::ParseArgument(const int argc, char* const argv[]) {
  binary_name_ = std::string(basename(argv[0]));
  GetOptions(argc, argv);

  if (process_group_.empty()) {
    process_group_ = DEFAULT_process_group_;
  }

  if (sched_name_.empty()) {
    sched_name_ = DEFAULT_sched_name_;
  }

  GlobalData::Instance()->SetProcessGroup(process_group_);
  GlobalData::Instance()->SetSchedName(sched_name_);
  AINFO << "binary_name_ is " << binary_name_ << ", process_group_ is "
        << process_group_ << ", has " << dag_conf_list_.size() << " dag conf";
  for (std::string& dag : dag_conf_list_) {
    AINFO << "dag_conf: " << dag;
  }
}

void ModuleArgument::GetOptions(const int argc, char* const argv[]) {
  //  解析命令行参数
  //  参数optarg：指向当前选项参数的指针
  //  参数optind：再次调用getopt时的下一个argv指针索引
  //  参数optopt：表示最后一个未知选项
  //  参数optstring: 比如getopt(argc, argv, "td:ch:q::")

  opterr = 0;  // extern int opterr  全局变量 默认为1 为0表示关闭输出到stderr,但可能会返回"?"
  int long_index = 0; // 长选项的索引  从0开始
  const std::string short_opts = "hd:p:s:";  // : 表示后面跟一个参数,该参数由optarg返回  ::表示可跟可不跟,如果有，必须紧跟在选项后
  static const struct option long_opts[] = {
      {"help", no_argument, nullptr, 'h'},  //  name (长参数名),
                                            //  has_arg (no_argument,表示不跟参数值, required_argument,表示一定要参数值， optional_argument表示可跟可不跟）
                                            //  flag  (如果为nullptr，则返回val；如果不是,则将val赋予flag指向的内存)
                                            //  val  (返回值)
      {"dag_conf", required_argument, nullptr, 'd'},
      {"process_name", required_argument, nullptr, 'p'},
      {"sched_name", required_argument, nullptr, 's'},
      {NULL, no_argument, nullptr, 0}};

  // log command for info
  std::string cmd("");
  for (int i = 0; i < argc; ++i) {
    cmd += argv[i];
    cmd += " ";
  }
  AINFO << "command: " << cmd;

  if (1 == argc) {
    DisplayUsage();
    exit(0);
  }

  do {
    //   字符串由optarg返回
    int opt =
        getopt_long(argc, argv, short_opts.c_str(), long_opts, &long_index);
    if (opt == -1) {
      break;
    }
    switch (opt) {
      case 'd':
        dag_conf_list_.emplace_back(std::string(optarg));
        for (int i = optind; i < argc; i++) {
          if (*argv[i] != '-') {
            dag_conf_list_.emplace_back(std::string(argv[i]));
          } else {
            break;
          }
        }
        break;
      case 'p':
        process_group_ = std::string(optarg);
        break;
      case 's':
        sched_name_ = std::string(optarg);
        break;
      case 'h':
        DisplayUsage();
        exit(0);
      default:
        break;
    }
  } while (true);

  if (optind < argc) {
    AINFO << "Found non-option ARGV-element \"" << argv[optind++] << "\"";
    DisplayUsage();
    exit(1);
  }

  if (dag_conf_list_.empty()) {
    AINFO << "-d parameter must be specified";
    DisplayUsage();
    exit(1);
  }
}

}  // namespace mainboard
}  // namespace cyber
}  // namespace apollo
