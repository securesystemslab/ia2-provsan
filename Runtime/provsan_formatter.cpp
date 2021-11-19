#include "provsan_formatter.h"

#include "llvm/ADT/Optional.h"
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace __provsan {

#define ATTEMPTS 128
#define ENTROPY 16

// Generates a unique filename to ensure we do not overlap or overwrite
// previously found faults.
llvm::Optional<std::string> makeUniqueFilename(std::string path,
                                               std::string base_name,
                                               std::string extension) {
  std::mt19937_64 mt_rand(std::random_device{}());

  // Loop for number of attempts just in case level of entropy is to low
  for (uint8_t attempt = 0; attempt != ATTEMPTS; ++attempt) {
    std::stringstream unique;
    unique << path << "/" << base_name << "-" << getpid() << "-"
           << std::setfill('0') << std::setw(ENTROPY) << std::hex << mt_rand()
           << "." << extension;
    struct stat info;
    // If file does not already exist, create and return ofstream
    if (stat(unique.str().c_str(), &info) == -1) {
      return unique.str();
    }
  }

  // Failed to make unique name
  REPORT("Failed to make uniqueFileID.\n");
  return llvm::None;
}

// Optionally returns a ofstream if it can successfully create a unique
// filename.
llvm::Optional<std::ofstream> makeUniqueStream(std::string path,
                                               std::string base_name,
                                               std::string extension) {
  auto Filename = makeUniqueFilename(path, base_name, extension);
  if (!Filename)
    return llvm::None;

  std::ofstream OS;
  OS.open(Filename.getValue());
  if (OS)
    return OS;

  REPORT("Failed to create uniqueOStream.\n");
  return llvm::None;
}

bool is_directory(std::string directory) {
  struct stat info;
  if (stat(directory.c_str(), &info) != 0)
    return false;
  else if (info.st_mode & S_IFDIR)
    return true;
  else
    return false;
}

// Function for handwriting the JSON output we want (to remove dependency on
// llvm/Support).
void writeJSON(std::ofstream &OS, std::unordered_set<AllocSite> &faultSet) {
  if (faultSet.size() <= 0)
    return;

  OS << "[\n";
  int64_t items_remaining = faultSet.size();
  for (auto fault : faultSet) {
    --items_remaining;
    OS << "{ \"id\": " << fault.id() << ", \"pkey\": " << fault.getPkey()
       << ", \"bbName\": \"" << fault.getBBName() << "\", \"funcName\": \""
       << fault.getFuncName() << "\""
       << ", \"isRealloc\": " << (fault.isReAlloc() ? "true" : "false") << " }"
       << (items_remaining ? "," : "") << "\n";
  }
  OS << "]\n";
}

// Writes output of the faultSet to a uniquely generated output file to ensure
// we do not overwrite previously discovered faulting values.
bool writeUniqueFile(std::unordered_set<AllocSite> &faultSet) {
  // Currently all results are stored by default in the folder TestResults.
  // Ensure this folder exists, or create one if it does not.
  std::string TestDirectory = "TestResults";
  if (!is_directory(TestDirectory)) {
    if (mkdir(TestDirectory.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
      REPORT("Failed to create TestResults directory.\n");
      return false;
    }
  }

  auto uniqueOS = makeUniqueStream(TestDirectory, "faulting-allocs", "json");
  if (!uniqueOS)
    return false;
  std::ofstream &OS = uniqueOS.getValue();
  writeJSON(OS, faultSet);
  OS.flush();

#ifdef MPK_STATS
  if (AllocSiteCount != 0) {
    auto uniqueSOS = makeUniqueStream(TestDirectory, "runtime-stats", "stat");
    if (!uniqueSOS)
      return false;
    std::ofstream &SOS = uniqueSOS.getValue();
    SOS << "Number of Times allocHook Called: " << allocHookCalls << "\n"
        << "Number of Times reallocHook Called: " << reallocHookCalls << "\n"
        << "Number of Times deallocHook Called: " << deallocHookCalls << "\n";
    uint64_t AllocSitesFound = 0;
    for (uint64_t i = 0; i < AllocSiteCount; i++) {
      if (AllocSiteUseCounter[i] > 0) {
        SOS << "AllocSite(" << i << ") faults: " << AllocSiteUseCounter[i]
            << "\n";
        ++AllocSitesFound;
      }
    }
    SOS << "Number of Unique AllocSites Found: " << AllocSitesFound << "\n";
    SOS.flush();
  }
#endif
  return true;
}

// Flush Allocs is to be called on program exit to flush all faulting
// allocations to disk/file.
void flush_allocs() {
  auto handler = AllocSiteHandler::getOrInit();
  auto fault_set = handler->faultingAllocs();
  if (fault_set.empty()) {
    REPORT("INFO : No faulting instructions to export, returning.\n");
    return;
  }

  REPORT("INFO : Serializing faulting allocations to disk.\n");

  // Simple method that requires either handling multiple files or a script for
  // combining them later.
  if (!writeUniqueFile(fault_set))
    REPORT("ERROR : Unable to successfully write unique files for "
           "given program run.\n");

  REPORT("INFO : Serialization complete.\n");
}

} // namespace __provsan

extern "C" {

static void __attribute__((constructor)) register_flush_allocs() {
  std::atexit(__provsan::flush_allocs);
}
}
