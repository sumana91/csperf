/*
*    Copyright (C) 2016 Nikhil AP
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include "csperf_config.h"
#include "csperf_defaults.h"
#include "csperf_network.h"
#include "csperf_common.h"

#define MAX_OPEN_DESCRIPTORS 20000

void
csperf_config_display_short_help()
{
    printf("Usage: csperf [-s|-c host] [options]\n");
    printf("Try csperf -h for more information\n");
}

void
csperf_config_display_long_help()
{
    printf("Usage: csperf [-s|-c host] [options]\n");
    printf(" -c <ip/hostname>      # Run as client and connect to ip address/hostname\n");
    printf(" -s                    # Run as server\n");
    printf(" -p <port>             # Server port to listen and client to connect. Default 5001\n");
    printf(" -B <data block size>  # Size of the data segment. Default 1KB\n");
    printf(" -n <num blcks>        # Number of data blocks to send. Default 1\n");
    printf(" -t <seconds>          # Number of seconds the client will be active\n");
    printf(" -e                    # Echo client data. Server echos client data\n");
    printf(" -C <num-clients>      # Total number of clients\n");
    printf(" -P <num-clients>      # Concurrent/Parallel clients that needs to connect to the server \n");
    printf(" -S <num-clients>      # Number of Clients that connects with the server every second\n");
    printf(" -r <repeat count>     # Repeat the test these many times. Setting -1 means run forever\n");
    printf(" -l <logfile>          # Logfile to write to. Default writes to csperf_xxx.txt xxx = client or server\n");
}

int
csperf_config_validate(csperf_config_t *config)
{
    if (!config) {
        return -1;
    }

    if (config->role == CS_CLIENT) {
        struct rlimit file_limit;
        int ret = getrlimit(RLIMIT_NOFILE, &file_limit);

        /* Validate total number of clients */
        if (config->total_clients < config->concurrent_clients) {
            fprintf(stdout, "Total number of clients should be greater or equal to concurrent clients\n");
            return -1;
        }
        if (config->total_clients < config->clients_per_sec) {
            fprintf(stdout, "Total number of clients should be greater or equal to clients per second\n");
            return -1;
        }

        /* Validate concurrent clients and clients per second */
        if (config->concurrent_clients && config->clients_per_sec) {
            fprintf(stdout, "Either concuurent clients or clients per second can be specified. Not both.\n");
            return -1;
        }

        /* Validate repeat count */
        if (!config->repeat_count) {
            fprintf(stdout, "Repeat count cannot be 0\n");
            return -1;
        }

        if (!ret && !config->concurrent_clients &&
                file_limit.rlim_cur < config->total_clients) {
            fprintf(stdout, "Error: Total clients are more than the"
                    " number of maximimum open descriptors: %d.\nIncrease the "
                    "limit by running 'ulimit -n %u'\n",
                    (int)file_limit.rlim_cur,  MAX_OPEN_DESCRIPTORS);
            return -1;
        }

        if (!ret && file_limit.rlim_cur < config->concurrent_clients) {
            fprintf(stdout, "Error: Concurrent clients are more than the"
                    " number of maximimum open descriptors: %d.\nIncrease the "
                    "limit by running 'ulimit -n %u'\n",
                    (int)file_limit.rlim_cur,  MAX_OPEN_DESCRIPTORS);
            return -1;
        }

        /* Warn if the open files are less than twice the client limit */
        if (!ret && !config->concurrent_clients &&
                file_limit.rlim_cur < config->total_clients * 2) {
            fprintf(stdout, "Warn: The current limit of open descriptors: %d "
                    " are more than the total clients to run.\nStill, due to time "
                    "waiting of TCP sockets, the number of sockets might not be "
                    "enough. Consider increasing the limit by running 'ulimit -n %u'\n"
                    "Enter any key to continue. Press Ctrl+c to stop\n",
                    (int)file_limit.rlim_cur,  MAX_OPEN_DESCRIPTORS);
            getchar();
        }

        if (!ret && file_limit.rlim_cur < config->concurrent_clients * 2) {
            fprintf(stdout, "Warn: The current limit of open descriptors: %d "
                    " are more than the concurrent clients to run.\nStill, due to time "
                    "waiting of TCP sockets, the number of sockets might not be "
                    "enough. Consider increasing the limit by running 'ulimit -n %u'\n"
                    "Enter any key to continue. Press Ctrl+c to stop\n",
                    (int)file_limit.rlim_cur,  MAX_OPEN_DESCRIPTORS);
            getchar();
        }

    } else if (config->role == CS_SERVER) {
    } else {
        /* Invalid role */
        return -1;
    }
    return 0;
}

int
csperf_config_parse_arguments(csperf_config_t *config,
        int argc, char **argv)
{
    int rget_opt = 0;
    if (argc < 2) {
        csperf_config_display_short_help();
        return -1;
    }

    static struct option longopts[] =
    {
        {"client", required_argument, NULL, 'c'},
        {"server", no_argument, NULL, 's'},
        {"total-clients", required_argument, NULL, 'C'},
        {"port", required_argument, NULL, 'p'},
        {"blocksize", required_argument, NULL, 'B'},
        {"numblocks", required_argument, NULL, 'n'},
        {"echo", no_argument, NULL, 'e'},
        {"repeat", required_argument, NULL, 'r'},
        {"logfile", required_argument, NULL, 'l'},
        {"markinterval", required_argument, NULL, 'm'},
        {"time", required_argument, NULL, 't'},
#if 0
        {"bytes", required_argument, NULL, 'b'},
#endif
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    while((rget_opt = getopt_long(argc, argv, "c:sp:C:B:P:S:n:em:r:l:t:h",
                    longopts, NULL)) != -1) {
        switch (rget_opt) {
        case 'c':
            config->role = CS_CLIENT;
            config->server_hostname = strdup(optarg);
            break;
        case 's':
            config->role = CS_SERVER;
            break;
        case 'C':
            config->total_clients = atoi(optarg);
            break;
        case 'p':
            config->server_port = atoi(optarg);
            break;
        case 'B':
            config->data_block_size = atoi(optarg);
            break;
        case 'n':
            config->total_data_blocks = atoi(optarg);
            break;
        case 'e':
            config->transfer_mode = CS_FLAG_DUPLEX;
            break;
        case 'r':
            config->repeat_count = atoi(optarg);
            break;
        case 'm':
            config->mark_interval_percentage = atoi(optarg);
            if (config->mark_interval_percentage < 1 ||
                    config->mark_interval_percentage > 100) {
                printf("-m is mark interval percentage\n");
                return -1;
            }
            break;
        case 'l':
            if (config->role == CS_CLIENT) {
                /* Release the default value */
                free(config->client_output_file);
                config->client_output_file = strdup(optarg);
            }
            if (config->role == CS_SERVER) {
                /* Release the default value */
                free(config->server_output_file);
                config->server_output_file = strdup(optarg);
            }
            break;
        case 'P':
            config->concurrent_clients = atoi(optarg);
            break;
        case 'S':
            config->clients_per_sec = atoi(optarg);
            break;
        case 't':
            config->client_runtime = atoi(optarg);
            if (config->client_runtime < 1) {
                printf("Client run time cannot be less than one\n");
                return -1;
            }
            break;
#if 0
        case 'b':
            /* Use iperf's way of setting data. Look '-n' */
            config->data_size = atoi(optarg);
            break;
#endif
        case 'h':
            csperf_config_display_long_help();
            return -1;
        default:
            csperf_config_display_short_help();
            return -1;
        }
    }

    if (csperf_config_validate(config)) {
        return -1;
    }

    return 0;
}

void
csperf_config_set_defaults(csperf_config_t *config)
{
    config->transfer_mode = CS_FLAG_HALF_DUPLEX;
    config->data_block_size = DEFAULT_DATA_BLOCKLEN;
    config->server_port = DEFAULT_SERVER_PORT;
    config->total_data_blocks = DEFAULT_DATA_BLOCKS;
    config->mark_interval_percentage = 100;
    config->repeat_count = 1;
    config->total_clients = 1;
    config->clients_per_sec = 0;
    config->concurrent_clients = 0;
    config->client_runtime = 0;

    config->client_output_file = strdup(DEFAULT_CLIENT_OUTPUT_FILE);
    config->server_output_file = strdup(DEFAULT_SERVER_OUTPUT_FILE);
}

void
csperf_config_cleanup(csperf_config_t *config)
{
    if (!config) {
        return;
    }
    if (config->server_hostname) {
        free(config->server_hostname);
    }

    if (config->client_output_file) {
        free(config->client_output_file);
    }
    if (config->server_output_file) {
        free(config->server_output_file);
    }
    free(config);
    config = NULL;
}

csperf_config_t *
csperf_config_init()
{
    csperf_config_t* config;

    config = (csperf_config_t *) calloc (1, sizeof(csperf_config_t));

    if (!config) {
        return NULL;
    }
    csperf_config_set_defaults(config);
    return config;
}
