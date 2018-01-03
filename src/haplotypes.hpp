#ifndef HAPLOTYPE_FWD_ALG_H
#define HAPLOTYPE_FWD_ALG_H

#include <cmath>
#include <vector>
#include <iostream>

#include "vg.pb.h"
#include "xg.hpp"

#include <gbwt/gbwt.h>
#include <gbwt/dynamic_gbwt.h>

using namespace std;

////////////////////////////////////////////////////////////////////////////////
// GENERAL USE
////////////////////////////////////////////////////////////////////////////////
// 
// A. Construct the following shared objects
//    1. an index, either a
//        i.   xg::XG index
//        ii.  gbwt::GBWT
//        iii. gbwt::DynamicGBWT
//    2. a memo for shared values used in calculations; a
//             haplo::haploMath::RRMemo, which takes the parameters
//                    i.    double -log(recombination probability)
//                    ii.   size_t population size
//
// B. Then, on a per-query-path basis (ie a vg::Path), receive score from a
//    haplo::haplo_DP::score(const vg::Path&, indexType& index, haploMath::RRMemo memo)
//    which takes in inputs
//        i.   const vg::Path& Path
//        ii.  indexType& index (where indexType is one of
//             a. xg::XG
//             b. gbwt::GBWT
//             c. gbwt::DynamicGBWT
//        iii. haploMath::RRMemo
//    and returns an output
//        pair<double, bool> where
//             arg 1  double  log(calculate probability)
//             arg 2  bool    whether the path is valid with respect to the index
//                            in terms of whether all edges in the path exist in
//                            the index
//
////////////////////////////////////////////////////////////////////////////////

namespace haplo {
  
using thread_t = vector<xg::XG::ThreadMapping>;

namespace haploMath{
  double logsum(double a, double b);
  double logdiff(double a, double b);
  double int_weighted_sum(vector<double> values, vector<int64_t> counts);
  double int_weighted_sum(double* values, int64_t* counts, size_t n_entries);

  // ---------------------------------------------------------------------------
  //  RRMemo
  //  Created by Jordan Eizenga on 6/21/16
  // ---------------------------------------------------------------------------
  //  Stores memoized values of constants and scaling factors which are used in
  //  the forward matrix extension
  // ---------------------------------------------------------------------------
  //
  //
  struct RRMemo {
  private:
    // LINEAR SPACE CONSTANTS --------------------------------------------------
    size_t population_size;
    double exp_rho;                          // lin space recombination penalty
    
    // LOG SPACE CONSTANTS -----------------------------------------------------
    double rho;                              // log space recombination penalty
    double log_continue_probability;         // 
    std::vector<double> logS_bases;

  public:
    RRMemo(double recombination_penalty, size_t population_size);

    double log_population_size();
    double log_recombination_penalty();
    double log_continue_factor(int64_t totwidth);
    
    double logT_base;
    double logT(int width);
    double logS(int height, int width);
    double logRRDiff(int height, int width);
  };
}

// -----------------------------------------------------------------------------

struct int_itvl_t{
  int64_t bottom;
  int64_t top;
  int64_t size() const;
  bool empty() const;
  static bool nondisjoint(const int_itvl_t& A, const int_itvl_t& B);
  static bool disjoint(const int_itvl_t& A, const int_itvl_t& B);
  // NB that the set of int_itvl_t's is closed under intersections but not under
  // unions or differences
  static int_itvl_t intersection(const int_itvl_t& A, const int_itvl_t& B);
};

// -----------------------------------------------------------------------------

struct haplo_DP_edge_memo {
private:  
  vector<vg::Edge> in;
  vector<vg::Edge> out;
public:
  haplo_DP_edge_memo();                      // for constructing null edge_memos
  haplo_DP_edge_memo(xg::XG& graph, 
                     xg::XG::ThreadMapping last_node, 
                     xg::XG::ThreadMapping node);
  const vector<vg::Edge>& edges_in() const;
  const vector<vg::Edge>& edges_out() const;
};

// -----------------------------------------------------------------------------

class hDP_graph_accessor {
public:
  const xg::XG::ThreadMapping old_node;
  const xg::XG::ThreadMapping new_node;
  const haplo_DP_edge_memo edges;
  const xg::XG& graph;
  haploMath::RRMemo& memo;
  
  // accessor for noninitial nodes in a haplotype
  hDP_graph_accessor(xg::XG& graph, 
                     xg::XG::ThreadMapping old_node, 
                     xg::XG::ThreadMapping new_node, 
                     haploMath::RRMemo& memo);
  // accessor for initial node in a haplotype                   
  // old_node and edge-vectors are null; do not use to extend nonempty states
  hDP_graph_accessor(xg::XG& graph, 
                     xg::XG::ThreadMapping new_node,
                     haploMath::RRMemo& memo);
                     
  int64_t new_side() const;
  int64_t new_height() const;
  int64_t old_height() const;
  int64_t new_length() const;
  
  bool has_edge() const;
  bool inclusive_interval() const { return false; }
};

// -----------------------------------------------------------------------------

struct gbwt_thread_t {
private:
  vector<gbwt::node_type> nodes;
  vector<size_t> node_lengths;
public:
  gbwt_thread_t();
  gbwt_thread_t(const vector<gbwt::node_type>& nodes, const vector<size_t>& node_lengths);
  void push_back(gbwt::node_type node, size_t node_length);
  gbwt::node_type& operator[](size_t i);
  const gbwt::node_type& operator[](size_t i) const;
  gbwt::node_type& back();
  const gbwt::node_type& back() const;
  size_t nodelength(size_t i) const;
  size_t size() const;
};

gbwt_thread_t path_to_gbwt_thread_t(const vg::Path& path);

template<class GBWTType>
class hDP_gbwt_graph_accessor {
public:
  const gbwt::node_type old_node;
  const gbwt::node_type new_node;
  const GBWTType& graph;
  haploMath::RRMemo& memo;
  size_t length;
  
  // accessor for noninitial nodes in a haplotype
  hDP_gbwt_graph_accessor(GBWTType& graph, 
                          gbwt::node_type old_node, 
                          gbwt::node_type new_node,
                          size_t new_length, 
                          haploMath::RRMemo& memo);
  // accessor for initial node in a haplotype                   
  // old_node and edge-vectors are null; do not use to extend nonempty states
  hDP_gbwt_graph_accessor(GBWTType& graph, 
                          gbwt::node_type new_node,
                          size_t new_length,
                          haploMath::RRMemo& memo);
                     
  size_t new_length() const;
  size_t new_side() const;
  bool inclusive_interval() const { return true; }
};

// -----------------------------------------------------------------------------

struct haplo_DP_rectangle{
private:
  typedef pair<size_t, size_t> gen_range_t;
  typedef size_t gen_flat_node;
  
  int64_t inner_value;
  gen_range_t state = make_pair(1, 0);
  gen_flat_node flat_node;
  int64_t previous_index = -1;
  bool int_is_inc;
public:
  haplo_DP_rectangle();
  haplo_DP_rectangle(bool inclusive_interval);
  double R;
  void set_offset(int offset);
  void extend(hDP_graph_accessor& ga);
  template<class accessorType>
  void false_extend(accessorType& ga, int_itvl_t delta);
  template<class GBWTType>
  void extend(hDP_gbwt_graph_accessor<GBWTType>& ga);
  int64_t I() const;
  int64_t interval_size() const;
  void set_prev_idx(int64_t index);
  int64_t prev_idx() const;
  bool is_new() const;
  void calculate_I(int64_t succ_o_val);
};

// -----------------------------------------------------------------------------

struct haplo_DP_column {
private:
  vector<double> previous_values;
  vector<int64_t> previous_sizes;
  vector<haplo_DP_rectangle*> entries;
  double previous_sum;
  double sum;
  double length;
  template<class accessorType>
  void binary_extend_intervals(accessorType& ga, 
                               int_itvl_t indices, 
                               int_itvl_t ss_deltas, 
                               int_itvl_t state_sizes);
  template<class accessorType>
  void standard_extend(accessorType& ga);
  void update_inner_values();
  void update_score_vector(haploMath::RRMemo& memo);
  double previous_R(size_t i) const;
public:
  template<class accessorType>
  haplo_DP_column(accessorType& ga);
  ~haplo_DP_column();
  template<class accessorType>
  void extend(accessorType& ga);
  vector<int64_t> get_sizes() const;
  vector<double> get_scores() const;
  double current_sum() const;
  void print(ostream& out) const;
};

thread_t path_to_thread_t(const vg::Path& path);

//------------------------------------------------------------------------------
// Outward-facing
//------------------------------------------------------------------------------
// return type for score function:
// -  haplo_score_type.first       double      log_continue_probability
// -  haplo_score_type.second      bool        true iff all edges in path/thread
//                                             exist in the index            
typedef pair<double, bool> haplo_score_type;
//------------------------------------------------------------------------------

struct haplo_DP {
private:
  haplo_DP_column DP_column;
public:
//------------------------------------------------------------------------------
// API functions
  static haplo_score_type score(const vg::Path& path, xg::XG& graph, haploMath::RRMemo& memo);
  template<class GBWTType>
  static haplo_score_type score(const vg::Path& path, GBWTType& graph, haploMath::RRMemo& memo);
//------------------------------------------------------------------------------

// public member functions which are not part of the API
  template<class accessorType>
  haplo_DP(accessorType& ga);
  haplo_DP_column* get_current_column();
  static haplo_score_type score(const thread_t& thread, xg::XG& graph, haploMath::RRMemo& memo);
  template<class GBWTType>
  static haplo_score_type score(const gbwt_thread_t& thread, GBWTType& graph, haploMath::RRMemo& memo);
};


//------------------------------------------------------------------------------
// template implementations
//------------------------------------------------------------------------------

template<class GBWTType>
hDP_gbwt_graph_accessor<GBWTType>::hDP_gbwt_graph_accessor(GBWTType& graph, 
                                                 gbwt::node_type new_node,
                                                 size_t new_length, 
                                                 haploMath::RRMemo& memo) :
  graph(graph), length(new_length),
  old_node(gbwt::invalid_node()), new_node(new_node), memo(memo) {
    
}

template<class GBWTType>
hDP_gbwt_graph_accessor<GBWTType>::hDP_gbwt_graph_accessor(GBWTType& graph, 
                                                 gbwt::node_type old_node, 
                                                 gbwt::node_type new_node,
                                                 size_t new_length, 
                                                 haploMath::RRMemo& memo) :
  graph(graph), length(new_length),
  old_node(old_node), new_node(new_node), memo(memo) {
    
}

template<class GBWTType>
size_t hDP_gbwt_graph_accessor<GBWTType>::new_length() const {
  return length;
}

template<class GBWTType>
size_t hDP_gbwt_graph_accessor<GBWTType>::new_side() const {
  return new_node;
}

//------------------------------------------------------------------------------

template<class GBWTType>
void haplo_DP_rectangle::extend(hDP_gbwt_graph_accessor<GBWTType>& ga) {
  if(previous_index == -1) {
    state = make_pair(0, ga.graph.nodeSize(ga.new_node) - 1);
  } else {
    state = ga.graph.LF(ga.old_node, state, ga.new_node);
  }
  flat_node = ga.new_node;
  inner_value = -1;
}

template<class accessorType>
void haplo_DP_rectangle::false_extend(accessorType& ga, 
                                      int_itvl_t delta) {
  flat_node = ga.new_side();
  state.first -= delta.bottom;
  state.second -= delta.top;
}

//------------------------------------------------------------------------------

template<class accessorType>
haplo_DP_column::haplo_DP_column(accessorType& ga) {
  haplo_DP_rectangle* first_rectangle = new haplo_DP_rectangle(ga.inclusive_interval());
  entries = {first_rectangle};
  first_rectangle->extend(ga);
  update_inner_values();
  update_score_vector(ga.memo);
}

template<class accessorType>
void haplo_DP_column::standard_extend(accessorType& ga) {
  previous_values = get_scores();
  previous_sizes = get_sizes();
  haplo_DP_rectangle* new_rectangle = new haplo_DP_rectangle(ga.inclusive_interval());
  new_rectangle->extend(ga);
  vector<haplo_DP_rectangle*> new_entries = {new_rectangle};
  for(size_t i = 0; i < entries.size(); i++) {
    // extend candidate
    haplo_DP_rectangle* candidate = entries[i];
    candidate->set_prev_idx(i);
    candidate->extend(ga);
    // check if the last rectangle added is nonempty
    if(candidate->interval_size() == new_entries.back()->interval_size()) {
      haplo_DP_rectangle* to_delete = new_entries.back();
      new_entries.pop_back();
      if(to_delete->prev_idx() != -1) {
        entries[to_delete->prev_idx()] = nullptr;
      }
      delete to_delete;
    }
    if(candidate->interval_size() != 0) {
      new_entries.push_back(candidate);
    } else {
      break;
    }
  }
  entries = new_entries;
  update_inner_values();
}

template<class accessorType>
void haplo_DP_column::extend(accessorType& ga) {
  standard_extend(ga);
  length = (double)(ga.new_length());
  update_score_vector(ga.memo);
}

//------------------------------------------------------------------------------

template<class accessorType>
haplo_DP::haplo_DP(accessorType& ga) : DP_column(haplo_DP_column(ga)) {
  
}

template<class GBWTType>
haplo_score_type haplo_DP::score(const vg::Path& path, GBWTType& graph, haploMath::RRMemo& memo) {
  return score(path_to_gbwt_thread_t(path), graph, memo);
}

template<class GBWTType>
haplo_score_type haplo_DP::score(const gbwt_thread_t& thread, GBWTType& graph, haploMath::RRMemo& memo) {
  hDP_gbwt_graph_accessor<GBWTType> ga_i(graph, thread[0], thread.nodelength(0), memo);
  haplo_DP hdp(ga_i);
  for(size_t i = 1; i < thread.size(); i++) {
    hDP_gbwt_graph_accessor<GBWTType> ga(graph, thread[i-1], thread[i], thread.nodelength(i), memo);
    hdp.DP_column.extend(ga);
  }
  return pair<double, bool>(hdp.DP_column.current_sum(), true);
}

} // namespace haplo

#endif