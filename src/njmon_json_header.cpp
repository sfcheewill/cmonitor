/*
 * njmon_json_header.cpp: routines to generate the JSON header
 * Developer: Nigel Griffiths, Francesco Montorsi.
 * (C) Copyright 2018 Nigel Griffiths, Francesco Montorsi

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <ifaddrs.h>
#include <iostream>
#include <memory.h>
#include <mntent.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <signal.h>
#include <sstream>
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/vfs.h>
#include <time.h>
#include <unistd.h>

#include "njmon.h"

void NjmonCollectorApp::file_read_one_stat(const char* file, const char* name)
{
    FILE* fp;
    char buf[1024 + 1];

    if ((fp = fopen(file, "r")) != NULL) {
        if (fgets(buf, 1024, fp) != NULL) {
            if (buf[strlen(buf) - 1] == '\n') /* remove last char = newline */
                buf[strlen(buf) - 1] = 0;
            pstring(name, buf);
        }
        fclose(fp);
    }
}

void NjmonCollectorApp::header_identity()
{
    int i;

    /* hostname */
    char label[512];
    struct addrinfo hints;
    struct addrinfo* info = NULL;
    struct addrinfo* p = NULL;

    /* network IP addresses */
    struct ifaddrs* interfaces = NULL;
    struct ifaddrs* ifaddrs_ptr = NULL;
    char address_buf[INET6_ADDRSTRLEN];
    char* str;

    DEBUGLOG_FUNCTION_START();

    psection("identity");
    get_hostname();
    pstring("hostname", m_strHostname.c_str());
    pstring("shorthostname", m_strShortHostname.c_str());

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    char hostname[1024] = { 0 };
    if (getaddrinfo(hostname, "http", &hints, &info) == 0) {
        for (p = info, i = 1; p != NULL; p = p->ai_next, i++) {
            sprintf(label, "fullhostname%d", i);
            pstring(label, p->ai_canonname);
        }
    }

    if (getifaddrs(&interfaces) == 0) { /* retrieve the current interfaces */
        for (ifaddrs_ptr = interfaces; ifaddrs_ptr != NULL; ifaddrs_ptr = ifaddrs_ptr->ifa_next) {

            if (ifaddrs_ptr->ifa_addr) {
                switch (ifaddrs_ptr->ifa_addr->sa_family) {
                case AF_INET:
                    if ((str = (char*)inet_ntop(ifaddrs_ptr->ifa_addr->sa_family,
                             &((struct sockaddr_in*)ifaddrs_ptr->ifa_addr)->sin_addr, address_buf, sizeof(address_buf)))
                        != NULL) {
                        sprintf(label, "%s_IP4", ifaddrs_ptr->ifa_name);
                        pstring(label, str);
                    }
                    break;
                case AF_INET6:
                    if ((str = (char*)inet_ntop(ifaddrs_ptr->ifa_addr->sa_family,
                             &((struct sockaddr_in6*)ifaddrs_ptr->ifa_addr)->sin6_addr, address_buf,
                             sizeof(address_buf)))
                        != NULL) {
                        sprintf(label, "%s_IP6", ifaddrs_ptr->ifa_name);
                        pstring(label, str);
                    }
                    break;
                default:
                    // sprintf(label,"%s_Not_Supported_%d", ifaddrs_ptr->ifa_name, ifaddrs_ptr->ifa_addr->sa_family);
                    // pstring(label,"");
                    break;
                }
            } else {
                sprintf(label, "%s_network_ignored", ifaddrs_ptr->ifa_name);
                pstring(label, "null_address");
            }
        }

        freeifaddrs(interfaces); /* free the dynamic memory */
    }

    /* POWER and AMD and may be others */
    if (access("/proc/device-tree", R_OK) == 0) {
        file_read_one_stat("/proc/device-tree/compatible", "compatible");
        file_read_one_stat("/proc/device-tree/model", "model");
        file_read_one_stat("/proc/device-tree/part-number", "part-number");
        file_read_one_stat("/proc/device-tree/serial-number", "serial-number");
        file_read_one_stat("/proc/device-tree/system-id", "system-id");
        file_read_one_stat("/proc/device-tree/vendor", "vendor");
    }
    /*x86_64 and AMD64 */
    if (access("/sys/devices/virtual/dmi/id/", R_OK) == 0) {
        file_read_one_stat("/sys/devices/virtual/dmi/id/product_serial", "serial-number");
        file_read_one_stat("/sys/devices/virtual/dmi/id/product_name", "model");
        file_read_one_stat("/sys/devices/virtual/dmi/id/sys_vendor", "vendor");
    }
    psectionend();
}

void NjmonCollectorApp::header_njmon_info(
    int argc, char** argv, long sampling_interval_sec, long num_samples, unsigned int collect_flags)
{
    /* user name and id */
    struct passwd* pw;
    uid_t uid;

    psection("njmon");

    char command[1024] = { 0 };
    for (int i = 0; i < argc; i++) {
        strcat(command, argv[i]);
        if (i != argc - 1)
            strcat(command, " ");
    }

    pstring("njmon_command", command);
    plong("sample_interval_seconds", sampling_interval_sec);
    plong("sample_num", num_samples);
    pstring("njmon_version", VERSION_STRING);

    std::string str;
    for (size_t j = 1; j < PK_MAX; j *= 2) {
        PerformanceKpiFamily k = (PerformanceKpiFamily)j;
        if (collect_flags & k) {
            std::string str2 = string2PerformanceKpiFamily(k);
            if (!str2.empty())
                str += str2 + ",";
        }
    }
    if (!str.empty())
        str.pop_back();
    pstring("collecting", str.c_str());

    uid = geteuid();
    if ((pw = getpwuid(uid)) != NULL) {
        pstring("username", pw->pw_name);
        plong("userid", uid);
    } else {
        pstring("username", "unknown");
    }
    psectionend();
}

void NjmonCollectorApp::header_etc_os_release()
{
    static FILE* fp = 0;
    char buf[1024 + 1];

    DEBUGLOG_FUNCTION_START();
    if (fp == 0) {
        if ((fp = fopen("/etc/os-release", "r")) == NULL) {
            return;
        }
    } else
        rewind(fp);

    psection("os_release");
    while (fgets(buf, 1024, fp) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */
        if (buf[strlen(buf) - 1] == '"')
            buf[strlen(buf) - 1] = 0; /* remove double quote */

        if (!strncmp(buf, "NAME=", strlen("NAME="))) {
            pstring("name", &buf[strlen("NAME=") + 1]);
        }
        if (!strncmp(buf, "VERSION=", strlen("VERSION="))) {
            pstring("version", &buf[strlen("VERSION=") + 1]);
        }
        if (!strncmp(buf, "PRETTY_NAME=", strlen("PRETTY_NAME="))) {
            pstring("pretty_name", &buf[strlen("PRETTY_NAME=") + 1]);
        }
        if (!strncmp(buf, "VERSION_ID=", strlen("VERSION_ID="))) {
            pstring("version_id", &buf[strlen("VERSION_ID=") + 1]);
        }
    }
    psectionend();
}

long power_timebase = 0;
long power_nominal_mhz = 0;
int ispower = 0;

void NjmonCollectorApp::header_cpuinfo()
{
    static FILE* fp = 0;
    char buf[1024 + 1];
    char string[1024 + 1];
    double value;
    int int_val;
    int processor;

    DEBUGLOG_FUNCTION_START();
    if (fp == 0) {
        if ((fp = fopen("/proc/cpuinfo", "r")) == NULL) {
            return;
        }
    } else
        rewind(fp);

    psection("cpuinfo");
    processor = -1;
    while (fgets(buf, 1024, fp) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */
        /* moronically cpuinfo file format has Tab characters !!! */

        if (!strncmp("processor", buf, strlen("processor"))) {
            // end previous section
            if (processor != -1)
                psubend();

            // start new section
            sscanf(&buf[12], "%d", &int_val);
            processor = int_val;
            sprintf(string, "proc%d", processor);
            psub(string);
            // processor++;
        }

        if (cgroup_is_allowed_cpu(processor)) {

            if (!strncmp("clock", buf, strlen("clock"))) { /* POWER ONLY */
                sscanf(&buf[9], "%lf", &value);
                pdouble("mhz_clock", value);
                power_nominal_mhz = value; /* save for sys_device_system_cpu() */
                ispower = 1;
            }
            if (!strncmp("vendor_id", buf, strlen("vendor_id"))) {
                pstring("vendor_id", &buf[12]);
            }
            if (!strncmp("cpu MHz", buf, strlen("cpu MHz"))) {
                sscanf(&buf[11], "%lf", &value);
                pdouble("cpu_mhz", value);
            }
            if (!strncmp("cache size", buf, strlen("cache size"))) {
                sscanf(&buf[13], "%lf", &value);
                pdouble("cache_size", value);
            }
            if (!strncmp("physical id", buf, strlen("physical id"))) {
                sscanf(&buf[14], "%d", &int_val);
                plong("physical_id", int_val);
            }
            if (!strncmp("siblings", buf, strlen("siblings"))) {
                sscanf(&buf[11], "%d", &int_val);
                plong("siblings", int_val);
            }
            if (!strncmp("core id", buf, strlen("core id"))) {
                sscanf(&buf[10], "%d", &int_val);
                plong("core_id", int_val);
            }
            if (!strncmp("cpu cores", buf, strlen("cpu cores"))) {
                sscanf(&buf[12], "%d", &int_val);
                plong("cpu_cores", int_val);
            }
            if (!strncmp("model name", buf, strlen("model name"))) {
                pstring("model_name", &buf[13]);
            }
            if (!strncmp("timebase", buf, strlen("timebase"))) { /* POWER only */
                ispower = 1;
                break;
            }
        }
    }
    if (processor != -1)
        psubend();
    psectionend();
    if (ispower) {
        psection("cpuinfo_power");
        if (!strncmp("timebase", buf, strlen("timebase"))) { /* POWER only */
            pstring("timebase", &buf[11]);
            power_timebase = atol(&buf[11]);
            plong("power_timebase", power_timebase);
        }
        while (fgets(buf, 1024, fp) != NULL) {
            buf[strlen(buf) - 1] = 0; /* remove newline */
            if (!strncmp("platform", buf, strlen("platform"))) { /* POWER only */
                pstring("platform", &buf[11]);
            }
            if (!strncmp("model", buf, strlen("model"))) {
                pstring("model", &buf[9]);
            }
            if (!strncmp("machine", buf, strlen("machine"))) {
                pstring("machine", &buf[11]);
            }
            if (!strncmp("firmware", buf, strlen("firmware"))) {
                pstring("firmware", &buf[11]);
            }
        }
        psectionend();
    }
}

void NjmonCollectorApp::header_version()
{
    static FILE* fp = 0;
    char buf[1024 + 1];

    DEBUGLOG_FUNCTION_START();
    if (fp == 0) {
        if ((fp = fopen("/proc/version", "r")) == NULL) {
            return;
        }
    } else
        rewind(fp);
    if (fgets(buf, 1024, fp) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */
        for (size_t i = 0; i < strlen(buf); i++) {
            if (buf[i] == '"')
                buf[i] = '|';
        }
        psection("proc_version");
        pstring("version", buf);
        psectionend();
    }
}


void NjmonCollectorApp::header_lscpu()
{
    FILE* pop = 0;
    int data_col = 21;
    int len = 0;
    char buf[1024 + 1];

    DEBUGLOG_FUNCTION_START();
    if ((pop = popen("/usr/bin/lscpu", "r")) == NULL)
        return;

    buf[0] = 0;
    psection("lscpu");
    while (fgets(buf, 1024, pop) != NULL) {
        buf[strlen(buf) - 1] = 0; /* remove newline */
        // LogDebug("DEBUG: lscpu line is |%s|\n", buf);
        if (!strncmp("Architecture:", buf, strlen("Architecture:"))) {
            len = strlen(buf);
            for (data_col = 14; data_col < len; data_col++) {
                if (isalnum(buf[data_col]))
                    break;
            }
            pstring("architecture", &buf[data_col]);
        }
        if (!strncmp("Byte Order:", buf, strlen("Byte Order:"))) {
            pstring("byte_order", &buf[data_col]);
        }
        if (!strncmp("CPU(s):", buf, strlen("CPU(s):"))) {
            pstring("cpus", &buf[data_col]);
        }
        if (!strncmp("On-line CPU(s) list:", buf, strlen("On-line CPU(s) list:"))) {
            pstring("online_cpu_list", &buf[data_col]);
        }
        if (!strncmp("Off-line CPU(s) list:", buf, strlen("Off-line CPU(s) list:"))) {
            pstring("online_cpu_list", &buf[data_col]);
        }
        if (!strncmp("Model:", buf, strlen("Model:"))) {
            pstring("model", &buf[data_col]);
        }
        if (!strncmp("Model name:", buf, strlen("Model name:"))) {
            pstring("model_name", &buf[data_col]);
        }
        if (!strncmp("Thread(s) per core:", buf, strlen("Thread(s) per core:"))) {
            pstring("threads_per_core", &buf[data_col]);
        }
        if (!strncmp("Core(s) per socket:", buf, strlen("Core(s) per socket:"))) {
            pstring("cores_per_socket", &buf[data_col]);
        }
        if (!strncmp("Socket(s):", buf, strlen("Socket(s):"))) {
            pstring("sockets", &buf[data_col]);
        }
        if (!strncmp("NUMA node(s):", buf, strlen("NUMA node(s):"))) {
            pstring("numa_nodes", &buf[data_col]);
        }
        if (!strncmp("CPU MHz:", buf, strlen("CPU MHz:"))) {
            pstring("cpu_mhz", &buf[data_col]);
        }
        if (!strncmp("CPU max MHz:", buf, strlen("CPU max MHz:"))) {
            pstring("cpu_max_mhz", &buf[data_col]);
        }
        if (!strncmp("CPU min MHz:", buf, strlen("CPU min MHz:"))) {
            pstring("cpu_min_mhz", &buf[data_col]);
        }
        /* Intel only */
        if (!strncmp("BogoMIPS:", buf, strlen("BogoMIPS:"))) {
            pstring("bogomips", &buf[data_col]);
        }
        if (!strncmp("Vendor ID:", buf, strlen("Vendor ID:"))) {
            pstring("vendor_id", &buf[data_col]);
        }
        if (!strncmp("CPU family:", buf, strlen("CPU family:"))) {
            pstring("cpu_family", &buf[data_col]);
        }
        if (!strncmp("Stepping:", buf, strlen("Stepping:"))) {
            pstring("stepping", &buf[data_col]);
        }
    }
    psectionend();
    pclose(pop);
}

void NjmonCollectorApp::header_lshw()
{
    FILE* pop = 0;
    char buf[4096 + 1];

    DEBUGLOG_FUNCTION_START();

    if (!file_exists("/usr/bin/header_lshw"))
        return;

    // header_lshw supports JSON output natively so we just copy/paste its output
    // into our output file.
    // IMPORTANT: unfortunately when running from inside a container header_lshw will
    //            not be able to provide all the information it provides if launched
    //            on the baremetal...

    if ((pop = popen("/usr/bin/header_lshw -json", "r")) == NULL)
        return;

    buf[0] = 0;
    praw("    \"header_lshw\": ");
    while (fgets(buf, 4096, pop) != NULL) {
        pbuffer_check();
        praw("    "); // indentation
        praw(buf);
        buf[0] = 0;
    }
    pclose(pop);
}
