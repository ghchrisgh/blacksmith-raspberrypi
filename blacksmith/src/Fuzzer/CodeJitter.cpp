#include "Fuzzer/CodeJitter.hpp"

#ifdef ENABLE_JITTING
// ---------------------------------------------------------------------------
// aarch64-Port. Stil/Encodings:
//   dc civac, xN   -> 0xD50B7E20 | Rt
//   mrs xN, pmccntr_el0 -> 0xD53B9D00 | Rt
//   dsb ish        -> 0xD5033B9F
//   isb            -> 0xD5033FDF
// Register-Mapping:
//   x9  = address scratch        (was rax)
//   x10 = load target, discarded  (was rbx/rcx)
//   x11 = Timestamp before    (was ebx)
//   x12 = Timestamp after     (was eax after rdtscp)
//   x13 = activation counter, counts down (was rsi)
//   x14 = Sync activations = return value (was edx)
// ---------------------------------------------------------------------------
namespace a64 = asmjit::a64;

static constexpr uint32_t DC_CIVAC_X9     = 0xD50B7E29u; // dc civac, x9
static constexpr uint32_t DSB_ISH         = 0xD5033B9Fu; // dsb ish      (mfence)
static constexpr uint32_t ISB_SY          = 0xD5033FDFu; // isb          (lfence)
static constexpr uint32_t MRS_PMCCNTR_X11 = 0xD53B9D0Bu; // mrs x11, pmccntr_el0
static constexpr uint32_t MRS_PMCCNTR_X12 = 0xD53B9D0Cu; // mrs x12, pmccntr_el0

// Sync-Refresh-Schwelle in pmccntr-Zyklen. Misst NUM_TIMED_ACCESSES Zugriffe.
static constexpr uint64_t SYNC_REF_THRESH = 1000;
#endif

CodeJitter::CodeJitter()
    : pattern_sync_each_ref(false),
      flushing_strategy(FLUSHING_STRATEGY::EARLIEST_POSSIBLE),
      fencing_strategy(FENCING_STRATEGY::LATEST_POSSIBLE),
      total_activations(5000000),
      num_aggs_for_sync(2) {
#ifdef ENABLE_JITTING
  logger = new asmjit::StringLogger;
#endif
}

CodeJitter::~CodeJitter() {
  cleanup();
}

void CodeJitter::cleanup() {
#ifdef ENABLE_JITTING
  if (fn!=nullptr) {
    runtime.release(fn);
    fn = nullptr;
  }
  if (logger!=nullptr) {
    delete logger;
    logger = nullptr;
  }
#endif
}

int CodeJitter::hammer_pattern(FuzzingParameterSet &fuzzing_parameters, bool verbose) {
  if (fn==nullptr) {
    Logger::log_error("Skipping hammering pattern as pattern could not be created successfully.");
    return -1;
  }
  if (verbose) Logger::log_info("Hammering the last generated pattern.");
  int total_sync_acts = fn();

  if (verbose) {
    Logger::log_info("Synchronization stats:");
    Logger::log_data(format_string("Total sync acts: %d", total_sync_acts));

    const auto total_acts_pattern = fuzzing_parameters.get_total_acts_pattern();
    auto pattern_rounds = fuzzing_parameters.get_hammering_total_num_activations()/total_acts_pattern;
    auto acts_per_pattern_round = pattern_sync_each_ref
                                  ? (total_acts_pattern/fuzzing_parameters.get_num_activations_per_t_refi())
                                  : 1;
    auto num_synced_refs = pattern_rounds*acts_per_pattern_round;
    Logger::log_data(format_string("Number of pattern reps while hammering: %d", pattern_rounds));
    Logger::log_data(format_string("Number of total synced REFs (est.): %d", num_synced_refs));
    Logger::log_data(format_string("Avg. number of acts per sync: %d", total_sync_acts/num_synced_refs));
  }

  return total_sync_acts;
}

void CodeJitter::jit_strict(int num_acts_per_trefi,
                            FLUSHING_STRATEGY flushing,
                            FENCING_STRATEGY fencing,
                            const std::vector<volatile char *> &aggressor_pairs,
                            bool sync_each_ref,
                            int num_aggressors_for_sync,
                            int total_num_activations) {

  // this is used by hammer_pattern but only for some stats calculations
  this->pattern_sync_each_ref = sync_each_ref;
  this->flushing_strategy = flushing;
  this->fencing_strategy = fencing;
  this->total_activations = total_num_activations;
  this->num_aggs_for_sync = num_aggressors_for_sync;

  const auto NUM_TIMED_ACCESSES = num_aggressors_for_sync;

  if (static_cast<size_t>(NUM_TIMED_ACCESSES) > aggressor_pairs.size()) {
    Logger::log_error(format_string("NUM_TIMED_ACCESSES (%d) is larger than #aggressor_pairs (%zu).",
        NUM_TIMED_ACCESSES,
        aggressor_pairs.size()));
    return;
  }

  if (fn!=nullptr) {
    Logger::log_error(
        "Function pointer is not NULL, cannot continue jitting code without leaking memory. Did you forget to call cleanup() before?");
    exit(1);
  }

#ifdef ENABLE_JITTING
  asmjit::CodeHolder code;
  code.init(runtime.environment());
  code.set_logger(logger);
  a64::Assembler a(&code);

  // Register-Aliase (siehe Mapping oben)
  const auto ADDR = a64::x9;
  const auto VAL  = a64::x10;
  const auto CNT  = a64::x13;
  const auto SYNC = a64::x14;

  asmjit::Label while1_begin = a.new_label();
  asmjit::Label while1_end = a.new_label();
  asmjit::Label for_begin = a.new_label();
  asmjit::Label for_end = a.new_label();

  // ==== here start's the actual program ====================================================
  // The following JIT instructions are based on hammer_sync in blacksmith.cpp

 // ------- part 1: synchronize with the beginning of an interval ---------------------------

  // warmup
  for (int idx = 0; idx < NUM_TIMED_ACCESSES; idx++) {
    a.mov(ADDR, (uint64_t) aggressor_pairs[idx]);
    a.ldr(VAL, a64::ptr(ADDR));
  }

  a.bind(while1_begin);
  // dc civac auf die Sync-Adressen (clflushopt)
  for (int idx = 0; idx < NUM_TIMED_ACCESSES; idx++) {
    a.mov(ADDR, (uint64_t) aggressor_pairs[idx]);
    a.embed_uint32(DC_CIVAC_X9);
  }
  a.embed_uint32(DSB_ISH);            // mfence

  // before = pmccntr  (rdtscp; lfence)
  a.embed_uint32(MRS_PMCCNTR_X11);    // T0 -> x11
  a.embed_uint32(ISB_SY);

  // use first NUM_TIMED_ACCESSES addresses for sync
  for (int idx = 0; idx < NUM_TIMED_ACCESSES; idx++) {
    a.mov(ADDR, (uint64_t) aggressor_pairs[idx]);
    a.ldr(VAL, a64::ptr(ADDR));
  }

  // after = pmccntr  (lfence; rdtscp).
  a.embed_uint32(ISB_SY);
  a.embed_uint32(MRS_PMCCNTR_X12);    // T1 -> x12

  // if ((after - before) > THRESH) break;
  a.sub(a64::x12, a64::x12, a64::x11);
  a.cmp(a64::x12, (uint64_t) SYNC_REF_THRESH);
  a.b_gt(while1_end);                 // jg
  a.b_al(while1_begin);               // jmp (unbedingt: B always)
  a.bind(while1_end);

  // ------- part 2: perform hammering ---------------------------------------------------------------------------------

 // initialize variables
  a.mov(CNT, (uint64_t) total_num_activations);  // Counter counting down (was rsi)
  a.mov(SYNC, (uint64_t) 0);                      // Sync-activation Counter (was edx)

  a.bind(for_begin);
  a.cmp(CNT, (uint64_t) 0);
  a.b_le(for_end);

// a map to keep track of aggressors that have been accessed before and need a fence before their next access
  std::unordered_map<uint64_t, bool> accessed_before;
  size_t cnt_total_activations = 0;

  // hammer each aggressor once
  for (int i = NUM_TIMED_ACCESSES; i < static_cast<int>(aggressor_pairs.size()) - NUM_TIMED_ACCESSES; i++) {
    auto cur_addr = (uint64_t) aggressor_pairs[i];

    if (accessed_before[cur_addr]) {
      // flush
      if (flushing==FLUSHING_STRATEGY::LATEST_POSSIBLE) {
        a.mov(ADDR, cur_addr);
        a.embed_uint32(DC_CIVAC_X9);
        accessed_before[cur_addr] = false;
      }
      // fence
      if (fencing==FENCING_STRATEGY::LATEST_POSSIBLE) {
        a.embed_uint32(DSB_ISH);      // mfence
        accessed_before[cur_addr] = false;
      }
    }

    // hammer
    a.mov(ADDR, cur_addr);
    a.ldr(VAL, a64::ptr(ADDR));
    accessed_before[cur_addr] = true;
    a.sub(CNT, CNT, (uint64_t) 1);    // dec rsi
    cnt_total_activations++;

    // flush
    if (flushing==FLUSHING_STRATEGY::EARLIEST_POSSIBLE) {
      a.mov(ADDR, cur_addr);
      a.embed_uint32(DC_CIVAC_X9);
    }
    if (sync_each_ref
        && ((cnt_total_activations%num_acts_per_trefi)==0)) {
      std::vector<volatile char *> aggs(aggressor_pairs.begin() + i,
          std::min(aggressor_pairs.begin() + i + NUM_TIMED_ACCESSES, aggressor_pairs.end()));
      sync_ref(aggs, a);
    }
  }

  // fences -> ensure that aggressors are not interleaved, i.e., we access aggressors always in same order
  a.embed_uint32(DSB_ISH);            // mfence

  // ------- part 3: synchronize with the end  -----------------------------------------------------------------------
  std::vector<volatile char *> last_aggs(aggressor_pairs.end() - NUM_TIMED_ACCESSES, aggressor_pairs.end());
  sync_ref(last_aggs, a);

  a.b_al(for_begin);                  // jmp ( B always)
  a.bind(for_end);

  // now move our counter for no. of activations in the end of interval sync. to the 1st output register, return: number Sync activations -> x0/w0 (was: mov eax, edx)
  a.mov(a64::x0, SYNC);
  a.ret(a64::x30);                    // , sonst Segfault

// add the generated code to the runtime.
  asmjit::Error err = runtime.add(&fn, &code);
  if (err != asmjit::Error::kOk) throw std::runtime_error("[-] Error occurred while jitting code. Aborting execution!");

  // printf("[DEBUG] asmjit logger content:\n%s\n", logger->corrupted_data());
#endif
#ifndef ENABLE_JITTING
  Logger::log_error("Cannot do code jitting. Set option ENABLE_JITTING to ON in CMakeLists.txt and do a rebuild.");
#endif
}

#ifdef ENABLE_JITTING
void CodeJitter::sync_ref(const std::vector<volatile char *> &aggressor_pairs, a64::Assembler &assembler) {
  asmjit::Label wbegin = assembler.new_label();
  asmjit::Label wend = assembler.new_label();

  assembler.bind(wbegin);

  // mfence; lfence
  assembler.embed_uint32(DSB_ISH);
  assembler.embed_uint32(ISB_SY);

  // before = pmccntr -> x11
  assembler.embed_uint32(MRS_PMCCNTR_X11);
  assembler.embed_uint32(ISB_SY);

  for (auto agg : aggressor_pairs) {
    // flush
    assembler.mov(a64::x9, (uint64_t) agg);
    assembler.embed_uint32(DC_CIVAC_X9);

    // access
    assembler.mov(a64::x9, (uint64_t) agg);
    assembler.ldr(a64::x10, a64::ptr(a64::x9));

    // Zaehler der Sync-Aktivierungen erhoehen (war inc edx)
    assembler.add(a64::x14, a64::x14, (uint64_t) 1);
  }

  // after = pmccntr -> x12
  assembler.embed_uint32(ISB_SY);
  assembler.embed_uint32(MRS_PMCCNTR_X12);

  // if ((after - before) > THRESH) break;
  assembler.sub(a64::x12, a64::x12, a64::x11);
  assembler.cmp(a64::x12, (uint64_t) SYNC_REF_THRESH);
  assembler.b_gt(wend);     // jg
  assembler.b_al(wbegin);   // jmp (unbedingt: B always)
  assembler.bind(wend);
}
#endif

#ifdef ENABLE_JSON

void to_json(nlohmann::json &j, const CodeJitter &p) {
  j = {{"pattern_sync_each_ref", p.pattern_sync_each_ref},
       {"flushing_strategy", to_string(p.flushing_strategy)},
       {"fencing_strategy", to_string(p.fencing_strategy)},
       {"total_activations", p.total_activations},
       {"num_aggs_for_sync", p.num_aggs_for_sync}
  };
}

void from_json(const nlohmann::json &j, CodeJitter &p) {
  j.at("pattern_sync_each_ref").get_to(p.pattern_sync_each_ref);
  from_string(j.at("flushing_strategy"), p.flushing_strategy);
  from_string(j.at("fencing_strategy"), p.fencing_strategy);
  j.at("total_activations").get_to(p.total_activations);
  j.at("num_aggs_for_sync").get_to(p.num_aggs_for_sync);
}

#endif
