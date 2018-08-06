/**
 * Copyright 2018, IBM.
 *
 * This source code is licensed under the Apache License, Version 2.0 found in
 * the LICENSE.txt file in the root directory of this source tree.
 */

/**
 * @file    operations.hpp
 * @brief   Simualator operations
 * @author  Christopher J. Wood <cjwood@us.ibm.com>
 */

#ifndef _aer_framework_operations_hpp_
#define _aer_framework_operations_hpp_

#include <algorithm> 
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <tuple>

#include "framework/types.hpp"
#include "framework/json.hpp"

namespace AER {
namespace Operations {

//------------------------------------------------------------------------------
// Op Class
//------------------------------------------------------------------------------

struct Op {
  // General Operations
  std::string name;               // operation name
  reg_t qubits;                   //  qubits operation acts on
  std::vector<complex_t> params;  // real or complex params for gates

  // Conditional Operations
  bool conditional = false; // is gate conditional gate
  uint_t conditional_reg;   // (opt) the (single) register location to look up for conditional
  
  // Measurement
  reg_t memory;             // (opt) register operation it acts on (measure)
  reg_t registers;          // (opt) register locations it acts on (measure, conditional)

  // Mat and Kraus
  std::vector<cmatrix_t> mats;

  // Snapshots
  using qubit_set_t = std::set<uint_t, std::greater<uint_t>>;
  using pauli_component_t = std::tuple<complex_t,    // component coefficient
                                       qubit_set_t,  // component qubits
                                       std::string>; // component pauli string
  using matrix_component_t = std::tuple<complex_t,          // component coefficient
                                        std::vector<reg_t>, // qubit subregisters Ex: [[2], [1, 0]]
                                        std::vector<cmatrix_t>>; // submatrices Ex: [M2, M10]
  std::string label;       // label for observables snapshots
  std::vector<pauli_component_t> params_pauli_obs;
  std::vector<matrix_component_t> params_mat_obs; // not that diagonal matrices are stored as
                                                  // 1 x M row-matrices
                                                  // Projector vectors are stored as
                                                  // M x 1 column-matrices
};

//------------------------------------------------------------------------------
// Error Checking
//------------------------------------------------------------------------------

inline void check_name(const std::string &name) {
  if (name.empty()) {
    throw std::invalid_argument("Invalid gate operation: \"name\" is empty.");
  }
};

inline void check_qubits(const reg_t &qubits) {
  // Check qubits isn't empty
  if (qubits.empty()) {
    throw std::invalid_argument("Invalid operation (\"qubits\" are empty)");
  }
  // Check qubits are unique
  auto cpy = qubits;
  std::unique(cpy.begin(), cpy.end());
  if (cpy != qubits) {
    throw std::invalid_argument("Invalid operation (\"qubits\" are not unique)");
  }
};


//------------------------------------------------------------------------------
// JSON conversion
//------------------------------------------------------------------------------

// Main JSON deserialization functions
Op json_to_op(const json_t &js); // Patial TODO
inline void from_json(const json_t &js, Op &op) {op = json_to_op(js);};

// Helper deserialization functions
Op json_to_op_gate(const json_t &js);
Op json_to_op_measure(const json_t &js);
Op json_to_op_reset(const json_t &js);

// Matrices
Op json_to_op_mat(const json_t &js);
Op json_to_op_dmat(const json_t &js);
Op json_to_op_kraus(const json_t &js);

// Snapshots
Op json_to_op_snapshot(const json_t &js);
Op json_to_op_snapshot_state(const json_t &js);
Op json_to_op_snapshot_matrix(const json_t &js);
Op json_to_op_snapshot_pauli(const json_t &js);
Op json_to_op_snapshot_probs(const json_t &js);

// TODO Classical bits
//Op json_to_op_roerror(const json_t &js); // TODO
//Op json_to_op_bfunc(const json_t &js); // TODO


//------------------------------------------------------------------------------
// Implementation: JSON deserialization
//------------------------------------------------------------------------------

// TODO: convert if-else to switch
Op json_to_op(const json_t &js) {
  // load operation identifier
  std::string name;
  JSON::get_value(name, "name", js);
  // Measure & Reset
  if (name == "measure")
    return json_to_op_measure(js);
  if (name == "reset")
    return json_to_op_reset(js);
  // Arbitrary matrix gates
  if (name == "mat")
    return json_to_op_mat(js);
  if (name == "dmat")
    return json_to_op_dmat(js);
  if (name == "kraus")
    return json_to_op_kraus(js);
  // Snapshot
  if (name == "snapshot")
    return json_to_op_snapshot(js);
  // Bit functions
  /* TODO: the following aren't implemented yet!
  if (name == "bfunc")
    return json_to_op_bfunc(js);
  if (name == "roerror")
    return json_to_op_roerror(js);
  */
  // Gates
  return json_to_op_gate(js);
}


//------------------------------------------------------------------------------
// Implementation: Gates, measure, reset deserialization
//------------------------------------------------------------------------------

Op json_to_op_gate(const json_t &js) {
  Op op;
  JSON::get_value(op.name, "name", js);
  JSON::get_value(op.qubits, "qubits", js);
  JSON::get_value(op.params, "params", js);

  // Validation
  check_name(op.name);
  check_qubits(op.qubits);
  return op;
}


Op json_to_op_measure(const json_t &js) {
  Op op;
  op.name = "measure";
  JSON::get_value(op.qubits, "qubits", js);
  JSON::get_value(op.memory, "memory", js);
  JSON::get_value(op.registers, "register", js);

  // Validation
  check_qubits(op.qubits);
  if (op.memory.empty() == false && op.memory.size() != op.qubits.size()) {
    throw std::invalid_argument("Invalid measure operation: \"memory\" and \"qubits\" are different lengths.");
  }
  if (op.registers.empty() == false && op.registers.size() != op.qubits.size()) {
    throw std::invalid_argument("Invalid measure operation: \"register\" and \"qubits\" are different lengths.");
  }
  return op;
}


Op json_to_op_reset(const json_t &js) {
  Op op;
  op.name = "reset";
  JSON::get_value(op.qubits, "qubits", js);
  JSON::get_value(op.params, "params", js);
  // If params is missing default reset to 0
  if (op.params.empty()) {
    op.params = cvector_t(op.qubits.size(), 0.);
  }
  // Validation
  check_qubits(op.qubits);
  if (op.params.size() != op.qubits.size()) {
    throw std::invalid_argument("Invalid reset operation: \"params\" and \"qubits\" are different lengths.");
  }
  return op;
}


//------------------------------------------------------------------------------
// Implementation: Matrix and Kraus deserialization
//------------------------------------------------------------------------------

Op json_to_op_mat(const json_t &js) {
  Op op;
  op.name = "mat";
  JSON::get_value(op.qubits, "qubits", js);
  cmatrix_t mat;
  JSON::get_value(mat, "params", js);
  op.mats.push_back(mat);

  // Validation
  check_qubits(op.qubits);
  return op;
}

Op json_to_op_dmat(const json_t &js) {
  Op op;
  op.name = "dmat";
  JSON::get_value(op.qubits, "qubits", js);
  JSON::get_value(op.params, "params", js); // store diagonal as complex vector

  // Validation
  check_qubits(op.qubits);
  return op;
}


Op json_to_op_kraus(const json_t &js) {
  Op op;
  op.name = "kraus";
  JSON::get_value(op.qubits, "qubits", js);
  JSON::get_value(op.mats, "params", js);

  // Validation
  check_qubits(op.qubits);
  return op;
}

//------------------------------------------------------------------------------
// Implementation: Snapshot deserialization
//------------------------------------------------------------------------------

Op json_to_op_snapshot(const json_t &js) {
  if (JSON::check_key("params", js)) {
    std::string type;
    JSON::get_value(type, "type", js["params"]);
    if (type == "state")
      return json_to_op_snapshot_state(js);
    if (type == "probabilities")
      return json_to_op_snapshot_probs(js);
    if (type == "pauli_observable")
      return json_to_op_snapshot_pauli(js);
    if (type == "matrix_observable")
      return json_to_op_snapshot_matrix(js);
  }
  throw std::invalid_argument("Invalid snapshot instruction: \"params\" field incomplete.");
}


Op json_to_op_snapshot_state(const json_t &js) {
  Op op;
  op.name = "snapshot_state";
  JSON::get_value(op.label, "label", js);
  return op;
}


Op json_to_op_snapshot_probs(const json_t &js) {
  Op op;
  op.name = "snapshot_probs";
  JSON::get_value(op.qubits, "qubits", js["params"]);
  JSON::get_value(op.label, "label", js);
  
  // Validation
  check_qubits(op.qubits);
  return op;
}


Op json_to_op_snapshot_pauli(const json_t &js) {
  Op op;
  op.name = "snapshot_pauli";
  JSON::get_value(op.label, "label", js);
  double threshold = 1e-10; // drop small Pauli values

  // Sort qubits (needed for storing cached Pauli terms)
  reg_t unsorted = op.qubits; // store original unsorted order
  std::sort(op.qubits.begin(), op.qubits.end(), std::greater<uint_t>());

  // Get components
  if (JSON::check_key("components", js["params"])) {
    for (const auto &comp : js["params"]["components"]) {
      complex_t coeff;
      JSON::get_value(coeff, "coeff", comp);
      if (std::abs(coeff) > threshold) { // if coeff is too small ignore component
        // When qubits are loaded as a set they are sorted in descending order
        // So we need to sort the Pauli string to match:
        reg_t qubits_unsorted;
        JSON::get_value(qubits_unsorted, "qubits", comp);
        Op::qubit_set_t qubits_sorted(qubits_unsorted.begin(), qubits_unsorted.end());
        std::string pauli_unsorted, pauli_sorted;
        JSON::get_value(pauli_unsorted, "op", comp);
        // Sort Pauli string to match sorted qubit order
        for (const auto qubit: qubits_sorted) {
          auto pos = std::distance(qubits_unsorted.begin(),
                                   std::find(qubits_unsorted.begin(), qubits_unsorted.end(), qubit));
          pauli_sorted.push_back(pauli_unsorted[pos]);
        }
        if (pauli_sorted.size() != qubits_sorted.size()) {
          throw std::invalid_argument("Invalid Pauli snapshot (length \"coeffs\" != length \"qubits\").");
        }
        // make tuple and add to components
        op.params_pauli_obs.push_back(std::make_tuple(coeff, qubits_sorted, pauli_sorted));
      } // end if > threshold
    } // end component loop
  } else {
    throw std::invalid_argument("Invalid Pauli snapshot  (\"components\" field missing).");
  }

  return op;
}


Op json_to_op_snapshot_matrix(const json_t &js) {
  Op op;
  op.name = "snapshot_matrix";
  JSON::get_value(op.label, "label", js);

  // Get components
  if (JSON::check_key("components", js["params"])) {
    for (const auto &comp : js["params"]["components"]) {
      
      Op::matrix_component_t param;
      auto &qubits = std::get<1>(param);
      auto &mats = std::get<2>(param);
      JSON::get_value(std::get<0>(param), "coeff", comp);
      JSON::get_value(qubits, "qubits", comp);
      JSON::get_value(mats, "ops", comp);
      // Check correct lengths
      if (qubits.size() != mats.size()) {
        throw std::invalid_argument("Invalid matrix snapshot (number of qubit subsets and matrices unequal).");
      }
      // Check subset are ok
      size_t num = 0;
      std::set<uint_t> unique;
      for (const auto &reg : qubits) {
        num += reg.size();
        unique.insert(reg.begin(), reg.end());
      }
      if (unique.size() != qubits.size() || num != qubits.size()) {
        throw std::invalid_argument("Invalid matrix snapshot (component qubit specification invalid.");
      }
      // make tuple and add to components
      op.params_mat_obs.push_back(param);
    } // end component loop
  } else {
    throw std::invalid_argument("Invalid matrix snapshot  (\"components\" field missing).");
  }

  return op;
}

//------------------------------------------------------------------------------
} // end namespace Operations
//------------------------------------------------------------------------------
} // end namespace AER
//------------------------------------------------------------------------------
#endif