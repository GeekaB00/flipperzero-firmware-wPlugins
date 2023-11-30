#include "ublox_worker_i.h"

#define TAG "UbloxWorker"

UbloxWorker* ublox_worker_alloc() {
    UbloxWorker* ublox_worker = malloc(sizeof(UbloxWorker));

    ublox_worker->thread =
        furi_thread_alloc_ex("UbloxWorker", 2 * 1024, ublox_worker_task, ublox_worker);

    ublox_worker->callback = NULL;
    ublox_worker->context = NULL;

    ublox_worker_change_state(ublox_worker, UbloxWorkerStateReady);

    return ublox_worker;
}

void ublox_worker_free(UbloxWorker* ublox_worker) {
    furi_assert(ublox_worker);

    furi_thread_free(ublox_worker->thread);

    free(ublox_worker);
}

UbloxWorkerState ublox_worker_get_state(UbloxWorker* ublox_worker) {
    return ublox_worker->state;
}

void ublox_worker_start(
    UbloxWorker* ublox_worker,
    UbloxWorkerState state,
    UbloxWorkerCallback callback,
    void* context) {
    furi_assert(ublox_worker);

    ublox_worker->callback = callback;
    ublox_worker->context = context;

    ublox_worker_change_state(ublox_worker, state);
    furi_thread_start(ublox_worker->thread);
}

void ublox_worker_stop(UbloxWorker* ublox_worker) {
    furi_assert(ublox_worker);
    furi_assert(ublox_worker->thread);
    Ublox* ublox = ublox_worker->context;
    furi_assert(ublox);
    
    if(furi_thread_get_state(ublox_worker->thread) != FuriThreadStateStopped) {
        ublox_worker_change_state(ublox_worker, UbloxWorkerStateStop);
        furi_thread_join(ublox_worker->thread);
    }

    // Now that the worker thread is dead, we can access these
    // safely. We have to have this separate from the nav_messages()
    // function because of state.
    if (ublox->log_state == UbloxLogStateLogging) {
	FURI_LOG_I(TAG, "closing file in worker_stop()");
	if(!kml_close_file(&(ublox->kmlfile))) {
	    FURI_LOG_E(TAG, "failed to close KML file!");
	}
	// and revert the state
	ublox->log_state = UbloxLogStateNone;
    }
}

void ublox_worker_change_state(UbloxWorker* ublox_worker, UbloxWorkerState state) {
    ublox_worker->state = state;
}

void clear_ublox_data() {
    int fails = 0;

    if(!furi_hal_i2c_is_device_ready(
           &furi_hal_i2c_handle_external,
           UBLOX_I2C_ADDRESS << 1,
           furi_ms_to_ticks(I2C_TIMEOUT_MS))) {
        FURI_LOG_E(TAG, "clear_ublox_data(): device not ready");
        return;
    }

    uint8_t tx[] = {0xff};
    uint8_t response = 0;

    while(response != 0xff && fails < 30) {
        if(!furi_hal_i2c_trx(
               &furi_hal_i2c_handle_external,
               UBLOX_I2C_ADDRESS << 1,
               tx, 1,
               &response, 1,
               furi_ms_to_ticks(I2C_TIMEOUT_MS))) {
            // if the GPS is disconnected during this loop, this will
            // loop forever, we must make that not happen. 30 loops is
            // plenty, and if the clearing doesn't work, the requisite
            // error will be generated by the caller on the next
            // actual attempt to reach the GPS.
            fails++;
            FURI_LOG_E(TAG, "clear_ublox_data(): error clearing ublox data");
        }
    }
}

int32_t ublox_worker_task(void* context) {
    UbloxWorker* ublox_worker = context;
    
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    if(ublox_worker->state == UbloxWorkerStateRead) {
        ublox_worker_read_nav_messages(context);
    } else if(ublox_worker->state == UbloxWorkerStateSyncTime) {
        ublox_worker_sync_to_gps_time(ublox_worker);
    } else if(ublox_worker->state == UbloxWorkerStateResetOdometer) {
        ublox_worker_reset_odo(ublox_worker);
    } else if(ublox_worker->state == UbloxWorkerStateStop) {
        FURI_LOG_D(TAG, "state stop");
    }

    ublox_worker_change_state(ublox_worker, UbloxWorkerStateStop);

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return 0;
}

void ublox_worker_read_nav_messages(void* context) {
    // this function is fairly complicated: it inits the GPS, handles
    // logging states, and reads data from the GPS to push it to the
    // main app struct.

    // IMPORTANT NOTE: we don't use a timer that continually respawns
    // the thread because that causes a memory leak.
    UbloxWorker* ublox_worker = context;
    Ublox* ublox = ublox_worker->context;

    // We only start logging at the same time we restart the worker.
    if(ublox->log_state == UbloxLogStateStartLogging) {
        FURI_LOG_I(TAG, "start logging");
	
        // assemble full logfile pathname
	FuriString* fullname = furi_string_alloc();
        path_concat(furi_string_get_cstr(ublox->logfile_folder), ublox->text_store, fullname);
        FURI_LOG_I(TAG, "fullname is %s", furi_string_get_cstr(fullname));

        if(!kml_open_file(ublox->storage, &(ublox->kmlfile), furi_string_get_cstr(fullname))) {
            FURI_LOG_E(TAG, "failed to open KML file %s!", furi_string_get_cstr(fullname));
            ublox->log_state = UbloxLogStateNone;
	    ublox_worker->callback(UbloxWorkerEventLogStateChanged, ublox_worker->context);
	    return;
        }
        ublox->log_state = UbloxLogStateLogging;
        furi_string_free(fullname);

        ublox_worker->callback(UbloxWorkerEventLogStateChanged, ublox_worker->context);
    }

    while(!ublox->gps_initted) {
        if(ublox_worker->state != UbloxWorkerStateRead) {
            return;
        }

        // have to clear right before init to make retrying init work
        clear_ublox_data();
        if(ublox_worker_init_gps(ublox_worker)) {
            ublox->gps_initted = true;
            break;
        } else {
            ublox_worker->callback(UbloxWorkerEventFailed, ublox_worker->context);
            FURI_LOG_E(TAG, "init GPS failed, try again");
        }
        uint32_t ticks = furi_get_tick();
        // don't try constantly, no reason to
        while(furi_get_tick() - ticks < furi_ms_to_ticks(500)) {
            if(ublox_worker->state != UbloxWorkerStateRead) {
                return;
            }
        }
        //furi_delay_ms(500);
    }

    // clear data so we don't an error on startup
    clear_ublox_data();

    // break the loop when the thread state changes
    while(ublox_worker->state == UbloxWorkerStateRead) {
        // reading takes a little time, measure here
        uint32_t ticks = furi_get_tick();
        // we interrupt with checking the state to help reduce
        // lag. it's not perfect, but it does overall improve things.
        bool pvt = ublox_worker_read_pvt(ublox_worker);

        if(ublox_worker->state != UbloxWorkerStateRead) break;
        // clearing makes the second read much faster
        clear_ublox_data();

        if(ublox_worker->state != UbloxWorkerStateRead) break;

        bool odo = ublox_worker_read_odo(ublox_worker);

        if(pvt && odo) {
	    // if we got good data, do stuff
            ublox_worker->callback(UbloxWorkerEventDataReady, ublox_worker->context);

	    // if logging, add point
            if(ublox->log_state == UbloxLogStateLogging) {
                if(!kml_add_path_point(
                       &(ublox->kmlfile),
                       // ublox returns values as floats * 1e7 in int form
                       (double)(ublox->nav_pvt.lat) / (double)1e7,
                       (double)(ublox->nav_pvt.lon) / (double)1e7,
                       ublox->nav_pvt.hMSL / 1e3)) { // convert altitude to meters
                    FURI_LOG_E(TAG, "failed to write line to file");
                }
            }
        } else {
	    // bad data
            ublox_worker->callback(UbloxWorkerEventFailed, ublox_worker->context);
        }

        while(furi_get_tick() - ticks <
              furi_ms_to_ticks(((ublox->data_display_state).refresh_rate * 1000))) {
	    // putting these *inside* the loop makes it respond faster
	    if(ublox_worker->state != UbloxWorkerStateRead) {
		return;
	    }

	    // if logging stop is requested, do it
	    if(ublox->log_state == UbloxLogStateStopLogging) {
		FURI_LOG_I(TAG, "stop logging in tick loop");
		if(!kml_close_file(&(ublox->kmlfile))) {
		    FURI_LOG_E(TAG, "failed to close KML file!");
		}
		ublox->log_state = UbloxLogStateNone;
		FURI_LOG_I(TAG, "state in tick loop: %d", ublox->log_state);
		ublox_worker->callback(UbloxWorkerEventLogStateChanged, ublox_worker->context);
	    }
        }


    }
}

void ublox_worker_sync_to_gps_time(void* context) {
    UbloxWorker* ublox_worker = context;
    Ublox* ublox = ublox_worker->context;

    UbloxFrame frame_tx;
    frame_tx.class = UBX_NAV_CLASS;
    frame_tx.id = UBX_NAV_TIMEUTC_MESSAGE;
    frame_tx.len = 0;
    frame_tx.payload = NULL;
    UbloxMessage* message_tx = ublox_frame_to_bytes(&frame_tx);

    UbloxMessage* message_rx =
        ublox_worker_i2c_transfer(message_tx, UBX_NAV_TIMEUTC_MESSAGE_LENGTH);
    if(message_rx == NULL) {
        FURI_LOG_E(TAG, "get_gps_time transfer failed");
        ublox_worker_change_state(ublox_worker, UbloxWorkerStateStop);
        ublox_worker->callback(UbloxWorkerEventFailed, ublox_worker->context);
        return;
    }
    FURI_LOG_I(TAG, "got message");
    UbloxFrame* frame_rx = ublox_bytes_to_frame(message_rx);
    ublox_message_free(message_rx);

    if(frame_rx == NULL) {
        FURI_LOG_E(TAG, "NULL pointer, something wrong with NAV-TIMEUTC message!");
        ublox_worker_change_state(ublox_worker, UbloxWorkerStateStop);
        ublox_worker->callback(UbloxWorkerEventFailed, ublox_worker->context);
        return;
    } else {
        Ublox_NAV_TIMEUTC_Message nav_timeutc = {
            .iTOW = (frame_rx->payload[0]) | (frame_rx->payload[1] << 8) |
                    (frame_rx->payload[2] << 16) | (frame_rx->payload[3] << 24),
            .tAcc = (frame_rx->payload[4]) | (frame_rx->payload[5] << 8) |
                    (frame_rx->payload[6] << 16) | (frame_rx->payload[7] << 24),
            .nano = (frame_rx->payload[8]) | (frame_rx->payload[9] << 8) |
                    (frame_rx->payload[10] << 16) | (frame_rx->payload[11] << 24),
            .year = (frame_rx->payload[12]) | (frame_rx->payload[13] << 8),
            .month = frame_rx->payload[14],
            .day = frame_rx->payload[15],
            .hour = frame_rx->payload[16],
            .min = frame_rx->payload[17],
            .sec = frame_rx->payload[18],
            .valid = frame_rx->payload[19],
        };

        ublox->nav_timeutc = nav_timeutc;
        ublox_frame_free(frame_rx);
        ublox_worker->callback(UbloxWorkerEventDataReady, ublox_worker->context);
    }
}

FuriString* print_uint8_array(uint8_t* array, int length) {
    FuriString* s = furi_string_alloc();

    for(int i = 0; i < length - 1; i++) {
        furi_string_cat_printf(s, "%x, ", array[i]);
    }
    furi_string_cat_printf(s, "%x", array[length - 1]);

    return s;
}

UbloxMessage* ublox_worker_i2c_transfer(UbloxMessage* message_tx, uint8_t read_length) {
    if(!furi_hal_i2c_is_device_ready(
           &furi_hal_i2c_handle_external,
           UBLOX_I2C_ADDRESS << 1,
           furi_ms_to_ticks(I2C_TIMEOUT_MS))) {
        FURI_LOG_E(TAG, "device not ready");
        return NULL;
    }

    // Either our I2C implementation is broken or the GPS's is, so we
    // end up reading a lot more data than we need to. That means that
    // the I2C comm code for this app is a little bit of a hack, but
    // it works fine and is fast enough, so I don't really care. It
    // certainly doesn't break the GPS.
    if(!furi_hal_i2c_tx(
           &furi_hal_i2c_handle_external,
           UBLOX_I2C_ADDRESS << 1,
           message_tx->message,
           message_tx->length,
           furi_ms_to_ticks(I2C_TIMEOUT_MS))) {
        FURI_LOG_E(TAG, "error writing message to GPS");
        return NULL;
    }
    uint8_t* response = malloc((size_t)read_length);
    // The GPS sends 0xff until it has a complete message to respond
    // with. We have to wait until it stops sending that. (Why this
    // works is a little bit...uh, well, I don't know. Shouldn't reading
    // more bytes make it so that the data is completely read out and no
    // longer available?)

    //FURI_LOG_I(TAG, "start ticks at %lu", furi_get_tick()); // returns ms
    while(true) {
        if(!furi_hal_i2c_rx(
               &furi_hal_i2c_handle_external,
               UBLOX_I2C_ADDRESS << 1,
               response,
               1,
               furi_ms_to_ticks(I2C_TIMEOUT_MS))) {
            FURI_LOG_E(TAG, "error reading first byte of response");
            free(response);
            return NULL;
        }

        // checking with 0xb5 prevents strange bursts of junk data from becoming an issue.
        if(response[0] != 0xff && response[0] == 0xb5) {
            //FURI_LOG_I(TAG, "read rest of message at %lu", furi_get_tick());
            if(!furi_hal_i2c_rx(
                   &furi_hal_i2c_handle_external,
                   UBLOX_I2C_ADDRESS << 1,
                   &(response[1]),
                   read_length - 1, // first byte already read
                   furi_ms_to_ticks(I2C_TIMEOUT_MS))) {
                FURI_LOG_E(TAG, "error reading rest of response");
                free(response);
                return NULL;
            }
            break;
        }
        furi_delay_ms(1);
    }

    UbloxMessage* message_rx = malloc(sizeof(UbloxMessage));
    message_rx->message = response;
    message_rx->length = read_length;
    return message_rx; // message_rx->message needs to be freed later
}

bool ublox_worker_read_pvt(UbloxWorker* ublox_worker) {
    //FURI_LOG_I(TAG, "mem free before PVT read: %u", memmgr_get_free_heap());
    Ublox* ublox = ublox_worker->context;

    // Read NAV-PVT by sending NAV-PVT with no payload
    UbloxFrame* frame_tx = malloc(sizeof(UbloxFrame));
    frame_tx->class = UBX_NAV_CLASS;
    frame_tx->id = UBX_NAV_PVT_MESSAGE;
    frame_tx->len = 0;
    frame_tx->payload = NULL;
    UbloxMessage* message_tx = ublox_frame_to_bytes(frame_tx);
    ublox_frame_free(frame_tx);

    UbloxMessage* message_rx = ublox_worker_i2c_transfer(message_tx, UBX_NAV_PVT_MESSAGE_LENGTH);
    ublox_message_free(message_tx);
    if(message_rx == NULL) {
        FURI_LOG_E(TAG, "read_pvt transfer failed");
        //ublox_worker_change_state(ublox_worker, UbloxWorkerStateStop);
        return false;
    }

    UbloxFrame* frame_rx = ublox_bytes_to_frame(message_rx);
    ublox_message_free(message_rx);

    if(frame_rx == NULL) {
        FURI_LOG_E(TAG, "NULL pointer, something wrong with NAV-PVT message!");
        //ublox_worker_change_state(ublox_worker, UbloxWorkerStateStop);
        return false;
    } else {
        // build nav-pvt struct. this is very ugly and there's not much I can do about it.
        Ublox_NAV_PVT_Message nav_pvt = {
            .iTOW = (frame_rx->payload[0]) | (frame_rx->payload[1] << 8) |
                    (frame_rx->payload[2] << 16) | (frame_rx->payload[3] << 24),
            .year = (frame_rx->payload[4]) | (frame_rx->payload[5] << 8),
            .month = frame_rx->payload[6],
            .day = frame_rx->payload[7],
            .hour = frame_rx->payload[8],
            .min = frame_rx->payload[9],
            .sec = frame_rx->payload[10],
            .valid = frame_rx->payload[11],
            .tAcc = (frame_rx->payload[12]) | (frame_rx->payload[13] << 8) |
                    (frame_rx->payload[14] << 16) | (frame_rx->payload[15] << 24),
            .nano = (frame_rx->payload[16]) | (frame_rx->payload[17] << 8) |
                    (frame_rx->payload[18] << 16) | (frame_rx->payload[19] << 24),
            .fixType = frame_rx->payload[20],
            .flags = frame_rx->payload[21],
            .flags2 = frame_rx->payload[22],
            .numSV = frame_rx->payload[23],
            .lon = (frame_rx->payload[24]) | (frame_rx->payload[25] << 8) |
                   (frame_rx->payload[26] << 16) | (frame_rx->payload[27] << 24),
            .lat = (frame_rx->payload[28]) | (frame_rx->payload[29] << 8) |
                   (frame_rx->payload[30] << 16) | (frame_rx->payload[31] << 24),
            .height = (frame_rx->payload[32]) | (frame_rx->payload[33] << 8) |
                      (frame_rx->payload[34] << 16) | (frame_rx->payload[35] << 24),
            .hMSL = (frame_rx->payload[36]) | (frame_rx->payload[37] << 8) |
                    (frame_rx->payload[38] << 16) | (frame_rx->payload[39] << 24),
            .hAcc = (frame_rx->payload[40]) | (frame_rx->payload[41] << 8) |
                    (frame_rx->payload[42] << 16) | (frame_rx->payload[43] << 24),
            .vAcc = (frame_rx->payload[44]) | (frame_rx->payload[45] << 8) |
                    (frame_rx->payload[46] << 16) | (frame_rx->payload[47] << 24),
            .velN = (frame_rx->payload[48]) | (frame_rx->payload[49] << 8) |
                    (frame_rx->payload[50] << 16) | (frame_rx->payload[51] << 24),
            .velE = (frame_rx->payload[52]) | (frame_rx->payload[53] << 8) |
                    (frame_rx->payload[54] << 16) | (frame_rx->payload[55] << 24),
            .velD = (frame_rx->payload[56]) | (frame_rx->payload[57] << 8) |
                    (frame_rx->payload[58] << 16) | (frame_rx->payload[59] << 24),
            .gSpeed = (frame_rx->payload[60]) | (frame_rx->payload[61] << 8) |
                      (frame_rx->payload[62] << 16) | (frame_rx->payload[63] << 24),
            .headMot = (frame_rx->payload[64]) | (frame_rx->payload[65] << 8) |
                       (frame_rx->payload[66] << 16) | (frame_rx->payload[67] << 24),
            .sAcc = (frame_rx->payload[68]) | (frame_rx->payload[69] << 8) |
                    (frame_rx->payload[70] << 16) | (frame_rx->payload[71] << 24),
            .headAcc = (frame_rx->payload[72]) | (frame_rx->payload[73] << 8) |
                       (frame_rx->payload[74] << 16) | (frame_rx->payload[75] << 24),
            .pDOP = (frame_rx->payload[76]) | (frame_rx->payload[77] << 8),
            .flags3 = (frame_rx->payload[78]) | (frame_rx->payload[79] << 8),
            .reserved1 = frame_rx->payload[80],
            .reserved2 = frame_rx->payload[81],
            .reserved3 = frame_rx->payload[82],
            .reserved4 = frame_rx->payload[83],
            .headVeh = (frame_rx->payload[84]) | (frame_rx->payload[85] << 8) |
                       (frame_rx->payload[86] << 16) | (frame_rx->payload[87] << 24),
            .magDec = (frame_rx->payload[88]) | (frame_rx->payload[89] << 8),
            .magAcc = (frame_rx->payload[90]) | (frame_rx->payload[91] << 8),
        };

        // Using a local variable for nav_pvt is fine, because nav_pvt in
        // the Ublox struct is also not a pointer, so this assignment
        // effectively compiles to a memcpy.
        ublox->nav_pvt = nav_pvt;
        ublox_frame_free(frame_rx);
        return true;
    }
    return false;
}

bool ublox_worker_read_odo(UbloxWorker* ublox_worker) {
    Ublox* ublox = ublox_worker->context;
    UbloxFrame* frame_tx = malloc(sizeof(UbloxFrame));
    frame_tx->class = UBX_NAV_CLASS;
    frame_tx->id = UBX_NAV_ODO_MESSAGE;
    frame_tx->len = 0;
    frame_tx->payload = NULL;
    UbloxMessage* message_tx = ublox_frame_to_bytes(frame_tx);
    ublox_frame_free(frame_tx);

    UbloxMessage* message_rx = ublox_worker_i2c_transfer(message_tx, UBX_NAV_ODO_MESSAGE_LENGTH);
    ublox_message_free(message_tx);
    if(message_rx == NULL) {
        FURI_LOG_E(TAG, "read_odo transfer failed");
        return false;
    }
    UbloxFrame* frame_rx = ublox_bytes_to_frame(message_rx);
    ublox_message_free(message_rx);

    if(frame_rx == NULL) {
        FURI_LOG_E(TAG, "NULL pointer, something wrong with NAV-ODO message!");
        return false;
    } else {
        Ublox_NAV_ODO_Message nav_odo = {
            .version = frame_rx->payload[0],
            .reserved1 = frame_rx->payload[1],
            .reserved2 = frame_rx->payload[2],
            .reserved3 = frame_rx->payload[3],
            .iTOW = (frame_rx->payload[4]) | (frame_rx->payload[5] << 8) |
                    (frame_rx->payload[6] << 16) | (frame_rx->payload[7] << 24),
            .distance = (frame_rx->payload[8]) | (frame_rx->payload[9] << 8) |
                        (frame_rx->payload[10] << 16) | (frame_rx->payload[11] << 24),
            .totalDistance = (frame_rx->payload[12]) | (frame_rx->payload[13] << 8) |
                             (frame_rx->payload[14] << 16) | (frame_rx->payload[15] << 24),
            .distanceStd = (frame_rx->payload[16]) | (frame_rx->payload[17] << 8) |
                           (frame_rx->payload[18] << 16) | (frame_rx->payload[19] << 24),
        };
        //FURI_LOG_I(TAG, "odo (m): %lu", nav_odo.distance);
        ublox->nav_odo = nav_odo;
        ublox_frame_free(frame_rx);
        return true;
    }
}

/**
 * Set the power mode to "Balanced", enable the odometer, and
 * configure odometer and dynamic platform model according to user
 * settings.
 */
bool ublox_worker_init_gps(UbloxWorker* ublox_worker) {
    Ublox* ublox = ublox_worker->context;
    // Set power mode
    /*** read initial cfg-pms configuration first ***/
    UbloxFrame pms_frame_tx;
    pms_frame_tx.class = UBX_CFG_CLASS;
    pms_frame_tx.id = UBX_CFG_PMS_MESSAGE;
    pms_frame_tx.len = 0;
    pms_frame_tx.payload = NULL;
    UbloxMessage* pms_message_tx = ublox_frame_to_bytes(&pms_frame_tx);

    UbloxMessage* pms_message_rx =
        ublox_worker_i2c_transfer(pms_message_tx, UBX_CFG_PMS_MESSAGE_LENGTH);
    ublox_message_free(pms_message_tx);
    if(pms_message_rx == NULL) {
        FURI_LOG_E(TAG, "CFG-PMS read transfer failed");
        return false;
    }

    // set power setup value to "balanced"
    pms_message_rx->message[6 + 1] = 0x01;

    pms_frame_tx.class = UBX_CFG_CLASS;
    pms_frame_tx.id = UBX_CFG_PMS_MESSAGE;
    pms_frame_tx.len = 8;
    pms_frame_tx.payload = pms_message_rx->message;

    pms_message_tx = ublox_frame_to_bytes(&pms_frame_tx);

    UbloxMessage* ack = ublox_worker_i2c_transfer(pms_message_tx, UBX_ACK_ACK_MESSAGE_LENGTH);
    if(ack == NULL) {
        FURI_LOG_E(TAG, "ACK after CFG-PMS set transfer failed");
        return false;
    }
    FURI_LOG_I(
        TAG, "CFG-PMS ack: id = %u, type = %s", ack->message[3], ack->message[3] ? "ACK" : "NAK");
    ublox_message_free(pms_message_tx);
    ublox_message_free(pms_message_rx);
    ublox_message_free(ack);

    /***** Odometer *****/
    // Enable odometer by changing CFG-ODO.
    UbloxFrame odo_frame_tx;
    odo_frame_tx.class = UBX_CFG_CLASS;
    odo_frame_tx.id = UBX_CFG_ODO_MESSAGE;
    odo_frame_tx.len = 0;
    odo_frame_tx.payload = NULL;
    UbloxMessage* odo_message_tx = ublox_frame_to_bytes(&odo_frame_tx);

    UbloxMessage* odo_message_rx =
        ublox_worker_i2c_transfer(odo_message_tx, UBX_CFG_ODO_MESSAGE_LENGTH);
    ublox_message_free(odo_message_tx);
    if(odo_message_rx == NULL) {
        FURI_LOG_E(TAG, "CFG-ODO transfer failed");
        return false;
    }

    odo_frame_tx.class = UBX_CFG_CLASS;
    odo_frame_tx.id = UBX_CFG_ODO_MESSAGE;
    odo_frame_tx.len = 20;
    odo_frame_tx.payload = odo_message_rx->message;

    // TODO: low-pass filters in settings?
    // enable useODO bit in flags
    odo_frame_tx.payload[4] |= 1;
    odo_frame_tx.payload[5] = (ublox->device_state).odometer_mode;

    odo_message_tx = ublox_frame_to_bytes(&odo_frame_tx);

    ack = ublox_worker_i2c_transfer(odo_message_tx, UBX_ACK_ACK_MESSAGE_LENGTH);
    if(ack == NULL) {
        FURI_LOG_E(TAG, "ACK after CFG-ODO set transfer failed");
        return false;
    }
    FURI_LOG_I(
        TAG, "CFG-ODO ack: id = %u, type = %s", ack->message[3], ack->message[3] ? "ACK" : "NAK");

    ublox_message_free(odo_message_tx);
    ublox_message_free(odo_message_rx);
    ublox_message_free(ack);

    // finally configure the navigation engine
    UbloxFrame nav5_frame_tx;
    nav5_frame_tx.class = UBX_CFG_CLASS;
    nav5_frame_tx.id = UBX_CFG_NAV5_MESSAGE;
    nav5_frame_tx.len = 0;
    nav5_frame_tx.payload = NULL;
    UbloxMessage* nav5_message_tx = ublox_frame_to_bytes(&nav5_frame_tx);

    UbloxMessage* nav5_message_rx =
        ublox_worker_i2c_transfer(nav5_message_tx, UBX_CFG_NAV5_MESSAGE_LENGTH);
    ublox_message_free(nav5_message_tx);

    if(nav5_message_rx == NULL) {
        FURI_LOG_E(TAG, "CFG-NAV5 transfer failed");
        return false;
    }

    // first two bytes tell the GPS what changes to apply, setting this
    // bit tells it to apply the dynamic platfrom model settings.
    nav5_frame_tx.class = UBX_CFG_CLASS;
    nav5_frame_tx.id = UBX_CFG_NAV5_MESSAGE;
    nav5_frame_tx.len = 36;
    nav5_frame_tx.payload = nav5_message_rx->message;

    nav5_frame_tx.payload[0] = 1; // tell GPS to apply only the platform model settings
    nav5_frame_tx.payload[2] = (ublox->device_state).platform_model;

    nav5_message_tx = ublox_frame_to_bytes(&nav5_frame_tx);

    ack = ublox_worker_i2c_transfer(nav5_message_tx, UBX_ACK_ACK_MESSAGE_LENGTH);
    if(ack == NULL) {
        FURI_LOG_E(TAG, "ACK after CFG-NAV5 set transfer failed");
        return false;
    }
    FURI_LOG_I(
        TAG, "CFG-NAV5 ack: id = %u, type = %s", ack->message[3], ack->message[3] ? "ACK" : "NAK");

    ublox_message_free(nav5_message_rx);
    ublox_message_free(ack);
    return true;
}

// this one is being kind of slow
void ublox_worker_reset_odo(UbloxWorker* ublox_worker) {
    FURI_LOG_I(TAG, "ublox_worker_reset_odo");
    UbloxFrame odo_frame_tx;
    odo_frame_tx.class = UBX_NAV_CLASS;
    odo_frame_tx.id = UBX_NAV_RESETODO_MESSAGE;
    odo_frame_tx.len = 0;
    odo_frame_tx.payload = NULL;
    UbloxMessage* odo_message_tx = ublox_frame_to_bytes(&odo_frame_tx);

    UbloxMessage* ack = ublox_worker_i2c_transfer(odo_message_tx, UBX_ACK_ACK_MESSAGE_LENGTH);

    ublox_message_free(odo_message_tx);

    if(ack == NULL) {
        FURI_LOG_E(TAG, "ACK after NAV-RESETODO set transfer failed");
        ublox_worker->callback(UbloxWorkerEventFailed, ublox_worker->context);
        return;
    } else {
        FURI_LOG_I(
            TAG,
            "NAV-RESETODO ack: id = %u, type = %s",
            ack->message[3],
            ack->message[3] ? "ACK" : "NAK");

        ublox_message_free(ack);
    }
    ublox_worker->callback(UbloxWorkerEventOdoReset, ublox_worker->context);
    // no reason to trigger an event on success, the user will see that
    // the odometer has been reset on the next update.
}
