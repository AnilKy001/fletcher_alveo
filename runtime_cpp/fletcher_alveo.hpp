#include <unistd.h>

#include <CL/opencl.h>
#include <CL/cl_ext.h>
#include "xclhal2.h"

#include "fletcher/fletcher.h"


#define debug_print(...) do { if (ENABLE_DEBUG_PRINT) fprintf(stderr, __VA_ARGS__); } while (0)

#define FLETCHER_PLATFORM_NAME "alveo"
#define ALVEO_DEVICE_NAME "xilinx_alveo_U250"  // Look at this and correct it.

typedef struct {

} AlveoConfig;

typedef struct {
	cl_int err;
	cl::Kernel kernel;
	cl::Device device;
	cl::Context context;
	cl::CommandQueue q;	
	cl::Program program;
	cl::Program::Binaries bins;
	auto fileBuf;
	auto platform_id;
	xclDeviceHandle handle;
} PlatformState;

PlatformState alveo_state ={NULL, ALVEO_DEVICE_NAME, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

/// @brief Store the platform name in a buffer of size /p size pointed to by /p name.
fstatus_t platformGetName(char *name, size_t size);

/// @brief Initialize the platform. \p arg may point to a null pointer or some custom structure for initialization
/// arguments.
fstatus_t platformInit(void *arg);

/// @brief Write \p value to MMIO register \p offset
fstatus_t platformWriteMMIO(uint64_t offset, uint32_t value);

/// @brief Read MMIO register \p offset into \p value
fstatus_t platformReadMMIO(uint64_t offset, uint32_t *value);

/// @brief Copy \p size bytes from host address \p host_source to device address \p device_destination.
fstatus_t platformCopyHostToDevice(const uint8_t *host_source, da_t device_destination, int64_t size);

/// @brief Copy \p size bytes from device address \p device_source to host address \p host_destination.
fstatus_t platformCopyDeviceToHost(const da_t device_source, uint8_t *host_destination, int64_t size);

/// @brief Allocate \p size bytes on the device.
fstatus_t platformDeviceMalloc(da_t *device_address, int64_t size);

/// @brief Free the memory allocated at \p device_address.
fstatus_t platformDeviceFree(da_t device_address);

/**
 * @brief Ensure the device can read \p size bytes from a host buffer at \p host_source.
 *
 * The address that the device can use to do so will be stored in \p device destination.
 *
 * For systems that operate in the same virtual address space as the application, this means the host source address
 * should just be copied into the device destination address. For systems that operate in a different address space
 * (for example, that must make a copy to on-board memory), this means this function must allocate a memory region to
 * copy the bytes to on the device. The address of this region will be the device destination address.
 *
 * This function can be used mainly for streamable applications. When data reuse is expected, on-board memory is often
 * faster. For this purpose, platformCacheHostBuffer can be used.
 *
 * @param host_source           Host address of the source data.
 * @param device_destination    Pointer to store the device destination address at.
 * @param size                  Number of bytes to prepare.
 * @param alloced               Whether the buffer caused a new allocation on the device, that should be freed after
 *                              usage (0 = not alloced, 1 = alloced).
 * @return                      FLETCHER_STATUS_OK if successful, FLETCHER_STATUS_ERROR otherwise.
 */
fstatus_t platformPrepareHostBuffer(const uint8_t *host_source, da_t *device_destination, int64_t size, int *alloced);

/**
 * @brief Explicitly cache \p size bytes from \p host_source on device on-board memory.
 *
 * The destination is stored at \p device_destination. This is essentially an allocate and copy. This function exists to
 * provide the means of explicitly copying the data to the device on-board memory, even when the device can initiate
 * loads in the same virtual address space as the application.
 *
 * @param host_source           Host address of the source data.
 * @param device_destination    Pointer to store the device destination address at.
 * @param size                  Number of bytes to prepare.
 * @return                      FLETCHER_STATUS_OK if successful, FLETCHER_STATUS_ERROR otherwise.
 */
fstatus_t platformCacheHostBuffer(const uint8_t *host_source, da_t *device_destination, int64_t size);

/**
 * @brief Terminate the platform.
 *
 * \p arg may point to a null pointer or some custom structure for termination arguments. Free any allocated memory.
 *
 * @param arg                   Arguments for termination.
 * @return                      FLETCHER_STATUS_OK if successful, FLETCHER_STATUS_ERROR otherwise.
 */
fstatus_t platformTerminate(void *arg);

