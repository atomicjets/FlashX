#ifndef __PART_GLOBAL_CACHED_PRIVATE_H__
#define __PART_GLOBAL_CACHED_PRIVATE_H__

#include <errno.h>

#include <tr1/unordered_map>

#include "parameters.h"
#include "messaging.h"
#include "global_cached_private.h"

const int REPLY_BUF_SIZE = 1000;
const int REQ_BUF_SIZE = 1000;
const int MSG_BUF_SIZE = 128;

// The size of a request >= sizeof(io_request).
const int NUMA_REQ_BUF_SIZE = NUMA_MSG_SIZE / sizeof(io_request);
// The size of a reply >= sizeof(io_reply).
const int NUMA_REPLY_BUF_SIZE = NUMA_MSG_SIZE / sizeof(io_reply);

struct thread_group;
class part_io_process_table;
class disk_read_thread;
class file_mapper;
class group_request_sender;

/**
 * This provides interface for application threads issue IO requests
 * in the NUMA architecture. If the requests are sent to disks connected
 * to the local processor, they are processed directly. No message passing
 * is needed.
 */
class part_global_cached_io: public io_interface
{
	part_io_process_table *global_table;
	const struct thread_group *local_group;
	const cache_config *cache_conf;

	global_cached_io *underlying;

	msg_queue<io_reply> *reply_queue;
	// This reply message buffer is used when copying remote messages
	// to the local memory.
	message<io_reply> local_reply_msgs[MSG_BUF_SIZE];
	// This request buffer is used when distributing requests.
	io_request local_req_buf[REQ_BUF_SIZE];
	// This reply buffer is used when processing replies.
	io_reply local_reply_buf[NUMA_REPLY_BUF_SIZE];

	/*
	 * there is a sender for each node.
	 */
	// group id <-> msg sender
	std::tr1::unordered_map<int, group_request_sender *> req_senders;

	/*
	 * These reply senders are to send replies to this IO. They are made
	 * to be thread-safe, so all threads can use them. However, the remote
	 * access on a NUMA machine is slow, so each NUMA node has a copy of
	 * the reply sender to improve performance.
	 */
	std::vector<thread_safe_msg_sender<io_reply> *> reply_senders;

	// All these variables are updated in one thread, so it's fine without
	// any concurrency control.
	long processed_requests;
	long sent_requests;
	long processed_replies;
	long remote_reads;

	// It's the callback from the user.
	callback *final_cb;

	int process_replies();
	void notify_upper(io_request *reqs[], int num);

	part_global_cached_io(io_interface *underlying, part_io_process_table *);
	~part_global_cached_io();
public:
	static part_io_process_table *open_file(
			const std::vector<disk_read_thread *> &io_threads,
			file_mapper *mapper, const cache_config *config);
	static int close_file(part_io_process_table *table);

	static part_global_cached_io *create(io_interface *underlying,
			part_io_process_table *table) {
		int node_id = underlying->get_node_id();
		assert(node_id >= 0);
		void *addr = numa_alloc_onnode(sizeof(part_global_cached_io), node_id);
		return new(addr) part_global_cached_io(underlying, table);
	}

	static void destroy(part_global_cached_io *io) {
		io->~part_global_cached_io();
		numa_free(io, sizeof(*io));
	}

	int init();

	thread_safe_msg_sender<io_reply> *get_reply_sender(int node_id) const {
		return reply_senders[node_id];
	}

	virtual bool set_callback(callback *cb) {
		this->final_cb = cb;
		return true;
	}

	virtual callback *get_callback() {
		return final_cb;
	}

	int reply(io_request *requests, io_reply *replies, int num);

	int process_requests(int max_nreqs);

	int process_reply(io_reply *reply);

	virtual void notify_completion(io_request *reqs[], int num);
	void access(io_request *requests, int num, io_status *status);
	io_status access(char *, off_t, ssize_t, int) {
		return IO_UNSUPPORTED;
	}

	void flush_requests();

	void cleanup();
	int preload(off_t start, long size);

	bool support_aio() {
		return true;
	}

	virtual int get_file_id() const {
		return underlying->get_file_id();
	}
	virtual int wait4complete(int num);
	virtual int num_pending_ios() const {
		// the number of pending requests on the remote nodes.
		return sent_requests - processed_replies
			// The number of pending requests in the local IO instance.
			+ underlying->num_pending_ios();
	}

	friend class node_cached_io;

#ifdef STATISTICS
	virtual void print_stat(int nthreads);
#endif
};

#endif
