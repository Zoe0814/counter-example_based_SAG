#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

#include <sys/resource.h>

#include "OptionParser.h"

#include "config.h"

#ifdef CONFIG_PARALLEL

#include "tbb/task_scheduler_init.h"

#endif

#include "problem.hpp"
#include "uni/space.hpp"
#include "uni/back.hpp"
#include "global/space.hpp"
#include "io.hpp"
#include "clock.hpp"

#define MAX_PROCESSORS 512

// command line options
static bool want_naive;
static bool want_dense;
static bool want_prm_iip;
static bool want_cw_iip;

static bool want_first_run_counterexample;
static bool want_second_run_counterexample;


static bool want_precedence = false;
static std::string precedence_file;

static bool want_aborts = false;
static std::string aborts_file;


static bool want_backtracks = false;
static std::string backtrack_file;


static bool want_multiprocessor = false;
static unsigned int num_processors = 1;

#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
static bool want_dot_graph;
#endif
static double timeout;
static unsigned int max_depth = 0;

static bool want_rta_file;

static bool continue_after_dl_miss = false;

#ifdef CONFIG_PARALLEL
static unsigned int num_worker_threads = 0;
#endif

bool detect_deadline_miss_first_run = false;

struct Analysis_result {

	bool schedulable;
	bool timeout;
	unsigned long number_of_states, number_of_edges, max_width, number_of_jobs;
	double cpu_time;
	std::string graph;
	std::string response_times_csv;
	std::string reduced_job_csv;
	std::string back_track_csv;
	std::string counter_example_csv;

};

// the analyze function, and this analyze function is called in the function called process_stream
template<class Time, class Space>
static Analysis_result analyze(
	std::istream &in,
	std::istream &dag_in,
	std::istream &aborts_in,
	std::istream &backtrack_in)
{
#ifdef CONFIG_PARALLEL
	tbb::task_scheduler_init init(
		num_worker_threads ? num_worker_threads : tbb::task_scheduler_init::automatic);
#endif

	// std::cout <<"I am in analyze function"<< '\n';

	NP::Scheduling_problem<Time> problem{
	NP::parse_file<Time>(in),
	NP::parse_dag_file(dag_in),
	NP::parse_abort_file<Time>(aborts_in),
	NP::parse_backtrack_file<Time>(backtrack_in),
	num_processors};

	// Set common analysis options
	NP::Analysis_options opts;

	opts.timeout = timeout;
	opts.max_depth = max_depth;
	opts.early_exit = !continue_after_dl_miss;
	opts.num_buckets = problem.jobs.size();
	opts.be_naive = want_naive;
	

	// Actually call the analysis engine
	auto space = Space::explore(problem, opts);

#ifndef CONFIG_COUNTER_EXAMPLE
	auto PoP = std::ostringstream();

	if (want_second_run_counterexample && !want_backtracks ) {

		bool detect_deadline_miss = false;

		unsigned int dm_from ; 
		unsigned int dm_to ; 	
		unsigned long  dm_task ; 
		unsigned long  dm_job ; 
		Time dm_ls ;

		//PoP << "FROM, TO, job, cos min, cos max, latest start,deadline miss, deadline, max finish time" << std::endl;
		PoP << "FROM, TO, task, job, latest start time" << std::endl;

		for (auto e : space.get_ce_edges()) {

				if (e.deadline_miss_possible()) {
					detect_deadline_miss = true;
					dm_from = e.source->get_state_ID(); 
					dm_to = e.target->get_state_ID(); 	
					dm_job = e.scheduled->get_job_id(); 
					dm_task = e.scheduled->get_task_id(); 
					dm_ls = e.latest_start_time();
					//dm_deadlinemiss = e.deadline_miss_possible(); 
					//dm_lf = e.latest_finish_time(); 						 												
					}
				else{
					PoP << e.source->get_state_ID()   << ", "
					    << e.target->get_state_ID()  << ", "
					    << e.scheduled-> get_task_id()<< ", "
					    << e.scheduled->get_job_id()  << ", "
					  	<< e.latest_start_time()
						<< std::endl;
				} 
		}

			if (detect_deadline_miss == true) {

				PoP << dm_from   << ", "
					<< dm_to << ", "
					<< dm_task << ", "					
					<< dm_job  << ", "
					<< dm_ls; 
					//<< std::endl;	 
			}
		PoP<< std::endl;
	}	

	//find the CE job set

	std::vector<std::size_t> sched_jobs_removed;
	sched_jobs_removed = space.get_reduced_job();

	//generate the reduced CSV file


	auto rj = std::ostringstream();

	if (want_first_run_counterexample && !want_backtracks) {

		rj << "Task ID, Job ID, r min, r max, cos min, cos max, deadline, Priority, deadline miss" << std::endl;
		
		for (const auto& j : problem.jobs) {
		    if (std::count(sched_jobs_removed.begin(), sched_jobs_removed.end(), space.index_of(j))) {
		    }

		   else {
				Interval<Time> finish = space.get_finish_times(j);

			//std::cout <<"what is the finish time looks  like" <<  finish.until() << '\n';
				//std::cout <<"what is the finish time looks  like" <<  finish.starting_at()<< '\n';

					//std::cout <<"what is the deadline looks  like" <<  j.exceeds_deadline(finish.until()) << '\n';
				//j.exceeds_deadline(finish.until());
				rj << j.get_task_id() << ", "
				    << j.get_job_id() << ", "
				    <<  j.earliest_arrival() << ", "
				    <<  j.latest_arrival() << ", "
				    <<  j.least_cost() << ", "
				    <<  j.maximal_cost() << ", "
				    <<  j.get_deadline() << ", "
				    << j.get_priority() << ", ";
					if ( j.exceeds_deadline(finish.until()) &&  finish.starting_at()!= 0) {
						rj <<  j.exceeds_deadline(finish.until());
						detect_deadline_miss_first_run = true;
					}
					else{
						 rj << 0;
					}

				    rj << std::endl;

   			}    
		}
	}

	
	// Extract the analysis results
	auto graph = std::ostringstream();
	#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
		if (want_dot_graph)
			graph << space;
	#endif

	auto rta = std::ostringstream();

	if (want_rta_file  && !want_backtracks) {
		rta << "Task ID, Job ID, BCCT, WCCT, BCRT, WCRT" << std::endl;
		for (const auto& j : problem.jobs) {
			Interval<Time> finish = space.get_finish_times(j);
			rta << j.get_task_id() << ", "
			    << j.get_job_id() << ", "
			    << finish.from() << ", "
			    << finish.until() << ", "
			    << std::max<long long>(0,
			                           (finish.from() - j.earliest_arrival()))
			    << ", "
			    << (finish.until() - j.earliest_arrival())
			    << std::endl;
		}
	}

	
	// if (!want_backtracks) {
		return {		
			space.is_schedulable(),
			space.was_timed_out(),
			space.number_of_states(),
			space.number_of_edges(),
			space.max_exploration_front_width(),
			problem.jobs.size(),
			space.get_cpu_time(),
			graph.str(),
			rta.str(),
			rj.str(),
			PoP.str()			
		};		
	// }
#else			
		auto rta = std::ostringstream();
			auto rj = std::ostringstream();
				auto PoP = std::ostringstream();
	auto graph = std::ostringstream();
	auto ce = std::ostringstream();
		// if (want_backtracks && !want_rta_file  && !want_first_run_counterexample && !want_second_run_counterexample) 
	if (want_backtracks) {

			ce << "Task ID, Job ID, earliest start time, latest start time" << std::endl;
			for (auto b : space.get_path()) {
				ce << b.scheduled->get_task_id() << ", "
				    << b.scheduled->get_job_id() << ", "
				    << b.earliest_start_time_counterexample() << ", "
				    << b.latest_start_time_counterexample()
				    << std::endl;
			}
		}		
	// else{
			return { 
				space.is_schedulable(),
				space.was_timed_out(),
				space.number_of_states(),
				space.number_of_edges(),
				space.max_exploration_front_width(),
				problem.backtracks.size(),
				space.get_cpu_time(),	
				graph.str(),
			rta.str(),
			rj.str(),
			PoP.str(),				
			ce.str()
		};	
#endif
	// }
}

static Analysis_result process_stream(
	

	std::istream &in,
	std::istream &dag_in,
	std::istream &aborts_in,
	std::istream &backtrack_in)
{	

 #ifndef CONFIG_COUNTER_EXAMPLE	

	// if (want_multiprocessor && want_dense && !want_backtracks )
	// 	return analyze<dense_t, NP::Global::State_space<dense_t>>(in, dag_in, aborts_in,backtrack_in);
	// else if (want_multiprocessor && !want_dense && !want_backtracks )
	// 	return analyze<dtime_t, NP::Global::State_space<dtime_t>>(in, dag_in, aborts_in,backtrack_in);
	 if (want_dense && want_prm_iip && !want_backtracks )
		return analyze<dense_t, NP::Uniproc::State_space<dense_t, NP::Uniproc::Precatious_RM_IIP<dense_t>>>(in, dag_in, aborts_in,backtrack_in);
	else if (want_dense && want_cw_iip && !want_backtracks )
		return analyze<dense_t, NP::Uniproc::State_space<dense_t, NP::Uniproc::Critical_window_IIP<dense_t>>>(in, dag_in, aborts_in,backtrack_in);
	else if (want_dense && !want_prm_iip && !want_backtracks )
		return analyze<dense_t, NP::Uniproc::State_space<dense_t>>(in, dag_in, aborts_in,backtrack_in);
	else if (!want_dense && want_prm_iip && !want_backtracks )
		return analyze<dtime_t, NP::Uniproc::State_space<dtime_t, NP::Uniproc::Precatious_RM_IIP<dtime_t>>>(in, dag_in, aborts_in,backtrack_in);
	else if (!want_dense && want_cw_iip && !want_backtracks )
		return analyze<dtime_t, NP::Uniproc::State_space<dtime_t, NP::Uniproc::Critical_window_IIP<dtime_t>>>(in, dag_in, aborts_in,backtrack_in);
	else
		return analyze<dtime_t, NP::Uniproc::State_space<dtime_t>>(in, dag_in, aborts_in,backtrack_in);
#else
	 //if (want_backtracks)
		return analyze<dtime_t, NP::Uniproc::Graph<dtime_t>>(in, dag_in, aborts_in,backtrack_in);
#endif

}



static void process_file(const std::string& fname)
{
	try {
		Analysis_result result;

		auto empty_dag_stream = std::istringstream("\n");
		auto empty_aborts_stream = std::istringstream("\n");
// add an empty stream for backward tracking file
		auto empty_backtrack_stream = std::istringstream("\n");		

		auto dag_stream = std::ifstream();
		auto aborts_stream = std::ifstream();
		auto backtrack_stream = std::ifstream();


		if (want_precedence)
			dag_stream.open(precedence_file);

		if (want_aborts)
			aborts_stream.open(aborts_file);

		if(want_backtracks)
			backtrack_stream.open(backtrack_file);	

		std::istream &dag_in = want_precedence ?
			static_cast<std::istream&>(dag_stream) :
			static_cast<std::istream&>(empty_dag_stream);

		std::istream &aborts_in = want_aborts ?
			static_cast<std::istream&>(aborts_stream) :
			static_cast<std::istream&>(empty_aborts_stream);

		// std::cout <<"my third run result"<< '\n';
		// 	std::cout <<want_backtracks<< '\n';

// add stream for backward tracking file
		std::istream &backtrack_in = want_backtracks ?
		static_cast<std::istream&>(backtrack_stream) :
		static_cast<std::istream&>(empty_backtrack_stream);	

		if (fname == "-")
			result = process_stream(std::cin, dag_in, aborts_in,backtrack_in);
		else {
			auto in = std::ifstream(fname, std::ios::in);
			result = process_stream(in, dag_in, aborts_in,backtrack_in);
#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
			if (want_dot_graph) {
				std::string dot_name = fname;
				auto p = dot_name.find(".csv");
				if (p != std::string::npos) {
					dot_name.replace(p, std::string::npos, ".dot");
					auto out  = std::ofstream(dot_name,  std::ios::out);
					out << result.graph;
					out.close();
				}
			}
#endif
			if (want_rta_file) {
				std::string rta_name = fname;
				auto p = rta_name.find(".csv");
				if (p != std::string::npos) {
					rta_name.replace(p, std::string::npos, ".rta.csv");
					auto out  = std::ofstream(rta_name,  std::ios::out);
					out << result.response_times_csv;
					out.close();
				}
			}

			if (want_backtracks) {

				std::string ce_name = fname;
				auto p = ce_name.find(".csv");
				if (p != std::string::npos) {
					ce_name.replace(p, std::string::npos, ".ce.csv");
					auto out  = std::ofstream(ce_name,  std::ios::out);
					out << result.counter_example_csv;
					out.close();
				}
			}
		
			//std::cout <<"do we meet deadline?" <<  detect_deadline_miss_first_run << '\n';

			if (want_first_run_counterexample && detect_deadline_miss_first_run) {
				std::string reduced_job_file = fname;
				auto p = reduced_job_file.find(".csv");
				if (p != std::string::npos) {
					reduced_job_file.replace(p, std::string::npos, ".rj.csv");
					auto out  = std::ofstream(reduced_job_file,  std::ios::out);
					out << result.reduced_job_csv;
					out.close();
				}
			}


			if (want_second_run_counterexample) {
				std::string want_second_run_counterexample = fname;
				auto p = want_second_run_counterexample.find(".csv");
				if (p != std::string::npos) {
					want_second_run_counterexample.replace(p, std::string::npos, ".pop.csv");
					auto out  = std::ofstream(want_second_run_counterexample,  std::ios::out);
					out << result.back_track_csv;
					out.close();
				}
			}


		}

		struct rusage u;
		long mem_used = 0;
		if (getrusage(RUSAGE_SELF, &u) == 0)
			mem_used = u.ru_maxrss;

		std::cout << fname;

		if (max_depth && max_depth < result.number_of_jobs)
			// mark result as invalid due to debug abort
			std::cout << ",  X";
		else
			std::cout << ",  " << (int) result.schedulable;

		std::cout << ",  " << result.number_of_jobs
		          << ",  " << result.number_of_states
		          << ",  " << result.number_of_edges
		          << ",  " << result.max_width
		          << ",  " << std::fixed << result.cpu_time
		          << ",  " << ((double) mem_used) / (1024.0)
		          << ",  " << (int) result.timeout
		          << ",  " << num_processors
		          << std::endl;
	} catch (std::ios_base::failure& ex) {
		std::cerr << fname;
		if (want_precedence)
			std::cerr << " + " << precedence_file;
		std::cerr <<  ": parse error" << std::endl;
		exit(1);
	} catch (NP::InvalidJobReference& ex) {
		std::cerr << precedence_file << ": bad job reference: job "
		          << ex.ref.job << " of task " << ex.ref.task
			      << " is not part of the job set given in "
			      << fname
			      << std::endl;
		exit(3);
	} catch (NP::InvalidAbortParameter& ex) {
		std::cerr << aborts_file << ": invalid abort parameter: job "
		          << ex.ref.job << " of task " << ex.ref.task
			      << " has an impossible abort time (abort before release)"
			      << std::endl;
		exit(4);
	} catch (std::exception& ex) {
		std::cerr << fname << ": '" << ex.what() << "'" << std::endl;
		exit(1);
	}
}

static void print_header(){
	std::cout << "# file name"
	          << ", schedulable?"
	          << ", #jobs"
	          << ", #states"
	          << ", #edges"
	          << ", max width"
	          << ", CPU time"
	          << ", memory"
	          << ", timeout"
	          << ", #CPUs"
	          << std::endl;
}



int main(int argc, char** argv)
{
	auto parser = optparse::OptionParser();

	parser.description("Exact NP Schedulability Tester");
	parser.usage("usage: %prog [OPTIONS]... [JOB SET FILES]...");

	parser.add_option("-t", "--time").dest("time_model")
	      .metavar("TIME-MODEL")
	      .choices({"dense", "discrete"}).set_default("discrete")
	      .help("choose 'discrete' or 'dense' time (default: discrete)");

	parser.add_option("-l", "--time-limit").dest("timeout")
	      .help("maximum CPU time allowed (in seconds, zero means no limit)")
	      .set_default("0");

	parser.add_option("-d", "--depth-limit").dest("depth")
	      .help("abort graph exploration after reaching given depth (>= 2)")
	      .set_default("0");

	parser.add_option("-n", "--naive").dest("naive").set_default("0")
	      .action("store_const").set_const("1")
	      .help("use the naive exploration method (default: merging)");

	parser.add_option("-i", "--iip").dest("iip")
	      .choices({"none", "P-RM", "CW"}).set_default("none")
	      .help("the IIP to use (default: none)");

	parser.add_option("-p", "--precedence").dest("precedence_file")
	      .help("name of the file that contains the job set's precedence DAG")
	      .set_default("");

	parser.add_option("-a", "--abort-actions").dest("abort_file")
	      .help("name of the file that contains the job set's abort actions")
	      .set_default("");

	parser.add_option("-m", "--multiprocessor").dest("num_processors")
	      .help("set the number of processors of the platform")
	      .set_default("1");

	parser.add_option("--threads").dest("num_threads")
	      .help("set the number of worker threads (parallel analysis)")
	      .set_default("0");

	parser.add_option("--header").dest("print_header")
	      .help("print a column header")
	      .action("store_const").set_const("1")
	      .set_default("0");

	parser.add_option("-g", "--save-graph").dest("dot").set_default("0")
	      .action("store_const").set_const("1")
	      .help("store the state graph in Graphviz dot format (default: off)");

	parser.add_option("-r", "--save-response-times").dest("rta").set_default("0")
	      .action("store_const").set_const("1")
	      .help("store the best- and worst-case response times (default: off)");

	parser.add_option("-c", "--continue-after-deadline-miss")
	      .dest("go_on_after_dl").set_default("0")
	      .action("store_const").set_const("1")
	      .help("do not abort the analysis on the first deadline miss "
	            "(default: off)");

	parser.add_option("--counter-example").dest("is_counter_example")
	      .choices({"first-run", "second-run"}).set_default("none")
	      .help("the counterexample to use (default: none)");

	parser.add_option("-b", "--back-tracks").dest("back_file")
	      .help("name of the file that contains the job set's needs backtracks")
	      .set_default("");


	auto options = parser.parse_args(argc, argv);

	const std::string& time_model = options.get("time_model");
	want_dense = time_model == "dense";

	const std::string& iip = options.get("iip");
	want_prm_iip = iip == "P-RM";
	want_cw_iip = iip == "CW";



	const std::string& is_counter_example = options.get("is_counter_example");
	want_first_run_counterexample= is_counter_example == "first-run";
	want_second_run_counterexample = is_counter_example == "second-run";


	want_naive = options.get("naive");

	//is_second_run = options.get("is_second_run");

	timeout = options.get("timeout");

	max_depth = options.get("depth");
	if (options.is_set_by_user("depth")) {
		if (max_depth <= 1) {
			std::cerr << "Error: invalid depth argument\n" << std::endl;
			return 1;
		}
		max_depth -= 1;
	}

	want_precedence = options.is_set_by_user("precedence_file");
	if (want_precedence && parser.args().size() > 1) {
		std::cerr << "[!!] Warning: multiple job sets "
		          << "with a single precedence DAG specified."
		          << std::endl;
	}
	precedence_file = (const std::string&) options.get("precedence_file");

	want_aborts = options.is_set_by_user("abort_file");
	if (want_aborts && parser.args().size() > 1) {
		std::cerr << "[!!] Warning: multiple job sets "
		          << "with a single abort action list specified."
		          << std::endl;
	}
	aborts_file = (const std::string&) options.get("abort_file");


	want_backtracks = options.is_set_by_user("back_file");
	if (want_backtracks && parser.args().size() > 1) {
		std::cerr << "[!!] Warning: multiple job sets "
		          << "with a single back track list specified."
		          << std::endl;
	}
	backtrack_file = (const std::string&) options.get("back_file");


	want_multiprocessor = options.is_set_by_user("num_processors");
	num_processors = options.get("num_processors");
	if (!num_processors || num_processors > MAX_PROCESSORS) {
		std::cerr << "Error: invalid number of processors\n" << std::endl;
		return 1;
	}

	want_rta_file = options.get("rta");

	continue_after_dl_miss = options.get("go_on_after_dl");

#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
	want_dot_graph = options.get("dot");
#else
	if (options.is_set_by_user("dot")) {
		std::cerr << "Error: graph collection support must be enabled "
		          << "during compilation (CONFIG_COLLECT_SCHEDULE_GRAPH "
		          << "is not set)." << std::endl;
		return 2;
	}
#endif

#ifdef CONFIG_PARALLEL
	num_worker_threads = options.get("num_threads");
#else
	if (options.is_set_by_user("num_threads")) {
		std::cerr << "Error: parallel analysis must be enabled "
		          << "during compilation (CONFIG_PARALLEL "
		          << "is not set)." << std::endl;
		return 3;
	}
#endif

	if (options.get("print_header"))
		print_header();


	for (auto f : parser.args())
		process_file(f);


	if (parser.args().empty())
		process_file("-");
	return 0;
}
