/** Copyright 2021 Alibaba Group Holding Limited. All Rights Reserved.
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
 */

#include "device.h"

#include <cuda.h>
#include <cuda_runtime.h>

extern "C"
void cuda_set_device(uint32_t dev_id){
    cudaSetDevice(dev_id);
}

extern "C"
uint32_t cuda_get_device(){
    int32_t dev_id;
    cudaGetDevice(&dev_id);
    return dev_id;
}

extern "C"
uint32_t cuda_get_device_count(){
    int32_t dev_cnt = 0;
    cudaGetDeviceCount(&dev_cnt);
    return dev_cnt;
}

extern "C"
cuda_stream_handle create_cuda_stream() {
    cudaStream_t retval;
    cudaStreamCreate(&retval);
    return static_cast<cuda_stream_handle>(retval);
}

extern "C"
cuda_event_handle create_cuda_event() {
    cudaEvent_t stop;
    cudaEventCreate(&stop);
    return static_cast<cuda_event_handle>(stop);
}

extern "C" 
void cuda_stream_event_record(cuda_stream_handle strm_hdl, cuda_event_handle eve_hdl){
    cudaEventRecord(static_cast<cudaEvent_t>(eve_hdl), static_cast<cudaStream_t>(strm_hdl));
}

extern "C" 
bool cuda_check(cuda_event_handle eve_hdl){
    return !(cudaEventQuery(static_cast<cudaEvent_t>(eve_hdl)) == cudaErrorNotReady);
}
