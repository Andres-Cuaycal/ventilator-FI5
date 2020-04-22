#include "board_sim.h"

#include "hal.h"
#include "board_driver.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

static int test_board_sim(void);
static int test_board_driver(void);

static int test_board_sim_read(int busId, int address, void* data, int size, int expectd_result);
static int test_board_sim_write(int busId, int address, void* data, int size, int expectd_result);
static int test_board_buffer(void* buffer, void* expected, int size);
static int test_board_dump_sensor_data(board_sensor_data_t* data);
static int test_board_read_sensor(board_sensor_data_t* data, board_sensor_data_t* test_data);

static void test_board_read_callback(int status);

static int board_sensor_read_completed = 0;
static int board_sensor_read_status = 0;

static int test_board_sim(void)
{
    int result;

    uint8_t buffer[32] = {0};

    printf("\n");
    printf("################################\n");
    printf("# test_board_sim test case     #\n");
    printf("################################\n");

    // Initialize the board 
    printf("board_sim_init() ");
    result = board_sim_init();
    if(result)
    {
        printf("FAILED [%d]\n", result);
        return 1;
    }
    else
    {
        printf("PASSED\n");
    }

    // Try to access a sensor
    if(test_board_sim_read(1, 0x29, 0, 0, -ENODEV))
        return 1;

    // Write the mux mapping register
    buffer[0] = 0x1;
    if(test_board_sim_write(1, 0x70, (void*)buffer, 1, 0))
        return 1;

    // Read back the mux mapping 
    buffer[0] = 0x0;
    if(test_board_sim_read(1, 0x70, (void*)buffer, 1, 0))
        return 1;

    printf("MUX status reg: %02x ", buffer[0]);
    if(buffer[0] == 0x1)
    {
        printf("PASSED\n");
    }
    else
    {
        printf("FAILED\n");
        return 1;
    }

    // Try to access a sensor
    if(test_board_sim_read(1, 0x29, 0, 0, -EBADF))
        return 1;

    // Try to access a sensor
    memset(buffer, 0x0, sizeof(buffer));
    if(test_board_sim_read(1, 0x29, buffer, 4, 0))
        return 1;

    uint8_t expected_sensor[] = { 0x10, 0x10, 0x10, 0x10, 0x00};
    if(test_board_buffer(buffer, expected_sensor, 5))
        return 1;

    // Write the mux mapping register
    buffer[0] = 0x3;
    if(test_board_sim_write(1, 0x70, (void*)buffer, 1, 0))
        return 1;

    // Read back the mux mapping 
    buffer[0] = 0x0;
    if(test_board_sim_read(1, 0x70, (void*)buffer, 1, 0))
        return 1;

    printf("MUX status reg: %02x ", buffer[0]);
    if(buffer[0] == 0x3)
    {
        printf("PASSED\n");
    }
    else
    {
        printf("FAILED\n");
        return 1;
    }

    // Try to access a sensor
    if(test_board_sim_read(1, 0x29, 0, 0, -EIO))
        return 1;

    printf("\nTEST CASE PASSED\n");

    return 0;
}

static int test_board_driver(void)
{
    int result;

    board_sensor_data_t sensor_data;
    board_sensor_data_t all_sensor_data;
    board_sensor_data_t test_sensor_data;

    printf("\n");
    printf("################################\n");
    printf("# test_board_driver test case  #\n");
    printf("################################\n");

    // Initialize the board 
    printf("board_sim_init() ");
    result = board_sim_init();
    if(result)
    {
        printf("FAILED [%d]\n", result);
        return 1;
    }
    else
    {
        printf("PASSED\n");
    }

    // Call the HAL initialization
    printf("HAL_Init() ");
    result = HAL_Init();
    if(result)
    {
        printf("FAILED [%d]\n", result);
        return 1;
    }
    else
    {
        printf("PASSED\n");
    }

    // Call the board initialization
    printf("board_init() ");
    result = board_init();
    if(result != RC_OK)
    {
        printf("FAILED [%d]\n", result);
        return 1;
    }
    else
    {
        printf("PASSED\n");
    }

    // Read the sensors
    printf("board_read_sensors() ");
    memset((void*)&sensor_data, 0x0, sizeof(board_sensor_data_t));
    sensor_data.read_mask = BOARD_ALL_SENSORS;
    result = board_read_sensors(&sensor_data);
    if(result != RC_OK)
    {
        printf("FAILED [%d]\n", result);
        return 1;
    }
    else
    {
        printf("PASSED\n");
    }

    printf("Check sensor data ");
    if(board_sensor_read_status == 0)
    {
        printf("PASSED\n");
        test_board_dump_sensor_data(&sensor_data);
    }
    else
    {
        printf("FAILED [%d] \n", board_sensor_read_status);
        return 1;
    }

    // Suppose that the first reading is ok.
    // Should check with a static sensor data value
    all_sensor_data = sensor_data;

    printf("board_read_sensors_async() ");
    memset((void*)&sensor_data, 0x0, sizeof(board_sensor_data_t));
    memcpy(&test_sensor_data, &all_sensor_data, sizeof(board_sensor_data_t));
    sensor_data.read_mask = BOARD_ALL_SENSORS;
    result = test_board_read_sensor(&sensor_data, &test_sensor_data);
    if(result != RC_OK)
    {
        printf("FAILED [%d]\n", result);
        return 1;
    }
    else
    {
        printf("PASSED\n");
    }

    printf("board_read_sensors_async() ");
    memset((void*)&sensor_data, 0x0, sizeof(board_sensor_data_t));
    memset((void*)&test_sensor_data, 0x0, sizeof(board_sensor_data_t));
    sensor_data.read_mask = BOARD_NO_SENSOR;
    test_sensor_data.read_mask = BOARD_NO_SENSOR;
    result = test_board_read_sensor(&sensor_data, &test_sensor_data);
    if(result != RC_OK)
    {
        printf("FAILED [%d]\n", result);
        return 1;
    }
    else
    {
        printf("PASSED\n");
    }

    printf("board_read_sensors_async() ");
    memset((void*)&sensor_data, 0x0, sizeof(board_sensor_data_t));
    memset((void*)&test_sensor_data, 0x0, sizeof(board_sensor_data_t));
    sensor_data.read_mask = BOARD_PRESSURE_1;
    test_sensor_data.read_mask = BOARD_PRESSURE_1;
    test_sensor_data.pressure1 = all_sensor_data.pressure1;
    test_sensor_data.temperature1 = all_sensor_data.temperature1;
    result = test_board_read_sensor(&sensor_data, &test_sensor_data);
    if(result != RC_OK)
    {
        printf("FAILED [%d]\n", result);
        return 1;
    }
    else
    {
        printf("PASSED\n");
    }

    printf("board_read_sensors_async() ");
    memset((void*)&sensor_data, 0x0, sizeof(board_sensor_data_t));
    memset((void*)&test_sensor_data, 0x0, sizeof(board_sensor_data_t));
    sensor_data.read_mask = BOARD_FLOW_2;
    test_sensor_data.read_mask = BOARD_FLOW_2;
    test_sensor_data.flow2 = all_sensor_data.flow2;
    result = test_board_read_sensor(&sensor_data, &test_sensor_data);
    if(result != RC_OK)
    {
        printf("FAILED [%d]\n", result);
        return 1;
    }
    else
    {
        printf("PASSED\n");
    }

    printf("\nTEST CASE PASSED\n");

    return 0;
}


static int test_board_sim_read(int busId, int address, void* data, int size, int expectd_result)
{
    int result;

    printf("board_sim_bus_read() ");

    // Call the bus read function
    result = board_sim_bus_read(busId, address, data, size);
    
    // If there's no error simulate the triggering of the 
    // read completion IRQ.
    if(result == 0)
    {
        result = board_sim_bus_irq(busId);
    }

    // Check that the result value equals the expected one
    if(result != expectd_result)
    {
        printf("FAILED [%d]\n", result);
        return 1;
    }
    else
    {
        printf("PASSED\n");
    }

    return 0;
}

static int test_board_sim_write(int busId, int address, void* data, int size, int expectd_result)
{
    int result;

    printf("board_sim_bus_write() ");

    // Call the bus read function
    result = board_sim_bus_write(busId, address, data, size);
    
    // If there's no error simulate the triggering of the 
    // read completion IRQ.
    if(result == 0)
    {
        result = board_sim_bus_irq(busId);
    }

    // Check that the result value equals the expected one
    if(result != expectd_result)
    {
        printf("FAILED [%d]\n", result);
        return 1;
    }
    else
    {
        printf("PASSED\n");
    }

    return 0;
}

static int test_board_buffer(void* buffer, void* expected, int size)
{
    int result;

    printf("Test buffer content ");

    result = memcmp(buffer, expected, size);

    if(result != 0) 
        printf("FAILED [%d]\n", result);
    else
        printf("PASSED\n");

    return result;
}

static int test_board_dump_sensor_data(board_sensor_data_t* sensor_data)
{
    printf("  pressure1    --> %hd\n", sensor_data->pressure1);
    printf("  pressure2    --> %hd\n", sensor_data->pressure2);
    printf("  pressure3    --> %hd\n", sensor_data->pressure3);
    printf("  flow1        --> %hu\n", sensor_data->flow1);
    printf("  flow2        --> %hu\n", sensor_data->flow2);
    printf("  oxigen       --> %hu\n", sensor_data->o2);
    printf("  temperature1 --> %hd\n", sensor_data->temperature1);
    printf("  temperature2 --> %hd\n", sensor_data->temperature2);
    printf("  temperature3 --> %hd\n", sensor_data->temperature3);
    printf("  gpio         --> %hu\n", sensor_data->gpio);

    return 0;
}

static int test_board_read_sensor(board_sensor_data_t* data, board_sensor_data_t* test_data)
{
    int result;
    int i = 0;

    board_sensor_read_completed = 0;

    result = board_read_sensors_async(data, test_board_read_callback);
    if(result != RC_OK)
        return result;

    while((i < 10) && (!board_sensor_read_completed))
    {
        board_sim_bus_irq(1);
    }

    if(!board_sensor_read_completed)
    {
        printf("FAILED [callback never called]\n");
        return 1;
    }

    if(test_data)
    {
        result = memcmp(data, test_data, sizeof(board_sensor_data_t));

        if(result != 0)
        {
            printf("test_data:\n");
            test_board_dump_sensor_data(test_data);
            printf("\ndata:\n");
            test_board_dump_sensor_data(data);
        }
    }

    return result;
}

static void test_board_read_callback(int status)
{
    board_sensor_read_completed = 1;
    board_sensor_read_status = status;
}

int main()
{
    test_board_sim();
    test_board_driver();
    return 0;
}