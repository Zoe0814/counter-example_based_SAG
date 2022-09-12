#include <iostream>
#include <map>
#include <deque>
#include <list>
#include <iterator>
#include <algorithm>
#include "time.hpp"

#include <ostream>
#include "Job_backtrack.hpp"
#include "io.hpp"
#include <sstream>
#include "jobs.hpp"

#include "problem.hpp"
#include "io.hpp"

namespace NP {


	namespace Uniproc {


	template<class Time> class Graph
		{
			public:

			typedef Scheduling_problem<Time> Problem;
			
			typedef std::vector<Job<Time>> Job_set;
			typedef typename Scheduling_problem<Time>::Back_tracking Back_tracking;
			typedef typename Scheduling_problem<Time>::Workload Workload;


			typedef Back_track<Time> Back;
			
			//typedef typename Scheduling_problem<Time>::Abort_actions Abort_actions;
			// convenience interface for tests
			static Graph explore(
					const Problem& prob,
					const Analysis_options& opts)
			{
				// this is a uniprocessor analysis
				assert(prob.num_processors == 1);

				auto g = Graph( prob.jobs,prob.backtracks);

				g.cpu_time.start();
				g.explore();
				g.cpu_time.stop();	
	
				// for(auto p : g.get_path()) {
				// 	std::cout <<"what is the path looks  like" << p.scheduled->get_id() << '\n';
				// }			

				return g;
			}


			void explore()
			{

				// std::cout <<"Perform backtracing start at state " <<  last_target<< '\n'; 
				
				//DFS(last_target,last_EFT_back);
				DFS(last_target);

			}

			bool is_schedulable() const
			{
				return 0;
			}

			bool was_timed_out() const
			{
				return 0;
			}

			unsigned long number_of_states() const
			{
				return num_states;
			}

			unsigned long number_of_edges() const
			{
				return num_edges;
			}

			unsigned long max_exploration_front_width() const
			{
				return width;
			}

			double get_cpu_time() const
			{
				return cpu_time;
			}


			struct Path {
				const Job<Time>* scheduled;
				const Time earliest_start_time_for_ddl;
				const Time latest_start_time_for_ddl;
				const unsigned int target;

				Path(const Job<Time>* s, const Time est, const Time lst, unsigned int tar)
				: scheduled(s)
				, earliest_start_time_for_ddl(est)
				, latest_start_time_for_ddl(lst)
				, target(tar)
				{
				}


				Time earliest_start_time_counterexample() const
				{
					return earliest_start_time_for_ddl;
				}

				Time latest_start_time_counterexample() const
				{
					return latest_start_time_for_ddl;
				}

			};

			const std::deque<Path>& get_path() const
			{
				return path;
			}
			

			private:

			const Workload& jobs;

			typedef const  Back_track<Time>* Back_ref;

			typedef std::multimap<unsigned int, Back_ref> By_time_map;

			std::map<unsigned int, bool> visited_map;

			typedef std::deque<Back> Back_list; 

			Back_list back_list;
			Back_list deadline_miss_path;

			typedef std::multimap<unsigned int, Back_list> By_from_map;
			
			By_time_map from_jobs_latest_arrival;
			
			By_from_map time_map;

			unsigned int last_source;
			unsigned int last_target;
			// JobID last_job_id;
			Time last_job_lt;
			Time last_EFT_back;
			unsigned int begin_source;

			Processor_clock cpu_time;

			unsigned long num_edges;

			unsigned long width;

			unsigned long num_states;

			std::deque<Path> path;

			Graph(const Workload& jobs,
				const Back_tracking &back_tracks)
			:jobs(jobs)
			{	

				
				auto dm_info = back_tracks[back_tracks.size() - 1];

				last_source = dm_info.get_source_id();
				last_target = dm_info.get_target_id();
				auto last_job_id = dm_info.get_id();
				last_job_lt = dm_info.latest_start_time();
	

				for (const Back_track<Time>& back : back_tracks) {
					const Job<Time>& j = lookup<Time>(jobs, back.get_id());
					if(back.get_id()==last_job_id){
						last_EFT_back = j.get_deadline()+1;
					}

				}

				for (const Back_track<Time>& back : back_tracks) {

					from_jobs_latest_arrival.insert({back.get_target_id(), &back});
				}

				auto begin_target = from_jobs_latest_arrival.begin()->first;


				for (unsigned int it = begin_target; it != last_target; ++it)
				{
				    back_list.clear();

				    auto range = from_jobs_latest_arrival.equal_range(it);
		  			
				    for (auto i = range.first; i != range.second; ++i)
				    {
				    	const Back_track<Time>& b = *(i->second);
				    	back_list.emplace_back(b.get_id(), b.get_source_id(), b.get_target_id(), b.latest_start_time());				       	
				    }
					time_map.insert({it, back_list});

					visited_map.insert({it, false});
				}							

			
				// insert the deadline miss map

    			back_list.clear();
				back_list.emplace_back(last_job_id, last_source, last_target, last_job_lt);
				time_map.insert({last_target, back_list});
				visited_map.insert({last_target, false});
			

			}
			private:
			Time est_m;
			bool found_one = false;

			std::vector<std::size_t> branch_list;
			
			//void DFS(unsigned int v, Time est_m)
			void DFS(unsigned int v)
			{

					if(path.size() ==0){
						est_m = last_EFT_back;
					}
					// Mark the current node as visited and print it
					visited_map[v] = true; 		
					found_one = false;
	
					// Recur for all the vertices adjacent to this vertex				
				    auto range = time_map.equal_range(v);				    
				    unsigned long n;
		  			
				    for (auto i = range.first; i != range.second; ++i)
				    {
						auto target = i->first;
						std::deque<Back> check = i->second;
						n = check.size();	
						if(n>1)
							branch_list.push_back(i->first);
						width = std::max(width, n);
						for(const Back_track<Time>& c : check) {

							const Job<Time>& j = lookup<Time>(jobs, c.get_id());
		
							est_m = back_earliest_start_time(j,est_m);

							if(est_m <= c.latest_start_time() && !visited_map[c.get_source_id()]) {

								const Time lst_m = c.latest_start_time();

								path.emplace_back(&j, est_m, lst_m, target);
 								n = path.size();	
								num_edges = std::max(num_edges, n);
								num_states = num_edges+1;
								
								found_one = true;
								
								if( c.get_source_id() == 0)
									return;
								//return DFS(c.get_source_id(), est_m);	
								return DFS(c.get_source_id());						
							}

						}
					// if didn't find one, return to the previous branch
				    }

					if(found_one == false){
						if(!branch_list.empty()){
							//auto v = branch_list.back();
							//std::cout <<"the last element"<< branch_list.size()<< '\n';
							branch_list.pop_back();
							//std::cout <<"didn't find one, need back to previous" << '\n';
							//clean the saved path
								for(auto p : path) {
							    	if(p.target == v)
										return;
									return path.pop_back();
								}
								path.pop_back();
								DFS(v);
						}
					}				    
			}

			Time back_earliest_start_time(const Job<Time>& j, Time eft_m)
			{
			
				return std::max((eft_m - j.maximal_cost()), j.earliest_arrival());
			}	
					
		};		
	}
}



