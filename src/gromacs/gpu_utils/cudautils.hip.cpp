/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2012,2014,2015,2016,2017, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */

#include "gmxpre.h"
#include <hip/hip_runtime.h>
#include <hip/hcc_detail/hip_runtime_api.h>
#include <hip/hcc_detail/hip_texture_types.h>
#include "cudautils.cuh"

#include <cassert>
#include <cstdlib>

#include "gromacs/gpu_utils/cuda_arch_utils.cuh"
#include "gromacs/gpu_utils/gpu_utils.h"
#include "gromacs/utility/gmxassert.h"
#include "gromacs/utility/smalloc.h"

/*** Generic CUDA data operation wrappers ***/

// TODO: template on transferKind to avoid runtime conditionals
int cu_copy_D2H(void *h_dest, void *d_src, size_t bytes,
                GpuApiCallBehavior transferKind, hipStream_t s = 0)
{
    hipError_t stat;

    if (h_dest == NULL || d_src == NULL || bytes == 0)
    {
        return -1;
    }

    switch (transferKind)
    {
        case GpuApiCallBehavior::Async:
            GMX_ASSERT(isHostMemoryPinned(h_dest), "Destination buffer was not pinned for CUDA");
            stat = hipMemcpyAsync(h_dest, d_src, bytes, hipMemcpyDeviceToHost, s);
            CU_RET_ERR(stat, "DtoH hipMemcpyAsync failed");
            break;

        case GpuApiCallBehavior::Sync:
            stat = hipMemcpy(h_dest, d_src, bytes, hipMemcpyDeviceToHost);
            CU_RET_ERR(stat, "DtoH hipMemcpy failed");
            break;

        default:
            throw;
    }

    return 0;
}

int cu_copy_D2H_sync(void * h_dest, void * d_src, size_t bytes)
{
    return cu_copy_D2H(h_dest, d_src, bytes, GpuApiCallBehavior::Sync);
}

/*!
 *  The copy is launched in stream s or if not specified, in stream 0.
 */
int cu_copy_D2H_async(void * h_dest, void * d_src, size_t bytes, hipStream_t s = 0)
{
    return cu_copy_D2H(h_dest, d_src, bytes, GpuApiCallBehavior::Async, s);
}

// TODO: template on transferKind to avoid runtime conditionals
int cu_copy_H2D(void *d_dest, void *h_src, size_t bytes,
                GpuApiCallBehavior transferKind, hipStream_t s = 0)
{
    hipError_t stat;

    if (d_dest == NULL || h_src == NULL || bytes == 0)
    {
        return -1;
    }

    switch (transferKind)
    {
        case GpuApiCallBehavior::Async:
            GMX_ASSERT(isHostMemoryPinned(h_src), "Source buffer was not pinned for CUDA");
            stat = hipMemcpyAsync(d_dest, h_src, bytes, hipMemcpyHostToDevice, s);
            CU_RET_ERR(stat, "HtoD hipMemcpyAsync failed");
            break;

        case GpuApiCallBehavior::Sync:
            stat = hipMemcpy(d_dest, h_src, bytes, hipMemcpyHostToDevice);
            CU_RET_ERR(stat, "HtoD hipMemcpy failed");
            break;

        default:
            throw;
    }

    return 0;
}

int cu_copy_H2D_sync(void * d_dest, void * h_src, size_t bytes)
{
    return cu_copy_H2D(d_dest, h_src, bytes, GpuApiCallBehavior::Sync);
}

/*!
 *  The copy is launched in stream s or if not specified, in stream 0.
 */
int cu_copy_H2D_async(void * d_dest, void * h_src, size_t bytes, hipStream_t s = 0)
{
    return cu_copy_H2D(d_dest, h_src, bytes, GpuApiCallBehavior::Async, s);
}

/**** Operation on buffered arrays (arrays with "over-allocation" in gmx wording) *****/

/*!
 * If the pointers to the size variables are NULL no resetting happens.
 */
void cu_free_buffered(void *d_ptr, int *n, int *nalloc)
{
    hipError_t stat;

    if (d_ptr)
    {
        stat = hipFree(d_ptr);
        CU_RET_ERR(stat, "hipFree failed");
    }

    if (n)
    {
        *n = -1;
    }

    if (nalloc)
    {
        *nalloc = -1;
    }
}

/*!
 *  Reallocation of the memory pointed by d_ptr and copying of the data from
 *  the location pointed by h_src host-side pointer is done. Allocation is
 *  buffered and therefore freeing is only needed if the previously allocated
 *  space is not enough.
 *  The H2D copy is launched in stream s and can be done synchronously or
 *  asynchronously (the default is the latter).
 */
void cu_realloc_buffered(void **d_dest, void *h_src,
                         size_t type_size,
                         int *curr_size, int *curr_alloc_size,
                         int req_size,
                         hipStream_t s,
                         bool bAsync = true)
{
    hipError_t stat;

    if (d_dest == NULL || req_size < 0)
    {
        return;
    }

    /* reallocate only if the data does not fit = allocation size is smaller
       than the current requested size */
    if (req_size > *curr_alloc_size)
    {
        /* only free if the array has already been initialized */
        if (*curr_alloc_size >= 0)
        {
            cu_free_buffered(*d_dest, curr_size, curr_alloc_size);
        }

        *curr_alloc_size = over_alloc_large(req_size);

        stat = hipMalloc(d_dest, *curr_alloc_size * type_size);
        CU_RET_ERR(stat, "hipMalloc failed in cu_free_buffered");
    }

    /* size could have changed without actual reallocation */
    *curr_size = req_size;

    /* upload to device */
    if (h_src)
    {
        if (bAsync)
        {
            cu_copy_H2D_async(*d_dest, h_src, *curr_size * type_size, s);
        }
        else
        {
            cu_copy_H2D_sync(*d_dest, h_src,  *curr_size * type_size);
        }
    }
}

/*! \brief Return whether texture objects are used on this device.
 *
 * \param[in]   pointer to the GPU device info structure to inspect for texture objects support
 * \return      true if texture objects are used on this device
 */
static inline bool use_texobj(const gmx_device_info_t *dev_info)
{
    assert(!c_disableCudaTextures);
    /* Only device CC >= 3.0 (Kepler and later) support texture objects */
    return (dev_info->prop.major >= 3);
}

/*! \brief Set up texture object for an array of type T.
 *
 * Set up texture object for an array of type T and bind it to the device memory
 * \p d_ptr points to.
 *
 * \tparam[in] T        Raw data type
 * \param[out] texObj   texture object to initialize
 * \param[in]  d_ptr    pointer to device global memory to bind \p texObj to
 * \param[in]  sizeInBytes  size of memory area to bind \p texObj to
 */
template <typename T>
static void setup1DTexture(hipTextureObject_t &texObj,
                           void                *d_ptr,
                           size_t               sizeInBytes)
{
    assert(!c_disableCudaTextures);

    hipError_t      stat;
    hipResourceDesc rd;
    hipTextureDesc  td;

    memset(&rd, 0, sizeof(rd));
    rd.resType                = hipResourceTypeLinear;
    rd.res.linear.devPtr      = d_ptr;
    rd.res.linear.desc        = hipCreateChannelDesc<T>();
    rd.res.linear.sizeInBytes = sizeInBytes;

    memset(&td, 0, sizeof(td));
    td.readMode                 = hipReadModeElementType;
    stat = hipCreateTextureObject(&texObj, &rd, &td, NULL);
    CU_RET_ERR(stat, "cudaCreateTextureObject failed");
}

/*! \brief Set up texture reference for an array of type T.
 *
 * Set up texture object for an array of type T and bind it to the device memory
 * \p d_ptr points to.
 *
 * \tparam[in] T        Raw data type
 * \param[out] texObj   texture reference to initialize
 * \param[in]  d_ptr    pointer to device global memory to bind \p texObj to
 * \param[in]  sizeInBytes  size of memory area to bind \p texObj to
 */
template <typename T>
static void setup1DTexture(const struct texture<T, 1, hipReadModeElementType> *texRef,
                           const void                                          *d_ptr,
                           size_t                                              sizeInBytes)
{
    assert(!c_disableCudaTextures);

    hipError_t           stat;
    const textureReference* texRefPtr;
    hipGetTextureReference(&texRefPtr, texRef);

  //  textureReference* texRefPtr2 = texRefPtr;
    hipChannelFormatDesc cd;
    cd   = hipCreateChannelDesc<T>();
    stat = hipBindTexture(nullptr, const_cast<textureReference*>(texRefPtr), d_ptr, &cd, sizeInBytes);
    CU_RET_ERR(stat, "hipBindTexture failed");
}

template <typename T>
void initParamLookupTable(T                        * &d_ptr,
                          hipTextureObject_t       &texObj,
                          const struct texture<T, 1, hipReadModeElementType> *texRef,
                          const T                   *h_ptr,
                          int                        numElem,
                          const gmx_device_info_t   *devInfo)
{
    const size_t sizeInBytes = numElem * sizeof(*d_ptr);
    hipError_t  stat        = hipMalloc((void **)&d_ptr, sizeInBytes);
    CU_RET_ERR(stat, "hipMalloc failed in initParamLookupTable");
    cu_copy_H2D_sync(d_ptr, (void *)h_ptr, sizeInBytes);

    if (!c_disableCudaTextures)
    {
       // if (use_texobj(devInfo))
        if(1)
        {
            setup1DTexture<T>(texObj, d_ptr, sizeInBytes);
        }
        else
        {
            setup1DTexture<T>(texRef, d_ptr, sizeInBytes);
        }
    }
}

template <typename T>
void destroyParamLookupTable(T                       *d_ptr,
                             hipTextureObject_t      texObj,
                             const struct texture<T, 1, hipReadModeElementType> *texRef,
                             const gmx_device_info_t *devInfo)
{
    if (!c_disableCudaTextures)
    {
        if (use_texobj(devInfo))
        {
            CU_RET_ERR(hipDestroyTextureObject(texObj), "cudaDestroyTextureObject on texObj failed");
        }
        else
        {
            CU_RET_ERR(hipUnbindTexture(texRef), "hipUnbindTexture on texRef failed");
        }
    }
    CU_RET_ERR(hipFree(d_ptr), "hipFree failed");
}

/*! \brief Add explicit instantiations of init/destroyParamLookupTable() here as needed.
 * One should also verify that the result of hipCreateChannelDesc<T>() during texture setup
 * looks reasonable, when instantiating the templates for new types - just in case.
 */
template void initParamLookupTable<float>(float * &, hipTextureObject_t &, const texture<float, 1, hipReadModeElementType> *, const float *, int, const gmx_device_info_t *);
template void destroyParamLookupTable<float>(float *, hipTextureObject_t, const texture<float, 1, hipReadModeElementType> *, const gmx_device_info_t *);
template void initParamLookupTable<int>(int * &, hipTextureObject_t &, const texture<int, 1, hipReadModeElementType> *, const int *, int, const gmx_device_info_t *);
template void destroyParamLookupTable<int>(int *, hipTextureObject_t, const texture<int, 1, hipReadModeElementType> *, const gmx_device_info_t *);
