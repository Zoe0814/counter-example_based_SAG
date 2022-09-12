#ifndef BACK_TRACK_HPP
#define BACK_TRACK_HPP

// #include <ostream>
// #include <vector>
// #include <algorithm> // for find
// #include <functional> // for hash
// #include <exception>
// #include <deque>
// #include <list>
// #include <iostream>
// #include <cassert>
// #include <set>
// #include "index_set.hpp"
// #include "cache.hpp"
// #include "time.hpp"

#include "jobs.hpp"


namespace NP {

	template<class Time>
	class Back_track {

	public:


		Back_track(	 JobID id,
					 unsigned int from ,
					 unsigned int  to,
		             Time latest_start,
		             bool visited = false)
		: job_id(id)
		, source(from)
		, target(to)
		, latest_start(latest_start)
		, visited(visited)
		//, deadline_miss(dm)
		//, max_complete(max_complete)
		{
		}

		JobID get_id() const
		{
			return job_id;
		}

		unsigned int get_source_id() const
		{
			return source;
		}

		unsigned int get_target_id() const
		{
			return target;
		}

		Time latest_start_time() const
		{
			return latest_start;
		}

		bool get_visited() const
		{
			return visited;
		}

			private:
			
			JobID job_id;
			unsigned int  source;
			unsigned int target;
			Time latest_start;
			bool visited;
	
	};

	 	
}

#endif
