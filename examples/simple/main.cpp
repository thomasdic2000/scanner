#include "scanner/api/evaluator.h"
#include "scanner/kernels/args.pb.h"
#include "scanner/api/commands.h"

#include <grpc/grpc_posix.h>

int main(int argc, char** argv) {
  grpc_use_signal(-1);

  std::string db_path = "/tmp/new_scanner_db_2";
  std::unique_ptr<storehouse::StorageConfig> sc(
      storehouse::StorageConfig::make_posix_config());

  // Create database
  scanner::create_database(sc.get(), db_path);
  // Ingest video
  scanner::ingest_videos(sc.get(), db_path, {"mean"},
                         {"/bigdata/wcrichto/videos/meanGirls_short.mp4"});
  // Initialize master and one worker
  scanner::DatabaseParameters db_params;
  db_params.storage_config = sc.get();
  db_params.memory_pool_config.mutable_cpu()->set_use_pool(false);
  db_params.db_path = db_path;
  scanner::ServerState master_state = scanner::start_master(db_params, false);
  scanner::proto::WorkerParameters worker_params =
      scanner::default_worker_params();
  scanner::ServerState worker_state =
      scanner::start_worker(db_params, worker_params, "localhost:5001", false);

  // Construct job parameters
  scanner::JobParameters params;
  params.master_address = "localhost:5001";
  params.job_name = "test_job";
  params.kernel_instances_per_node = 1;

  // Specify job tasks
  scanner::Task task;
  task.output_table_name = "blurred_mean";
  scanner::TableSample sample;
  sample.table_name = "mean";
  sample.column_names = {"frame", "frame_info"};
  for (int i = 0; i < 695; i += 2) {
    sample.rows.push_back(i);
  }
  task.samples.push_back(sample);
  params.task_set.tasks.push_back(task);

  scanner::Evaluator *input =
      scanner::make_input_evaluator({"frame", "frame_info"});

  // scanner::proto::BlurArgs args;
  // args.set_kernel_size(3);
  // args.set_sigma(0.5);
  // size_t arg_size = args.ByteSize();
  // char* arg = new char[arg_size];
  // args.SerializeToArray(arg, arg_size);
  // scanner::Evaluator *blur = new scanner::Evaluator(
  //     "Blur", {scanner::EvalInput(input, {"frame", "frame_info"})},
  //     scanner::DeviceType::CPU, arg, arg_size);
  scanner::Evaluator *histogram = new scanner::Evaluator(
      "Histogram", {scanner::EvalInput(input, {"frame", "frame_info"})},
      scanner::DeviceType::CPU);

  scanner::Evaluator *output = scanner::make_output_evaluator(
      {scanner::EvalInput(histogram, {"histogram"})});

  // Launch job
  params.task_set.output_evaluator = output;
  scanner::new_job(params);
}