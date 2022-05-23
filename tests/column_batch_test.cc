/** Copyright 2022 Alibaba Group Holding Limited. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless CHECKd by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define BOOST_TEST_MODULE column_batch

#include <hiactor/core/column_batch.hh>
#include <hiactor/core/actor_message.hh>
#include <hiactor/core/reference_base.hh>
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(test_column_batch)

BOOST_AUTO_TEST_CASE(fixed_cb_dump) {
    const int batch_size = 10;
    hiactor::byte_tmpbuf_t tmp_buf;
    /// fixed column batch
    hiactor::cb::column_batch<int64_t> fcb{batch_size};
    for (int i = 0; i < 6; ++i) {
        fcb.push_back(5102 + i);
    }
    fcb.dump_to(tmp_buf);
    hiactor::cb::column_batch<int64_t> r_fcb{std::move(tmp_buf)};
    BOOST_CHECK_EQUAL(r_fcb.size(), 6);
    for (size_t i = 0; i < r_fcb.size(); ++i) {
        BOOST_CHECK_EQUAL(r_fcb[i], 5102 + i);
    }
}

BOOST_AUTO_TEST_CASE(dynamic_cb_dump) {
    const int batch_size = 10;
    hiactor::cb::column_batch<hiactor::cb::path> dcb{batch_size, 10};

    hiactor::byte_tmpbuf_t tmp_buf[2];
    std::vector<hiactor::cb::path::data_type> path_content{0, 0, 0};
    hiactor::cb::path::data_type counter = 0;
    for (int i = 0; i < 6; ++i) {
        path_content[0] = ++counter;
        path_content[1] = ++counter;
        path_content[2] = ++counter;
        dcb.push_back(hiactor::cb::path{
            path_content.data(),
            static_cast<uint32_t>(path_content.size())
        });
    }

    dcb.dump_to(tmp_buf[0], tmp_buf[1]);
    hiactor::cb::column_batch<hiactor::cb::path> r_dcb{std::move(tmp_buf[0]), std::move(tmp_buf[1])};

    counter = 0;
    BOOST_CHECK_EQUAL(r_dcb.size(), 6);
    for (size_t i = 0; i < r_dcb.size(); ++i) {
        BOOST_CHECK_EQUAL(r_dcb[i].len, path_content.size());
        for (uint32_t j = 0; j < r_dcb[i].len; ++j) {
            BOOST_CHECK_EQUAL(++counter, r_dcb[i].ptr[j]);
        }
    }
}

BOOST_AUTO_TEST_CASE(fixed_cb_ref_count) {
    hiactor::cb::column_batch<int32_t> ib_0{10};
    for (int i = 0; i < 6; ++i) {
        ib_0.push_back(i + 10);
    }

    // share a fcb
    const auto ib_1 = ib_0.share();
    BOOST_CHECK_EQUAL(ib_1.size(), ib_0.size());
    BOOST_CHECK_EQUAL(ib_1.capacity(), ib_0.capacity());
    for (size_t i = 0; i < ib_1.size(); ++i) {
        BOOST_CHECK_EQUAL(ib_0[i], ib_1[i]);
    }
}

BOOST_AUTO_TEST_CASE(dynamic_cb_ref_count) {
    hiactor::cb::column_batch<hiactor::cb::path> pb_0{10, 20};
    std::vector<hiactor::cb::path::data_type> path_content{0, 0, 0};
    hiactor::cb::path::data_type counter = 0;
    for (int i = 0; i < 6; ++i) {
        path_content[0] = ++counter;
        path_content[1] = ++counter;
        path_content[2] = ++counter;
        pb_0.push_back(hiactor::cb::path{
            path_content.data(),
            static_cast<uint32_t>(path_content.size())
        });
    }

    // share a dcb
    auto pb_1 = pb_0.share();
    BOOST_CHECK_EQUAL(pb_1.size(), pb_0.size());
    BOOST_CHECK_EQUAL(pb_1.capacity(), pb_0.capacity());
    for (size_t i = 0; i < pb_1.size(); ++i) {
        BOOST_CHECK_EQUAL(pb_1[i].len, pb_0[i].len);
        for (uint32_t j = 0; j < pb_1[i].len; ++j) {
            BOOST_CHECK_EQUAL(pb_1[i].ptr[j], pb_0[i].ptr[j]);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
