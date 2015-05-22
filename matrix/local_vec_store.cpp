/*
 * Copyright 2015 Open Connectome Project (http://openconnecto.me)
 * Written by Da Zheng (zhengda1936@gmail.com)
 *
 * This file is part of FlashMatrix.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "log.h"
#include "local_vec_store.h"
#include "mem_data_frame.h"
#include "mem_vv_store.h"
#include "mem_vec_store.h"

namespace fm
{

detail::vec_store::ptr local_vec_store::sort_with_index()
{
	char *data_ptr = get_raw_arr();
	if (data_ptr == NULL) {
		BOOST_LOG_TRIVIAL(error) << "This local vec store is read-only";
		return detail::vec_store::ptr();
	}

	local_vec_store::ptr buf(new local_buf_vec_store(get_global_start(),
				get_length(), get_scalar_type<off_t>(), get_node_id()));
	// TODO we need to make is serial.
	get_type().get_sorter().sort_with_index(data_ptr,
			(off_t *) buf->get_raw_arr(), get_length(), false);
	return buf;
}

void local_vec_store::sort()
{
	char *data_ptr = get_raw_arr();
	assert(data_ptr);
	get_type().get_sorter().serial_sort(data_ptr, get_length(), false);
}

bool local_vec_store::is_sorted() const
{
	// Test if the array is sorted in the ascending order.
	return get_type().get_sorter().is_sorted(get_raw_arr(), get_length(), false);
}

data_frame::ptr local_vec_store::groupby(
		const gr_apply_operate<local_vec_store> &op, bool with_val) const
{
	const scalar_type &output_type = op.get_output_type();
	const agg_operate &find_next = get_type().get_agg_ops().get_find_next();

	assert(is_sorted());
	detail::vec_store::ptr agg;
	// TODO it might not be a good idea to create a mem_vector
	if (op.get_num_out_eles() == 1)
		agg = detail::mem_vec_store::create(0, output_type);
	else
		agg = detail::mem_vv_store::create(output_type);
	detail::mem_vec_store::ptr val;
	if (with_val)
		val = detail::mem_vec_store::create(0, get_type());

	size_t out_size;
	// If the user can predict the number of output elements, we can create
	// a buffer of the expected size.
	if (op.get_num_out_eles() > 0)
		out_size = op.get_num_out_eles();
	else
		// If the user can't, we create a small buffer.
		out_size = 16;
	local_buf_vec_store one_agg(0, out_size, output_type, -1);
	std::vector<const char *> val_locs;
	off_t init_global_start = get_global_start();
	size_t loc = 0;
	while (loc < get_length()) {
		size_t curr_length = get_length() - loc;
		const char *curr_ptr = get_raw_arr() + get_entry_size() * loc;
		size_t rel_end;
		find_next.run(curr_length, curr_ptr, &rel_end);
		local_cref_vec_store lcopy(curr_ptr, init_global_start + loc, rel_end,
				get_type(), get_node_id());
		op.run(curr_ptr, lcopy, one_agg);
		agg->append(one_agg);
		if (with_val)
			val_locs.push_back(curr_ptr);
		loc += rel_end;
	}
	if (with_val) {
		val->resize(val_locs.size());
		val->set(val_locs);
	}
	mem_data_frame::ptr ret = mem_data_frame::create();
	if (with_val)
		ret->add_vec("val", val);
	ret->add_vec("agg", agg);
	return ret;
}

bool local_vec_store::expose_sub_vec(off_t start, size_t length)
{
	if (start + length > orig_length)
		return false;
	off_t new_global_start = orig_global_start + start;
	off_t rel_off = new_global_start - global_start;
	global_start = new_global_start;
	detail::vec_store::resize(length);
	if (data)
		data += rel_off * get_entry_size();
	const_data += rel_off * get_entry_size();
	return true;
}

bool local_ref_vec_store::resize(size_t new_length)
{
	BOOST_LOG_TRIVIAL(error)
		<< "can't resize a local reference vector store";
	assert(0);
	return false;
}

bool local_cref_vec_store::resize(size_t new_length)
{
	BOOST_LOG_TRIVIAL(error)
		<< "can't resize a local reference vector store";
	assert(0);
	return false;
}

bool local_buf_vec_store::resize(size_t new_length)
{
	if (get_length() < new_length) {
		arr.expand(new_length * get_type().get_size());
		set_data(arr.get_raw(), arr.get_raw());
	}
	detail::vec_store::resize(new_length);
	return true;
}

}