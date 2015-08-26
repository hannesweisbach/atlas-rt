#include <fstream>
#include <vector>
#include <iostream>
#include <string>
#include <stdexcept>
#include <chrono>
#include <iomanip>

#include <stdio.h>

#include "predictor.h"

static double extract(std::string &line, size_t &offset) {
  double d;
  int chars;

  int ret = sscanf(line.data() + offset, "%la%n", &d, &chars);
  if (ret <= 0)
    throw std::runtime_error(std::string("Conversion failed ") +
                             (line.data() + offset));

  offset += static_cast<size_t>(chars);

  return d;
}

static std::vector<double> extract_metrics(std::string &line, size_t &offset) {
  std::vector<double> metrics;

  for (; offset < line.size();) {
    try {
      metrics.push_back(extract(line, offset));
    } catch (const std::runtime_error &) {
      break;
    }
  }

  return metrics;
}

int main(int argc, char *argv[]) {
  using namespace std::chrono;
  using namespace std::literals::chrono_literals;
  atlas::estimator estimator;

  if (argc < 2)
    throw std::runtime_error("No filename given.");

  std::ifstream file;
  file.open(argv[1]);

  std::ofstream parser_check("./parser_check");

  file.setf(std::ios::fixed | std::ios::scientific);
  file >> std::hex;

  parser_check << std::hex << std::setprecision(13);
  parser_check.setf(std::ios::fixed | std::ios::scientific);

  std::cout << "File " << argv[1] << " opened." << std::endl;

  uint64_t records = 0;

  while (true) {
    char action;
    uint64_t type;
    file >> action >> type;
    if (!file)
      break;
    std::string line;
    std::getline(file, line);
    parser_check << action << " 0x" << type << " ";
    size_t offset = 0;
    if (action == 'p') {
      const double prediction = extract(line, offset);
      const double reservation = extract(line, offset);
      auto metrics = extract_metrics(line, offset);
      {
        parser_check << prediction << " " << reservation;
        for (const auto &metric : metrics)
          parser_check << " " << metric;
        parser_check << std::endl;
      }
      {
        auto reference_duration =
            duration_cast<nanoseconds>(duration<double>{reservation});
        /* job id 0, because each job-type processes in FIFO order */
        auto just_prediction =
            estimator.predict(type, 0, metrics.data(), metrics.size());
        if ((just_prediction < reference_duration - 1ns) ||
            (just_prediction > reference_duration + 1ns)) {
          std::cerr << "Exepected prediction of " << reference_duration.count()
                    << "ns (" << reservation << ")" << std::endl;
          std::cerr << "Got prediction of " << just_prediction.count() << "ns"
                    << std::endl;
        }
      }
    } else if (action == 't') {
      double exectime = extract(line, offset);
      auto metrics = extract_metrics(line, offset);
      {
        parser_check << exectime;
        for (const auto &metric : metrics)
          parser_check << " " << metric;
        parser_check << std::endl;
      }
      /* + 0.5ns to correct for rounding errors */
      estimator.train(type, 0, duration_cast<nanoseconds>(
                                   duration<double>{exectime} + 0.5ns));
    } else {
      throw std::runtime_error("Unkown action " + std::to_string(action));
    }
    ++records;
  }

  std::cout << records << " records processed." << std::endl;
}
