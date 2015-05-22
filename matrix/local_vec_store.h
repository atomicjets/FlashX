#ifndef __LOCAL_VEC_STORE_H__
#define __LOCAL_VEC_STORE_H__

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

#include <string.h>

#include "vec_store.h"
#include "bulk_operate.h"
#include "raw_data_array.h"

namespace fm
{

class data_frame;

class local_vec_store: public detail::vec_store
{
	const off_t orig_global_start;
	const size_t orig_length;
	const char *const_data;
	char *data;
	off_t global_start;
	int node_id;
protected:
	off_t get_local_start() const {
		return global_start - orig_global_start;
	}

	void set_data(const char *const_data, char *data) {
		this->data = data;
		this->const_data = const_data;
	}

public:
	local_vec_store(const char *const_data, char *data, off_t _global_start,
			size_t length, const scalar_type &type,
			int node_id): detail::vec_store(length, type, true), orig_global_start(
				_global_start), orig_length(length) {
		this->data = data;
		this->const_data = const_data;
		this->global_start = _global_start;
		this->node_id = node_id;
	}

	int get_node_id() const {
		return node_id;
	}

	off_t get_global_start() const {
		return global_start;
	}

	typedef std::shared_ptr<local_vec_store> ptr;
	typedef std::shared_ptr<const local_vec_store> const_ptr;

	using detail::vec_store::get_raw_arr;
	virtual const char *get_raw_arr() const {
		return const_data;
	}
	virtual char *get_raw_arr() {
		return data;
	}

	std::shared_ptr<data_frame> groupby(
			const gr_apply_operate<local_vec_store> &op, bool with_val) const;

	virtual void reset_data() {
		memset(get_raw_arr(), 0, get_length() * get_entry_size());
	}

	virtual void set_data(const set_operate &op) {
		// We always view a vector as a single-column matrix.
		op.set(get_raw_arr(), get_length(), global_start, 0);
	}

	/*
	 * Expose part of the local vector store.
	 * @start is the start location relative to the start of the original
	 * local vector store.
	 */
	virtual bool expose_sub_vec(off_t start, size_t length);

	virtual detail::vec_store::ptr sort_with_index();
	virtual void sort();
	virtual bool is_sorted() const;

	template<class T>
	T get(off_t idx) const {
		return *(const T *) (const_data + idx * sizeof(T));
	}
	template<class T>
	void set(off_t idx, T val) {
		assert(const_data == data);
		*(T *) (data + idx * sizeof(T)) = val;
	}

	/*
	 * The following methods aren't needed by this class and its child classes.
	 */

	virtual std::shared_ptr<local_vec_store> get_portion(off_t loc,
			size_t size) {
		assert(0);
		return std::shared_ptr<local_vec_store>();
	}
	virtual std::shared_ptr<const local_vec_store> get_portion(off_t loc,
			size_t size) const {
		assert(0);
		return std::shared_ptr<local_vec_store>();
	}
	virtual size_t get_portion_size() const {
		assert(0);
		return 0;
	}
	virtual bool append(std::vector<detail::vec_store::const_ptr>::const_iterator vec_it,
			std::vector<detail::vec_store::const_ptr>::const_iterator vec_end) {
		assert(0);
		return false;
	}
	virtual bool append(const detail::vec_store &vec) {
		assert(0);
		return false;
	}
	virtual detail::vec_store::ptr deep_copy() const {
		assert(0);
		return detail::vec_store::ptr();
	}
};

class local_ref_vec_store: public local_vec_store
{
public:
	local_ref_vec_store(char *data, off_t global_start, size_t length,
			const scalar_type &type, int node_id): local_vec_store(data,
				data, global_start, length, type, node_id) {
	}
	virtual bool resize(size_t new_length);

	virtual detail::vec_store::ptr shallow_copy() {
		return detail::vec_store::ptr(new local_ref_vec_store(*this));
	}
	virtual detail::vec_store::const_ptr shallow_copy() const {
		return detail::vec_store::const_ptr(new local_ref_vec_store(*this));
	}
};

class local_cref_vec_store: public local_vec_store
{
public:
	local_cref_vec_store(const char *data, off_t global_start, size_t length,
			const scalar_type &type, int node_id): local_vec_store(data,
				NULL, global_start, length, type, node_id) {
	}
	virtual bool resize(size_t new_length);

	virtual detail::vec_store::ptr shallow_copy() {
		return detail::vec_store::ptr(new local_cref_vec_store(*this));
	}
	virtual detail::vec_store::const_ptr shallow_copy() const {
		return detail::vec_store::const_ptr(new local_cref_vec_store(*this));
	}
};

class local_buf_vec_store: public local_vec_store
{
	detail::raw_data_array arr;
public:
	local_buf_vec_store(off_t global_start, size_t length,
			const scalar_type &type, int node_id): local_vec_store(NULL,
				NULL, global_start, length, type, node_id),
			arr(length * type.get_size()) {
		set_data(arr.get_raw(), arr.get_raw());
	}

	virtual bool resize(size_t new_length);

	virtual detail::vec_store::ptr shallow_copy() {
		return detail::vec_store::ptr(new local_buf_vec_store(*this));
	}
	virtual detail::vec_store::const_ptr shallow_copy() const {
		return detail::vec_store::const_ptr(new local_buf_vec_store(*this));
	}
};

}

#endif