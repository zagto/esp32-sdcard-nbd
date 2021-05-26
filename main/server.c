/*
 *  based on examples/wifi/getting_started/station from esp-idf
 */

#include <ctype.h>
#include <assert.h>
#include "esp_log.h"
#include "sdmmc_cmd.h"
#include "lwip/sockets.h"
#include "server.h"
#include "switch.h"


static const uint16_t NBD_FLAG_FIXED_NEWSTYLE = 0x1;
static const uint16_t NBD_FLAG_NO_ZEROES = 0x2;

static const uint32_t NBD_OPT_EXPORT_NAME = 0x1;
static const uint32_t NBD_OPT_ABORT = 0x2;
static const uint32_t NBD_OPT_LIST = 0x3;

static const uint32_t NBD_REP_ACK = 0x1;
static const uint32_t NBD_REP_SERVER = 0x2;
static const uint32_t NBD_REP_ERR_UNSUP = 0x80000001;
static const uint32_t NBD_REP_ERR_INVALID = 0x80000002;

static const uint32_t NBD_CMD_READ = 0x0;
static const uint32_t NBD_CMD_WRITE = 0x1;
static const uint32_t NBD_CMD_DISC = 0x2;

struct server_init_packet {
    uint8_t init_passwd[8];
    uint8_t cliserv_magic[8];
    uint16_t server_flags;
};

struct option_packet {
    uint8_t cliserv_magic[8];
    uint32_t option_number;
    uint32_t option_length;
};

struct option_reply_packet {
    uint8_t reply_magic[8];
    uint32_t option_number;
    uint32_t reply_type;
    uint32_t reply_length;
};

struct __attribute__((__packed__)) device_init_packet {
    uint64_t device_size;
    uint16_t device_flags;
};

struct __attribute__((__packed__)) data_request_packet {
    uint32_t magic;
    uint32_t type;
    uint64_t handle;
    uint64_t from;
    uint32_t length;
};

struct data_reply_packet {
    uint32_t magic;
    uint32_t error;
    uint64_t handle;
};


static uint64_t htonq(uint64_t value) {
    return ((uint64_t)htonl(value) << 32) | htonl(value >> 32);
}


static uint64_t ntohq(uint64_t value) {
    return htonq(value);
}

/* lwip ignored the MSG_WAITALL flag so implement it here */
ssize_t recv_waitall(int socket, void *buffer, size_t length, int flags) {
    ssize_t received_length = 0;
    while (length > 0) {
        ssize_t result = recv(socket, buffer, length, flags);
        if (result > 0) {
            received_length += result;
        }
        if (result <= 0 || (flags & MSG_WAITALL) == 0) {
            return result;
        }

        buffer += result;
        length -= result;
    }
    return received_length;
}

static bool ignore_bytes(int fd, size_t amount) {
    for (size_t i = 0; i < amount; i++) {
        char byte;
        ssize_t len = recv_waitall(fd, &byte, 1, MSG_WAITALL);
        if (len != 1) {
            return false;
        }
    }
    return true;
}


static bool send_reply(int fd, uint32_t option_number, uint32_t reply_type, uint32_t length, uint8_t *data) {
    struct option_reply_packet reply = {
        .reply_magic = {0x00,0x03,0xe8,0x89,0x04,0x55,0x65,0xa9},
        .option_number = htonl(option_number),
        .reply_type = htonl(reply_type),
        .reply_length = htonl(length)
    };

    ssize_t len = send(fd, &reply, sizeof(reply), MSG_NOSIGNAL);
    if (len != sizeof(reply)) {
        return false;
    }

    if (length != 0) {
        assert(data != NULL);
        len = send(fd, data, length, MSG_NOSIGNAL);
        if (len != (ssize_t)length) {
            return false;
        }
    }
    return true;
}


static bool handshake(int fd) {
    struct server_init_packet init = {
        .init_passwd = "NBDMAGIC",
        .cliserv_magic = "IHAVEOPT",
        .server_flags = htons(NBD_FLAG_FIXED_NEWSTYLE|NBD_FLAG_NO_ZEROES)
    };
    ssize_t len = send(fd, &init, sizeof(init), MSG_NOSIGNAL);
    if (len != sizeof(init)) {
        return false;
    }

    uint32_t client_flags;
    len = recv_waitall(fd, &client_flags, sizeof(client_flags), MSG_WAITALL);
    if (len != sizeof(client_flags)) {
        return false;
    }

    if (ntohl(client_flags) != (NBD_FLAG_FIXED_NEWSTYLE|NBD_FLAG_NO_ZEROES)) {
        ESP_LOGW("server", "Unsupported client flags");
        return false;
    }

    while (true) {
        struct option_packet option;
        len = recv_waitall(fd, &option, sizeof(option), MSG_WAITALL);
        if (len != sizeof(option)) {
            return false;
        }

        if (memcmp(option.cliserv_magic, "IHAVEOPT", 8) != 0) {
            ESP_LOGW("server", "invalid option magic");
            return false;
        }

        uint32_t number = ntohl(option.option_number);

        switch (number) {
        case NBD_OPT_EXPORT_NAME:
            if (ntohl(option.option_length) != 0) {
                /* client non-empty name. This is not supported */
                if (ignore_bytes(fd, ntohl(option.option_length))
                        && send_reply(fd, number, NBD_REP_ERR_INVALID, 0, NULL)) {
                    break;
                } else {
                    return false;
                }
            }

            /* client sent correct empty name */
            ESP_LOGI("server", "Handshake successful");
            return true;

        case NBD_OPT_ABORT:
            ESP_LOGW("server", "Handshake aborted by client");
            send_reply(fd, number, NBD_REP_ACK, 0, NULL);
            return false;

        case NBD_OPT_LIST:
            if (!send_reply(fd, number, NBD_REP_SERVER, 0, NULL)) {
                return false;
            }
            if (!send_reply(fd, number, NBD_REP_ACK, 0, NULL)) {
                return false;
            }
            break;

        default:
            ESP_LOGI("server", "Client tried unknown option %u", number);
            if (!ignore_bytes(fd, ntohl(option.option_length))) {
                return false;
            }
            if (!send_reply(fd, number, NBD_REP_ERR_UNSUP, 0, NULL)) {
                return false;
            }
            break;
        }
    }
}


static bool transfer_card_data(int fd, sdmmc_card_t *card, uint32_t action, uint64_t global_offset, uint64_t length) {
    if (length == 0) {
        return true;
    }

    uint32_t sector_size = card->csd.sector_size;
    uint64_t start_offset = global_offset % sector_size;
    uint64_t end_offset = (sector_size - (global_offset + length) % sector_size) % sector_size;
    size_t first_sector = global_offset / sector_size;

    assert((start_offset + length + end_offset) % sector_size == 0);
    size_t num_sectors = (start_offset + length + end_offset) / sector_size;

    static uint8_t buffer[64 * 1024];
    esp_err_t status;
    assert(sizeof(buffer) >= sector_size);

    while (num_sectors > 0) {
        size_t max_sectors_in_buffer = sizeof(buffer) / sector_size;
        size_t sectors_in_buffer = num_sectors > max_sectors_in_buffer ? max_sectors_in_buffer : num_sectors;

        if (start_offset != 0 && action == NBD_CMD_WRITE) {
            /* For writes which don't align with sector boarders, we need to read the first sector
             * to know what should be written into the area that should not be changed but is part
             * of the same sector */
            status = sdmmc_read_sectors(card, buffer, first_sector, 1);
            if (status != ESP_OK) {
                ESP_LOGE("server", "Failed to access card: %s", esp_err_to_name(status));
                return false;
            }
        }

        if (num_sectors == sectors_in_buffer && end_offset != 0 && (num_sectors > 1 || start_offset == 0)) {
            /* same for the last block, unless there is only one sector that has just been written
             * as the first */
            status = sdmmc_read_sectors(card,
                                        buffer + (num_sectors - 1) * sector_size,
                                        first_sector + num_sectors - 1,
                                        1);
            if (status != ESP_OK) {
                ESP_LOGE("server", "Failed to read card: %s", esp_err_to_name(status));
                return false;
            }
        }

        uint32_t transfer_length = sectors_in_buffer * sector_size - start_offset;
        if (num_sectors == sectors_in_buffer) {
            transfer_length -= end_offset;
        }

        if (action == NBD_CMD_READ) {
            status = sdmmc_read_sectors(card, buffer, first_sector, sectors_in_buffer);
            if (status != ESP_OK) {
                ESP_LOGE("server", "Failed to read card: %s", esp_err_to_name(status));
                return false;
            }

            ssize_t len = send(fd, buffer + start_offset, transfer_length, MSG_NOSIGNAL);
            if (len != (ssize_t)transfer_length) {
                ESP_LOGE("server", "Failure while sending read data");
            }
        } else {
            ssize_t len = recv_waitall(fd, buffer + start_offset, transfer_length, MSG_WAITALL);
            if (len != (ssize_t)transfer_length) {
                ESP_LOGE("server", "Failure while recieving data to writelwi");
            }

            status = sdmmc_write_sectors(card, buffer, first_sector, sectors_in_buffer);
            if (status != ESP_OK) {
                ESP_LOGE("server", "Failed to write card: %s", esp_err_to_name(status));
                return false;
            }
        }

        /* if all required sectors did not fit in the buffer, do a next iteration starting at the put until
         * which we could handle in this iteration */
        num_sectors -= sectors_in_buffer;
        first_sector += sectors_in_buffer;
        start_offset = 0;
    }

    return true;
}


static void data_phase(int fd, sdmmc_card_t *card) {
    struct device_init_packet init = {
        .device_flags = 0,
        .device_size = htonq((uint64_t)card->csd.capacity * card->csd.sector_size)
    };
    ssize_t len = send(fd, &init, sizeof(init), MSG_NOSIGNAL);
    if (len != sizeof(init)) {
        return;
    }

    while (true) {
        struct data_request_packet request;
        len = recv_waitall(fd, &request, sizeof(request), MSG_WAITALL);
        if (len != sizeof(request)) {
            return;
        }

        if (ntohl(request.magic) != 0x25609513) {
            ESP_LOGW("server", "Got invalid request magic");
            return;
        }

        uint32_t action = ntohl(request.type);
        if (action == NBD_CMD_DISC) {
            ESP_LOGI("server", "Client requested disconnect");
            return;
        }

        struct data_reply_packet reply = {
            .magic = htonl(0x67446698),
            .error = 0,
            .handle = request.handle
        };

        uint64_t global_offset = ntohq(request.from);
        uint64_t length = ntohl(request.length);
        bool request_ok = false;
        if (action == NBD_CMD_READ || action == NBD_CMD_WRITE) {
            if (global_offset + length <= (uint64_t)card->csd.capacity * card->csd.sector_size) {
                request_ok = true;
            } else {
                ESP_LOGW("server",
                         "Request to access %llu bytes at %llu, which exceeds card size",
                         length,
                         global_offset);
                reply.error = 1; /* TODO: find out if this field is specified anywhere */
            }
        } else {
            ESP_LOGW("server", "Got unknown request: %u", action);
        }

        if (!request_ok) {
            reply.error = 1; /* TODO: find out if this field is specified anywhere */
        }
        ssize_t len = send(fd, &reply, sizeof(reply), MSG_NOSIGNAL);
        if (len != sizeof(reply)) {
            return;
        }

        if (request_ok) {
            bool success = transfer_card_data(fd, card, action, global_offset, length);
            if (!success) {
                /* if the card no longer works we disconnect */
                return;
            }
        }
    }
}


static void handle_connection(int fd) {
    ESP_LOGI("server", "Got connection");
    sdmmc_card_t card;
    bool success = enter_flash_mode(&card);
    if (!success) {
        ESP_LOGW("server", "Closed connection because card is not available for flash mode");
        shutdown(fd, SHUT_RDWR);
        return;
    }

    success = handshake(fd);
    if (success) {
        ESP_LOGI("server", "Handshake successful, device is now accessible to client");
        data_phase(fd, &card);
        ESP_LOGI("server", "Connection closed");
    }
    leave_flash_mode();
}


void server_task(void) {
    struct sockaddr_in addr = {
        .sin_addr = {0},
        .sin_family = AF_INET,
        .sin_port = htons(10809)
    };

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        ESP_LOGE("server", "Unable to create socket");
        abort();
    }

    int status = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (status != 0) {
        ESP_LOGE("server", "bind failed");
        abort();
    }

    status = listen(fd, 0);
    if (status != 0) {
        ESP_LOGE("server", "listen failed");
        abort();
    }

    while (true) {
        int client_fd = accept(fd, NULL, 0);
        if (client_fd < 0) {
            ESP_LOGE("server", "accept failed");
            abort();
        }

        handle_connection(client_fd);
    }
}
