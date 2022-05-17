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

#pragma once

typedef void* cuda_stream_handle;
typedef void* cuda_event_handle;

extern "C"
void cuda_set_device(uint32_t dev_id);

extern "C"
uint32_t cuda_get_device();

extern "C"
uint32_t cuda_get_device_count();

extern "C"
bool cuda_check(cuda_event_handle eve_hdl);

extern "C"
void cuda_stream_event_record(cuda_stream_handle strm_hdl, cuda_event_handle eve_hdl);

extern "C"
cuda_stream_handle create_cuda_stream();

extern "C"
cuda_event_handle create_cuda_event();


