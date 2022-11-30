#include "I2CSensor.h"

static uint8_t sensors_count = 0;

uint8_t unitemp_i2c_readReg(I2CSensor* i2c_sensor, uint8_t reg) {
    //Блокировка шины
    furi_hal_i2c_acquire(i2c_sensor->i2c);
    uint8_t buff[1];
    furi_hal_i2c_read_mem(i2c_sensor->i2c, i2c_sensor->currentI2CAdr << 1, reg, buff, 1, 10);
    furi_hal_i2c_release(i2c_sensor->i2c);
    return buff[0];
}

bool unitemp_i2c_readRegArray(I2CSensor* i2c_sensor, uint8_t startReg, uint8_t len, uint8_t* data) {
    furi_hal_i2c_acquire(i2c_sensor->i2c);
    bool status = furi_hal_i2c_read_mem(
        i2c_sensor->i2c, i2c_sensor->currentI2CAdr << 1, startReg, data, len, 10);
    furi_hal_i2c_release(i2c_sensor->i2c);
    return status;
}

bool unitemp_i2c_writeReg(I2CSensor* i2c_sensor, uint8_t reg, uint8_t value) {
    //Блокировка шины
    furi_hal_i2c_acquire(i2c_sensor->i2c);
    uint8_t buff[1] = {value};
    bool status =
        furi_hal_i2c_write_mem(i2c_sensor->i2c, i2c_sensor->currentI2CAdr << 1, reg, buff, 1, 10);
    furi_hal_i2c_release(i2c_sensor->i2c);
    return status;
}

bool unitemp_I2C_sensor_alloc(Sensor* sensor, char* args) {
    bool status = false;
    I2CSensor* instance = malloc(sizeof(I2CSensor));
    if(instance == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor %s instance allocation error", sensor->name);
        return false;
    }
    instance->i2c = &furi_hal_i2c_handle_external;
    sensor->instance = instance;

    //Указание функций инициализации, деинициализации и обновления данных, а так же адреса на шине I2C
    status = sensor->type->allocator(sensor, args);
    int i2c_addr;
    sscanf(args, "%X", &i2c_addr);

    //Установка адреса шины I2C
    if(i2c_addr >= instance->minI2CAdr && i2c_addr <= instance->maxI2CAdr) {
        instance->currentI2CAdr = i2c_addr;
    } else {
        instance->currentI2CAdr = instance->minI2CAdr;
    }

    //Блокировка портов GPIO
    sensors_count++;
    unitemp_gpio_lock(unitemp_gpio_getFromInt(15), &I2C);
    unitemp_gpio_lock(unitemp_gpio_getFromInt(16), &I2C);

    return status;
}

bool unitemp_I2C_sensor_free(Sensor* sensor) {
    bool status = sensor->type->mem_releaser(sensor);
    free(sensor->instance);
    if(--sensors_count == 0) {
        unitemp_gpio_unlock(unitemp_gpio_getFromInt(15));
        unitemp_gpio_unlock(unitemp_gpio_getFromInt(16));
    }

    return status;
}

UnitempStatus unitemp_I2C_sensor_update(Sensor* sensor) {
    if(sensor->status != UT_OK) {
        sensor->type->initializer(sensor);
    }
    return sensor->type->updater(sensor);
}